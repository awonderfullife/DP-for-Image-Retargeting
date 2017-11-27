#pragma once

#include <windowsx.h>

#include "image.h"

namespace seam_carving {
	class window {
	public:
		window(LPCTSTR cls_name, WNDPROC msg_handler, DWORD style = WS_OVERLAPPEDWINDOW) {
			HINSTANCE hinst = GetModuleHandle(nullptr);
			WNDCLASSEX wcex;
			std::memset(&wcex, 0, sizeof(wcex));
			wcex.style = CS_OWNDC;
			wcex.lpfnWndProc = msg_handler;
			wcex.cbSize = sizeof(wcex);
			wcex.hInstance = hinst;
			wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wcex.lpszClassName = cls_name;
			_wndatom = RegisterClassEx(&wcex);
			_hwnd = CreateWindowEx(
				0, cls_name, cls_name, style,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				nullptr, nullptr, hinst, nullptr
			);
		}
		~window() {
			DestroyWindow(_hwnd);
			UnregisterClass(reinterpret_cast<LPCTSTR>(_wndatom), GetModuleHandle(nullptr));
		}

		bool peek_message() {
			MSG msg;
			if (PeekMessage(&msg, _hwnd, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				return true;
			}
			return false;
		}
		inline static bool wait_message_all() {
			MSG msg;
			BOOL res = GetMessage(&msg, nullptr, 0, 0);
			assert(res != -1);
			if (res != 0 && res != -1) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				return true;
			}
			return false;
		}

		void set_center() {
			RECT r, work;
			GetWindowRect(_hwnd, &r);
			SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
			MoveWindow(
				_hwnd,
				(work.right - work.left - r.right + r.left) / 2 + work.left,
				(work.bottom - work.top - r.bottom + r.top) / 2 + work.top,
				r.right - r.left,
				r.bottom - r.top,
				false
			);
		}
		void set_client_size(size_t w, size_t h) {
			RECT r, c;
			GetWindowRect(_hwnd, &r);
			GetClientRect(_hwnd, &c);
			MoveWindow(
				_hwnd,
				r.left,
				r.top,
				static_cast<int>(w) - c.right + r.right - r.left,
				static_cast<int>(h) - c.bottom + r.bottom - r.top,
				false
			);
		}
		void set_caption(LPCTSTR str) {
			SetWindowText(_hwnd, str);
		}

		void get_client_mouse_pos(int &x, int &y) {
			POINT p;
			GetCursorPos(&p);
			ScreenToClient(_hwnd, &p);
			x = p.x;
			y = p.y;
		}

		void show() {
			ShowWindow(_hwnd, SW_SHOW);
		}
		void hide() {
			ShowWindow(_hwnd, SW_HIDE);
		}

		HWND get_handle() const {
			return _hwnd;
		}
		HDC get_dc() const {
			return GetDC(_hwnd);
		}
	protected:
		HWND _hwnd;
		ATOM _wndatom;
	};

	inline void get_xy_from_lparam(LPARAM lparam, SHORT &x, SHORT &y) {
		x = GET_X_LPARAM(lparam);
		y = GET_Y_LPARAM(lparam);
	}
}
