#pragma once

#include <vector>

#include "image.h"

namespace seam_carving {
	class dancing_link_retargeter {
	public:
		using real_t = float;
		using color_t = color_rgba_u8;
		using ptr_t = size_t;

		constexpr static ptr_t null = -1;

		void set_image(const image_rgba_u8 &img) {
			_w = img.width();
			_h = img.height();
			_n = std::vector<_node>(img.width() * img.height(), _node());
			_tl = 0;
			_br = img.width() * img.height() - 1;
			const color_rgba_u8 *color = img.at_y(0);
			_n[_tl].color = *color;
			for (ptr_t x = 1; x < img.width(); ++x) {
				_n[x].color = *++color;
				_n[x].left = x - 1;
				_n[x - 1].right = x;
			}
			for (ptr_t y = 1, ys = img.width(); y < img.height(); ++y, ys += img.width()) {
				color = img.at_y(y);
				_n[ys].color = *color;
				_n[ys].up = ys - img.width();
				_n[_n[ys].up].down = ys;
				for (ptr_t x = 1, ptr = ys + x; x < img.width(); ++x, ++ptr) {
					_n[ptr].color = *++color;
					_n[ptr].left = ptr - 1;
					_n[ptr - 1].right = ptr;
					_n[ptr].up = ptr - img.width();
					_n[_n[ptr].up].down = ptr;
				}
			}
			// calculate energy
			for (ptr_t i = 0; i < _n.size(); ++i) {
				_calc_energy_elem(i);
			}
		}
		image_rgba_u8 get_image() const {
			image_rgba_u8 res(_w, _h);
			size_t yi = 0;
			for (ptr_t y = _tl; y != null; y = _n[y].down, ++yi) {
				color_rgba_u8 *c = res.at_y(yi);
				for (ptr_t x = y; x != null; x = _n[x].right, ++c) {
					*c = _n[x].color;
				}
			}
			assert(yi == _h);
			return res;
		}
		sys_image get_sys_image(HDC dc) const {
			sys_image res(dc, _w, _h);
			size_t yi = 0;
			for (ptr_t y = _tl; y != null; y = _n[y].down, ++yi) {
				sys_color *c = res.at_y(_h - yi - 1);
				for (ptr_t x = y; x != null; x = _n[x].right, ++c) {
					*c = sys_color(_n[x].color);
				}
			}
			assert(yi == _h);
			return res;
		}

		ptr_t get_vertical_carve_path() {
			return _get_carve_path_impl<&_node::left, &_node::right, &_node::up, &_node::down>();
		}
		void carve_path_vertical(ptr_t ptr) {
			_carve_path_impl<&_node::left, &_node::right, &_node::up, &_node::down>(ptr);
			_cps.push_back({ptr, orientation::vertical});
			--_w;
		}
		ptr_t get_horizontal_carve_path() {
			return _get_carve_path_impl<&_node::up, &_node::down, &_node::left, &_node::right>();
		}
		void carve_path_horizontal(ptr_t ptr) {
			_carve_path_impl<&_node::up, &_node::down, &_node::left, &_node::right>(ptr);
			_cps.push_back({ptr, orientation::horizontal});
			--_h;
		}
		void restore_path() {
			switch (_cps.back().second) {
			case orientation::horizontal:
				_restore_path_impl<&_node::up, &_node::down>(_cps.back().first);
				++_h;
				break;
			case orientation::vertical:
				_restore_path_impl<&_node::left, &_node::right>(_cps.back().first);
				++_w;
				break;
			}
			_cps.pop_back();
		}

		void retarget(size_t width, size_t height) {
			if (width < _w || height < _h) {
				do {
					if (width < _w) {
						carve_path_vertical(get_vertical_carve_path());
					}
					if (height < _h) {
						carve_path_horizontal(get_horizontal_carve_path());
					}
				} while (width < _w || height < _h);
			} else {
				while (_cps.size() > 0) {
					if (_cps.back().second == orientation::horizontal) {
						if (_h >= height) {
							break;
						}
					} else {
						if (_w >= width) {
							break;
						}
					}
					restore_path();
				}
			}
		}

		void validate_graph_structure() { // TODO right & bottom boundary check
			assert(_n[_tl].left == null && _n[_tl].up == null);
			size_t xn = 1;
			for (ptr_t x = _n[_tl].right; x != null; x = _n[x].right, ++xn) {
				assert(_n[x].up == null);
				assert(_n[_n[x].left].right == x);
			}
			assert(xn == _w);
			size_t yn = 1;
			for (ptr_t y = _n[_tl].down; y != null; y = _n[y].down, ++yn) {
				xn = 1;
				assert(_n[y].left == null);
				assert(_n[_n[y].up].down == y);
				for (ptr_t x = _n[y].right; x != null; x = _n[x].right, ++xn) {
					assert(_n[_n[x].left].right == x);
					assert(_n[_n[x].up].down == x);
				}
				assert(xn == _w);
			}
			assert(yn == _h);
		}

		size_t current_width() const {
			return _w;
		}
		size_t current_height() const {
			return _h;
		}

		void clear() {
			_n.clear();
			_tl = _br = null;
		}
	protected:
		struct _node {
			real_t energy, dp;
			color_t color;
			ptr_t left = null, up = null, right = null, down = null, path_ptr = null;
		};

		void _calc_energy_elem(ptr_t nptr) {
			_node &n = _n[nptr];
			color_t
				lc = n.left == null ? n.color : _n[n.left].color, rc = n.right == null ? n.color : _n[n.right].color,
				uc = n.up == null ? n.color : _n[n.up].color, dc = n.down == null ? n.color : _n[n.down].color;
			color_rgba_f hd = rc.cast<float>() - lc.cast<float>(), vd = dc.cast<float>() - uc.cast<float>();
			n.energy = squared(hd.r) + squared(hd.g) + squared(hd.b) + squared(vd.r) + squared(vd.g) + squared(vd.b);
		}

		template <ptr_t _node::*XN, ptr_t _node::*XP> void _detach_elem(ptr_t elem) {
			_node &en = _n[elem];
			if (elem == _tl) {
				_tl = en.*XP;
			} else if (elem == _br) {
				_br = en.*XN;
			}
			if (en.*XN != null) {
				_n[en.*XN].*XP = en.*XP;
			}
			if (en.*XP != null) {
				_n[en.*XP].*XN = en.*XN;
			}
		}
		template <ptr_t _node::*XN, ptr_t _node::*XP, ptr_t _node::*YN, ptr_t _node::*YP> void _fix_detached_links(ptr_t prev) {
			_node &pn = _n[prev], &nn = _n[pn.path_ptr];
			ptr_t u, d;
			if (nn.*XP == pn.*YP) {
				u = pn.*XN;
				d = nn.*XP;
			} else {
				assert(nn.*XN == pn.*YP);
				u = pn.*XP;
				d = nn.*XN;
			}
			_n[u].*YP = d;
			_n[d].*YN = u;
		}

		template <ptr_t _node::*XN, ptr_t _node::*XP, ptr_t _node::*YN, ptr_t _node::*YP> ptr_t _get_carve_path_impl() {
			for (ptr_t x = _br; x != null; x = _n[x].*XN) {
				_n[x].dp = _n[x].energy;
				_n[x].path_ptr = null;
			}
			for (ptr_t y = _n[_br].*YN; y != null; y = _n[y].*YN) {
				ptr_t x = y;
				_node *xn = &_n[x], *xdn = &_n[xn->*YP];
				xn->path_ptr = xn->*YP;
				if (_n[xdn->*XN].dp < xdn->dp) {
					xn->path_ptr = xdn->*XN;
					xn->dp = _n[xn->path_ptr].dp;
				} else {
					xn->dp = xdn->dp;
				}
				xn->dp += xn->energy;
				for (x = xn->*XN, xn = &_n[x]; xn->*XN != null; x = xn->*XN, xn = &_n[x]) {
					xdn = &_n[xn->*YP];
					real_t mdpv = xdn->dp;
					xn->path_ptr = xn->*YP;
					if (_n[xdn->*XN].dp < mdpv) {
						xn->path_ptr = xdn->*XN;
						mdpv = _n[xdn->*XN].dp;
					}
					if (_n[xdn->*XP].dp < mdpv) {
						xn->path_ptr = xdn->*XP;
						mdpv = _n[xdn->*XP].dp;
					}
					xn->dp = mdpv + xn->energy;
				}
				xdn = &_n[xn->*YP];
				xn->path_ptr = xn->*YP;
				if (_n[xdn->*XP].dp < xdn->dp) {
					xn->path_ptr = xdn->*XP;
					xn->dp = _n[xn->path_ptr].dp;
				} else {
					xn->dp = xdn->dp;
				}
				xn->dp += xn->energy;
			}
			ptr_t res = _tl;
			for (ptr_t cur = _n[_tl].*XP; cur != null; cur = _n[cur].*XP) {
				if (_n[cur].dp < _n[res].dp) {
					res = cur;
				}
			}
			return res;
		}
		template <ptr_t _node::*XN, ptr_t _node::*XP, ptr_t _node::*YN, ptr_t _node::*YP> ptr_t _get_carve_path_incremental_impl(ptr_t lastpath) {
			// TODO
		}
		template <ptr_t _node::*XN, ptr_t _node::*XP, ptr_t _node::*YN, ptr_t _node::*YP> void _carve_path_impl(ptr_t head) {
			_detach_elem<XN, XP>(head);
			for (ptr_t prv = head, n = _n[prv].path_ptr; n != null; prv = n, n = _n[n].path_ptr) {
				_detach_elem<XN, XP>(n);
				if (n != _n[prv].*YP) {
					_fix_detached_links<XN, XP, YN, YP>(prv);
				}
			}
			_recalc_path_side_energy<XN, XP>(head);
		}
		template <ptr_t _node::*XN, ptr_t _node::*XP> void _restore_path_impl(ptr_t p) {
			for (ptr_t cur = p; cur != null; cur = _n[cur].path_ptr) {
				_node &cn = _n[cur];
				if (cn.left != null) {
					_n[cn.left].right = cur;
				}
				if (cn.up != null) {
					_n[cn.up].down = cur;
				}
				if (cn.right != null) {
					_n[cn.right].left = cur;
				}
				if (cn.down != null) {
					_n[cn.down].up = cur;
				}
				if (cn.left == null && cn.up == null) {
					_tl = cur;
				}
				if (cn.right == null && cn.down == null) {
					_br = cur;
				}
			}
			_recalc_path_side_energy<XN, XP>(p);
		}
		template <ptr_t _node::*XN, ptr_t _node::*XP> void _recalc_path_side_energy(ptr_t head) {
			for (ptr_t cur = head; cur != null; cur = _n[cur].path_ptr) {
				if (_n[cur].*XN != null) {
					_calc_energy_elem(_n[cur].*XN);
				}
				if (_n[cur].*XP != null) {
					_calc_energy_elem(_n[cur].*XP);
				}
			}
		}

		std::vector<_node> _n;
		std::vector<std::pair<ptr_t, orientation>> _cps;
		ptr_t _tl = null, _br = null;
		size_t _w = 0, _h = 0;
	};
}
