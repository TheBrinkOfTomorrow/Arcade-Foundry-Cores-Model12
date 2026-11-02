#include "runtime/stereo.h"

#include <algorithm>
#include <cmath>

namespace rt {

namespace {

constexpr int W = Video::W, H = Video::H;
constexpr uint32_t kOpaque = 0xff000000u;
constexpr float kNear = 0.02f; // Head: the eye's near plane, metres

// A viewport over the whole original screen: the 3D scene itself (not an
// inset window, a rear-view mirror or a 2D screen's polygons).
bool full_screen(const GeoPoly &p) { return p.viewport[0] <= 0 && p.viewport[2] >= W - 1; }

const GeoPoly *camera_poly(const Video &video) {
    for (const GeoPoly &p : video.gpu_polys())
        if (p.window <= video.gpu_windows() && full_screen(p) && p.num_vertices >= 3 && p.focus[0] != 0 && p.focus[1] != 0)
            return &p;
    return nullptr;
}

} // namespace

bool Stereo::scene(const Video &video) {
    if (!camera_poly(video) || video.back_layer().size() < size_t(W) * H) return false;
    const Raster &r = video.raster();
    size_t covered = 0;
    for (int y = 0; y < H; ++y) {
        const uint32_t *row = r.pixels() + size_t(y) * size_t(r.stride());
        for (int x = 0; x < W; ++x) covered += row[x] != 0;
    }
    return covered * 6 >= size_t(W) * H;
}

bool Stereo::render(const Video &video, const StereoView &view) {
    if (!scene(video) || !(view.scale > 0)) return false;
    const std::vector<uint32_t> &front = video.front_layer();
    hud_.assign(size_t(W) * H, 0u);
    for (size_t i = 0; i < hud_.size() && i < front.size(); ++i)
        if (front[i]) hud_[i] = front[i] | kOpaque;
    if (view.mode == StereoView::Mode::Pair) render_pair(video, view);
    else render_head(video, view, *camera_poly(video));
    return true;
}

void Stereo::render_pair(const Video &video, const StereoView &view) {
    const std::vector<uint32_t> &back = video.back_layer();
    const int bw = int(back.size() / H);
    const float fx = camera_poly(video)->focus[0];
    const float zc = std::max(view.convergence, kNear) / view.scale; // game units
    raster_.set_wide_margin(0);
    for (int i = 0; i < 2; ++i) {
        // The eye at x = e beside the camera: camera-space x - e, and the
        // images moved together so depth zc has no parallax. In the focused
        // units the rasterizer projects (x / z): x - e f + e f z / zc.
        const float e = (i ? 0.5f : -0.5f) * view.ipd / view.scale;
        polys_.assign(video.gpu_polys().begin(), video.gpu_polys().end());
        for (GeoPoly &p : polys_) {
            const float ef = e * p.focus[0];
            for (int k = 0; k < p.num_vertices; ++k) p.v[k].x += ef * (p.v[k].p[0] / zc - 1.0f);
        }
        raster_.render(polys_, video.gpu_windows(), video.gpu_mem(), video.crtc_x(), video.crtc_y(), video.render_x(),
                       video.render_y(), 0, W - 1, 0, H - 1);
        // Back layers at infinity (moved by e f / zc), the 3D scene, the
        // front layers at screen depth.
        const int shift = int(std::lround(e * fx / zc));
        std::vector<uint32_t> &out = eye_[i];
        out.resize(size_t(W) * H);
        w_[i] = W, h_[i] = H;
        for (int y = 0; y < H; ++y) {
            const uint32_t *b = back.data() + size_t(y) * size_t(bw);
            const uint32_t *r = raster_.pixels() + size_t(y) * size_t(raster_.stride());
            const uint32_t *f = hud_.data() + size_t(y) * W;
            uint32_t *o = out.data() + size_t(y) * W;
            for (int x = 0; x < W; ++x) {
                uint32_t c = b[std::clamp(x - shift, 0, bw - 1)];
                if (r[x]) c = r[x];
                if (f[x]) c = f[x];
                o[x] = c | kOpaque;
            }
        }
    }
}

void Stereo::render_head(const Video &video, const StereoView &view, const GeoPoly &camera) {
    const std::vector<uint32_t> &back = video.back_layer();
    const int bw = int(back.size() / H);
    struct V { float x, y, z, u, v; };
    for (int i = 0; i < 2; ++i) {
        const StereoEye &eye = view.eye[i];
        const float *m = eye.view;
        const int ew = std::clamp(eye.width, 16, kMaxWidth), eh = std::clamp(eye.height, 16, kMaxHeight);
        raster_.set_wide_margin(ew > 512 ? (ew - W + 1) / 2 : 0);
        const float tl = eye.tan_left, tr = eye.tan_right, td = eye.tan_down, tu = eye.tan_up;
        if (!(tr > tl) || !(tu > td)) return;
        const float fx = float(ew) / (tr - tl), fy = float(eh) / (tu - td);
        const float cx = -tl * fx, cy = tu * fy;
        const int16_t c0 = int16_t(std::lround(cx)), c1 = int16_t(std::lround(float(H) - cy));
        const float dx = cx - float(c0), dy = cy - float(H - c1); // what the whole-pixel centre leaves over

        polys_.clear();
        for (const GeoPoly &p : video.gpu_polys()) {
            if (p.window > video.gpu_windows() || !full_screen(p) || p.num_vertices < 3 || p.focus[0] == 0 || p.focus[1] == 0)
                continue;
            // Camera space in metres, into the eye's space.
            V in[8], out[9];
            for (int k = 0; k < p.num_vertices; ++k) {
                const GeoVertex &g = p.v[k];
                const float X = g.x / p.focus[0] * view.scale, Y = g.y / p.focus[1] * view.scale, Z = g.p[0] * view.scale;
                in[k] = {m[0] * X + m[4] * Y + m[8] * Z + m[12], m[1] * X + m[5] * Y + m[9] * Z + m[13],
                         m[2] * X + m[6] * Y + m[10] * Z + m[14], g.p[1], g.p[2]};
            }
            // Clip to the near plane (texture coordinates are linear in space).
            int n = 0;
            for (int k = 0; k < p.num_vertices; ++k) {
                const V &a = in[k], &b = in[(k + 1) % p.num_vertices];
                const bool ain = a.z >= kNear, bin = b.z >= kNear;
                if (ain) out[n++] = a;
                if (ain != bin) {
                    const float t = (kNear - a.z) / (b.z - a.z);
                    out[n++] = {a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), kNear, a.u + t * (b.u - a.u), a.v + t * (b.v - a.v)};
                }
            }
            if (n < 3) continue;
            // Into the eye's image, in the rasterizer's terms: x and y times
            // the focal scale, the centre as the principal point.
            auto emit = [&](const int *idx, int count) {
                GeoPoly q = p;
                q.center[0] = c0, q.center[1] = c1;
                q.viewport[0] = 0, q.viewport[2] = int16_t(ew - 1);
                q.viewport[3] = H, q.viewport[1] = int16_t(H - (eh - 1));
                q.num_vertices = uint8_t(count);
                for (int k = 0; k < count; ++k) {
                    const V &s = out[idx[k]];
                    q.v[k].x = fx * s.x + dx * s.z;
                    q.v[k].y = fy * s.y - dy * s.z;
                    q.v[k].p[0] = s.z;
                    q.v[k].p[1] = s.u;
                    q.v[k].p[2] = s.v;
                }
                polys_.push_back(q);
            };
            static constexpr int first[8] = {0, 1, 2, 3, 4, 5, 6, 7}, rest[3] = {0, 7, 8};
            emit(first, std::min(n, 8));
            if (n == 9) emit(rest, 3); // a cut octagon: the last corner as its own triangle
        }
        raster_.render(polys_, video.gpu_windows(), video.gpu_mem(), 0, 0, 0, 0, 0, ew - 1, 0, eh - 1);

        // The back layers at infinity: each pixel's direction back in the
        // game camera's space, through the game's own projection.
        const float gx = camera.focus[0], gy = camera.focus[1];
        const float ox = float(video.crtc_x() + camera.center[0]), oy = float((H - camera.center[1]) + video.crtc_y());
        std::vector<uint32_t> &img = eye_[i];
        img.resize(size_t(ew) * eh);
        w_[i] = ew, h_[i] = eh;
        for (int y = 0; y < eh; ++y) {
            const uint32_t *r = raster_.pixels() + size_t(y) * size_t(raster_.stride());
            uint32_t *o = img.data() + size_t(y) * ew;
            const float ey = tu - (float(y) + 0.5f) / fy;
            for (int x = 0; x < ew; ++x) {
                if (r[x]) {
                    o[x] = r[x] | kOpaque;
                    continue;
                }
                const float ex = (float(x) + 0.5f) / fx + tl;
                const float X = m[0] * ex + m[1] * ey + m[2], Y = m[4] * ex + m[5] * ey + m[6], Z = m[8] * ex + m[9] * ey + m[10];
                int sx, sy;
                if (Z > 1e-4f) {
                    sx = int(std::lround(ox + gx * X / Z));
                    sy = int(std::lround(oy - gy * Y / Z));
                } else { // behind the game's camera: the nearer edge
                    sx = W / 2;
                    sy = Y > 0 ? 0 : H - 1;
                }
                o[x] = back[size_t(std::clamp(sy, 0, H - 1)) * size_t(bw) + size_t(std::clamp(sx, 0, bw - 1))] | kOpaque;
            }
        }
    }
}

} // namespace rt
