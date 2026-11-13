// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
//
// Model 1 video: the flat-shaded 3D renderer that walks the TGP display
// lists, the System 24 tilemaps and the screen composition. Transplanted
// from MAME's src/mame/sega/model1_v.cpp (and model1.h's types) at
// dddd73680656e355bb2b5beecab1167c9f07bf81 (BSD-3-Clause; notice above kept
// as the licence requires); the tilemap drawing is segaic24's draw_common
// as src/runtime/video.cpp has it for Model 2. See THIRD_PARTY.md.
//
// Changes from MAME: memory is the board's RAM (m1::Memory) read where MAME
// reads its shares; pens are computed from palette RAM when drawing
// (model1_paletteram_w's rule); glm's vec3, dot and normalize are spelled
// out with glm's operation order; float-to-int conversions that can see a
// NaN or overflow take x86-64's result (MAME's reference build), and the
// integer arithmetic MAME lets wrap is done unsigned, so every host agrees.
#pragma once

#include "model1/memory.h"
#include "model1/replay_bus.h"
#include "runtime/state_io.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace m1 {

class Video : public DeviceModel {
public:
    static constexpr int W = 496, H = 384;

    // polygons: the "polygons" region (TGP model ROMs, 32-bit LE floats)
    Video(const Memory &mem, const std::vector<uint8_t> &polygons);

    // 680000-680003, the display list control (model1_listctl_r/w)
    uint16_t listctl_r(uint32_t offset) const { return offset ? listctl_[1] : uint16_t(listctl_[0] | 0x30); }
    void listctl_w(uint32_t offset, uint16_t data, uint16_t mem_mask) {
        listctl_[offset & 1] = uint16_t((listctl_[offset & 1] & ~mem_mask) | (data & mem_mask));
    }

    // DeviceModel: the V60's 16-bit accesses to 680000-680003 (a 32-bit one
    // is two, low half first)
    uint32_t read(uint32_t a, unsigned size) override {
        if (size == 4) return listctl_r(0) | uint32_t(listctl_r(1)) << 16;
        const uint16_t w = listctl_r((a >> 1) & 1);
        return size == 1 ? (a & 1 ? w >> 8 : w & 0xff) : w;
    }
    void write(uint32_t a, uint32_t v, unsigned size) override {
        if (size == 4) {
            listctl_w(0, uint16_t(v), 0xffff);
            listctl_w(1, uint16_t(v >> 16), 0xffff);
        } else if (size == 2) {
            listctl_w((a >> 1) & 1, uint16_t(v), 0xffff);
        } else {
            listctl_w((a >> 1) & 1, uint16_t(v << (a & 1 ? 8 : 0)), a & 1 ? 0xff00 : 0x00ff);
        }
    }

    // screen_update_model1, for frame `frame` (the screen's frame number)
    void screen_update(uint64_t frame);
    // screen_vblank_model1's rising edge
    void vblank(uint64_t frame);

    const std::vector<uint32_t> &screen() const { return screen_; }
    // The hashes MAME's frame log has (patch 0006): after the background
    // tilemaps, after the 3D below the HUD, composed.
    uint64_t hash_bg = 0, hash_3d = 0, hash_scr = 0;
    static uint64_t hash(const std::vector<uint32_t> &b);
    // The three hashes above cost three passes over the screen a frame; the
    // core library, which never reads them, turns them off.
    void set_hashes(bool on) { hashes_ = on; }
    // Save states: the 3D's view, its RAMs and list control, and the screen
    // as shown; the tile layers are built again from the board's RAM.
    void state(rt::StateIO &s);

    struct vec3 {
        float x = 0, y = 0, z = 0;
    };
    struct spoint_t {
        int32_t x = 0, y = 0;
    };
    struct point_t {
        float x = 0, y = 0, z = 0;
        float xx = 0, yy = 0;
        spoint_t s;
    };
    struct quad_t {
        quad_t() = default;
        quad_t(int ccol, float cz, point_t *p0, point_t *p1, point_t *p2, point_t *p3) : z(cz), col(ccol) {
            p[0] = p0;
            p[1] = p1;
            p[2] = p2;
            p[3] = p3;
        }
        int compare(const quad_t *other) const;
        point_t *p[4] = {nullptr, nullptr, nullptr, nullptr};
        float z = 0;
        int col = 0;
    };

    // Stereo (enhancement, off unless set_keep_stereo): what the frame's
    // below-HUD 3D pass drew, kept for m1::Stereo to draw again per eye.
    // Object quads are kept before the frustum clip, in the camera space
    // transform_point gives (x right, y up, z forward, game units); direct
    // quads are already screen space (x, y from the viewport centre).
    struct StereoProjection { // view_t's projection when the quads were made
        float xc = 0, yc = 0, zoomx = 0, zoomy = 0, viewx = 0, viewy = 0;
        int x1 = 0, x2 = 0, y1 = 0, y2 = 0; // the viewport (clip), inclusive
        bool operator==(const StereoProjection &) const = default;
        bool full_screen() const { return x1 <= 0 && x2 >= W - 1; }
    };
    struct StereoQuad {
        float x[4], y[4], z[4];
        uint32_t color;     // 0xRRGGBB
        bool moire, direct;
        uint16_t projection; // index into stereo_projections()
        uint32_t object;     // which push_object call made it (direct quads: 0)
    };
    void set_keep_stereo(bool on) {
        keep_stereo_ = on;
        if (!on) stereo_quads_.clear(), stereo_z_.clear(), stereo_projections_.clear(), stereo_back_.clear(), stereo_mid_.clear();
    }
    // In drawing order (each display list batch sorted as MAME sorts it).
    const std::vector<StereoQuad> &stereo_quads() const { return stereo_quads_; }
    const std::vector<StereoProjection> &stereo_projections() const { return stereo_projections_; }
    // The screen after the background tilemaps, and after the below-HUD 3D
    // (what the HUD tilemaps and above-HUD 3D then change is the HUD).
    const std::vector<uint32_t> &stereo_back() const { return stereo_back_; }
    const std::vector<uint32_t> &stereo_mid() const { return stereo_mid_; }

    struct lightparam_t {
        float a = 0, d = 0, s = 0;
        int p = 0;
    };
    struct view_t {
        void init_translation_matrix();
        void set_viewport(float xcenter, float ycenter, float xl, float xr, float yb, float yt);
        void set_lightparam(int index, float diffuse, float ambient, float specular, int power);
        void set_zoom(float x, float y);
        void set_light_direction(float x, float y, float z);
        void set_translation_matrix(const float *mat);
        void set_view_translation(float x, float y);
        void project_point(point_t *p) const;
        void project_point_direct(point_t *p) const;
        void transform_vector(vec3 &p) const;
        void transform_point(point_t *p) const;
        void recompute_frustum();
        int xc = 0, yc = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        float zoomx = 0, zoomy = 0, viewx = 0, viewy = 0;
        float a_bottom = 0, a_top = 0, a_left = 0, a_right = 0;
        float vxx = 0, vyy = 0, vzz = 0, ayy = 0, ayyc = 0, ayys = 0;
        float translation[12] = {};
        vec3 light;
        lightparam_t lightparams[256];
        bool spec_enable = false;
    };

private:
    enum render_pass { RENDER_BELOW_HUD, RENDER_ABOVE_HUD };
    struct Rect {
        int min_x, max_x, min_y, max_y;
    };

    uint16_t ram16(uint32_t addr) const {
        const uint8_t *p = mem_.ram(addr);
        return uint16_t(p[0] | p[1] << 8);
    }
    uint16_t display_list(uint32_t i) const { return ram16(list_base_ + ((i & 0x7fff) << 1)); }
    uint16_t palette_word(uint32_t i) const { return ram16(0x900000 + ((i & 0x1fff) << 1)); }
    uint16_t color_xlat(uint32_t i) const { return ram16(0x910000 + (i << 1)); }
    uint32_t pen(uint32_t i) const;

    uint32_t readi(int adr) const;
    int16_t readi16(int adr) const;
    float readf(int adr) const;
    float view_determinant(const point_t *p1, const point_t *p2, const point_t *p3) const;

    static bool fclip_isc_bottom(view_t *, point_t *);
    static bool fclip_isc_top(view_t *, point_t *);
    static bool fclip_isc_left(view_t *, point_t *);
    static bool fclip_isc_right(view_t *, point_t *);
    static void fclip_clip_bottom(view_t *, point_t *, point_t *, point_t *);
    static void fclip_clip_top(view_t *, point_t *, point_t *, point_t *);
    static void fclip_clip_left(view_t *, point_t *, point_t *, point_t *);
    static void fclip_clip_right(view_t *, point_t *, point_t *, point_t *);
    bool clip_isclipped(int level, view_t *v, point_t *p) const;
    void clip_clip(int level, view_t *v, point_t *pt, point_t *p1, point_t *p2) const;

    void tgp_render(const Rect &cliprect, render_pass pass);
    void tgp_scan();
    void sort_quads();
    void unsort_quads();
    void draw_quads(const Rect &cliprect);
    void draw_hline(int x1, int x2, int y, int color);
    void draw_hline_moired(int x1, int x2, int y, int color);
    void fill_slope(view_t *view, int color, int32_t x1, int32_t x2, int32_t sl1, int32_t sl2, int32_t y1, int32_t y2,
                    int32_t *nx1, int32_t *nx2);
    void fill_line(view_t *view, int color, int32_t y, int32_t x1, int32_t x2);
    void fill_quad(view_t *view, const quad_t &q);
    void draw_wireframe_line(const Rect &clip, int x1, int y1, int x2, int y2, uint32_t color, bool moire);
    void fclip_push_quad_next(int level, quad_t &q, point_t *p1, point_t *p2, point_t *p3, point_t *p4);
    void fclip_push_quad(int level, quad_t &q);
    static float min4f(float a, float b, float c, float d);
    static float max4f(float a, float b, float c, float d);
    float compute_specular(const vec3 &normal, const vec3 &light, float diffuse, int lmode) const;
    int push_direct(int list_offset);
    int skip_direct(int list_offset) const;
    void push_object(uint32_t tex_adr, uint32_t poly_adr, uint32_t size, float &old_z);
    void draw_objects(const Rect &cliprect);
    void set_current_render_list();
    int get_list_number();
    void end_frame(uint64_t frame);
    void build_overlay_mask(const Rect &cliprect);
    void apply_overlay_stencil(const Rect &cliprect);
    void stereo_keep(const quad_t &q, bool direct);
    void stereo_sort();

    // System 24 tilemaps (segaic24)
    uint16_t tile(uint32_t i) const { return ram16(0x700000 + ((i & 0x7fff) << 1)); }
    void build_layers();
    void build_tile(const uint8_t *chars, int layer, uint32_t t, uint16_t val);
    void tiles_draw(int layer, int flags);
    void draw_rect(const uint16_t *mask, uint16_t tpri, int flags, int win, int L, int sx, int sy, int xx1, int yy1,
                   int xx2, int yy2);
    void tilemap_draw(int L, int sx, int sy, int minx, int maxx, int miny, int maxy, int flags);

    const Memory &mem_;
    std::vector<uint32_t> poly_rom_;
    std::vector<uint32_t> screen_;
    std::unique_ptr<view_t> view_;
    std::vector<uint32_t> poly_ram_;
    std::vector<uint16_t> tgp_ram_;
    std::vector<point_t> pointdb_;
    point_t *pointpt_ = nullptr;
    std::vector<quad_t> quaddb_;
    quad_t *quadpt_ = nullptr;
    std::vector<quad_t *> quadind_;
    uint16_t listctl_[2] = {0, 0};
    bool render_done_ = false;
    uint32_t list_base_ = 0x600000; // m_display_list_current
    uint64_t frame_ = 0;            // m_screen->frame_number() during an update
    std::vector<uint8_t> overlay_block_;
    std::vector<uint32_t> overlay_hud_snapshot_;
    uint32_t pens_[4096];           // the tilemap pens, from palette RAM per frame
    std::vector<uint16_t> pixmap_[4];
    std::vector<uint8_t> flags_[4];
    bool hashes_ = true;
#ifdef M1_FAST_PATHS
    // build_layers redraws only the tiles whose tile word or character data
    // changed since the last frame (the same pixmaps).
    std::vector<uint16_t> layer_tiles_;  // each layer tile's word when drawn
    std::vector<uint8_t> chars_seen_;    // character RAM when last compared
    std::vector<uint8_t> char_changed_;  // per character code, this frame
    bool layers_valid_ = false;
#endif

    bool keep_stereo_ = false;
    render_pass pass_ = RENDER_BELOW_HUD;
    size_t stereo_batch_ = 0; // the first quad of the batch not yet sorted
    uint32_t stereo_object_ = 0;
    std::vector<StereoQuad> stereo_quads_, stereo_sorted_;
    std::vector<float> stereo_z_;
    std::vector<StereoProjection> stereo_projections_;
    std::vector<uint32_t> stereo_back_, stereo_mid_;
};

} // namespace m1
