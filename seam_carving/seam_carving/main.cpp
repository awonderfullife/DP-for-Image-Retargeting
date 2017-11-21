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

image<color_rgba_u8> orig_img;
carver::image_rgba_r carved_img;
sys_image *simg = nullptr;

void generate_sys_image(const image<color_rgba_u8> &img) {
	if (simg) {
		delete simg;
	}
	simg = new sys_image(main_window->get_dc(), img.width(), img.height());
	simg->copy_from_image(img);
}
template <typename ColorElem> void generate_sys_image(const image<color_rgba<ColorElem>> &img) {
	generate_sys_image(image_cast<color_rgba_u8>(img));
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
	case WM_SIZE:
		{
			size_t w = LOWORD(lparam), h = HIWORD(lparam);
			while (carved_img.width() > w) {
				carver c;
				c.set_image(carved_img);
				carver::carve_path_data path = c.get_vertical_carve_path();
				carved_img = c.carve_vertical(path);
			}
			generate_sys_image(carved_img);
		}
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

int main() {
	new_window(main_window, TEXT("main_window"), main_window_proc);

	image_loader loader;
	orig_img = loader.load_image(L"image.png");
	main_window->set_client_size(orig_img.width(), orig_img.height());
	image_cast(orig_img, carved_img);
	generate_sys_image(orig_img);

	main_window->show();
	while (window::wait_message_all()) {
	}
	return 0;
}
