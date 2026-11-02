// Model 1 stereo (enhancement, off unless a front end asks): the last
// screen's 3D drawn again for two eyes, for VR headsets and 3D displays.
// The Model 2 counterpart is src/runtime/stereo.h; the modes are the same.
//
// Video keeps what the below-HUD 3D pass drew (set_keep_stereo): the object
// quads in camera space before the frustum clip, the direct (screen space)
// quads, and the screen after the background tilemaps and after the 3D.
// Each eye is those quads moved into the eye's view and filled flat here;
// nothing in the game or in the normal screen changes.
//   Pair: two eyes beside the game's camera with zero parallax at a
//         convergence distance; the background tilemaps and the direct
//         quads (skies, ground fills) at infinity, the HUD at screen depth.
//   Head: any view and field of view per eye; the background reprojected at
//         infinity, the HUD left out (hud()) for the front end to place.
// Objects fixed to the camera (a logo, a panel: a quad within 30 cm, all of
// it in front and within 1 m) go with the HUD in both modes.
//         Only the full-screen 3D viewport is drawn (not mirrors or insets).
#pragma once

#include "model1/video.h"
#include "runtime/stereo_view.h"

#include <cstdint>
#include <vector>

namespace m1 {

class Stereo {
public:
    static constexpr int kMaxWidth = 1024, kMaxHeight = 512;

    // Both eyes from the screen `video` last composed. False when that screen
    // has no 3D scene (a 2D screen): show the flat screen instead.
    bool render(const Video &video, const rt::StereoView &view);

    // 0xAARRGGBB, rows packed, alpha 0xff.
    const std::vector<uint32_t> &eye(int i) const { return eye_[i & 1]; }
    int eye_width(int i) const { return w_[i & 1]; }
    int eye_height(int i) const { return h_[i & 1]; }
    // The HUD (what the HUD tilemaps and above-HUD 3D drew, and 3D fixed to
    // the camera), Video::W x Video::H, alpha 0 where see-through.
    const std::vector<uint32_t> &hud() const { return hud_; }

    // The full-screen viewport's projection, if the screen has a 3D scene
    // (at least kMinQuads object quads drawn in it).
    static const Video::StereoProjection *scene(const Video &video);
    static constexpr int kMinQuads = 16;

private:
    struct Point {
        float x, y;
    };
    struct Target { // where fill() draws
        uint32_t *pixels;
        int width, x1, x2, y1, y2; // clip, inclusive
    };
    static void fill(const Target &t, const Point *p, int n, uint32_t color, bool moire);
    static void line(const Target &t, Point a, Point b, uint32_t color);
    static void draw(const Target &t, const Point *p, int n, uint32_t color, bool moire);

    std::vector<uint32_t> eye_[2], hud_, back_, cam_;
    std::vector<uint8_t> camera_object_; // per object: fixed to the camera (drawn with the HUD)

    void find_camera_objects(const Video &video, float scale);
    bool at_camera(const Video::StereoQuad &q) const {
        return !q.direct && q.object < camera_object_.size() && camera_object_[q.object];
    }
    int w_[2] = {0, 0}, h_[2] = {0, 0};

    void render_pair(const Video &video, const rt::StereoView &view, const Video::StereoProjection &camera);
    void render_head(const Video &video, const rt::StereoView &view, const Video::StereoProjection &camera);
};

} // namespace m1
