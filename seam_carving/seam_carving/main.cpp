#include <utility>
#include <chrono>

#include "window.h"
#include "carver.h"
#include "dancing_link_carver.h"

using namespace seam_carving;

//#define USE_DL_CARVER

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

#ifdef USE_DL_CARVER
enum class enlarge_status {
	none,
	horizontal,
	vertical
};
struct image_enlarger {
public:
	void prepare(retargeter_t::enlarge_table_t tbl, enlarge_status t) {
		table = std::move(tbl);
		repeat = dynamic_array2<bool>(orig_img.width(), orig_img.height());
		repeat.memset(false);
		enlarged_size = 0;
		type = t;
	}

	bool can_enlarge() const {
		return enlarged_size < table.size();
	}
	void enlarge() {
		for (auto &&pair : table[enlarged_size]) {
			repeat.at(pair.first, pair.second) = true;
		}
		++enlarged_size;
	}
	bool can_shrink() const {
		return enlarged_size > 0;
	}
	void shrink() {
		--enlarged_size;
		for (auto &&pair : table[enlarged_size]) {
			repeat.at(pair.first, pair.second) = false;
		}
	}
	void retarget(size_t w, size_t h) {
		size_t ev, et;
		if (type == enlarge_status::horizontal) {
			ev = orig_img.width();
			et = w;
		} else if (type == enlarge_status::vertical) {
			ev = orig_img.height();
			et = h;
		}
		if (can_enlarge() && ev + enlarged_size < et) {
			do {
				enlarge();
			} while (can_enlarge() && ev + enlarged_size < et);
		} else if (can_shrink() && ev + enlarged_size > et) {
			do {
				shrink();
			} while (can_shrink() && ev + enlarged_size > et);
		}
	}

	sys_image get_sys_image(HDC dc) const {
		return type == enlarge_status::horizontal ? get_sys_image_horizontal(dc) : get_sys_image_vertical(dc);
	}
	image_rgba_u8 get_image() const {
		return type == enlarge_status::horizontal ? get_image_horizontal() : get_image_vertical();
	}

	sys_image get_sys_image_horizontal(HDC dc) const {
		sys_image img(dc, orig_img.width() + enlarged_size, orig_img.height());
		_get_image_horizontal_impl(img);
		return img;
	}
	image_rgba_u8 get_image_horizontal() const {
		image_rgba_u8 img(orig_img.width() + enlarged_size, orig_img.height());
		_get_image_horizontal_impl(img);
		return img;
	}

	sys_image get_sys_image_vertical(HDC dc) const {
		sys_image img(dc, orig_img.width(), orig_img.height() + enlarged_size);
		_get_image_vertical_impl(img);
		return img;
	}
	image_rgba_u8 get_image_vertical() const {
		image_rgba_u8 img(orig_img.width(), orig_img.height() + enlarged_size);
		_get_image_vertical_impl(img);
		return img;
	}

	size_t current_width() const {
		return orig_img.width() + (type == enlarge_status::horizontal ? enlarged_size : 0);
	}
	size_t current_height() const {
		return orig_img.height() + (type == enlarge_status::vertical ? enlarged_size : 0);
	}

	retargeter_t::enlarge_table_t table;
	dynamic_array2<bool> repeat;
	size_t enlarged_size = 0;
	enlarge_status type = enlarge_status::none;
protected:
	template <typename Img> void _get_image_horizontal_impl(Img &img) const {
		for (size_t y = 0; y < orig_img.height(); ++y) {
			const color_rgba_u8 *src = orig_img.at_y(y);
			Img::element_type *dst = img.at_y(y);
			const bool *rep = repeat.at_y(y);
			for (size_t x = 0; x < orig_img.width(); ++x, ++src, ++dst, ++rep) {
				*dst = Img::element_type(*src);
				if (*rep) {
					dst[1] = dst[0];
					++dst;
				}
			}
		}
	}
	template <typename Img> void _get_image_vertical_impl(Img &img) const {
		for (size_t x = 0; x < orig_img.width(); ++x) {
			for (size_t y = 0, dy = 0; y < orig_img.height(); ++y, ++dy) {
				img.at(x, dy) = Img::element_type(orig_img.at(x, y));
				if (repeat.at(x, y)) {
					img.at(x, dy + 1) = img.at(x, dy);
					++dy;
				}
			}
		}
	}
};

image_enlarger enlarger;
#endif

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
"H: Prepare horizontal enlarging\n"
"V: Prepare vertical enlarging\n"
"\n"
"Left Mouse Button: Paint excluded region\n"
"Right Mouse Button: Paint favored region\n"
"A / Z: Increase / decrease paint brush radius (currently %u)\n";

void refresh_displayed_image(bool repaint) {
#ifdef USE_DL_CARVER
	if (enlarger.type == enlarge_status::none) {
		if (show_compensation) {
			simg = retargeter.get_sys_image<retargeter_t::blend_compensation>(main_window.get_dc());
		} else {
			simg = retargeter.get_sys_image<>(main_window.get_dc());
		}
	} else {
		simg = enlarger.get_sys_image(main_window.get_dc());
	}
#else
	auto img = retargeter.get_image();
	simg = sys_image(main_window.get_dc(), img.width(), img.height());
	simg.copy_from_image(img);
#endif
	if (repaint) {
		main_window.invalidate_visual();
	}
}
void fit_image_size() {
	main_window.set_client_size(simg.width(), simg.height());
	main_window.invalidate_visual();
}

std::chrono::high_resolution_clock::time_point now() {
	return std::chrono::high_resolution_clock::now();
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

void set_cursor(int cursor) {
	HANDLE img = LoadImage(
		nullptr, MAKEINTRESOURCE(cursor),
		IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE
	);
	SetCursor(static_cast<HCURSOR>(img));
}

template <typename T> inline bool hit_test_capsule(T x1, T y1, T x2, T y2, T px, T py, T rad) {
	rad *= rad;
	T dx = x2 - x1, dy = y2 - y1, pdx = px - x1, pdy = py - y1;
	if (-dy * pdy - dx * pdx >= 0) {
		return pdx * pdx + pdy * pdy < rad;
	}
	pdx = px - x2, pdy = py - y2;
	if (dy * pdy + dx * pdx >= 0) {
		return pdx * pdx + pdy * pdy < rad;
	}
	T res = pdx * dy - pdy * dx;
	res *= res;
	return res < (dx * dx + dy * dy) * rad;
}
#ifdef USE_DL_CARVER
void paint_compensate_region(size_t x1, size_t y1, size_t x2, size_t y2, retargeter_t::real_t v) {
	size_t
		ymin = std::max(std::min(y1, y2), brush_rad) - brush_rad,
		ymax = std::min(std::max(y1, y2) + brush_rad + 1, retargeter.current_height()),
		xmin = std::max(std::min(x1, x2), brush_rad) - brush_rad,
		xmax = std::min(std::max(x1, x2) + brush_rad + 1, retargeter.current_width());
	retargeter_t::ptr_t ynode = retargeter.at(xmin, ymin);
	for (size_t yp = ymin; yp < ymax; ++yp, ynode = retargeter.deref(ynode).down) {
		bool had = false;
		retargeter_t::ptr_t curptr = ynode;
		for (size_t xp = xmin; xp < xmax; ++xp, curptr = retargeter.deref(curptr).right) {
			if (hit_test_capsule<long long>(x1, y1, x2, y2, xp, yp, brush_rad)) {
				retargeter.deref(curptr).compensation = v;
				had = true;
			} else if (had) {
				break;
			}
		}
	}
	refresh_displayed_image(true);
}
#endif

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
				std::snprintf(tmp, sizeof(tmp), help_message, show_compensation ? "on" : "off", brush_rad);
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
#ifdef USE_DL_CARVER
			if (enlarger.type == enlarge_status::none) {
				retargeter.retarget(width, height);
				restrict_size(static_cast<int>(wparam), r, xdiff, ydiff, 2, retargeter.current_width(), 2, retargeter.current_height());
				refresh_displayed_image(true);
			} else {
				enlarger.retarget(width, height);
				restrict_size(static_cast<int>(wparam), r, xdiff, ydiff, orig_img.width(), enlarger.current_width(), orig_img.height(), enlarger.current_height());
				refresh_displayed_image(true);
			}
#else
			retargeter.retarget(width, height);
			restrict_size(static_cast<int>(wparam), r, xdiff, ydiff, 2, retargeter.current_width(), 2, retargeter.current_height());
			refresh_displayed_image(true);
#endif
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
				refresh_displayed_image(true);
				break;

#ifdef USE_DL_CARVER
			case 'R':
				enlarger.type = enlarge_status::none;
				retargeter.set_image(orig_img);
				refresh_displayed_image(false);
				fit_image_size();
				break;
			case 'S':
				{
					enlarger.type = enlarge_status::none;
					orig_img = retargeter.get_image<>();
					retargeter.set_image(orig_img);
					refresh_displayed_image(true);
				}
				break;
#endif

			case 'I':
				retargeter.retarget(retargeter.current_width(), retargeter.current_height() - 1);
				refresh_displayed_image(false);
				fit_image_size();
				break;
			case 'J':
				retargeter.retarget(retargeter.current_width() - 1, retargeter.current_height());
				refresh_displayed_image(false);
				fit_image_size();
				break;
			case 'K':
				retargeter.retarget(retargeter.current_width(), retargeter.current_height() + 1);
				refresh_displayed_image(false);
				fit_image_size();
				break;
			case 'L':
				retargeter.retarget(retargeter.current_width() + 1, retargeter.current_height());
				refresh_displayed_image(false);
				fit_image_size();
				break;

#ifdef USE_DL_CARVER
			case 'H':
				if (retargeter.is_carved() || enlarger.type != enlarge_status::none) {
					MessageBox(main_window.get_handle(), TEXT("The image must be at its original state"), TEXT("Error"), MB_OK);
				} else {
					auto begt = now();
					set_cursor(OCR_WAIT);
					enlarger.prepare(retargeter.prepare_horizontal_enlarging(), enlarge_status::horizontal);
					double dur = std::chrono::duration<double>(now() - begt).count();
					char msg[50];
					std::snprintf(msg, sizeof(msg), "Preparation cmoplete\nTime usage: %lfs", dur);
					MessageBoxA(main_window.get_handle(), msg, "Info", MB_OK);
				}
				break;
			case 'V':
				if (retargeter.is_carved() || enlarger.type != enlarge_status::none) {
					MessageBox(main_window.get_handle(), TEXT("The image must be at its original state"), TEXT("Error"), MB_OK);
				} else {
					auto begt = now();
					set_cursor(OCR_WAIT);
					enlarger.prepare(retargeter.prepare_vertical_enlarging(), enlarge_status::vertical);
					double dur = std::chrono::duration<double>(now() - begt).count();
					char msg[50];
					std::snprintf(msg, sizeof(msg), "Preparation cmoplete\nTime usage: %lfs", dur);
					MessageBoxA(main_window.get_handle(), msg, "Info", MB_OK);
				}
				break;
#endif

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

#ifdef USE_DL_CARVER
	case WM_MOUSEMOVE:
		{
			int rx = GET_X_LPARAM(lparam), ry = GET_Y_LPARAM(lparam);
			if (rx >= 0 && ry >= 0) {
				size_t x = static_cast<size_t>(rx), y = static_cast<size_t>(ry);
				if (x < retargeter.current_width() && y < retargeter.current_height()) {
					if (wparam & MK_LBUTTON) {
						paint_compensate_region(lastx, lasty, x, y, std::numeric_limits<retargeter_t::real_t>::infinity());
					} else if (wparam & MK_RBUTTON) {
						paint_compensate_region(lastx, lasty, x, y, -200000.0); // TODO magik!
					}
					lastx = x;
					lasty = y;
				}
			}
		}
		return 0;
#endif
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		MessageBox(nullptr, TEXT("Usage: seam_carving [filename]"), TEXT("Usage"), MB_OK);
		return 0;
	}

	main_window = window(
		TEXT("main_window"), main_window_proc,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX
	);
	fnt = font::get_default();

	{
		size_t nc = MultiByteToWideChar(CP_OEMCP, MB_PRECOMPOSED, argv[1], -1, nullptr, 0);
		LPWSTR str = new WCHAR[nc];
		MultiByteToWideChar(CP_OEMCP, MB_PRECOMPOSED, argv[1], -1, str, nc);
		image_loader loader;
		orig_img = loader.load_image(reinterpret_cast<LPCWSTR>(str));
		delete[] str;
	}
	retargeter.set_image(orig_img);
	refresh_displayed_image(false);
	fit_image_size();

	main_window.show();
	while (window::wait_message_all()) {
	}
	return 0;
}
