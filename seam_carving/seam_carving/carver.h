#pragma once

#include <initializer_list>
#include <vector>

#include "image.h"

namespace seam_carving {
	class carver {
	public:
		using real_t = float;
		using color_rgba_r = color_rgba<real_t>;
		using image_rgba_r = image<color_rgba_r>;
		using carve_path_data = std::vector<size_t>;

		enum class path_orientation {
			horizontal,
			vertical
		};
		struct carve_path {
			path_orientation orientation = path_orientation::horizontal;
			carve_path_data data;
		};

		void set_image(const image_rgba_u8 &img) {
			image_cast(img, _carve_img);
			_calc_gradient();
		}
		void set_image(image_rgba_r img) {
			_carve_img = std::move(img);
			_calc_gradient();
		}

		carve_path_data get_vertical_carve_path() const {
			dynamic_array2<_dp_state> dp(_carve_img.width(), _carve_img.height());
			_dp_state *curv = dp.at_y(0);
			const real_t *curgrad = _grad_img.at_y(0);
			for (size_t x = 0; x < dp.width(); ++x, ++curv, ++curgrad) {
				*curv = _dp_state(*curgrad);
			}
			for (size_t y = 1; y < dp.height(); ++y) {
				_dp_state *lastv = dp.at_y(y - 1);
				curv = dp.at_y(y);
				curgrad = _grad_img.at_y(y);
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
		image_rgba_r carve_vertical(const carve_path_data &data) const {
			image_rgba_r result(_carve_img.width() - 1, _carve_img.height());
			for (size_t y = 0; y < _carve_img.height(); ++y) {
				const color_rgba_r *src = _carve_img.at_y(y);
				color_rgba_r *dst = result.at_y(y);
				size_t x = 0;
				for (; x < data[y]; ++x, ++src, ++dst) {
					*dst = *src;
				}
				for (++x, ++src; x < _carve_img.width(); ++x, ++src, ++dst) {
					*dst = *src;
				}
			}
			return result;
		}
	protected:
		image_rgba_r _carve_img;
		dynamic_array2<real_t> _grad_img;

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

		inline static _dp_state _synth(_dp_state *statearr, real_t *gradarr, size_t l, size_t m, size_t r) {
			_dp_state ds(statearr[l].min_energy + gradarr[l], -1);
			real_t mv = statearr[m].min_energy + gradarr[m], rv = statearr[r].min_energy + gradarr[r];
			if (mv <= ds.min_energy) {
				ds = _dp_state(mv, 0);
			}
			return rv < ds.min_energy ? _dp_state(rv, 1) : ds;
		}

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
			_grad_img = dynamic_array2<real_t>(_carve_img.width(), _carve_img.height());
			_calc_gradient_row(_carve_img[0], _carve_img[0], _carve_img[1], _grad_img[0]);
			for (size_t y = 2; y < _carve_img.height(); ++y) {
				_calc_gradient_row(_carve_img[y - 1], _carve_img[y - 2], _carve_img[y], _grad_img[y - 1]);
			}
			size_t y = _carve_img.height() - 1;
			_calc_gradient_row(_carve_img[y], _carve_img[y - 1], _carve_img[y], _grad_img[y]);
		}
	};
}
