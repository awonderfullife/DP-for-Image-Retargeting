#pragma once

#include <cassert>

#include <Windows.h>
#include <wincodec.h>

#ifdef NDEBUG
#	define SC_COM_CHECK(X) (X)
#	define SC_WINAPI_CHECK(X) (X)
#else
#	define SC_COM_CHECK(X) assert((X) == S_OK)
#	define SC_WINAPI_CHECK(X) assert((X) != 0)
#endif

namespace seam_carving {
	struct com_usage {
		com_usage() {
			HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			if (hr != S_FALSE) {
				SC_COM_CHECK(hr);
			}
		}
		com_usage(const com_usage&) = delete;
		com_usage(com_usage&&) = delete;
		com_usage &operator=(const com_usage&) = delete;
		com_usage &operator=(com_usage&&) = delete;
		~com_usage() {
			CoUninitialize();
		}
	};
}
