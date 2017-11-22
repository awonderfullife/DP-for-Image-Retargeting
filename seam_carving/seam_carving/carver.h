#pragma once

#include <initializer_list>
#include <vector>
#include <algorithm>

#include "image.h"

namespace seam_carving {
	enum class orientation {
		horizontal,
		vertical
	};

	class carver {
	public:
		using real_t = float;
		using color_rgba_r = color_rgba<real_t>;
		using image_rgba_r = image<color_rgba_r>;
		using carve_path_data = std::vector<size_t>;

		void set_image(const image_rgba_u8 &img) {
			image_cast(img, _carve_img);
			_calc_gradient();
		}
		void set_image(image_rgba_r img) {
			_carve_img = std::move(img);
			_calc_gradient();
		}
		image_rgba_r &get_image() {
			return _carve_img;
		}
		const image_rgba_r &get_image() const {
			return _carve_img;
		}

		carve_path_data get_vertical_carve_path() const {
			dynamic_array2<_dp_state> dp(_carve_img.width(), _carve_img.height());
			_dp_state *curv = dp.at_y(0);
			const real_t *curgrad = _grad.at_y(0);
			for (size_t x = 0; x < dp.width(); ++x, ++curv, ++curgrad) {
				*curv = _dp_state(*curgrad);
			}
			for (size_t y = 1; y < dp.height(); ++y) {
				_dp_state *lastv = dp.at_y(y - 1);
				curv = dp.at_y(y);
				curgrad = _grad.at_y(y);
				*curv = _dp_state::minimum({
					_dp_state(lastv[0].min_energy + *curgrad, 0),
					_dp_state(lastv[1].min_energy + *curgrad, 1)
				});
				++curv, ++curgrad;
				for (size_t x = 2; x < dp.width(); ++x, ++curv, ++curgrad, ++lastv) {
					*curv = _dp_state::minimum({
						_dp_state(lastv[0].min_energy + *curgrad, -1),
						_dp_state(lastv[1].min_energy + *curgrad, 0),
						_dp_state(lastv[2].min_energy + *curgrad, 1)
					});
				}
				*curv = _dp_state::minimum({
					_dp_state(lastv[0].min_energy + *curgrad, -1),
					_dp_state(lastv[1].min_energy + *curgrad, 0)
				});
			}
			// backtracking
			carve_path_data result(dp.height(), 0);
			curv = dp.at_y(dp.height() - 1);
			real_t minenergy = curv->min_energy;
			++curv;
			for (size_t i = 1; i < dp.width(); ++i, ++curv) {
				if (curv->min_energy < minenergy) {
					minenergy = curv->min_energy;
					result.back() = i;
				}
			}
			for (size_t y = dp.height() - 1, last = result.back(); y > 0; ) {
				last += dp[y][last].min_energy_diff;
				result[--y] = last;
			}
			return result;
		}
		carve_path_data get_horizontal_carve_path() const {
			dynamic_array2<_dp_state> dp(_carve_img.width(), _carve_img.height());
			std::vector<_dp_state*> dpheaders(_carve_img.height(), nullptr);
			std::vector<const real_t*> gheaders(_carve_img.height(), nullptr);
			for (size_t y = 0; y < dp.height(); ++y) {
				*(dpheaders[y] = dp.at_y(y)) = _dp_state(*(gheaders[y] = _grad.at_y(y)));
			}
			for (size_t x = 1; x < dp.width(); ++x) {
				real_t curg = *++gheaders[0];
				dpheaders[0][1] = _dp_state::minimum({
					_dp_state(dpheaders[0]->min_energy + curg, 0),
					_dp_state(dpheaders[1]->min_energy + curg, 1)
				});
				for (size_t y = 1; y < dp.height() - 1; ++y) {
					curg = *++gheaders[y];
					dpheaders[y][1] = _dp_state::minimum({
						_dp_state(dpheaders[y - 1]->min_energy + curg, -1),
						_dp_state(dpheaders[y]->min_energy + curg, 0),
						_dp_state(dpheaders[y + 1]->min_energy + curg, 1),
					});
					++dpheaders[y - 1];
				}
				curg = *++gheaders.back();
				dpheaders.back()[1] = _dp_state::minimum({
					_dp_state(dpheaders.back()->min_energy + curg, -1),
					_dp_state(dpheaders[dpheaders.size() - 2]->min_energy + curg, 0)
				});
				++dpheaders[dpheaders.size() - 2];
				++dpheaders.back();
			}
			// backtracking
			carve_path_data result(dp.width(), 0);
			real_t minenergy = dp[0][dp.width() - 1].min_energy;
			for (size_t i = 1; i < dp.height(); ++i) {
				real_t newenergy = dp[i][dp.width() - 1].min_energy;
				if (newenergy < minenergy) {
					minenergy = newenergy;
					result.back() = i;
				}
			}
			for (size_t x = dp.width() - 1, last = result.back(); x > 0; ) {
				last += dp[last][x].min_energy_diff;
				result[--x] = last;
			}
			return result;
		}
		image_rgba_r carve_vertical(const carve_path_data &data) const {
			return carve_vertical(_carve_img, data);
		}
		image_rgba_r carve_horizontal(const carve_path_data &data) const {
			return carve_horizontal(_carve_img, data);
		}

		template <typename Color> inline static std::vector<Color> get_carved_pixels_vertical(
			const image<Color> &img, const carve_path_data &data
		) {
			std::vector<Color> result(img.height(), Color());
			for (size_t i = 0; i < img.height(); ++i) {
				result[i] = img[i][data[i]];
			}
			return result;
		}
		template <typename Color> inline static std::vector<Color> get_carved_pixels_horizontal(
			const image<Color> &img, const carve_path_data &data
		) {
			std::vector<Color> result(img.width(), Color());
			for (size_t i = 0; i < img.width(); ++i) {
				result[i] = img[data[i]][i];
			}
			return result;
		}

		template <typename Color> inline static image<Color> carve_vertical(
			const image<Color> &img, const carve_path_data &data
		) {
			image<Color> result(img.width() - 1, img.height());
			for (size_t y = 0; y < img.height(); ++y) {
				const Color *src = img.at_y(y);
				Color *dst = result.at_y(y);
				size_t x = 0;
				for (; x < data[y]; ++x, ++src, ++dst) {
					*dst = *src;
				}
				for (++x, ++src; x < img.width(); ++x, ++src, ++dst) {
					*dst = *src;
				}
			}
			return result;
		}
		template <typename Color> inline static image<Color> carve_horizontal(
			const image<Color> &img, const carve_path_data &data
		) {
			image<Color> result(img.width(), img.height() - 1);
			for (size_t y = 1; y < img.height(); ++y) {
				const Color *cur = img.at_y(y - 1), *next = img.at_y(y);
				Color *dst = result.at_y(y - 1);
				for (size_t x = 0; x < img.width(); ++x, ++cur, ++next, ++dst) {
					*dst = (y <= data[x] ? *cur : *next);
				}
			}
			return result;
		}

		template <typename Color> inline static image<Color> restore_vertical(
			const image<Color> &img, const carve_path_data &data, const std::vector<Color> &pixels
		) {
			image<Color> result(img.width() + 1, img.height());
			for (size_t y = 0; y < img.height(); ++y) {
				const Color *src = img.at_y(y);
				Color *dst = result.at_y(y);
				size_t x = 0;
				for (; x < data[y]; ++x, ++src, ++dst) {
					*dst = *src;
				}
				*dst = pixels[y];
				for (++dst; x < img.width(); ++x, ++src, ++dst) {
					*dst = *src;
				}
			}
			return result;
		}
		template <typename Color> inline static image<Color> restore_horizontal(
			const image<Color> &img, const carve_path_data &data, const std::vector<Color> &pixels
		) {
			image<Color> result(img.width(), img.height() + 1);
			for (size_t y = 0; y <= img.height(); ++y) {
				const Color *cur = img.at_y(std::min(y, img.height() - 1)), *prev = img.at_y(std::max<size_t>(y, 1) - 1);
				Color *dst = result.at_y(y);
				for (size_t x = 0; x < img.width(); ++x, ++cur, ++prev, ++dst) {
					*dst = (y == data[x] ? pixels[x] : (y < data[x] ? *cur : *prev));
				}
			}
			return result;
		}
	protected:
		image_rgba_r _carve_img;
		dynamic_array2<real_t> _grad;

		struct _dp_state {
			_dp_state() = default;
			_dp_state(real_t energy, int diff = 0) : min_energy(energy), min_energy_diff(diff) {
			}

			real_t min_energy = 0.0f;
			int min_energy_diff = 0;

			inline static _dp_state minimum(std::initializer_list<_dp_state> vars) {
				auto miter = vars.begin();
				for (auto c = miter + 1; c != vars.end(); ++c) {
					if (c->min_energy < miter->min_energy) {
						miter = c;
					}
				}
				return *miter;
			}
		};

		inline static real_t _calc_grad_elem(
			const color_rgba_r &left, const color_rgba_r &right,
			const color_rgba_r &up, const color_rgba_r &down
		) {
			color_rgba_r hor = right - left, vert = up - down;
			return squared(hor.r) + squared(hor.g) + squared(hor.b) + squared(vert.r) + squared(vert.g) + squared(vert.b);
		}
		void _calc_gradient_row(
			const color_rgba_r *l,
			const color_rgba_r *u, const color_rgba_r *d,
			real_t *cur
		) {
			const color_rgba_r *r = l + 1;
			*cur = _calc_grad_elem(*l, *r, *u, *d);
			++r, ++u, ++d, ++cur;
			for (size_t x = 2; x < _carve_img.width(); ++x, ++l, ++r, ++u, ++d, ++cur) {
				*cur = _calc_grad_elem(*l, *r, *u, *d);
			}
			*cur = _calc_grad_elem(*l, *--r, *u, *d);
		}
		void _calc_gradient() {
			assert(_carve_img.height() > 1 && _carve_img.width() > 1);
			_grad = dynamic_array2<real_t>(_carve_img.width(), _carve_img.height());
			_calc_gradient_row(_carve_img[0], _carve_img[0], _carve_img[1], _grad[0]);
			for (size_t y = 2; y < _carve_img.height(); ++y) {
				_calc_gradient_row(_carve_img[y - 1], _carve_img[y - 2], _carve_img[y], _grad[y - 1]);
			}
			size_t y = _carve_img.height() - 1;
			_calc_gradient_row(_carve_img[y], _carve_img[y - 1], _carve_img[y], _grad[y]);
		}
	};

	class dynamic_retargeter {
	public:
		struct carve_path {
			carver::carve_path_data path_data;
			std::vector<color_rgba_u8> pixel_data;
			orientation path_orientation = orientation::horizontal;
		};

		void set_image(image_rgba_u8 img) {
			_curimg = std::move(img);
			_carved.clear();
		}
		const image_rgba_u8 &get_image() const {
			return _curimg;
		}

		void retarget(size_t w, size_t h) {
			if (w < _curimg.width() || h < _curimg.height()) {
				while (_curimg.width() > w || _curimg.height() > h) {
					if (_curimg.width() > w) {
						carver carv;
						carv.set_image(_curimg);
						carve_path path;
						path.path_orientation = orientation::vertical;
						path.path_data = carv.get_vertical_carve_path();
						path.pixel_data = carver::get_carved_pixels_vertical(_curimg, path.path_data);
						_curimg = carver::carve_vertical(_curimg, path.path_data);
						_carved.push_back(std::move(path));
					}
					if (_curimg.height() > h) {
						carver carv;
						carv.set_image(_curimg);
						carve_path path;
						path.path_orientation = orientation::horizontal;
						path.path_data = carv.get_horizontal_carve_path();
						path.pixel_data = carver::get_carved_pixels_horizontal(_curimg, path.path_data);
						_curimg = carver::carve_horizontal(_curimg, path.path_data);
						_carved.push_back(std::move(path));
					}
				}
			} else {
				while (_carved.size() > 0 && (
					(w > _curimg.width() && _carved.back().path_orientation == orientation::vertical) ||
					(h > _curimg.height() && _carved.back().path_orientation == orientation::horizontal)
					)) {
					switch (_carved.back().path_orientation) {
					case orientation::horizontal:
						_curimg = carver::restore_horizontal(_curimg, _carved.back().path_data, _carved.back().pixel_data);
						break;
					case orientation::vertical:
						_curimg = carver::restore_vertical(_curimg, _carved.back().path_data, _carved.back().pixel_data);
						break;
					}
					_carved.pop_back();
				}
			}
		}
	protected:
		image_rgba_u8 _curimg;
		std::vector<carve_path> _carved;
	};
}
