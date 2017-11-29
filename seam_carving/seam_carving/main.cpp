#include <utility>
#include <chrono>

#include "window.h"
#include "carver.h"
#include "dancing_link_carver.h"

using namespace seam_carving;

#define USE_DL_CARVER

window main_window;
font fnt;

image_rgba_u8 orig_img;
#ifdef USE_DL_CARVER
dancing_link_retargeter retargeter;
#else
dynamic_retargeter retargeter;
#endif
sys_image simg;

bool show_help = true;

constexpr TCHAR help_message[] = TEXT(
	"F1: Toggle help message\n"
	"(Hold)e: Paint excluded region\n"
);

bool is_painting_excluded_region() {
	return is_key_down('E');
}

void generate_sys_image(const image_rgba_u8 &img) {
	simg = sys_image(main_window.get_dc(), img.width(), img.height());
	simg.copy_from_image(img);
}
template <typename ColorElem> void generate_sys_image(const image<color_rgba<ColorElem>> &img) {
	generate_sys_image(image_cast<color_rgba_u8>(img));
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
				DrawText(bdc, help_message, -1, &nr, DT_TOP | DT_LEFT);
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
#ifdef USE_DL_CARVER
			simg = retargeter.get_sys_image(main_window.get_dc());
#else
			generate_sys_image(retargeter.get_image());
#endif
			restrict_size(static_cast<int>(wparam), r, xdiff, ydiff, 2, retargeter.current_width(), 2, retargeter.current_height());
			InvalidateRect(main_window.get_handle(), nullptr, false);
		}
		return TRUE;
	case WM_KEYDOWN:
		{
			switch (wparam) {
			case VK_F1:
				show_help = !show_help;
				InvalidateRect(main_window.get_handle(), nullptr, false);
				break;
			}
		}
		return 0;
	case WM_SETCURSOR:
		{
			if (LOWORD(lparam) == HTCLIENT) {
				int cid = is_painting_excluded_region() ? OCR_CROSS : OCR_NORMAL;
				SetCursor(static_cast<HCURSOR>(LoadImage(
					nullptr, MAKEINTRESOURCE(cid),
					IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE
				)));
				return TRUE;
			}
		}
		break;
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
	orig_img = loader.load_image(L"image.png");
	main_window.set_client_size(orig_img.width(), orig_img.height());
	retargeter.set_image(orig_img);
	generate_sys_image(orig_img);

	main_window.show();
	while (window::wait_message_all()) {
	}
	return 0;
}
