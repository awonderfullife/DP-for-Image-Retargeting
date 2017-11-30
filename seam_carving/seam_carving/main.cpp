#include <utility>
#include <chrono>

#include "window.h"
#include "carver.h"
#include "dancing_link_carver.h"

using namespace seam_carving;

#define USE_DL_CARVER

#ifdef USE_DL_CARVER
using retargeter_t = dancing_link_retargeter;
#else
using retargeter_t = dynamic_retargeter;
#endif

window main_window;
font fnt;

image_rgba_u8 orig_img;
retargeter_t retargeter;
sys_image simg;

bool show_help = true, show_compensation = true;
size_t brush_rad = 10, lastx = 0, lasty = 0;

constexpr char help_message[] =
"F1: Toggle help message\n"
"C: Toggle show favored / excluded regions (currently %s)\n"
"\n"
"R: Reset image\n"
"S: Set current image as original\n"
"\n"
"I: Remove a horizontal seam\n"
"J: Remove a vertical seam\n"
"K: Restore a horizontal seam\n"
"L: Restore a vertical seam\n"
"\n"
"Left Mouse Button: Paint excluded region\n"
"Right Mouse Button: Paint favored region\n"
"A / Z: Increase / decrease paint brush radius (currently %u)\n";

void refresh_displayed_image() {
#ifdef USE_DL_CARVER
	if (show_compensation) {
		simg = retargeter.get_sys_image<retargeter_t::blend_compensation>(main_window.get_dc());
	} else {
		simg = retargeter.get_sys_image<>(main_window.get_dc());
	}
#else
	generate_sys_image(retargeter.get_image());
#endif
	main_window.invalidate_visual();
}
void fit_image_size() {
	main_window.set_client_size(retargeter.current_width(), retargeter.current_height());
}

void restrict_size(bool adjfirst, bool adjsecond, LONG &f, LONG &s, size_t delta, size_t min, size_t max) {
	size_t cursz = s - f - delta;
	cursz = (cursz > max ? max : (cursz < min ? min : cursz));
	if (adjfirst) {
		f = s - static_cast<LONG>(cursz + delta);
	} else if (adjsecond) {
		s = f + static_cast<LONG>(cursz + delta);
	}
}
const bool resize_edges[][4] = {
	// left, top, right, bottom
	{false, false, false, false},
	{true, false, false, false},
	{false, false, true, false},
	{false, true, false, false},
	{true, true, false, false},
	{false, true, true, false},
	{false, false, false, true},
	{true, false, false, true},
	{false, false, true, true}
};
void restrict_size(int adjustdir, RECT &r, size_t dw, size_t dh, size_t minw, size_t maxw, size_t minh, size_t maxh) {
	restrict_size(resize_edges[adjustdir][0], resize_edges[adjustdir][2], r.left, r.right, dw, minw, maxw);
	restrict_size(resize_edges[adjustdir][1], resize_edges[adjustdir][3], r.top, r.bottom, dh, minh, maxh);
}

template <typename T> inline T get_sqr_ptln_distance(T x1, T y1, T x2, T y2, T px, T py) {
	T dx = x2 - x1, dy = y2 - y1;
	// ccw 90: -dy, dx
	T pdx = px - x1, pdy = py - y1;
	if (-dy * pdy - dx * pdx >= 0) {
		return pdx * pdx + pdy * pdy;
	}
	pdx = px - x2, pdy = py - y2;
	// ccw 90: dy, -dx
	if (dy * pdy + dx * pdx >= 0) {
		return pdx * pdx + pdy * pdy;
	}
	T res = pdx * dy - pdy * dx;
	res *= res;
	return res / (dx * dx + dy * dy);
}
void paint_compensate_region(size_t x, size_t y, retargeter_t::real_t v) {
	size_t
		ymin = std::max(std::min(y, lasty), brush_rad) - brush_rad,
		ymax = std::min(std::max(y, lasty) + brush_rad + 1, retargeter.current_height()),
		xmin = std::max(std::min(x, lastx), brush_rad) - brush_rad,
		xmax = std::min(std::max(x, lastx) + brush_rad + 1, retargeter.current_width());
	for (size_t yp = ymin; yp < ymax; ++yp) {
		for (size_t xp = xmin; xp < xmax; ++xp) {
			if (get_sqr_ptln_distance<long long>(x, y, lastx, lasty, xp, yp) < brush_rad * brush_rad) {
				retargeter.deref(retargeter.at(xp, yp)).compensation = v;
			}
		}
	}
	refresh_displayed_image();
}

LRESULT CALLBACK main_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		{
			RECT client;
			SC_WINAPI_CHECK(GetClientRect(main_window.get_handle(), &client));
			sys_image buffer(main_window.get_dc(), client.right, client.bottom);
			HDC bdc = buffer.get_dc();
			if (simg.valid()) {
				simg.display(bdc);
			}
			if (show_help) {
				fnt.select(bdc);
				RECT nr = client;
				nr.left = 10;
				nr.top = 10;
				char tmp[1000];
				snprintf(tmp, 1000, help_message, show_compensation ? "on" : "off", brush_rad);
				DrawTextA(bdc, tmp, -1, &nr, DT_TOP | DT_LEFT);
			}

			PAINTSTRUCT ps;
			HDC dc = BeginPaint(main_window.get_handle(), &ps);
			buffer.display(dc);
			EndPaint(main_window.get_handle(), &ps);
		}
		return 0;
	case WM_SIZING:
		{
			RECT &r = *reinterpret_cast<RECT*>(lparam), wndr, clientr;
			SC_WINAPI_CHECK(GetWindowRect(main_window.get_handle(), &wndr));
			SC_WINAPI_CHECK(GetClientRect(main_window.get_handle(), &clientr));
			size_t
				xdiff = wndr.right - wndr.left - clientr.right, ydiff = wndr.bottom - wndr.top - clientr.bottom,
				width = std::max<size_t>(2, r.right - r.left - xdiff), height = std::max<size_t>(2, r.bottom - r.top - ydiff);
			retargeter.retarget(width, height);
			restrict_size(static_cast<int>(wparam), r, xdiff, ydiff, 2, retargeter.current_width(), 2, retargeter.current_height());
			refresh_displayed_image();
		}
		return TRUE;
	case WM_KEYDOWN:
		{
			switch (wparam) {
			case VK_F1:
				show_help = !show_help;
				main_window.invalidate_visual();
				break;
			case 'C':
				show_compensation = !show_compensation;
				refresh_displayed_image();
				break;

			case 'R':
				retargeter.set_image(orig_img);
				fit_image_size();
				refresh_displayed_image();
				break;
			case 'S':
				{
					image_rgba_u8 img = retargeter.get_image<>();
					retargeter.set_image(img);
					refresh_displayed_image();
				}
				break;

			case 'I':
				retargeter.retarget(retargeter.current_width(), retargeter.current_height() - 1);
				fit_image_size();
				refresh_displayed_image();
				break;
			case 'J':
				retargeter.retarget(retargeter.current_width() - 1, retargeter.current_height());
				fit_image_size();
				refresh_displayed_image();
				break;
			case 'K':
				retargeter.retarget(retargeter.current_width(), retargeter.current_height() + 1);
				fit_image_size();
				refresh_displayed_image();
				break;
			case 'L':
				retargeter.retarget(retargeter.current_width() + 1, retargeter.current_height());
				fit_image_size();
				refresh_displayed_image();
				break;

			case 'A':
				++brush_rad;
				main_window.invalidate_visual();
				break;
			case 'Z':
				if (brush_rad > 0) {
					--brush_rad;
					main_window.invalidate_visual();
				}
				break;
			}
		}
		return 0;
	case WM_MOUSEMOVE:
		{
			int rx = GET_X_LPARAM(lparam), ry = GET_Y_LPARAM(lparam);
			if (rx >= 0 && ry >= 0) {
				size_t x = static_cast<size_t>(rx), y = static_cast<size_t>(ry);
				if (x < retargeter.current_width() && y < retargeter.current_height()) {
					if (wparam & MK_LBUTTON) {
						paint_compensate_region(x, y, std::numeric_limits<retargeter_t::real_t>::infinity());
					} else if (wparam & MK_RBUTTON) {
						paint_compensate_region(x, y, -200000.0); // TODO magik!
					}
					lastx = x;
					lasty = y;
				}
			}
		}
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

int main() {
	main_window = window(
		TEXT("main_window"), main_window_proc,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX
	);
	fnt = font::get_default();

	image_loader loader;
	orig_img = loader.load_image(L"image.jpg");
	retargeter.set_image(orig_img);
	fit_image_size();
	refresh_displayed_image();

	main_window.show();
	while (window::wait_message_all()) {
	}
	return 0;
}
