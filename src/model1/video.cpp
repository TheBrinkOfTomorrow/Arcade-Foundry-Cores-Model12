// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
//
// Model 1 video, transplanted from MAME's src/mame/sega/model1_v.cpp at
// dddd73680656e355bb2b5beecab1167c9f07bf81 (see video.h for what changed).
// The System 24 tilemap part follows segaic24.cpp (copyright-holders:Olivier
// Galibert) as src/runtime/video.cpp transplanted it for Model 2.

#include "model1/video.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace m1 {

namespace {
enum { FRAC_SHIFT = 16 };
enum { MOIRE = 0x01000000 };
constexpr uint8_t PIXEL_LAYER0 = 0x10;  // TILEMAP_PIXEL_LAYER0
constexpr uint8_t CATEGORY_MASK = 0x0f; // TILEMAP_PIXEL_CATEGORY_MASK
constexpr int DRAW_OPAQUE = 0x80;       // TILEMAP_DRAW_OPAQUE
constexpr size_t kPoints = 1 << 18, kQuads = 1 << 17;

float u2f(uint32_t v) {
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
// float to int32 as x86-64 converts (cvttss2si): NaN and out of range give
// INT32_MIN. Spelled out so every host agrees with MAME's reference build.
int32_t f2i(float f) {
    if (!(f >= -2147483648.0f && f < 2147483648.0f)) return INT32_MIN;
    return int32_t(f);
}
// Wrapping int32 arithmetic (MAME's overflows wrap on its x86-64 build).
int32_t wadd(int32_t a, int32_t b) { return int32_t(uint32_t(a) + uint32_t(b)); }
int32_t wsub(int32_t a, int32_t b) { return int32_t(uint32_t(a) - uint32_t(b)); }
int32_t wmul(int32_t a, int32_t b) { return int32_t(uint32_t(a) * uint32_t(b)); }
int32_t wshl(int32_t a, int n) { return int32_t(uint32_t(a) << n); }
int32_t pal5bit(int32_t b) { return (b << 3) | (b >> 2); }
inline uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) { return 0xff000000u | (r << 16) | (g << 8) | b; }

// glm's float vec3 operations, in glm's order
float dot(const Video::vec3 &a, const Video::vec3 &b) {
    const float x = a.x * b.x, y = a.y * b.y, z = a.z * b.z;
    return (x + y) + z;
}
Video::vec3 normalize(const Video::vec3 &v) {
    const float inv = 1.0f / std::sqrt(dot(v, v)); // glm::inversesqrt
    return {v.x * inv, v.y * inv, v.z * inv};
}
} // namespace

Video::Video(const Memory &mem, const std::vector<uint8_t> &polygons)
    : mem_(mem), poly_rom_(polygons.size() / 4), screen_(size_t(W) * H), view_(std::make_unique<view_t>()),
      poly_ram_(0x400000, 0), tgp_ram_(0x100000 - 0x40000, 0), pointdb_(kPoints), quaddb_(kQuads),
      quadind_(kQuads, nullptr), overlay_block_(size_t(W) * H), overlay_hud_snapshot_(size_t(W) * H) {
    std::memcpy(poly_rom_.data(), polygons.data(), poly_rom_.size() * 4);
    pointpt_ = pointdb_.data();
    quadpt_ = quaddb_.data();
    for (int l = 0; l < 4; l++) pixmap_[l].assign(512 * 512, 0), flags_[l].assign(512 * 512, 0);
}

uint64_t Video::hash(const std::vector<uint32_t> &b) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (uint32_t px : b)
        for (int k = 0; k < 4; k++) {
            h ^= (px >> (8 * k)) & 0xff;
            h *= 0x100000001b3ULL;
        }
    return h;
}

// model1_paletteram_w: xBGR 555, bit 15 clear halves the channels
uint32_t Video::pen(uint32_t i) const {
    const uint16_t v = palette_word(i);
    int r = pal5bit((v >> 0) & 0x1f);
    int g = pal5bit((v >> 5) & 0x1f);
    int b = pal5bit((v >> 10) & 0x1f);
    if (!(v & 0x8000)) {
        r >>= 1;
        g >>= 1;
        b >>= 1;
    }
    return rgb(uint32_t(r), uint32_t(g), uint32_t(b));
}

uint32_t Video::readi(int adr) const { return display_list(uint32_t(adr)) | uint32_t(display_list(uint32_t(adr + 1))) << 16; }
int16_t Video::readi16(int adr) const { return int16_t(display_list(uint32_t(adr))); }
float Video::readf(int adr) const { return u2f(readi(adr)); }

void Video::view_t::transform_point(point_t *p) const {
    point_t q = *p;
    float xx = translation[0] * q.x + translation[3] * q.y + translation[6] * q.z + translation[9] + vxx;
    p->y = translation[1] * q.x + translation[4] * q.y + translation[7] * q.z + translation[10] + vyy;
    float zz = translation[2] * q.x + translation[5] * q.y + translation[8] * q.z + translation[11] + vzz;
    p->x = ayyc * xx - ayys * zz;
    p->z = ayys * xx + ayyc * zz;
}

void Video::view_t::transform_vector(vec3 &p) const {
    const vec3 q = p;
    const vec3 row1{translation[0], translation[3], translation[6]};
    const vec3 row2{translation[1], translation[4], translation[7]};
    const vec3 row3{translation[2], translation[5], translation[8]};
    p = vec3{dot(q, row1), dot(q, row2), dot(q, row3)};
}

float Video::view_determinant(const point_t *p1, const point_t *p2, const point_t *p3) const {
    float x1 = p2->x - p1->x;
    float y1 = p2->y - p1->y;
    float z1 = p2->z - p1->z;
    float x2 = p3->x - p1->x;
    float y2 = p3->y - p1->y;
    float z2 = p3->z - p1->z;
    return p1->x * (y1 * z2 - y2 * z1) + p1->y * (z1 * x2 - z2 * x1) + p1->z * (x1 * y2 - x2 * y1);
}

void Video::view_t::project_point(point_t *p) const {
    p->xx = p->x / p->z;
    p->yy = p->y / p->z;
    p->s.x = f2i(float(xc) + (p->xx * zoomx + viewx));
    p->s.y = f2i(float(yc) - (p->yy * zoomy + viewy));
}

void Video::view_t::project_point_direct(point_t *p) const {
    p->xx = p->x;
    p->yy = p->y;
    p->s.x = f2i(float(xc) + p->xx);
    p->s.y = f2i(float(yc) - p->yy);
}

void Video::draw_hline(int x1, int x2, int y, int color) {
    uint32_t *const base = &screen_[size_t(y) * W];
    while (x1 <= x2) {
        base[x1] = uint32_t(color);
        x1++;
    }
}

void Video::draw_hline_moired(int x1, int x2, int y, int color) {
    uint32_t *const base = &screen_[size_t(y) * W];
    while (x1 <= x2) {
        if (!((x1 ^ y) & 1)) base[x1] = uint32_t(color);
        x1++;
    }
}

void Video::fill_slope(view_t *view, int color, int32_t x1, int32_t x2, int32_t sl1, int32_t sl2, int32_t y1, int32_t y2,
                       int32_t *nx1, int32_t *nx2) {
    if (y1 > view->y2) return;

    if (y2 <= view->y1) {
        int delta = y2 - y1;
        *nx1 = wadd(x1, wmul(delta, sl1));
        *nx2 = wadd(x2, wmul(delta, sl2));
        return;
    }

    if (y2 > view->y2) y2 = view->y2 + 1;

    if (y1 < view->y1) {
        int delta = view->y1 - y1;
        x1 = wadd(x1, wmul(delta, sl1));
        x2 = wadd(x2, wmul(delta, sl2));
        y1 = view->y1;
    }

    if (x1 > x2 || (x1 == x2 && sl1 > sl2)) {
        std::swap(x1, x2);
        std::swap(sl1, sl2);
        std::swap(nx1, nx2);
    }

    while (y1 < y2) {
        if (y1 >= view->y1) {
            int xx1 = x1 >> FRAC_SHIFT;
            int xx2 = x2 >> FRAC_SHIFT;
            if (xx1 <= view->x2 || xx2 >= view->x1) {
                if (xx1 < view->x1) xx1 = view->x1;
                if (xx2 > view->x2) xx2 = view->x2;
                if (color & MOIRE) draw_hline_moired(xx1, xx2, y1, color);
                else draw_hline(xx1, xx2, y1, color);
            }
        }
        x1 = wadd(x1, sl1);
        x2 = wadd(x2, sl2);
        y1++;
    }
    *nx1 = x1;
    *nx2 = x2;
}

void Video::fill_line(view_t *view, int color, int32_t y, int32_t x1, int32_t x2) {
    int xx1 = x1 >> FRAC_SHIFT;
    int xx2 = x2 >> FRAC_SHIFT;

    if (y > view->y2 || y < view->y1) return;

    if (xx1 <= view->x2 || xx2 >= view->x1) {
        if (xx1 < view->x1) xx1 = view->x1;
        if (xx2 > view->x2) xx2 = view->x2;
        if (color & MOIRE) draw_hline_moired(xx1, xx2, y, color);
        else draw_hline(xx1, xx2, y, color);
    }
}

// MAME: "Draw a solid line between two screen points (integer Bresenham)",
// a wireframe primitive, clipped to the viewport first (Liang-Barsky).
void Video::draw_wireframe_line(const Rect &clip, int x1, int y1, int x2, int y2, uint32_t color, bool moire) {
    const double lx1 = x1, ly1 = y1;
    const double cdx = double(x2) - lx1, cdy = double(y2) - ly1;
    const double p[4] = {-cdx, cdx, -cdy, cdy};
    const double q[4] = {lx1 - clip.min_x, clip.max_x - lx1, ly1 - clip.min_y, clip.max_y - ly1};
    double t0 = 0.0, t1 = 1.0;
    for (int i = 0; i < 4; i++) {
        if (p[i] == 0.0) {
            if (q[i] < 0.0) return;
        } else {
            const double t = q[i] / p[i];
            if (p[i] < 0.0) t0 = std::max(t0, t);
            else t1 = std::min(t1, t);
        }
    }
    if (t0 > t1) return;

    if (t1 < 1.0) {
        x2 = int(std::lround(lx1 + t1 * cdx));
        y2 = int(std::lround(ly1 + t1 * cdy));
    }
    if (t0 > 0.0) {
        x1 = int(std::lround(lx1 + t0 * cdx));
        y1 = int(std::lround(ly1 + t0 * cdy));
    }

    int dx = std::abs(x2 - x1), dy = std::abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        if (x1 >= 0 && x1 < W && y1 >= 0 && y1 < H)
            if (!moire || !((x1 ^ y1) & 1)) screen_[size_t(y1) * W + size_t(x1)] = color;
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void Video::fill_quad(view_t *view, const quad_t &q) {
    spoint_t p[8];
    int color = q.col;

    if (color < 0) color = -1 - color;

    // A wireframe primitive: a degenerate quad with two distinct vertices.
    {
        int ax = q.p[0]->s.x, ay = q.p[0]->s.y;
        int bx = ax, by = ay, ndist = 1;
        for (int i = 1; i < 4; i++) {
            int vx = q.p[i]->s.x, vy = q.p[i]->s.y;
            if (vx == ax && vy == ay) continue;
            if (ndist == 1) {
                bx = vx;
                by = vy;
                ndist = 2;
            } else if (vx != bx || vy != by) {
                ndist = 3;
                break;
            }
        }
        if (ndist == 2) {
            draw_wireframe_line(Rect{view->x1, view->x2, view->y1, view->y2}, ax, ay, bx, by, uint32_t(color & ~MOIRE),
                                (color & MOIRE) != 0);
            return;
        }
    }

    for (int i = 0; i < 4; i++) {
        p[i].x = p[i + 4].x = wshl(q.p[i]->s.x, FRAC_SHIFT);
        p[i].y = p[i + 4].y = q.p[i]->s.y;
    }

    int pmin = 0;
    int pmax = 0;
    for (int i = 1; i < 4; i++) {
        if (p[i].y < p[pmin].y) pmin = i;
        if (p[i].y > p[pmax].y) pmax = i;
    }

    int32_t cury = p[pmin].y;
    int32_t limy = p[pmax].y;

    int32_t x1, x2;
    if (cury == limy) {
        x1 = p[0].x;
        x2 = p[0].x;
        for (int i = 1; i < 4; i++) {
            if (p[i].x < x1) x1 = p[i].x;
            if (p[i].x > x2) x2 = p[i].x;
        }
        fill_line(view, color, cury, x1, x2);
        return;
    }

    if (cury > view->y2) return;
    if (limy <= view->y1) return;
    if (limy > view->y2) limy = view->y2;

    int ps1 = pmin + 4;
    int ps2 = pmin;
    int32_t sl1, sl2;

    // MAME jumps into the loop at "startup"
    auto startup = [&] {
        while (p[ps1 - 1].y == cury) ps1--;
        while (p[ps2 + 1].y == cury) ps2++;
        x1 = p[ps1].x;
        x2 = p[ps2].x;
        sl1 = wsub(x1, p[ps1 - 1].x) / (cury - p[ps1 - 1].y);
        sl2 = wsub(x2, p[ps2 + 1].x) / (cury - p[ps2 + 1].y);
    };
    startup();
    for (;;) {
        if (p[ps1 - 1].y == p[ps2 + 1].y) {
            fill_slope(view, color, x1, x2, sl1, sl2, cury, p[ps1 - 1].y, &x1, &x2);
            cury = p[ps1 - 1].y;
            if (cury >= limy) break;
            ps1--;
            ps2++;
            startup();
        } else if (p[ps1 - 1].y < p[ps2 + 1].y) {
            fill_slope(view, color, x1, x2, sl1, sl2, cury, p[ps1 - 1].y, &x1, &x2);
            cury = p[ps1 - 1].y;
            if (cury >= limy) break;
            ps1--;
            while (p[ps1 - 1].y == cury) ps1--;
            x1 = p[ps1].x;
            sl1 = wsub(x1, p[ps1 - 1].x) / (cury - p[ps1 - 1].y);
        } else {
            fill_slope(view, color, x1, x2, sl1, sl2, cury, p[ps2 + 1].y, &x1, &x2);
            cury = p[ps2 + 1].y;
            if (cury >= limy) break;
            ps2++;
            while (p[ps2 + 1].y == cury) ps2++;
            x2 = p[ps2].x;
            sl2 = wsub(x2, p[ps2 + 1].x) / (cury - p[ps2 + 1].y);
        }
    }
    if (cury == limy) fill_line(view, color, cury, x1, x2);
}

int Video::quad_t::compare(const quad_t *other) const {
    float z2 = other->z;
    if (z < z2) return +1;
    if (z > z2) return -1;
    if (this - other < 0) return -1;
    return +1;
}

namespace {
int comp_quads(const void *q1, const void *q2) {
    const Video::quad_t *const *c1 = static_cast<const Video::quad_t *const *>(q1);
    const Video::quad_t *const *c2 = static_cast<const Video::quad_t *const *>(q2);
    return (*c1)->compare(*c2);
}
} // namespace

void Video::sort_quads() {
    const int count = int(quadpt_ - quaddb_.data());
    for (int i = 0; i < count; i++) quadind_[size_t(i)] = &quaddb_[size_t(i)];
    // MAME's qsort; the order is total (ties broken by position) unless a z
    // is NaN, where the result is the C library's.
    std::qsort(quadind_.data(), size_t(count), sizeof(quad_t *), comp_quads);
}

void Video::unsort_quads() {
    const int count = int(quadpt_ - quaddb_.data());
    for (int i = 0; i < count; i++) quadind_[size_t(i)] = &quaddb_[size_t(i)];
}

void Video::draw_quads(const Rect &cliprect) {
    view_t *view = view_.get();
    const int count = int(quadpt_ - quaddb_.data());

    const int save_x1 = view->x1, save_x2 = view->x2, save_y1 = view->y1, save_y2 = view->y2;
    view->x1 = std::max(view->x1, cliprect.min_x);
    view->x2 = std::min(view->x2, cliprect.max_x);
    view->y1 = std::max(view->y1, cliprect.min_y);
    view->y2 = std::min(view->y2, cliprect.max_y);

    for (int i = 0; i < count; i++) fill_quad(view, *quadind_[size_t(i)]);

    view->x1 = save_x1;
    view->x2 = save_x2;
    view->y1 = save_y1;
    view->y2 = save_y2;
}

void Video::view_t::recompute_frustum() {
    a_left = (float(x1) - float(xc) - viewx) / zoomx;
    a_right = (float(x2) - float(xc) - viewx) / zoomx;
    a_bottom = (float(-y1) + float(yc) - viewy) / zoomy;
    a_top = (float(-y2) + float(yc) - viewy) / zoomy;
}

bool Video::fclip_isc_bottom(view_t *view, point_t *p) { return p->y > (p->z * view->a_bottom); }
void Video::fclip_clip_bottom(view_t *view, point_t *pt, point_t *p1, point_t *p2) {
    float t = (p2->z * view->a_bottom - p2->y) / ((p2->z - p1->z) * view->a_bottom - (p2->y - p1->y));
    pt->x = p1->x * t + p2->x * (1 - t);
    pt->y = p1->y * t + p2->y * (1 - t);
    pt->z = p1->z * t + p2->z * (1 - t);
    view->project_point(pt);
}
bool Video::fclip_isc_top(view_t *view, point_t *p) { return p->y < (p->z * view->a_top); }
void Video::fclip_clip_top(view_t *view, point_t *pt, point_t *p1, point_t *p2) {
    float t = (p2->z * view->a_top - p2->y) / ((p2->z - p1->z) * view->a_top - (p2->y - p1->y));
    pt->x = p1->x * t + p2->x * (1 - t);
    pt->y = p1->y * t + p2->y * (1 - t);
    pt->z = p1->z * t + p2->z * (1 - t);
    view->project_point(pt);
}
bool Video::fclip_isc_left(view_t *view, point_t *p) { return p->x < (p->z * view->a_left); }
void Video::fclip_clip_left(view_t *view, point_t *pt, point_t *p1, point_t *p2) {
    float t = (p2->z * view->a_left - p2->x) / ((p2->z - p1->z) * view->a_left - (p2->x - p1->x));
    pt->x = p1->x * t + p2->x * (1 - t);
    pt->y = p1->y * t + p2->y * (1 - t);
    pt->z = p1->z * t + p2->z * (1 - t);
    view->project_point(pt);
}
bool Video::fclip_isc_right(view_t *view, point_t *p) { return p->x > (p->z * view->a_right); }
void Video::fclip_clip_right(view_t *view, point_t *pt, point_t *p1, point_t *p2) {
    float t = (p2->z * view->a_right - p2->x) / ((p2->z - p1->z) * view->a_right - (p2->x - p1->x));
    pt->x = p1->x * t + p2->x * (1 - t);
    pt->y = p1->y * t + p2->y * (1 - t);
    pt->z = p1->z * t + p2->z * (1 - t);
    view->project_point(pt);
}

// m_clipfn[level]: bottom, top, left, right
bool Video::clip_isclipped(int level, view_t *v, point_t *p) const {
    switch (level) {
    case 0: return fclip_isc_bottom(v, p);
    case 1: return fclip_isc_top(v, p);
    case 2: return fclip_isc_left(v, p);
    default: return fclip_isc_right(v, p);
    }
}
void Video::clip_clip(int level, view_t *v, point_t *pt, point_t *p1, point_t *p2) const {
    if (pt >= pointdb_.data() + pointdb_.size()) throw std::runtime_error("model1 video: point buffer full");
    switch (level) {
    case 0: fclip_clip_bottom(v, pt, p1, p2); break;
    case 1: fclip_clip_top(v, pt, p1, p2); break;
    case 2: fclip_clip_left(v, pt, p1, p2); break;
    default: fclip_clip_right(v, pt, p1, p2); break;
    }
}

void Video::fclip_push_quad_next(int level, quad_t &q, point_t *p1, point_t *p2, point_t *p3, point_t *p4) {
    quad_t cquad(q.col, q.z, p1, p2, p3, p4);
    fclip_push_quad(level + 1, cquad);
}

void Video::fclip_push_quad(int level, quad_t &q) {
    view_t *view = view_.get();

    if (level == 4) {
        if (quadpt_ == quaddb_.data() + quaddb_.size()) throw std::runtime_error("model1 video: quad buffer full");
        *quadpt_ = q;
        quadpt_++;
        return;
    }

    bool is_out[4];
    for (int i = 0; i < 4; i++) is_out[i] = clip_isclipped(level, view, q.p[i]);

    if (!is_out[0] && !is_out[1] && !is_out[2] && !is_out[3]) {
        fclip_push_quad(level + 1, q);
        return;
    }
    if (is_out[0] && is_out[1] && is_out[2] && is_out[3]) return;

    int i;
    for (i = 0; i < 4; i++)
        if (is_out[i] && !is_out[(i - 1) & 3]) break;

    point_t *pt[4];
    bool is_out2[4];
    for (int j = 0; j < 4; j++) {
        pt[j] = q.p[(i + j) & 3];
        is_out2[j] = is_out[(i + j) & 3];
    }

    if (is_out2[1]) {
        if (is_out2[2]) {
            clip_clip(level, view, pointpt_, pt[2], pt[3]);
            point_t *pi1 = pointpt_++;
            clip_clip(level, view, pointpt_, pt[3], pt[0]);
            point_t *pi2 = pointpt_++;
            fclip_push_quad_next(level, q, pi1, pt[3], pi2, pi2);
        } else {
            clip_clip(level, view, pointpt_, pt[1], pt[2]);
            point_t *pi1 = pointpt_++;
            clip_clip(level, view, pointpt_, pt[3], pt[0]);
            point_t *pi2 = pointpt_++;
            fclip_push_quad_next(level, q, pi1, pt[2], pt[3], pi2);
        }
    } else {
        if (is_out2[2]) {
            clip_clip(level, view, pointpt_, pt[0], pt[1]);
            point_t *pi1 = pointpt_++;
            clip_clip(level, view, pointpt_, pt[1], pt[2]);
            point_t *pi2 = pointpt_++;
            fclip_push_quad_next(level, q, pi1, pt[1], pi2, pi2);
            clip_clip(level, view, pointpt_, pt[2], pt[3]);
            pi1 = pointpt_++;
            clip_clip(level, view, pointpt_, pt[3], pt[0]);
            pi2 = pointpt_++;
            fclip_push_quad_next(level, q, pi1, pt[3], pi2, pi2);
        } else {
            clip_clip(level, view, pointpt_, pt[0], pt[1]);
            point_t *pi1 = pointpt_++;
            clip_clip(level, view, pointpt_, pt[3], pt[0]);
            point_t *pi2 = pointpt_++;
            fclip_push_quad_next(level, q, pi1, pt[1], pt[2], pt[3]);
            fclip_push_quad_next(level, q, pt[3], pi2, pi1, pi1);
        }
    }
}

float Video::min4f(float a, float b, float c, float d) {
    float m = a;
    if (b < m) m = b;
    if (c < m) m = c;
    if (d < m) m = d;
    return m;
}

float Video::max4f(float a, float b, float c, float d) {
    float m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    if (d > m) m = d;
    return m;
}

float Video::compute_specular(const vec3 &normal, const vec3 &light, float diffuse, int lmode) const {
    if (!view_->spec_enable) return 0.0f;
    const lightparam_t &lp = view_->lightparams[lmode];
    if (lp.p == 0 || lp.s <= 0.0f) return 0.0f;
    float s = (2.0f * diffuse * normal.z) - light.z;
    if (s <= 0.0f) return 0.0f;
    if (lp.p >= 2) s *= s;
    if (lp.p >= 4) s *= s;
    if (lp.p >= 7) s *= s;
    return std::min(s * lp.s, 1.0f);
}

// old_z: the flat-z "reuse" register, kept across objects within a list walk
void Video::push_object(uint32_t tex_adr, uint32_t poly_adr, uint32_t size, float &old_z) {
    // MAME: "Protect against bad data when attacking a super destroyer"
    if (tex_adr == 0xffffffff || size >= 0x1000000) return;
    ++stereo_object_;

    const uint32_t *poly_data;
    size_t poly_words;
    if (poly_adr & 0x800000) poly_data = poly_ram_.data(), poly_words = poly_ram_.size();
    else poly_data = poly_rom_.data(), poly_words = poly_rom_.size();
    poly_adr &= 0x7fffff;
    auto pf = [&](uint32_t i) {
        if (i >= poly_words) throw std::runtime_error("model1 video: object data past its memory");
        return u2f(poly_data[i]);
    };
    auto new_point = [&] {
        if (pointpt_ == pointdb_.data() + pointdb_.size()) throw std::runtime_error("model1 video: point buffer full");
        return pointpt_++;
    };

    if (!size) size = 0xffffffff;

    point_t *old_p0 = new_point();
    point_t *old_p1 = new_point();

    old_p0->x = pf(poly_adr + 0);
    old_p0->y = pf(poly_adr + 1);
    old_p0->z = pf(poly_adr + 2);
    old_p1->x = pf(poly_adr + 3);
    old_p1->y = pf(poly_adr + 4);
    old_p1->z = pf(poly_adr + 5);
    view_->transform_point(old_p0);
    view_->transform_point(old_p1);

    if (old_p0->z > 0) view_->project_point(old_p0);
    else old_p0->s.x = old_p0->s.y = 0;
    if (old_p1->z > 0) view_->project_point(old_p1);
    else old_p1->s.x = old_p1->s.y = 0;

    poly_adr += 6;

    for (uint32_t i = 0; i < size; i++) {
        if (poly_adr >= poly_words) throw std::runtime_error("model1 video: object data past its memory");
        uint32_t flags = poly_data[poly_adr];

        int type = flags & 3;
        if (!type) break;

        if (flags & 0x00001000) tex_adr++;
        int lightmode = ((flags >> 17) & 15) | ((flags & 0x00400000) ? 0x80 : 0);

        point_t *p0 = new_point();
        point_t *p1 = new_point();

        vec3 vn{pf(poly_adr + 1), pf(poly_adr + 2), pf(poly_adr + 3)};
        p0->x = pf(poly_adr + 4);
        p0->y = pf(poly_adr + 5);
        p0->z = pf(poly_adr + 6);
        p1->x = pf(poly_adr + 7);
        p1->y = pf(poly_adr + 8);
        p1->z = pf(poly_adr + 9);

        if (type == 2) {
            p1->x = p0->x;
            p1->y = p0->y;
            p1->z = p0->z;
        }

        int link = (flags >> 8) & 3;

        view_->transform_vector(vn);
        view_->transform_point(p0);
        view_->transform_point(p1);
        if (p0->z > 0) view_->project_point(p0);
        else p0->s.x = p0->s.y = 0;
        if (p1->z > 0) view_->project_point(p1);
        else p1->s.x = p1->s.y = 0;

        quad_t cquad;

        if (link && ((flags & 0x00004000) || !(view_determinant(old_p1, old_p0, p0) > 0))) {
            vn = normalize(vn);

            cquad.p[0] = old_p1;
            cquad.p[1] = old_p0;
            cquad.p[2] = p0;
            cquad.p[3] = p1;

            switch ((flags >> 10) & 3) {
            case 0: cquad.z = old_z; break;
            case 1: cquad.z = old_z = min4f(old_p1->z, old_p0->z, p0->z, p1->z); break;
            case 2: cquad.z = old_z = max4f(old_p1->z, old_p0->z, p0->z, p1->z); break;
            case 3:
            default: cquad.z = 0.0; break;
            }

            {
                float dif = dot(vn, view_->light);
                float spec = compute_specular(vn, view_->light, dif, lightmode);
                float ln = view_->lightparams[lightmode].a + view_->lightparams[lightmode].d * std::max(0.0f, dif) + spec;
                int lumval = f2i(255.0f * std::min(1.0f, ln));
                if (tex_adr - 0x40000 >= tgp_ram_.size()) throw std::runtime_error("model1 video: colour index past its RAM");
                uint16_t tex_data = tgp_ram_[tex_adr - 0x40000];
                int color = palette_word(0x1000 | (tex_data & 0x3ff));
                int r = (color >> 0x0) & 0x1f;
                int g = (color >> 0x5) & 0x1f;
                int b = (color >> 0xA) & 0x1f;
                if (((tex_data >> 10) & 3) == 1 && (frame_ & 1)) {
                    // rotate b -> g -> r -> b
                    int tmp = b;
                    b = r;
                    r = g;
                    g = tmp;
                }

                lumval >>= 2; // MAME: "there must be a luma translation table somewhere"
                if (lumval > 0x3f) lumval = 0x3f;
                else if (lumval < 0) lumval = 0;

                if (tex_data & 0x400) lumval = 0x3f; // flat UI element: unlit

                r = (color_xlat(uint32_t((r << 8) | lumval | 0x0)) >> 3) & 0x1f;
                g = (color_xlat(uint32_t((g << 8) | lumval | 0x2000)) >> 3) & 0x1f;
                b = (color_xlat(uint32_t((b << 8) | lumval | 0x4000)) >> 3) & 0x1f;
                cquad.col = (pal5bit(r) << 16) | (pal5bit(g) << 8) | (pal5bit(b) << 0);
            }

            if (flags & 0x00002000) cquad.col |= MOIRE;

            if (keep_stereo_ && pass_ == RENDER_BELOW_HUD) stereo_keep(cquad, false);
            fclip_push_quad(0, cquad);
        }

        poly_adr += 10;
        switch (link) {
        case 0:
        case 2:
            old_p0 = p0;
            old_p1 = p1;
            break;
        case 1: old_p1 = p0; break;
        case 3: old_p0 = p1; break;
        }
    }
}

int Video::push_direct(int list_offset) {
    uint32_t tex_adr = readi(list_offset + 2);
    auto new_point = [&] {
        if (pointpt_ == pointdb_.data() + pointdb_.size()) throw std::runtime_error("model1 video: point buffer full");
        return pointpt_++;
    };

    point_t *old_p0 = new_point();
    point_t *old_p1 = new_point();

    old_p0->x = readf(list_offset + 2 + 4);
    old_p0->y = readf(list_offset + 2 + 6);
    old_p0->z = readf(list_offset + 2 + 8);
    old_p1->x = readf(list_offset + 2 + 12);
    old_p1->y = readf(list_offset + 2 + 14);
    old_p1->z = readf(list_offset + 2 + 16);

    // Direct polys are screen space: every vertex projected unconditionally.
    view_->project_point_direct(old_p0);
    view_->project_point_direct(old_p1);

    list_offset += 18;

    for (;;) {
        uint32_t flags = readi(list_offset + 2);

        int type = flags & 3;
        if (!type) break;

        if (flags & 0x00001000) tex_adr++;

        point_t *p0 = new_point();
        point_t *p1 = new_point();

        uint32_t lum = readi(list_offset + 2 + 2);

        float z = 0;
        if (type == 2) {
            p0->x = readf(list_offset + 2 + 6);
            p0->y = readf(list_offset + 2 + 8);
            p0->z = readf(list_offset + 2 + 10);
            z = p0->z;
            *p1 = *p0;
            list_offset += 12;
        } else {
            p0->x = readf(list_offset + 2 + 6);
            p0->y = readf(list_offset + 2 + 8);
            p0->z = readf(list_offset + 2 + 10);
            p1->x = readf(list_offset + 2 + 14);
            p1->y = readf(list_offset + 2 + 16);
            p1->z = readf(list_offset + 2 + 18);
            z = readf(list_offset + 2 + 12);
            list_offset += 20;
        }

        int link = (flags >> 8) & 3;

        view_->project_point_direct(p0);
        view_->project_point_direct(p1);

        quad_t cquad;
        bool skip = !link;
        if (!skip) {
            cquad.p[0] = old_p1;
            cquad.p[1] = old_p0;
            cquad.p[2] = p0;
            cquad.p[3] = p1;
            cquad.z = z;
            if (tex_adr - 0x40000 >= tgp_ram_.size()) throw std::runtime_error("model1 video: colour index past its RAM");
            {
                int lumval = f2i(float(lum >> 24) * 2.0f);
                int color = palette_word(0x1000 | (tgp_ram_[tex_adr - 0x40000] & 0x3ff));
                int r = (color >> 0x0) & 0x1f;
                int g = (color >> 0x5) & 0x1f;
                int b = (color >> 0xA) & 0x1f;
                lumval >>= 2;
                if (lumval > 0x3f) lumval = 0x3f;
                else if (lumval < 0) lumval = 0;
                r = (color_xlat(uint32_t((r << 8) | lumval | 0x0)) >> 3) & 0x1f;
                g = (color_xlat(uint32_t((g << 8) | lumval | 0x2000)) >> 3) & 0x1f;
                b = (color_xlat(uint32_t((b << 8) | lumval | 0x4000)) >> 3) & 0x1f;
                cquad.col = (pal5bit(r) << 16) | (pal5bit(g) << 8) | (pal5bit(b) << 0);
            }
            if (flags & 0x00002000) {
                cquad.col |= MOIRE;
                // a moire poly with the transparent pen fades toward the backdrop
                if ((tgp_ram_[tex_adr - 0x40000] & 0x3ff) == 0) cquad.col = int(MOIRE | (pen(0) & 0xffffff));
                // a degenerate moire record (all four vertices on one point)
                if (cquad.p[0]->s.x == cquad.p[1]->s.x && cquad.p[1]->s.x == cquad.p[2]->s.x &&
                    cquad.p[2]->s.x == cquad.p[3]->s.x && cquad.p[0]->s.y == cquad.p[1]->s.y &&
                    cquad.p[1]->s.y == cquad.p[2]->s.y && cquad.p[2]->s.y == cquad.p[3]->s.y)
                    skip = true;
            }
            // screen space: no frustum clip, straight to the queue
            if (!skip) {
                if (keep_stereo_ && pass_ == RENDER_BELOW_HUD) stereo_keep(cquad, true);
                if (quadpt_ == quaddb_.data() + quaddb_.size()) throw std::runtime_error("model1 video: quad buffer full");
                *quadpt_++ = cquad;
            }
        }

        switch (link) {
        case 0:
        case 2:
            old_p0 = p0;
            old_p1 = p1;
            break;
        case 1: old_p1 = p0; break;
        case 3: old_p0 = p1; break;
        }
    }
    list_offset += 4;
    return list_offset;
}

int Video::skip_direct(int list_offset) const {
    list_offset += 18;
    while (true) {
        uint32_t flags = readi(list_offset + 2);
        int type = flags & 3;
        if (!type) break;
        if (type == 2) list_offset += 12;
        else list_offset += 20;
    }
    list_offset += 4;
    return list_offset;
}

void Video::draw_objects(const Rect &cliprect) {
    if (keep_stereo_ && pass_ == RENDER_BELOW_HUD) stereo_sort();
    if (quadpt_ != quaddb_.data()) {
        sort_quads();
        draw_quads(cliprect);
    }
    quadpt_ = quaddb_.data();
    pointpt_ = pointdb_.data();
}

void Video::stereo_keep(const quad_t &q, bool direct) {
    const view_t &v = *view_;
    StereoProjection p{float(v.xc), float(v.yc), v.zoomx, v.zoomy, v.viewx, v.viewy,
                       std::max(v.x1, 0), std::min(v.x2, W - 1), std::max(v.y1, 0), std::min(v.y2, H - 1)};
    if (stereo_projections_.empty() || !(stereo_projections_.back() == p)) {
        if (stereo_projections_.size() > 0xfffe) return;
        stereo_projections_.push_back(p);
    }
    int col = q.col;
    if (col < 0) col = -1 - col;
    StereoQuad k;
    for (int i = 0; i < 4; i++) {
        k.x[i] = q.p[i]->x;
        k.y[i] = q.p[i]->y;
        k.z[i] = q.p[i]->z;
    }
    k.color = uint32_t(col) & 0xffffff;
    k.moire = (col & MOIRE) != 0;
    k.direct = direct;
    k.projection = uint16_t(stereo_projections_.size() - 1);
    k.object = direct ? 0 : stereo_object_;
    stereo_quads_.push_back(k);
    stereo_z_.push_back(q.z);
}

// The batch since the last draw_objects in sort_quads' order: far to near,
// ties in the order made.
void Video::stereo_sort() {
    const size_t n = stereo_quads_.size() - stereo_batch_;
    if (n > 1) {
        std::vector<uint32_t> order(n);
        for (size_t i = 0; i < n; i++) order[i] = uint32_t(i);
        const float *z = stereo_z_.data() + stereo_batch_;
        std::stable_sort(order.begin(), order.end(), [z](uint32_t a, uint32_t b) { return z[a] > z[b]; });
        stereo_sorted_.assign(stereo_quads_.begin() + std::ptrdiff_t(stereo_batch_), stereo_quads_.end());
        for (size_t i = 0; i < n; i++) stereo_quads_[stereo_batch_ + i] = stereo_sorted_[order[i]];
    }
    stereo_batch_ = stereo_quads_.size();
}

void Video::set_current_render_list() {
    if (!(listctl_[0] & 4)) listctl_[0] = uint16_t((listctl_[0] & ~0x40) | (listctl_[0] & 8 ? 0x40 : 0));
    list_base_ = listctl_[0] & 0x40 ? 0x610000 : 0x600000;
}

int Video::get_list_number() {
    if (!(listctl_[0] & 4)) listctl_[0] = uint16_t((listctl_[0] & ~0x40) | (listctl_[0] & 8 ? 0x40 : 0));
    return listctl_[0] & 0x40 ? 0 : 1;
}

void Video::end_frame(uint64_t frame) {
    if ((listctl_[0] & 4) && (frame & 1)) listctl_[0] ^= 0x40;
}

void Video::view_t::init_translation_matrix() {
    std::fill(std::begin(translation), std::end(translation), 0.0f);
    translation[0] = 1.0;
    translation[4] = 1.0;
    translation[8] = 1.0;
}

void Video::view_t::set_viewport(float xcenter, float ycenter, float xl, float xr, float yb, float yt) {
    xc = f2i(xcenter);
    yc = f2i(ycenter);
    x1 = f2i(xl);
    x2 = f2i(xr);
    y1 = f2i(yb);
    y2 = f2i(yt);
    recompute_frustum();
}

void Video::view_t::set_lightparam(int index, float diffuse, float ambient, float specular, int power) {
    lightparams[index & 0xff].d = diffuse;
    lightparams[index & 0xff].a = ambient;
    lightparams[index & 0xff].s = specular;
    lightparams[index & 0xff].p = power;
}

void Video::view_t::set_zoom(float x, float y) {
    zoomx = x;
    zoomy = y;
    recompute_frustum();
}

void Video::view_t::set_light_direction(float x, float y, float z) { light = normalize(vec3{x, y, z}); }

void Video::view_t::set_translation_matrix(const float *mat) {
    for (int i = 0; i < 12; i++) translation[i] = mat[i];
}

void Video::view_t::set_view_translation(float x, float y) {
    viewx = x;
    viewy = y;
    recompute_frustum();
}

void Video::tgp_render(const Rect &cliprect, render_pass pass) {
    render_done_ = true;
    if ((listctl_[1] & 0x1f) != 0x1f) return;
    set_current_render_list();

    view_->init_translation_matrix();
    float old_z = 0;

    int list_offset = 0;
    for (;;) {
        int type = int(readi(list_offset + 0));
        switch (type) {
        case 0: list_offset += 2; break;
        case 1: // object below the HUD tilemaps
            if (pass == RENDER_BELOW_HUD)
                push_object(readi(list_offset + 2), readi(list_offset + 4), readi(list_offset + 6), old_z);
            list_offset += 8;
            break;
        case 0x41: // object above the HUD
            if (pass == RENDER_ABOVE_HUD)
                push_object(readi(list_offset + 2), readi(list_offset + 4), readi(list_offset + 6), old_z);
            list_offset += 8;
            break;
        case 2: // direct polys, in the below-HUD pass
            if (pass == RENDER_BELOW_HUD) list_offset = push_direct(list_offset);
            else list_offset = skip_direct(list_offset);
            break;
        case 3: {
            draw_objects(cliprect);
            float xc = readi16(list_offset + 4);
            float yc = float(383 - (readi16(list_offset + 6) - 39));
            float x1 = readi16(list_offset + 8);
            float y2 = float(383 - (readi16(list_offset + 10) - 39));
            float x2 = readi16(list_offset + 12);
            float y1 = float(383 - (readi16(list_offset + 14) - 39));
            view_->set_viewport(xc, yc, x1, x2, y1, y2);
            list_offset += 16;
            break;
        }
        case 4: {
            int adr = int(readi(list_offset + 2));
            int len = int(readi(list_offset + 4)) + 1;
            for (int i = 0; i < len; i++) {
                const uint32_t at = uint32_t(adr - 0x40000 + i);
                if (at >= tgp_ram_.size()) throw std::runtime_error("model1 video: colour write past its RAM");
                tgp_ram_[at] = uint16_t(readi16(list_offset + 6 + 2 * i));
            }
            list_offset += 6 + len * 2;
            break;
        }
        case 5: {
            int adr = int(readi(list_offset + 2));
            int len = int(readi(list_offset + 4));
            for (int i = 0; i < len; i++) {
                const uint32_t at = uint32_t(adr - 0x800000 + i);
                if (at >= poly_ram_.size()) throw std::runtime_error("model1 video: object write past its RAM");
                poly_ram_[at] = readi(list_offset + 2 * i + 6);
            }
            list_offset += 6 + len * 2;
            break;
        }
        case 6: {
            int adr = int(readi(list_offset + 2));
            int len = int(readi(list_offset + 4));
            for (int i = 0; i < len; i++) {
                int v = int(readi(list_offset + 6 + i * 2));
                float diffuse = (float(v & 0xff)) / 255.0f;
                float ambient = (float((v >> 8) & 0xff)) / 255.0f;
                float specular = (float((v >> 16) & 0xff)) / 255.0f;
                int power = (v >> 24) & 0xff;
                view_->set_lightparam(i + adr, diffuse, ambient, specular, power);
            }
            list_offset += 6 + len * 2;
            break;
        }
        case 7: // mode word: bit 0 enables the specular term
            view_->spec_enable = readi(list_offset + 2) & 1;
            list_offset += 4;
            break;
        case 8: list_offset += 4; break;
        case 9:
            view_->set_zoom(readf(list_offset + 2) * 4, readf(list_offset + 4) * 4);
            list_offset += 6;
            break;
        case 0xa:
            view_->set_light_direction(readf(list_offset + 2), readf(list_offset + 4), readf(list_offset + 6));
            list_offset += 8;
            break;
        case 0xb: {
            float mat[12];
            for (int i = 0; i < 12; i++) mat[i] = readf(list_offset + 2 + 2 * i);
            view_->set_translation_matrix(mat);
            list_offset += 26;
            break;
        }
        case 0xc:
            view_->set_view_translation(readf(list_offset + 2), readf(list_offset + 4));
            list_offset += 6;
            break;
        case 0xf:
        default: goto end;
        }
    }
end:
    draw_objects(cliprect);
}

void Video::tgp_scan() {
    if (!render_done_ && (listctl_[1] & 0x1f) == 0x1f) {
        set_current_render_list();
        // Skip everything but the data uploads
        int list_offset = 0;
        for (;;) {
            int type = int(readi(list_offset + 0));
            switch (type) {
            case 0: list_offset += 2; break;
            case 1:
            case 0x41: list_offset += 8; break;
            case 2: list_offset = skip_direct(list_offset); break;
            case 3: list_offset += 16; break;
            case 4: {
                int adr = int(readi(list_offset + 2));
                int len = int(readi(list_offset + 4)) + 1;
                for (int i = 0; i < len; i++) {
                    const uint32_t at = uint32_t(adr - 0x40000 + i);
                    if (at >= tgp_ram_.size()) throw std::runtime_error("model1 video: colour write past its RAM");
                    tgp_ram_[at] = uint16_t(readi16(list_offset + 6 + 2 * i));
                }
                list_offset += 6 + len * 2;
                break;
            }
            case 5: {
                int adr = int(readi(list_offset + 2));
                int len = int(readi(list_offset + 4));
                for (int i = 0; i < len; i++) {
                    const uint32_t at = uint32_t(adr - 0x800000 + i);
                    if (at >= poly_ram_.size()) throw std::runtime_error("model1 video: object write past its RAM");
                    poly_ram_[at] = readi(list_offset + 2 * i + 6);
                }
                list_offset += 6 + len * 2;
                break;
            }
            case 6: {
                int adr = int(readi(list_offset + 2));
                int len = int(readi(list_offset + 4));
                for (int i = 0; i < len; i++) {
                    int v = int(readi(list_offset + 6 + i * 2));
                    lightparam_t &lp = view_->lightparams[(i + adr) & 0xff];
                    lp.d = (float(v & 0xff)) / 255.0f;
                    lp.a = (float((v >> 8) & 0xff)) / 255.0f;
                    lp.s = (float((v >> 16) & 0xff)) / 255.0f;
                    lp.p = (v >> 24) & 0xff;
                }
                list_offset += 6 + len * 2;
                break;
            }
            case 7: list_offset += 4; break;
            case 8: list_offset += 4; break;
            case 9: list_offset += 6; break;
            case 0xa: list_offset += 8; break;
            case 0xb: list_offset += 26; break;
            case 0xc: list_offset += 6; break;
            case 0xf:
            case -1:
            default: goto end;
            }
        }
    end:;
    }
    render_done_ = false;
}

// The HUD after the cat1 tilemaps: a bright pixel is a "feature".
void Video::build_overlay_mask(const Rect &cliprect) {
#ifdef M1_FAST_PATHS
    // The whole screen: only the snapshot; apply_overlay_stencil tests it
    // (the mask is a function of the snapshot's pixel).
    if (cliprect.min_x == 0 && cliprect.max_x == W - 1 && cliprect.min_y == 0 && cliprect.max_y == H - 1) {
        std::memcpy(overlay_hud_snapshot_.data(), screen_.data(), screen_.size() * sizeof(uint32_t));
        return;
    }
#endif
    constexpr int feature_level = 8;
    for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
        for (int x = cliprect.min_x; x <= cliprect.max_x; x++) {
            const size_t i = size_t(y) * W + size_t(x);
            const uint32_t argb = screen_[i];
            const int r = (argb >> 16) & 0xff, g = (argb >> 8) & 0xff, b = argb & 0xff;
            overlay_block_[i] = (r > feature_level || g > feature_level || b > feature_level);
            overlay_hud_snapshot_[i] = argb;
        }
}

void Video::apply_overlay_stencil(const Rect &cliprect) {
#ifdef M1_FAST_PATHS
    if (cliprect.min_x == 0 && cliprect.max_x == W - 1 && cliprect.min_y == 0 && cliprect.max_y == H - 1) {
        // the snapshot's pixel has a channel above feature_level (8)
        const uint32_t *snap = overlay_hud_snapshot_.data();
        uint32_t *scr = screen_.data();
        for (size_t i = 0, n = screen_.size(); i < n; i++) {
            const uint32_t v = snap[i];
            const uint32_t r = (v >> 16) & 0xff, g = (v >> 8) & 0xff, b = v & 0xff;
            if (r > 8 || g > 8 || b > 8) scr[i] = v;
        }
        return;
    }
#endif
    for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
        for (int x = cliprect.min_x; x <= cliprect.max_x; x++) {
            const size_t i = size_t(y) * W + size_t(x);
            if (overlay_block_[i]) screen_[i] = overlay_hud_snapshot_[i];
        }
}

void Video::screen_update(uint64_t frame) {
    frame_ = frame;
    view_t *view = view_.get();
    const Rect cliprect{0, W - 1, 0, H - 1};

    view->ayyc = std::cos(view->ayy);
    view->ayys = std::sin(view->ayy);

    for (uint32_t i = 0; i < 4096; i++) pens_[i] = pen(i);
    build_layers();
    std::fill(screen_.begin(), screen_.end(), pen(0));

    // tilemap B as opaque
    tiles_draw(6, DRAW_OPAQUE);
    tiles_draw(4, DRAW_OPAQUE);
    tiles_draw(2, 0);
    tiles_draw(0, 0);
    if (hashes_) hash_bg = hash(screen_);

    if (keep_stereo_) {
        stereo_quads_.clear();
        stereo_z_.clear();
        stereo_projections_.clear();
        stereo_batch_ = 0;
        stereo_object_ = 0;
        stereo_back_ = screen_;
    }
    pass_ = RENDER_BELOW_HUD;
    tgp_render(cliprect, RENDER_BELOW_HUD);
    if (hashes_) hash_3d = hash(screen_);
    if (keep_stereo_) stereo_mid_ = screen_;

    // HUD tilemaps (cat1)
    tiles_draw(7, 0);
    tiles_draw(5, 0);
    tiles_draw(3, 0);
    tiles_draw(1, 0);

    build_overlay_mask(cliprect);
    pass_ = RENDER_ABOVE_HUD;
    tgp_render(cliprect, RENDER_ABOVE_HUD);
    apply_overlay_stencil(cliprect);
    if (hashes_) hash_scr = hash(screen_);
}

void Video::vblank(uint64_t frame) {
    tgp_scan();
    end_frame(frame);
}

// segaic24 tile_info_0s/0w/1s/1w, every tile (MAME's tilemap caches).
void Video::build_tile(const uint8_t *chars, int layer, uint32_t t, uint16_t val) {
    uint16_t *pm = pixmap_[layer].data();
    uint8_t *fm = flags_[layer].data();
    const uint32_t code = val & 0x3fff;
    const uint32_t color = (val >> 7) & 0xff;
    const uint8_t category = (val & 0x8000) ? 1 : 0;
    const uint32_t tx = (t & 63) * 8, ty = (t >> 6) * 8;
    for (uint32_t y = 0; y < 8; y++)
        for (uint32_t x = 0; x < 8; x++) {
            const uint32_t b = code * 32 + y * 4 + (x >> 1);
            const uint8_t byte = chars[b ^ 1];
            const uint8_t pix = (x & 1) ? (byte & 0x0f) : (byte >> 4);
            const size_t i = size_t(ty + y) * 512 + (tx + x);
            pm[i] = uint16_t(color * 16 + pix);
            fm[i] = uint8_t(category | (pix ? PIXEL_LAYER0 : 0));
        }
}

void Video::build_layers() {
    const uint8_t *chars = mem_.ram(0x780000);
#ifdef M1_FAST_PATHS
    // A tile's pixels depend only on its word and its character's 32 bytes
    // (b ^ 1 stays inside them): any tile with both unchanged is as drawn.
    constexpr uint32_t kCodes = 0x4000, kCharBytes = kCodes * 32;
    if (!layers_valid_) {
        layer_tiles_.assign(4 * 64 * 64, 0);
        chars_seen_.assign(chars, chars + kCharBytes);
        char_changed_.assign(kCodes, 1);
    } else {
        for (uint32_t c = 0; c < kCodes; c++) {
            const size_t o = size_t(c) * 32;
            const bool changed = std::memcmp(chars + o, &chars_seen_[o], 32) != 0;
            char_changed_[c] = changed;
            if (changed) std::memcpy(&chars_seen_[o], chars + o, 32);
        }
    }
    for (int layer = 0; layer < 4; layer++) {
        const uint32_t base = uint32_t(layer) * 0x1000;
        for (uint32_t t = 0; t < 64 * 64; t++) {
            const uint16_t val = tile(t | base);
            uint16_t &seen = layer_tiles_[base + t];
            if (layers_valid_ && seen == val && !char_changed_[val & 0x3fff]) continue;
            seen = val;
            build_tile(chars, layer, t, val);
        }
    }
    layers_valid_ = true;
#else
    for (int layer = 0; layer < 4; layer++) {
        const uint32_t base = uint32_t(layer) * 0x1000;
        for (uint32_t t = 0; t < 64 * 64; t++) build_tile(chars, layer, t, tile(t | base));
    }
#endif
}

void Video::draw_rect(const uint16_t *mask, uint16_t tpri, int flags, int win, int L, int sx, int sy, int xx1, int yy1,
                      int xx2, int yy2) {
    const uint16_t *source = &pixmap_[L][size_t(sy) * 512 + size_t(sx)];
    const uint8_t *trans = &flags_[L][size_t(sy) * 512 + size_t(sx)];
    uint32_t *dest = &screen_[size_t(yy1) * W + size_t(xx1)];
    tpri |= PIXEL_LAYER0;
    mask += yy1 * 4;
    yy2 -= yy1;
    while (xx1 >= 128) {
        xx1 -= 128;
        xx2 -= 128;
        mask++;
    }
    for (int y = 0; y < yy2; y++) {
        const uint16_t *src = source;
        const uint8_t *srct = trans;
        uint32_t *dst = dest;
        const uint16_t *mask1 = mask;
        int llx = xx2;
        int cur_x = xx1;
        while (llx > 0) {
            uint16_t m = *mask1++;
            if (win) m = uint16_t(~m);
            if (!cur_x && llx >= 128) {
                if (!m) {
                    for (int x = 0; x < 128; x++) {
                        if (*srct++ == tpri || (flags & DRAW_OPAQUE)) *dst = pens_[*src];
                        src++;
                        dst++;
                    }
                } else if (m == 0xffff) {
                    src += 128;
                    srct += 128;
                    dst += 128;
                } else {
                    for (int x = 0; x < 128; x += 8) {
                        if (!(m & 0x8000))
                            for (int xx = 0; xx < 8; xx++)
                                if (srct[xx] == tpri || (flags & DRAW_OPAQUE)) dst[xx] = pens_[src[xx]];
                        src += 8;
                        srct += 8;
                        dst += 8;
                        m = uint16_t(m << 1);
                    }
                }
            } else {
                const int llx1 = llx >= 128 ? 128 : llx;
                if (!m) {
                    for (int x = cur_x; x < llx1; x++) {
                        if (*srct++ == tpri || (flags & DRAW_OPAQUE)) *dst = pens_[*src];
                        src++;
                        dst++;
                    }
                } else if (m == 0xffff) {
                    src += 128 - cur_x;
                    srct += 128 - cur_x;
                    dst += 128 - cur_x;
                } else {
                    for (int x = cur_x; x < llx1; x++) {
                        if ((*srct++ == tpri || (flags & DRAW_OPAQUE)) && !(m & (0x8000 >> (x >> 3)))) *dst = pens_[*src];
                        src++;
                        dst++;
                    }
                }
            }
            llx -= 128;
            cur_x = 0;
        }
        source += 512;
        trans += 512;
        dest += W;
        mask += 4;
    }
}

// tilemap_t::draw with one scroll value
void Video::tilemap_draw(int L, int sx, int sy, int minx, int maxx, int miny, int maxy, int flags) {
    const uint8_t cat = uint8_t(flags & CATEGORY_MASK);
    const uint8_t mask = (flags & DRAW_OPAQUE) ? CATEGORY_MASK : uint8_t(CATEGORY_MASK | PIXEL_LAYER0);
    const uint8_t value = (flags & DRAW_OPAQUE) ? cat : uint8_t(cat | PIXEL_LAYER0);
    for (int y = std::max(miny, 0); y <= std::min(maxy, H - 1); y++)
        for (int x = std::max(minx, 0); x <= std::min(maxx, W - 1); x++) {
            const size_t i = size_t((y + sy) & 511) * 512 + size_t((x + sx) & 511);
            if ((flags_[L][i] & mask) == value) screen_[size_t(y) * W + size_t(x)] = pens_[pixmap_[L][i]];
        }
}

// segaic24 draw_common, cliprect = the whole screen
void Video::tiles_draw(int layer, int flags) {
    uint16_t hscr = tile(0x5000 + uint32_t(layer >> 1));
    uint16_t vscr = tile(0x5004 + uint32_t(layer >> 1));
    const uint16_t ctrl = tile(0x5004 + uint32_t((layer >> 1) & 2));
    uint16_t mask[0x800];
    for (uint32_t i = 0; i < 0x800; i++) mask[i] = tile((layer & 4 ? 0x6800 : 0x6000) + i);
    const uint16_t tpri = uint16_t(layer & 1);
    layer >>= 1;
    const int fl = tpri | flags;

    if (vscr & 0x8000) return; // layer disable

    if (ctrl & 0x6000) { // special window/scroll modes
        if (layer & 1) return;
        const int sy = vscr & 0x1ff;
        if (hscr & 0x8000) {
            const uint32_t hscrtb = 0x4000 + 0x200 * uint32_t(layer);
            switch ((ctrl & 0x6000) >> 13) {
            case 1: {
                const uint16_t v = uint16_t((-vscr) & 0x1ff);
                if (!((-vscr) & 0x200)) layer ^= 1;
                for (int y = 0; y < H; y++) {
                    const int l1 = y >= v ? layer ^ 1 : layer;
                    const uint16_t h = tile(hscrtb + uint32_t(y)) & 0x1ff;
                    tilemap_draw(l1, -h, sy, 0, W - 1, y, y, fl);
                }
                break;
            }
            case 2:
            case 3:
                for (int y = 0; y < H; y++) {
                    hscr = tile(hscrtb + uint32_t(y));
                    const int h = hscr & 0x1ff;
                    int l1 = layer;
                    if (!(hscr & 0x200)) l1 ^= 1;
                    tilemap_draw(l1, -h, sy, 0, std::min(W - 1, h - 1), y, y, fl);
                    tilemap_draw(l1 ^ 1, -h, sy, std::max(0, h), W - 1, y, y, fl);
                }
                break;
            }
        } else {
            const int sx = -(hscr & 0x1ff);
            switch ((ctrl & 0x6000) >> 13) {
            case 1: {
                const int v = (-vscr) & 0x1ff;
                if (!((-vscr) & 0x200)) layer ^= 1;
                tilemap_draw(layer, sx, sy, 0, W - 1, 0, std::min(H - 1, v - 1), fl);
                tilemap_draw(layer ^ 1, sx, sy, 0, W - 1, std::max(0, v), H - 1, fl);
                break;
            }
            case 2:
            case 3: {
                const int h = hscr & 0x1ff;
                if (!(hscr & 0x200)) layer ^= 1;
                tilemap_draw(layer, sx, sy, 0, std::min(W - 1, h - 1), 0, H - 1, fl);
                tilemap_draw(layer ^ 1, sx, sy, std::max(0, h), W - 1, 0, H - 1, fl);
                break;
            }
            }
        }
        return;
    }

    const int win = layer & 1;
    if (hscr & 0x8000) {
        const uint32_t hscrtb = 0x4000 + 0x200 * uint32_t(layer);
        vscr &= 0x1ff;
        for (int y = 0; y < H; y++) {
            hscr = uint16_t((-tile(hscrtb + uint32_t(y))) & 0x1ff);
            if (hscr + W <= 512) {
                draw_rect(mask, tpri, flags, win, layer, hscr, vscr, 0, y, W, y + 1);
            } else {
                draw_rect(mask, tpri, flags, win, layer, hscr, vscr, 0, y, 512 - hscr, y + 1);
                draw_rect(mask, tpri, flags, win, layer, 0, vscr, 512 - hscr, y, W, y + 1);
            }
            vscr = (vscr + 1) & 0x1ff;
        }
    } else {
        hscr = uint16_t((-hscr) & 0x1ff);
        vscr = uint16_t((+vscr) & 0x1ff);
        if (hscr + W <= 512) {
            if (vscr + H <= 512) {
                draw_rect(mask, tpri, flags, win, layer, hscr, vscr, 0, 0, W, H);
            } else {
                draw_rect(mask, tpri, flags, win, layer, hscr, vscr, 0, 0, W, 512 - vscr);
                draw_rect(mask, tpri, flags, win, layer, hscr, 0, 0, 512 - vscr, W, H);
            }
        } else {
            if (vscr + H <= 512) {
                draw_rect(mask, tpri, flags, win, layer, hscr, vscr, 0, 0, 512 - hscr, H);
                draw_rect(mask, tpri, flags, win, layer, 0, vscr, 512 - hscr, 0, W, H);
            } else {
                draw_rect(mask, tpri, flags, win, layer, hscr, vscr, 0, 0, 512 - hscr, 512 - vscr);
                draw_rect(mask, tpri, flags, win, layer, 0, vscr, 512 - hscr, 0, W, 512 - vscr);
                draw_rect(mask, tpri, flags, win, layer, hscr, 0, 0, 512 - vscr, 512 - hscr, H);
                draw_rect(mask, tpri, flags, win, layer, 0, 0, 512 - hscr, 512 - vscr, W, H);
            }
        }
    }
}

} // namespace m1
