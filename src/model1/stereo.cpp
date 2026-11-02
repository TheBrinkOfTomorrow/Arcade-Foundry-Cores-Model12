#include "model1/stereo.h"

#include <algorithm>
#include <cmath>

namespace m1 {

namespace {

constexpr int W = Video::W, H = Video::H;
constexpr uint32_t kOpaque = 0xff000000u, kRgb = 0x00ffffffu;
constexpr float kNear = 0.02f; // the eye's near plane, metres
// An object fixed to the game's camera (Virtua Racing's spinning SEGA logo a
// few centimetres away, Wing War's tutorial panels) is drawn with the HUD at
// screen depth, not in the scene: one with a quad nearer than kHudDepth
// everywhere, all of it in front of the camera and within kHudReach.
constexpr float kHudDepth = 0.3f, kHudReach = 1.0f; // metres

struct V {
    float x, y, z;
};

// A quad's distinct corners (MAME draws one with two as a line).
int distinct(const V *in, V *out) {
    int n = 0;
    for (int i = 0; i < 4; i++) {
        bool seen = false;
        for (int k = 0; k < n; k++) seen = seen || (in[i].x == out[k].x && in[i].y == out[k].y && in[i].z == out[k].z);
        if (!seen) out[n++] = in[i];
    }
    return n;
}

// The polygon's part in front of z = near.
int clip_near(const V *in, int n, float near, V *out) {
    int m = 0;
    for (int k = 0; k < n; k++) {
        const V &a = in[k], &b = in[(k + 1) % n];
        const bool ain = a.z >= near, bin = b.z >= near;
        if (ain) out[m++] = a;
        if (ain != bin) {
            const float t = (near - a.z) / (b.z - a.z);
            out[m++] = {a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), near};
        }
    }
    return m;
}

} // namespace

const Video::StereoProjection *Stereo::scene(const Video &video) {
    if (video.stereo_back().size() != size_t(W) * H || video.stereo_mid().size() != size_t(W) * H) return nullptr;
    const auto &proj = video.stereo_projections();
    std::vector<int> count(proj.size(), 0);
    for (const Video::StereoQuad &q : video.stereo_quads())
        if (!q.direct && proj[q.projection].full_screen() && ++count[q.projection] >= kMinQuads) return &proj[q.projection];
    return nullptr;
}

bool Stereo::render(const Video &video, const rt::StereoView &view) {
    const Video::StereoProjection *camera = scene(video);
    if (!camera || !(view.scale > 0)) return false;
    const std::vector<uint32_t> &scr = video.screen(), &mid = video.stereo_mid();
    hud_.assign(size_t(W) * H, 0u);
    for (size_t i = 0; i < hud_.size(); ++i)
        if ((scr[i] ^ mid[i]) & kRgb) hud_[i] = (scr[i] & kRgb) | kOpaque;
    // Quads fixed to the camera join the HUD, behind what the HUD drew.
    const auto &proj = video.stereo_projections();
    find_camera_objects(video, view.scale);
    cam_.assign(size_t(W) * H, 0u);
    for (const Video::StereoQuad &q : video.stereo_quads()) {
        if (!at_camera(q)) continue;
        const Video::StereoProjection &p = proj[q.projection];
        V in[4], c[4], v[8];
        for (int k = 0; k < 4; ++k) in[k] = {q.x[k], q.y[k], q.z[k]};
        int n = distinct(in, c);
        const float near = 1e-6f;
        n = n == 2 ? (c[0].z >= near && c[1].z >= near ? 2 : 0) : clip_near(c, n, near, v);
        const V *src = n == 2 ? c : v;
        Point s[8];
        for (int k = 0; k < n; ++k)
            s[k] = {p.xc + (src[k].x / src[k].z * p.zoomx + p.viewx), p.yc - (src[k].y / src[k].z * p.zoomy + p.viewy)};
        draw(Target{cam_.data(), W, p.x1, p.x2, p.y1, p.y2}, s, n, q.color | kOpaque, q.moire);
    }
    for (size_t i = 0; i < hud_.size(); ++i)
        if (!hud_[i]) hud_[i] = cam_[i];
    if (view.mode == rt::StereoView::Mode::Pair) render_pair(video, view, *camera);
    else render_head(video, view, *camera);
    return true;
}

void Stereo::find_camera_objects(const Video &video, float scale) {
    struct Reach {
        bool near = false, ok = true;
    };
    std::vector<Reach> obj;
    for (const Video::StereoQuad &q : video.stereo_quads()) {
        if (q.direct) continue;
        if (q.object >= obj.size()) obj.resize(q.object + 1);
        Reach &r = obj[q.object];
        bool all_near = true;
        for (int k = 0; k < 4; k++) {
            const float z = q.z[k] * scale;
            all_near = all_near && z < kHudDepth;
            r.ok = r.ok && z > 0 && z < kHudReach;
        }
        r.near = r.near || all_near;
    }
    camera_object_.assign(obj.size(), 0);
    for (size_t i = 0; i < obj.size(); i++) camera_object_[i] = obj[i].near && obj[i].ok;
}

void Stereo::fill(const Target &t, const Point *p, int n, uint32_t color, bool moire) {
    float miny = p[0].y, maxy = p[0].y;
    for (int k = 1; k < n; ++k) miny = std::min(miny, p[k].y), maxy = std::max(maxy, p[k].y);
    const int ya = std::max(t.y1, int(std::ceil(miny - 0.5f))), yb = std::min(t.y2, int(std::floor(maxy - 0.5f)));
    for (int y = ya; y <= yb; ++y) {
        const float yc = float(y) + 0.5f;
        float xl = 1e30f, xr = -1e30f;
        for (int k = 0; k < n; ++k) {
            const Point &a = p[k], &b = p[(k + 1) % n];
            if ((a.y <= yc && yc < b.y) || (b.y <= yc && yc < a.y)) {
                const float x = a.x + (yc - a.y) * (b.x - a.x) / (b.y - a.y);
                xl = std::min(xl, x), xr = std::max(xr, x);
            }
        }
        if (xl > xr) continue;
        const int xa = std::max(t.x1, int(std::ceil(xl - 0.5f))), xb = std::min(t.x2, int(std::ceil(xr - 0.5f)) - 1);
        uint32_t *row = t.pixels + size_t(y) * size_t(t.width);
        for (int x = xa; x <= xb; ++x)
            if (!moire || !((x ^ y) & 1)) row[x] = color;
    }
}

void Stereo::line(const Target &t, Point a, Point b, uint32_t color) {
    int x0 = int(std::lround(a.x)), y0 = int(std::lround(a.y));
    const int x1 = int(std::lround(b.x)), y1 = int(std::lround(b.y));
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int steps = 0; steps < 8192; ++steps) {
        if (x0 >= t.x1 && x0 <= t.x2 && y0 >= t.y1 && y0 <= t.y2) t.pixels[size_t(y0) * size_t(t.width) + size_t(x0)] = color;
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) err += dy, x0 += sx;
        if (e2 <= dx) err += dx, y0 += sy;
    }
}

void Stereo::draw(const Target &t, const Point *p, int n, uint32_t color, bool moire) {
    if (n == 2) line(t, p[0], p[1], color);
    else if (n >= 3) fill(t, p, n, color, moire);
}

void Stereo::render_pair(const Video &video, const rt::StereoView &view, const Video::StereoProjection &camera) {
    const std::vector<uint32_t> &back = video.stereo_back();
    const auto &proj = video.stereo_projections();
    const float zc = std::max(view.convergence, kNear) / view.scale, near = kNear / view.scale; // game units
    for (int i = 0; i < 2; ++i) {
        // The eye at x = e beside the camera: camera-space x - e, the images
        // moved together so depth zc has no parallax (+ e zoom / zc on the
        // screen, which is where infinity lands).
        const float e = (i ? 0.5f : -0.5f) * view.ipd / view.scale;
        std::vector<uint32_t> &out = eye_[i];
        out.resize(size_t(W) * H);
        w_[i] = W, h_[i] = H;
        const int shift = int(std::lround(e * camera.zoomx / zc));
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) out[size_t(y) * W + size_t(x)] = back[size_t(y) * W + size_t(std::clamp(x - shift, 0, W - 1))];

        for (int pass = 0; pass < 2; ++pass) // the direct quads behind everything, then the objects in order
            for (const Video::StereoQuad &q : video.stereo_quads()) {
                if (q.direct != (pass == 0) || at_camera(q)) continue;
                const Video::StereoProjection &p = proj[q.projection];
                const Target t{out.data(), W, p.x1, p.x2, p.y1, p.y2};
                V in[4], c[4], v[8];
                for (int k = 0; k < 4; ++k) in[k] = {q.x[k], q.y[k], q.z[k]};
                int n = distinct(in, c);
                Point s[8];
                if (q.direct) {
                    for (int k = 0; k < n; ++k) s[k] = {p.xc + c[k].x + float(shift), p.yc - c[k].y};
                } else {
                    for (int k = 0; k < n; ++k) c[k].x -= e;
                    n = n == 2 ? (c[0].z >= near && c[1].z >= near ? 2 : 0) : clip_near(c, n, near, v);
                    const V *src = n == 2 ? c : v;
                    const float off = e * p.zoomx / zc;
                    for (int k = 0; k < n; ++k)
                        s[k] = {p.xc + (src[k].x / src[k].z * p.zoomx + p.viewx) + off, p.yc - (src[k].y / src[k].z * p.zoomy + p.viewy)};
                }
                draw(t, s, n, q.color | kOpaque, q.moire);
            }

        for (size_t k = 0; k < out.size(); ++k) out[k] = (hud_[k] ? hud_[k] : out[k]) | kOpaque;
    }
}

void Stereo::render_head(const Video &video, const rt::StereoView &view, const Video::StereoProjection &camera) {
    const auto &proj = video.stereo_projections();
    // The background at infinity: the back tilemaps and the direct quads.
    back_ = video.stereo_back();
    for (const Video::StereoQuad &q : video.stereo_quads()) {
        if (!q.direct) continue;
        const Video::StereoProjection &p = proj[q.projection];
        V in[4], c[4];
        for (int k = 0; k < 4; ++k) in[k] = {q.x[k], q.y[k], q.z[k]};
        const int n = distinct(in, c);
        Point s[4];
        for (int k = 0; k < n; ++k) s[k] = {p.xc + c[k].x, p.yc - c[k].y};
        draw(Target{back_.data(), W, p.x1, p.x2, p.y1, p.y2}, s, n, q.color, q.moire);
    }

    for (int i = 0; i < 2; ++i) {
        const rt::StereoEye &eye = view.eye[i];
        const float *m = eye.view;
        const int ew = std::clamp(eye.width, 16, kMaxWidth), eh = std::clamp(eye.height, 16, kMaxHeight);
        const float tl = eye.tan_left, tr = eye.tan_right, td = eye.tan_down, tu = eye.tan_up;
        if (!(tr > tl) || !(tu > td)) return;
        const float fx = float(ew) / (tr - tl), fy = float(eh) / (tu - td);
        std::vector<uint32_t> &img = eye_[i];
        img.resize(size_t(ew) * eh);
        w_[i] = ew, h_[i] = eh;

        // Each pixel's direction back in the game camera's space, through
        // the game's own projection into the background.
        for (int y = 0; y < eh; ++y) {
            uint32_t *o = img.data() + size_t(y) * ew;
            const float ey = tu - (float(y) + 0.5f) / fy;
            for (int x = 0; x < ew; ++x) {
                const float ex = (float(x) + 0.5f) / fx + tl;
                const float X = m[0] * ex + m[1] * ey + m[2], Y = m[4] * ex + m[5] * ey + m[6], Z = m[8] * ex + m[9] * ey + m[10];
                int sx, sy;
                if (Z > 1e-4f) {
                    sx = int(std::lround(camera.xc + (X / Z * camera.zoomx + camera.viewx)));
                    sy = int(std::lround(camera.yc - (Y / Z * camera.zoomy + camera.viewy)));
                } else { // behind the game's camera: the nearer edge
                    sx = W / 2;
                    sy = Y > 0 ? 0 : H - 1;
                }
                o[x] = (back_[size_t(std::clamp(sy, 0, H - 1)) * W + size_t(std::clamp(sx, 0, W - 1))] & kRgb) | kOpaque;
            }
        }

        // The scene: camera space in metres, into the eye's space.
        const Target t{img.data(), ew, 0, ew - 1, 0, eh - 1};
        for (const Video::StereoQuad &q : video.stereo_quads()) {
            if (q.direct || !proj[q.projection].full_screen() || at_camera(q)) continue;
            V in[4], c[4], v[8];
            for (int k = 0; k < 4; ++k) {
                const float X = q.x[k] * view.scale, Y = q.y[k] * view.scale, Z = q.z[k] * view.scale;
                in[k] = {m[0] * X + m[4] * Y + m[8] * Z + m[12], m[1] * X + m[5] * Y + m[9] * Z + m[13],
                         m[2] * X + m[6] * Y + m[10] * Z + m[14]};
            }
            int n = distinct(in, c);
            n = n == 2 ? (c[0].z >= kNear && c[1].z >= kNear ? 2 : 0) : clip_near(c, n, kNear, v);
            const V *src = n == 2 ? c : v;
            Point s[8];
            for (int k = 0; k < n; ++k) s[k] = {(src[k].x / src[k].z - tl) * fx, (tu - src[k].y / src[k].z) * fy};
            draw(t, s, n, q.color | kOpaque, q.moire);
        }
    }
}

} // namespace m1
