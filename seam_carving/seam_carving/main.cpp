#include <utility>

#include "window.h"

using namespace seam_carving;

window *main_window = nullptr;
window *side_window = nullptr;
image<color_rgba<unsigned char>> img;
sys_image *simg = nullptr;
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
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

int main() {
	new_window(main_window, TEXT("main_window"), main_window_proc);

	wic_image_loader loader;
	img = loader.load_image(L"image.png");
	simg = new sys_image(main_window->get_dc(), img.width(), img.height());
	simg->copy_from_image(img);
	main_window->set_client_size(img.width(), img.height());

	main_window->show();
	while (window::wait_message_all()) {
	}
	return 0;
}
