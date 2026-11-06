// What a front end asks stereo for (afcore_stereo, common/include/afcore.h), for
// every board's stereo renderer: Model 2's rt::Stereo, Model 1's m1::Stereo.
#pragma once

namespace rt {

struct StereoEye {
    // Column-major 4x4 from the game camera's space to this eye's, in metres:
    // x right, y up, z forward (where the camera looks).
    float view[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    // The eye's field of view: tangents of its edges (left and down negative).
    float tan_left = -1, tan_right = 1, tan_down = -1, tan_up = 1;
    int width = 512, height = 512; // at most kMaxWidth x kMaxHeight
};

struct StereoView {
    enum class Mode { Pair, Head } mode = Mode::Pair;
    float scale = 1;           // metres per game unit
    float ipd = 0.063f;        // Pair: the distance between the eyes, metres
    float convergence = 10.f;  // Pair: the distance shown at screen depth, metres
    StereoEye eye[2];          // Head: left, right
};

} // namespace rt
