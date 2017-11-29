#pragma once

#define OEMRESOURCE
#include <windowsx.h>

#include "image.h"

namespace seam_carving {
	class window {
	public:
		window() = default;
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
		window(window &&src) : _hwnd(src._hwnd), _wndatom(src._wndatom) {
			src._hwnd = nullptr;
		}
		window(const window&) = delete;
		window &operator=(window &&src) {
			std::swap(_hwnd, src._hwnd);
			std::swap(_wndatom, src._wndatom);
			return *this;
		}
		window &operator=(const window&) = delete;
		~window() {
			close();
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

		void close() {
			if (valid()) {
				DestroyWindow(_hwnd);
				UnregisterClass(reinterpret_cast<LPCTSTR>(_wndatom), GetModuleHandle(nullptr));
				_hwnd = nullptr;
			}
		}

		HWND get_handle() const {
			return _hwnd;
		}
		HDC get_dc() const {
			return GetDC(_hwnd);
		}
		bool valid() const {
			return _hwnd != nullptr;
		}
	protected:
		HWND _hwnd = nullptr;
		ATOM _wndatom;
	};

	class font {
	public:
		font() = default;
		font(size_t height, LPCWSTR name) {
			_fon = CreateFont(
				static_cast<int>(height), 0, 0, 0,
				FW_DONTCARE, false, false, false, DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, name
			);
			SC_WINAPI_CHECK(_fon);
		}
		font(const LOGFONT &fdata) {
			_fon = CreateFontIndirect(&fdata);
			SC_WINAPI_CHECK(_fon);
		}
		font(font &&src) : _fon(src._fon) {
			src._fon = nullptr;
		}
		font(const font&) = delete;
		font &operator=(font &&src) {
			std::swap(_fon, src._fon);
			return *this;
		}
		font &operator=(const font&) = delete;
		~font() {
			if (valid()) {
				SC_WINAPI_CHECK(DeleteObject(_fon));
			}
		}

		HGDIOBJ select(HDC dc) {
			assert(valid());
			return SelectObject(dc, _fon);
		}
		void deselect(HDC dc, HGDIOBJ obj) {
			SelectObject(dc, obj);
		}

		HFONT get_handle() const {
			return _fon;
		}

		bool valid() const {
			return _fon != nullptr;
		}

		inline static font get_default() {
			NONCLIENTMETRICS ncm;
			ncm.cbSize = sizeof(ncm);
			SC_WINAPI_CHECK(SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0));
			return font(ncm.lfMenuFont);
		}
	protected:
		HFONT _fon = nullptr;
	};

	inline bool is_key_down(int key) {
		return GetAsyncKeyState(key) & 0x8000;
	}
	inline void get_xy_from_lparam(LPARAM lparam, int &x, int &y) {
		x = static_cast<int>(GET_X_LPARAM(lparam));
		y = static_cast<int>(GET_Y_LPARAM(lparam));
	}
}
