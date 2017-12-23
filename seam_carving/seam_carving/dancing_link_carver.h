#pragma once

#include <vector>

#include "image.h"

//#define USE_INDEX_PTR
#define USE_INCREMENTAL

namespace seam_carving {
	class dancing_link_retargeter {
	public:
		struct node;
#ifdef USE_INDEX_PTR
		using ptr_t = size_t;
		using const_ptr_t = size_t;
		constexpr static ptr_t null = -1;
#else
		using ptr_t = node * ;
		using const_ptr_t = const node*;
		constexpr static ptr_t null = nullptr;
#endif

		using real_t = float;
		using color_t = color_rgba_u8;
		using enlarge_table_t = std::vector<std::vector<std::pair<size_t, size_t>>>;

		struct node {
			real_t energy, dp, compensation = 0.0;
			color_t color;
			ptr_t left = null, up = null, right = null, down = null, path_ptr = null;
		};
		struct keep_original {
			inline static color_rgba_u8 process(const node &n) {
				return n.color;
			}
		};
		struct blend_compensation {
			inline static color_rgba_u8 process(const node &n) {
				if (n.compensation < 1e-6) {
					return color_rgba_u8::blend(color_rgba_u8(
						0, 255, 0, static_cast<unsigned char>(127 * (1.0 - std::exp(n.compensation)))
					), n.color);
				} else if (n.compensation > 1e-6) {
					return color_rgba_u8::blend(color_rgba_u8(
						255, 0, 0, static_cast<unsigned char>(127 * (1.0 - std::exp(-n.compensation)))
					), n.color);
				}
				return n.color;
			}
		};

		void set_image(const image_rgba_u8 &img) {
			_w = img.width();
			_h = img.height();
			_cps.clear();
			_n = std::vector<node>(img.width() * img.height(), node());
			_tl = _pref(_n[0]);
			_br = _pref(_n.back());
			const color_rgba_u8 *color = img.at_y(0);
			_pderef(_tl).color = *color;
			ptr_t cur = _pref(_n[1]), last = _pref(_n[0]);
			for (size_t x = 1; x < img.width(); ++x, last = cur, cur = _pref(_pderef(cur + 1))) {
				_pderef(cur).color = *++color;
				_pderef(cur).left = last;
				_pderef(last).right = cur;
			}
			cur = _pref(_n[0]), last = cur;
			for (size_t y = 1; y < img.height(); ++y, last = cur) {
				cur = _pref(_pderef(cur + img.width()));
				color = img.at_y(y);
				_pderef(cur).color = *color;
				_pderef(cur).up = last;
				_pderef(last).down = cur;
				ptr_t curx = cur, lastx = cur;
				for (size_t x = 1; x < img.width(); ++x, lastx = curx) {
					curx = _pref(_pderef(curx + 1));
					_pderef(curx).color = *++color;
					_pderef(curx).left = lastx;
					_pderef(lastx).right = curx;
					ptr_t up = _pref(_pderef(curx - img.width()));
					_pderef(curx).up = up;
					_pderef(up).down = curx;
				}
			}
			// calculate energy
			for (size_t i = 0; i < _n.size(); ++i) {
				_calc_energy_elem(_pref(_n[i]));
			}
		}
		template <typename ColorProc = keep_original> image_rgba_u8 get_image() const {
			image_rgba_u8 res(_w, _h);
			_get_image_impl<ColorProc>(res);
			return res;
		}
		template <typename ColorProc = keep_original> sys_image get_sys_image(HDC dc) const {
			sys_image res(dc, _w, _h);
			_get_image_impl<ColorProc>(res);
			return res;
		}

		ptr_t get_vertical_carve_path() {
			_update_dp<&node::left, &node::right, &node::up, &node::down>(orientation::vertical);
			return _get_carve_path_impl<&node::left, &node::right, &node::up, &node::down>();
		}
		void carve_path_vertical(ptr_t ptr) {
			_carve_path_impl<&node::left, &node::right, &node::up, &node::down>(ptr);
			_cps.push_back({ptr, orientation::vertical});
			--_w;
		}
		ptr_t get_horizontal_carve_path() {
			_update_dp<&node::up, &node::down, &node::left, &node::right>(orientation::horizontal);
			return _get_carve_path_impl<&node::up, &node::down, &node::left, &node::right>();
		}
		void carve_path_horizontal(ptr_t ptr) {
			_carve_path_impl<&node::up, &node::down, &node::left, &node::right>(ptr);
			_cps.push_back({ptr, orientation::horizontal});
			--_h;
		}
		void restore_path() {
			switch (_cps.back().second) {
			case orientation::horizontal:
				_restore_path_impl<&node::up, &node::down>(_cps.back().first);
				++_h;
				break;
			case orientation::vertical:
				_restore_path_impl<&node::left, &node::right>(_cps.back().first);
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
			assert(_pderef(_tl).left == null && _pderef(_tl).up == null);
			size_t xn = 1;
			for (ptr_t x = _pderef(_tl).right; x != null; x = _pderef(x).right, ++xn) {
				assert(_pderef(x).up == null);
				assert(_pderef(_pderef(x).left).right == x);
			}
			assert(xn == _w);
			size_t yn = 1;
			for (ptr_t y = _pderef(_tl).down; y != null; y = _pderef(y).down, ++yn) {
				xn = 1;
				assert(_pderef(y).left == null);
				assert(_pderef(_pderef(y).up).down == y);
				for (ptr_t x = _pderef(y).right; x != null; x = _pderef(x).right, ++xn) {
					assert(_pderef(_pderef(x).left).right == x);
					assert(_pderef(_pderef(x).up).down == x);
				}
				assert(xn == _w);
			}
			assert(yn == _h);
		}

		const_ptr_t at_y(size_t y) const {
			return _pfrompos(_at_y_impl(y));
		}
		const_ptr_t at(size_t x, size_t y) const {
			return _pfrompos(_at_impl(x, y));
		}
		ptr_t at_y(size_t y) {
			return _pfrompos(_at_y_impl(y));
		}
		ptr_t at(size_t x, size_t y) {
			return _pfrompos(_at_impl(x, y));
		}
		node &deref(ptr_t p) {
			return _pderef(p);
		}
		const node &deref(ptr_t p) const {
			return _pderef(p);
		}

		bool is_carved() const {
			return _cps.size() > 0;
		}
		size_t current_width() const {
			return _w;
		}
		size_t current_height() const {
			return _h;
		}

		enlarge_table_t prepare_horizontal_enlarging() {
			return _prepare_enlarging_impl<&node::left, &node::right, &node::up, &node::down>(_w, _h, orientation::horizontal);
		}
		enlarge_table_t prepare_vertical_enlarging() {
			return _prepare_enlarging_impl<&node::up, &node::down, &node::left, &node::right>(_h, _w, orientation::vertical);
		}

		void clear() {
			_n.clear();
			_tl = _br = null;
		}
	protected:
		void _calc_energy_elem(ptr_t nptr) {
			node &n = _pderef(nptr);
			color_t
				lc = n.left == null ? n.color : _pderef(n.left).color, rc = n.right == null ? n.color : _pderef(n.right).color,
				uc = n.up == null ? n.color : _pderef(n.up).color, dc = n.down == null ? n.color : _pderef(n.down).color;
			color_rgba_f hd = rc.cast<float>() - lc.cast<float>(), vd = dc.cast<float>() - uc.cast<float>();
			n.energy = squared(hd.r) + squared(hd.g) + squared(hd.b) + squared(vd.r) + squared(vd.g) + squared(vd.b);
		}

		template <ptr_t node::*XN, ptr_t node::*XP> void _detach_elem(ptr_t elem) {
			node &en = _pderef(elem);
			if (elem == _tl) {
				_tl = en.*XP;
			} else if (elem == _br) {
				_br = en.*XN;
			}
			if (en.*XN != null) {
				_pderef(en.*XN).*XP = en.*XP;
			}
			if (en.*XP != null) {
				_pderef(en.*XP).*XN = en.*XN;
			}
		}
		template <ptr_t node::*XN, ptr_t node::*XP, ptr_t node::*YN, ptr_t node::*YP> void _fix_detached_links(ptr_t prev) {
			node &pn = _pderef(prev), &nn = _pderef(pn.path_ptr);
			ptr_t u, d;
			if (nn.*XP == pn.*YP) {
				u = pn.*XN;
				d = nn.*XP;
			} else {
				assert(nn.*XN == pn.*YP);
				u = pn.*XP;
				d = nn.*XN;
			}
			_pderef(u).*YP = d;
			_pderef(d).*YN = u;
		}

		template <ptr_t node::*XN, ptr_t node::*XP, ptr_t node::*YN, ptr_t node::*YP> void _recalc_dp() {
			for (ptr_t x = _br; x != null; x = _pderef(x).*XN) {
				_pderef(x).dp = _pderef(x).energy;
				_pderef(x).path_ptr = null;
			}
			for (ptr_t y = _pderef(_br).*YN; y != null; y = _pderef(y).*YN) {
				ptr_t x = y;
				node *xn = &_pderef(x), *xdn = &_pderef(xn->*YP);
				xn->path_ptr = xn->*YP;
				if (xn->*XN != null) {
					if (_pderef(xdn->*XN).dp < xdn->dp) {
						xn->path_ptr = xdn->*XN;
						xn->dp = _pderef(xn->path_ptr).dp;
					} else {
						xn->dp = xdn->dp;
					}
					xn->dp += xn->energy + xn->compensation;
					for (x = xn->*XN, xn = &_pderef(x); xn->*XN != null; x = xn->*XN, xn = &_pderef(x)) {
						xdn = &_pderef(xn->*YP);
						real_t mdpv = xdn->dp;
						xn->path_ptr = xn->*YP;
						if (_pderef(xdn->*XN).dp < mdpv) {
							xn->path_ptr = xdn->*XN;
							mdpv = _pderef(xdn->*XN).dp;
						}
						if (_pderef(xdn->*XP).dp < mdpv) {
							xn->path_ptr = xdn->*XP;
							mdpv = _pderef(xdn->*XP).dp;
						}
						xn->dp = mdpv + xn->energy + xn->compensation;
					}
					xdn = &_pderef(xn->*YP);
					xn->path_ptr = xn->*YP;
					if (_pderef(xdn->*XP).dp < xdn->dp) {
						xn->path_ptr = xdn->*XP;
						xn->dp = _pderef(xn->path_ptr).dp;
					} else {
						xn->dp = xdn->dp;
					}
					xn->dp += xn->energy + xn->compensation;
				} else {
					xn->dp = xdn->dp + xn->energy + xn->compensation;
				}
			}
			_fresh_dp = true;
		}
		template <ptr_t node::*XN, ptr_t node::*XP, ptr_t node::*YN, ptr_t node::*YP> bool _validate_dp() {
			for (ptr_t x = _br; x != null; x = _pderef(x).*XN) {
				assert(_pderef(x).dp == _pderef(x).energy);
				assert(_pderef(x).path_ptr == null);
			}
			bool allcorrect = true;
			for (ptr_t y = _pderef(_br).*YN; y != null; y = _pderef(y).*YN) {
				ptr_t x = y;
				node *xn = &_pderef(x), *xdn = &_pderef(xn->*YP);
				if (xn->*XN != null) {
					if (_pderef(xdn->*XN).dp < xdn->dp) {
						assert(xn->path_ptr == xdn->*XN);
						assert(xn->dp == _pderef(xdn->*XN).dp + xn->energy + xn->compensation);
					} else {
						assert(xn->path_ptr == xn->*YP);
						assert(xn->dp == xdn->dp + xn->energy + xn->compensation);
					}
					for (x = xn->*XN, xn = &_pderef(x); xn->*XN != null; x = xn->*XN, xn = &_pderef(x)) {
						xdn = &_pderef(xn->*YP);
						real_t mdpv = xdn->dp;
						ptr_t best = xn->*YP;
						if (_pderef(xdn->*XN).dp < mdpv) {
							best = xdn->*XN;
							mdpv = _pderef(xdn->*XN).dp;
						}
						if (_pderef(xdn->*XP).dp < mdpv) {
							best = xdn->*XP;
							mdpv = _pderef(xdn->*XP).dp;
						}
						assert(xn->path_ptr == best);
						assert(xn->dp == mdpv + xn->energy + xn->compensation);
					}
					xdn = &_pderef(xn->*YP);
					if (_pderef(xdn->*XP).dp < xdn->dp) {
						assert(xn->path_ptr == xdn->*XP);
						assert(xn->dp == _pderef(xn->path_ptr).dp + xn->energy + xn->compensation);
					} else {
						assert(xn->path_ptr == xn->*YP);
						assert(xn->dp == xdn->dp + xn->energy + xn->compensation);
					}
				} else {
					assert(xn->path_ptr == xn->*YP);
					assert(xn->dp == xdn->dp + xn->energy + xn->compensation);
				}
			}
			return allcorrect;
		}
		template <ptr_t node::*XN, ptr_t node::*XP, ptr_t node::*YN, ptr_t node::*YP> bool _update_dp_elem(ptr_t pos) {
			node &cur = _pderef(pos);
			ptr_t best = cur.*YP;
			node &down = _pderef(best);
			if (down.*XN != null) {
				if (_pderef(down.*XN).dp < down.dp) {
					best = down.*XN;
				}
			}
			if (down.*XP != null) {
				if (_pderef(down.*XP).dp < _pderef(best).dp) {
					best = down.*XP;
				}
			}
			real_t ndp = _pderef(best).dp + cur.energy + cur.compensation;
			if (cur.path_ptr != best || ndp != cur.dp) {
				cur.dp = ndp;
				cur.path_ptr = best;
				return true;
			}
			return false;
		}
		template <ptr_t node::*XN, ptr_t node::*XP, ptr_t node::*YN, ptr_t node::*YP> void _calc_dp_incremental(ptr_t lastpath) {
			std::vector<ptr_t> path;
			for (ptr_t p = lastpath; p != null; p = _pderef(p).path_ptr) {
				path.push_back(p);
			}
			std::vector<ptr_t> nextv, nnextv;

			node *cur = &_pderef(path.back());
			if (cur->*XN != null) {
				node &xn = _pderef(cur->*XN);
				xn.dp = xn.energy + xn.compensation;
				ptr_t xnup = xn.*YN;
				nnextv.push_back(xnup);
				node &xnu = _pderef(xnup);
				if (xnu.*XN != null) {
					nnextv.push_back(xnu.*XN);
				}
				if (xnu.*XP != null) {
					nnextv.push_back(xnu.*XP);
				}
			}
			if (cur->*XP != null) {
				node &xp = _pderef(cur->*XP);
				xp.dp = xp.energy + xp.compensation;
				ptr_t xpup = xp.*YN;
				nnextv.push_back(xpup);
				node &xpu = _pderef(xpup);
				if (xpu.*XN != null) {
					nnextv.push_back(xpu.*XN);
				}
				if (xpu.*XP != null) {
					nnextv.push_back(xpu.*XP);
				}
			}
			path.pop_back();
			cur = &_pderef(path.back());
			if (cur->*XN != null) {
				nnextv.push_back(cur->*XN);
			}
			if (cur->*XP != null) {
				nnextv.push_back(cur->*XP);
			}

			do {
				std::swap(nextv, nnextv);
				nnextv.clear();
				std::sort(nextv.begin(), nextv.end());
				ptr_t last = null;
				for (ptr_t p : nextv) {
					if (p != last) {
						if (_update_dp_elem<XN, XP, YN, YP>(p)) {
							ptr_t cup = _pderef(p).*YN;
							nnextv.push_back(cup);
							node &cu = _pderef(cup);
							if (cu.*XN != null) {
								nnextv.push_back(cu.*XN);
							}
							if (cu.*XP != null) {
								nnextv.push_back(cu.*XP);
							}
						}
						last = p;
					}
				}
				path.pop_back();
				cur = &_pderef(path.back());
				if (cur->*XN != null) {
					nnextv.push_back(cur->*XN);
				}
				if (cur->*XP != null) {
					nnextv.push_back(cur->*XP);
				}
			} while (path.size() > 1);

			std::sort(nnextv.begin(), nnextv.end());
			ptr_t last = null;
			for (ptr_t p : nnextv) {
				if (p != last) {
					_update_dp_elem<XN, XP, YN, YP>(p);
					last = p;
				}
			}
			_fresh_dp = true;
		}
		template <ptr_t node::*XN, ptr_t node::*XP, ptr_t node::*YN, ptr_t node::*YP> void _update_dp(orientation orient) {
#ifdef USE_INCREMENTAL
			if (_fresh_dp && _cps.size() > 0 && _cps.back().second == orient) {
				_calc_dp_incremental<XN, XP, YN, YP>(_cps.back().first);
			} else {
				_recalc_dp<XN, XP, YN, YP>();
			}
#else
			_recalc_dp<XN, XP, YN, YP>();
#endif
		}

		template <ptr_t node::*XN, ptr_t node::*XP, ptr_t node::*YN, ptr_t node::*YP> ptr_t _get_carve_path_impl() {
			ptr_t res = _tl;
			for (ptr_t cur = _pderef(_tl).*XP; cur != null; cur = _pderef(cur).*XP) {
				if (_pderef(cur).dp < _pderef(res).dp) {
					res = cur;
				}
			}
			return res;
		}
		template <ptr_t node::*XN, ptr_t node::*XP, ptr_t node::*YN, ptr_t node::*YP> void _carve_path_impl(ptr_t head) {
			_detach_elem<XN, XP>(head);
			for (ptr_t prv = head, n = _pderef(prv).path_ptr; n != null; prv = n, n = _pderef(n).path_ptr) {
				_detach_elem<XN, XP>(n);
				if (n != _pderef(prv).*YP) {
					_fix_detached_links<XN, XP, YN, YP>(prv);
				}
			}
			_recalc_path_side_energy<XN, XP>(head);
		}
		template <ptr_t node::*XN, ptr_t node::*XP> void _restore_path_impl(ptr_t p) {
			for (ptr_t cur = p; cur != null; cur = _pderef(cur).path_ptr) {
				node &cn = _pderef(cur);
				if (cn.left != null) {
					_pderef(cn.left).right = cur;
				}
				if (cn.up != null) {
					_pderef(cn.up).down = cur;
				}
				if (cn.right != null) {
					_pderef(cn.right).left = cur;
				}
				if (cn.down != null) {
					_pderef(cn.down).up = cur;
				}
				if (cn.left == null && cn.up == null) {
					_tl = cur;
				}
				if (cn.right == null && cn.down == null) {
					_br = cur;
				}
			}
			_recalc_path_side_energy<XN, XP>(p);
			_fresh_dp = false;
		}
		template <ptr_t node::*XN, ptr_t node::*XP> void _recalc_path_side_energy(ptr_t head) {
			for (ptr_t cur = head; cur != null; cur = _pderef(cur).path_ptr) {
				if (_pderef(cur).*XN != null) {
					_calc_energy_elem(_pderef(cur).*XN);
				}
				if (_pderef(cur).*XP != null) {
					_calc_energy_elem(_pderef(cur).*XP);
				}
			}
		}

		template <typename ColorProc, typename Img> void _get_image_impl(Img &img) const {
			size_t yi = 0;
			for (ptr_t y = _tl; y != null; y = _pderef(y).down, ++yi) {
				Img::element_type *dst = img.at_y(yi);
				for (ptr_t x = y; x != null; x = _pderef(x).right, ++dst) {
					*dst = Img::element_type(ColorProc::process(_pderef(x)));
				}
			}
			assert(yi == _h);
		}

		template <
			ptr_t node::*XN, ptr_t node::*XP, ptr_t node::*YN, ptr_t node::*YP
		> enlarge_table_t _prepare_enlarging_impl(size_t wv, size_t hv, orientation orient) {
			assert(_cps.size() == 0);
			enlarge_table_t table;
			std::vector<ptr_t> heads;
			table.reserve(wv);
			heads.reserve(wv);
			for (size_t i = 0; i < wv; ++i) {
				_update_dp<XN, XP, YN, YP>(orient);
				std::vector<std::pair<size_t, size_t>> cpath;
				cpath.reserve(hv);
				ptr_t path = _get_carve_path_impl<XN, XP, YN, YP>();
				heads.push_back(path);
				for (ptr_t c = path; c != null; c = _pderef(c).path_ptr) {
					size_t i = _pgetpos(c);
					cpath.push_back({i % _w, i / _w});
				}
				_carve_path_impl<XN, XP, YN, YP>(path);
				table.push_back(std::move(cpath));
			}
			for (auto i = heads.rbegin(); i != heads.rend(); ++i) {
				_restore_path_impl<XN, XP>(*i);
			}
			return table;
		}

		size_t _at_y_impl(size_t y) const {
			const_ptr_t res = _tl;
			for (size_t i = 0; i < y; ++i) {
				res = _pderef(res).down;
			}
			return _pgetpos(res);
		}
		size_t _at_impl(size_t x, size_t y) const {
			const_ptr_t res = at_y(y);
			for (size_t i = 0; i < x; ++i) {
				res = _pderef(res).right;
			}
			return _pgetpos(res);
		}

#ifdef USE_INDEX_PTR
		ptr_t _pref(node &n) const {
			return &n - _n.data();
		}
		node &_pderef(ptr_t p) {
			return _n[p];
		}
		const node &_pderef(const_ptr_t p) const {
			return _n[p];
		}
		size_t _pgetpos(const_ptr_t p) const {
			return p;
		}
		ptr_t _pfrompos(size_t v) const {
			return v;
		}
#else
		ptr_t _pref(node &n) {
			return &n;
		}
		const_ptr_t _pref(const node &n) const {
			return &n;
		}
		node &_pderef(ptr_t p) {
			return *p;
		}
		const node &_pderef(const_ptr_t p) const {
			return *p;
		}
		size_t _pgetpos(const_ptr_t p) const {
			return p - _n.data();
		}
		ptr_t _pfrompos(size_t v) {
			return &_n[v];
		}
		const_ptr_t _pfrompos(size_t v) const {
			return &_n[v];
		}
#endif

		std::vector<node> _n;
		std::vector<std::pair<ptr_t, orientation>> _cps;
		ptr_t _tl = null, _br = null;
		size_t _w = 0, _h = 0;
		bool _fresh_dp = false;
	};
}
