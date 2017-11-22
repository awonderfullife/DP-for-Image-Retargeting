#include <utility>

#include "window.h"
#include "carver.h"

using namespace seam_carving;

window *main_window = nullptr;
window *side_window = nullptr;
size_t window_count = 0;

template <typename ...Args> void new_window(window *& wptr, Args &&...args) {
	assert(main_window == nullptr);
	wptr = new window(std::forward<Args>(args)...);
	++window_count;
}
void try_delete_window(window *&ptr) {
	if (ptr) {
		delete ptr;
		if (--window_count == 0) {
			PostQuitMessage(0);
		}
	}
}

image_rgba_u8 orig_img;
dynamic_retargeter retargeter;
sys_image *simg = nullptr;

void generate_sys_image(const image_rgba_u8 &img) {
	if (simg) {
		delete simg;
	}
	simg = new sys_image(main_window->get_dc(), img.width(), img.height());
	simg->copy_from_image(img);
}
template <typename ColorElem> void generate_sys_image(const image<color_rgba<ColorElem>> &img) {
	generate_sys_image(image_cast<color_rgba_u8>(img));
}

void restrict_size(bool adjfirst, bool adjsecond, LONG &f, LONG &s, size_t delta, size_t min, size_t max) {
	size_t cursz = s - f - delta;
	cursz = (cursz > max ? max : (cursz < min ? min : cursz));
	if (adjfirst) {
		f = s - cursz - delta;
	} else if (adjsecond) {
		s = f + cursz + delta;
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
		try_delete_window(main_window);
		return 0;
	case WM_PAINT:
		if (simg) {
			simg->display(main_window->get_dc());
		}
		return 0;
	case WM_SIZING:
		{
			RECT &r = *reinterpret_cast<RECT*>(lparam), wndr, clientr;
			SC_WINAPI_CHECK(GetWindowRect(main_window->get_handle(), &wndr));
			SC_WINAPI_CHECK(GetClientRect(main_window->get_handle(), &clientr));
			size_t
				xdiff = wndr.right - wndr.left - clientr.right, ydiff = wndr.bottom - wndr.top - clientr.bottom,
				width = std::max<size_t>(2, r.right - r.left - xdiff), height = std::max<size_t>(2, r.bottom - r.top - ydiff);
			retargeter.retarget(width, height);
			generate_sys_image(retargeter.get_image());
			restrict_size(wparam, r, xdiff, ydiff, 2, retargeter.get_image().width(), 2, retargeter.get_image().height());
		}
		return TRUE;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

int main() {
	new_window(
		main_window, TEXT("main_window"), main_window_proc,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX
	);

	image_loader loader;
	orig_img = loader.load_image(L"image.png");
	main_window->set_client_size(orig_img.width(), orig_img.height());
	retargeter.set_image(orig_img);
	generate_sys_image(orig_img);

	main_window->show();
	while (window::wait_message_all()) {
	}
	return 0;
}
