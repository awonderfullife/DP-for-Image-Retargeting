#pragma once

#include <cassert>

#include <Windows.h>
#include <wincodec.h>
#undef min
#undef max

#ifdef NDEBUG
#	define SC_COM_CHECK(X) (X)
#	define SC_WINAPI_CHECK(X) (X)
#else
#	define SC_COM_CHECK(X) assert((X) == S_OK)
#	define SC_WINAPI_CHECK(X) assert((X) != 0)
#endif

namespace seam_carving {
	template <typename T> inline T squared(T v) {
		return v * v;
	}

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

	template <typename Elem> struct dynamic_array2 {
	public:
		using element_type = Elem;

		dynamic_array2() = default;
		dynamic_array2(size_t w, size_t h) : _w(w), _h(h) {
			if (_w > 0 && _h > 0) {
				_ps = static_cast<Elem*>(std::malloc(sizeof(Elem) * _w * _h));
			} else {
				_w = _h = 0;
			}
		}
		dynamic_array2(dynamic_array2 &&src) : _w(src._w), _h(src._h), _ps(src._ps) {
			src._w = src._h = 0;
			src._ps = nullptr;
		}
		dynamic_array2(const dynamic_array2 &src) : dynamic_array2(src._w, src._h) {
			std::memcpy(_ps, src._ps, sizeof(Elem) * _w * _h);
		}
		dynamic_array2 &operator=(dynamic_array2 src) {
			std::swap(_w, src._w);
			std::swap(_h, src._h);
			std::swap(_ps, src._ps);
			return *this;
		}
		~dynamic_array2() {
			if (_ps) {
				std::free(_ps);
			}
		}

		Elem *data() {
			return _ps;
		}
		const Elem *data() const {
			return _ps;
		}

		Elem &at(size_t x, size_t y) {
			assert(x < _w && y < _h);
			return _ps[y * _w + x];
		}
		const Elem &at(size_t x, size_t y) const {
			assert(x < _w && y < _h);
			return _ps[y * _w + x];
		}

		Elem *at_y(size_t y) {
			assert(y < _h);
			return _ps + (y * _w);
		}
		const Elem *at_y(size_t y) const {
			assert(y < _h);
			return _ps + (y * _w);
		}
		Elem *operator[](size_t y) {
			return at_y(y);
		}
		const Elem *operator[](size_t y) const {
			return at_y(y);
		}

		size_t width() const {
			return _w;
		}
		size_t height() const {
			return _h;
		}
	protected:
		Elem *_ps = nullptr;
		size_t _w = 0, _h = 0;
	};
}
