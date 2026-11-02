// Stereo (enhancement, off unless a front end asks): the last screen's 3D
// scene drawn again for two eyes, for VR headsets and 3D displays.
//
// The geometrizer's polygons are still in the game camera's space when the
// rasterizer gets them (x and y times the focal scale, z the depth), so each
// eye is the same display list moved into that eye's view and drawn by the
// reference rasterizer. Nothing in the game or in the normal screen changes:
// this reads what Video kept of the frame (set_keep_layers) and draws into
// its own buffers.
//
// Two modes:
//   Pair: two eyes either side of the game's own camera, looking where it
//         looks, with zero parallax at a convergence distance: the original
//         screen as a window into the scene. Each eye is 496 x 384 with the
//         2D layers in it (the back layers at infinity, the HUD at screen
//         depth). For 3D monitors and a screen in a headset.
//   Head: any view and field of view per eye (a headset's poses), the scene
//         re-projected into it, the back layers reprojected at infinity; the
//         HUD is left out (hud(): the front layers alone) for the front end to
//         place. Only full-screen 3D windows are drawn.
#pragma once

#include "runtime/raster.h"
#include "runtime/stereo_view.h"
#include "runtime/video.h"

#include <cstdint>
#include <vector>

namespace rt {

class Stereo {
public:
    static constexpr int kMaxWidth = 1024, kMaxHeight = 512;

    // Draws both eyes from the screen `video` last composed (with
    // set_keep_layers on). False when that screen has no 3D scene to draw so
    // (a 2D screen: titles, menus, test mode): show the flat screen instead.
    bool render(const Video &video, const StereoView &view);

    // 0xAARRGGBB, rows packed, alpha 0xff.
    const std::vector<uint32_t> &eye(int i) const { return eye_[i & 1]; }
    int eye_width(int i) const { return w_[i & 1]; }
    int eye_height(int i) const { return h_[i & 1]; }
    // The front 2D layers of that screen (HUD, text), Video::W x Video::H,
    // alpha 0 where see-through.
    const std::vector<uint32_t> &hud() const { return hud_; }

    // Whether the screen has a 3D scene (a full-screen 3D window drawn over
    // at least a sixth of it).
    static bool scene(const Video &video);

private:
    Raster raster_;
    std::vector<GeoPoly> polys_;
    std::vector<uint32_t> eye_[2], hud_;
    int w_[2] = {0, 0}, h_[2] = {0, 0};

    void render_pair(const Video &video, const StereoView &view);
    void render_head(const Video &video, const StereoView &view, const GeoPoly &camera);
};

} // namespace rt
