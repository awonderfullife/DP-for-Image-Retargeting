#pragma once

#include <cstdlib>
#include <cstring>

#include <Windows.h>

#include "utils.h"

namespace seam_carving {
	template <typename Elem> struct array2 {
	public:
		using element_type = Elem;

		array2() = default;
		array2(size_t w, size_t h) : _w(w), _h(h) {
			_ps = static_cast<Elem*>(std::malloc(sizeof(Elem) * _w * _h));
		}
		array2(array2 &&src) : _w(src._w), _h(src._h), _ps(src._ps) {
			src._w = src._h = 0;
			src._ps = nullptr;
		}
		array2(const array2 &src) : array2(src._w, src._h) {
			std::memcpy(_ps, src._ps, sizeof(Elem) * _w * _h);
		}
		array2 &operator=(array2 src) {
			std::swap(_w, src._w);
			std::swap(_h, src._h);
			std::swap(_ps, src._ps);
			return *this;
		}
		~array2() {
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

	template <typename Elem = unsigned char> struct color_rgb {
		color_rgb() = default;
		color_rgb(Elem rr, Elem gg, Elem bb) : r(rr), g(gg), b(bb) {
		}

		Elem r, g, b;
	};
	template <typename Elem = unsigned char> struct color_rgba {
		color_rgba() = default;
		color_rgba(Elem rr, Elem gg, Elem bb, Elem aa) : r(rr), g(gg), b(bb), a(aa) {
		}

		Elem r, g, b, a;
	};

	template <typename Color> using image = array2<Color>;

	struct wic_image_loader {
	public:
		wic_image_loader() {
			HRESULT hr = CoCreateInstance(
				CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
				IID_IWICImagingFactory, reinterpret_cast<LPVOID*>(&_factory)
			);
			assert(hr == S_OK);
		}
		~wic_image_loader() {
			_factory->Release();
		}
		image<color_rgba<>> load_image(LPCWSTR filename) {
			IWICBitmapDecoder *decoder = nullptr;
			SC_COM_CHECK(_factory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder));
			IWICBitmapFrameDecode *frame = nullptr;
			IWICBitmapSource *convertedframe = nullptr;
			SC_COM_CHECK(decoder->GetFrame(0, &frame));
			SC_COM_CHECK(WICConvertBitmapSource(GUID_WICPixelFormat32bppRGBA, frame, &convertedframe));
			frame->Release();
			UINT w, h;
			SC_COM_CHECK(convertedframe->GetSize(&w, &h));
			image<color_rgba<>> result(static_cast<size_t>(w), static_cast<size_t>(h));
			SC_COM_CHECK(convertedframe->CopyPixels(
				nullptr, static_cast<UINT>(4 * w), static_cast<UINT>(4 * w * h),
				reinterpret_cast<BYTE*>(result.data())
			));
			convertedframe->Release();
			decoder->Release();
			return result;
		}
	protected:
		IWICImagingFactory *_factory = nullptr;
		com_usage _uses_com;
	};

#define SC_DEVICE_COLOR_ARGB(A, R, G, B)      \
	(								          \
		(static_cast<DWORD>(A) << 24) |       \
		(static_cast<DWORD>(R) << 16) |       \
		(static_cast<DWORD>(G) << 8) |        \
		static_cast<DWORD>(B)		          \
	)								          \

#define SC_DEVICE_COLOR_GETB(X) static_cast<unsigned char>((X) & 0xFF)
#define SC_DEVICE_COLOR_GETG(X) static_cast<unsigned char>(((X) >> 8) & 0xFF)
#define SC_DEVICE_COLOR_GETR(X) static_cast<unsigned char>(((X) >> 16) & 0xFF)
#define SC_DEVICE_COLOR_GETA(X) static_cast<unsigned char>(((X) >> 24) & 0xFF)

#define SC_DEVICE_COLOR_SETB(X, B) ((X) ^= ((X) & 0xFF) ^ static_cast<DWORD>(B))
#define SC_DEVICE_COLOR_SETG(X, G) ((X) ^= ((X) & 0xFF00) ^ (static_cast<DWORD>(G) << 8))
#define SC_DEVICE_COLOR_SETR(X, R) ((X) ^= ((X) & 0xFF0000) ^ (static_cast<DWORD>(R) << 16))
#define SC_DEVICE_COLOR_SETA(X, A) ((X) ^= ((X) & 0xFF000000) ^ (static_cast<DWORD>(A) << 24))

	struct device_color {
		device_color() = default;
		device_color(unsigned char a, unsigned char r, unsigned char g, unsigned char b) : argb(SC_DEVICE_COLOR_ARGB(a, r, g, b)) {
		}
		explicit device_color(const color_rgba<unsigned char> &c) : device_color(c.a, c.r, c.g, c.b) {
		}
		explicit operator color_rgba<unsigned char>() const {
			return color_rgba<unsigned char>(
				SC_DEVICE_COLOR_GETR(argb),
				SC_DEVICE_COLOR_GETG(argb),
				SC_DEVICE_COLOR_GETB(argb),
				SC_DEVICE_COLOR_GETA(argb)
			);
		}

		void set(unsigned char a, unsigned char r, unsigned char g, unsigned char b) {
			argb = SC_DEVICE_COLOR_ARGB(a, r, g, b);
		}

		unsigned char get_a() const {
			return SC_DEVICE_COLOR_GETA(argb);
		}
		unsigned char get_r() const {
			return SC_DEVICE_COLOR_GETR(argb);
		}
		unsigned char get_g() const {
			return SC_DEVICE_COLOR_GETG(argb);
		}
		unsigned char get_b() const {
			return SC_DEVICE_COLOR_GETB(argb);
		}

		void set_a(unsigned char a) {
			SC_DEVICE_COLOR_SETA(argb, a);
		}
		void set_r(unsigned char r) {
			SC_DEVICE_COLOR_SETR(argb, r);
		}
		void set_g(unsigned char g) {
			SC_DEVICE_COLOR_SETG(argb, g);
		}
		void set_b(unsigned char b) {
			SC_DEVICE_COLOR_SETB(argb, b);
		}

		DWORD argb;
	};

	class sys_image {
	public:
		typedef device_color element_type;

		sys_image(HDC hdc, size_t w, size_t h) : _w(w), _h(h), _dc(CreateCompatibleDC(hdc)) {
			BITMAPINFO info;
			std::memset(&info, 0, sizeof(BITMAPINFO));
			info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			info.bmiHeader.biWidth = _w;
			info.bmiHeader.biHeight = _h;
			info.bmiHeader.biPlanes = 1;
			info.bmiHeader.biBitCount = 32;
			info.bmiHeader.biCompression = BI_RGB;
			_bmp = CreateDIBSection(_dc, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&_arr), nullptr, 0);
			_old = static_cast<HBITMAP>(SelectObject(_dc, _bmp));
		}
		sys_image(sys_image&&) = delete;
		sys_image(const sys_image&) = delete;
		sys_image &operator=(sys_image&&) = delete;
		sys_image &operator=(const sys_image&) = delete;
		~sys_image() {
			SelectObject(_dc, _old);
			DeleteObject(_bmp);
			DeleteDC(_dc);
		}

		size_t width() const {
			return _w;
		}
		size_t height() const {
			return _h;
		}
		device_color *data() const {
			return _arr;
		}
		device_color &at(size_t x, size_t y) {
			assert(x < _w && y < _h);
			return _arr[_w * y + x];
		}

		void copy_from_image(const image<color_rgba<unsigned char>> &img, size_t x = 0, size_t y = 0) {
			for (size_t dy = 0; dy < img.height(); ++dy) {
				device_color *cptr = _arr + ((y + dy) * _w + x);
				const color_rgba<unsigned char> *orig = img.at_y(img.height() - dy - 1);
				for (size_t dx = 0; dx < img.width(); ++dx, ++cptr, ++orig) {
					*cptr = device_color(*orig);
				}
			}
		}

		void display(HDC hdc) const {
			display_region(hdc, 0, 0, 0, 0, _w, _h);
		}
		void display_region(HDC hdc, int sx, int sy, int dx, int dy, size_t w, size_t h) const {
			SC_WINAPI_CHECK(BitBlt(hdc, dx, dy, w, h, _dc, sx, sy, SRCCOPY));
		}
	protected:
		size_t _w, _h;
		HBITMAP _bmp, _old;
		HDC _dc;
		device_color *_arr = nullptr;
	};
}
