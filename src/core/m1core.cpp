// The game core's C interface (include/afcore.h) for a Model 1 game: the
// board (src/model1/board) run a frame at a time, for the Arcade Foundry: Insert Coin front
// ends. Link play: the comm board (src/model1/comm_board) over TCP.
#include "afcore.h"
#include "core_versions.h"

#include "app/link_socket.h"

#include "model1/board.h"
#include "model1/rom_set.h"
#include "model1/stereo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <exception>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct afcore {
    std::unique_ptr<app::TcpLink> link; // declared first: outlives the board, which holds it
    std::unique_ptr<m1::Board> board;
    std::string save_dir;
    std::deque<float> audio[3]; // the sound board's FM and PCM, the DSB's MPEG (swa)
    int gear = -1;     // the last gear the front end asked for (-1: none yet)
    int shift = 0;     // frames left of a shift pulse: > 0 up, < 0 down
    bool shift_buttons = false; // the front end sends AFCORE_SHIFT_*: gear changes are then not shifts
    m1::Stereo stereo;
    int stereo_idle = -1; // frames since the last render_eyes, -1 with Video not keeping the 3D for it
    bool stopped = false; // the board hit a fatal error (reported once): the screen stays
    std::string error;    // why
};

namespace {

void set_error(char *err, size_t size, const char *what) {
    if (err && size) std::snprintf(err, size, "%s", what);
}

std::string save_path(const afcore *c, const char *name) { return c->save_dir + "/" + name; }

void load_file(const std::string &path, uint8_t *into, size_t size) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return;
    std::vector<uint8_t> d{std::istreambuf_iterator<char>(f), {}};
    if (d.size() == size) std::copy(d.begin(), d.end(), into);
}
void save_file(const std::string &path, const uint8_t *from, size_t size) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char *>(from), std::streamsize(size));
}

// Star Wars Arcade's ports (MAME's swa inputs): the stick 27-227 (X
// reversed), the throttle 28-200 (reversed: 200 closed), IN.0 coin, test,
// service, start 1 and 2, IN.1 the stick's three buttons.
m1::Controls swa_controls(const afcore_input &in) {
    m1::Controls k = m1::Controls::at_rest("swa");
    k.adc[0] = uint8_t(std::lround(127 - std::clamp(in.steer, -1.f, 1.f) * 100));
    k.adc[1] = uint8_t(std::lround(127 + std::clamp(in.stick_y, -1.f, 1.f) * 100));
    k.adc[2] = uint8_t(std::lround(200 - std::clamp(in.accel, 0.f, 1.f) * 172));
    const auto low = [&](uint8_t &port, uint8_t bit, uint32_t button) {
        if (in.buttons & button) port &= uint8_t(~bit);
    };
    low(k.in0, 0x01, AFCORE_COIN);
    low(k.in0, 0x04, AFCORE_TEST);
    low(k.in0, 0x08, AFCORE_SERVICE);
    low(k.in0, 0x10, AFCORE_START);
    low(k.in0, 0x20, AFCORE_START2);
    low(k.in1, 0x01, AFCORE_TRIGGER1);
    low(k.in1, 0x02, AFCORE_BUTTON2);
    low(k.in1, 0x10, AFCORE_BUTTON3);
    return k;
}

// Virtua Fighter's ports (MAME's vf inputs): IN.0 coin, test, service,
// start 1 and 2, IN.1 and IN.2 the two players' punch, kick, guard and
// joystick (the same bits as Virtua Fighter 2's).
m1::Controls vf_controls(const afcore_input &in) {
    m1::Controls k = m1::Controls::at_rest("vf");
    const auto low = [&](uint8_t &port, uint8_t bit, uint32_t button) {
        if (in.buttons & button) port &= uint8_t(~bit);
    };
    low(k.in0, 0x01, AFCORE_COIN);
    low(k.in0, 0x04, AFCORE_TEST);
    low(k.in0, 0x08, AFCORE_SERVICE);
    low(k.in0, 0x10, AFCORE_START);
    low(k.in0, 0x20, AFCORE_START2);
    static const struct { uint8_t bit; uint32_t button; } fight[] = {
        {0x01, AFCORE_PUNCH}, {0x02, AFCORE_KICK}, {0x04, AFCORE_GUARD}, {0x10, AFCORE_DOWN},
        {0x20, AFCORE_UP},    {0x40, AFCORE_RIGHT}, {0x80, AFCORE_LEFT}};
    for (const auto &f : fight) {
        low(k.in1, f.bit, f.button);
        if (in.buttons2 & f.button) k.in2 &= uint8_t(~f.bit);
    }
    return k;
}

// Virtua Racing's ports (MAME's vr inputs): the wheel 0-0xff, the pedals
// 1-0xff, IN.0 coin, test, service, start, VR1-3, IN.1 VR4 and the shifter
// (down, up). A front end with only a gear number (it never sends the shift
// buttons) shifts by its changes.
// Wing War's ports (MAME's wingwar inputs): the stick (X reversed) and the
// throttle on the ADC; IN.0 coin, test, service, start, views 1-3; IN.1
// view 4, machine gun, missile, smoke.
m1::Controls wingwar_controls(const afcore_input &in) {
    m1::Controls k = m1::Controls::at_rest("wingwar");
    k.adc[0] = uint8_t(std::lround(0x80 - std::clamp(in.steer, -1.f, 1.f) * 0x7f));
    k.adc[1] = uint8_t(std::lround(0x80 + std::clamp(in.stick_y, -1.f, 1.f) * 0x7f));
    k.adc[2] = uint8_t(std::lround(1 + std::clamp(in.accel, 0.f, 1.f) * 0xfe));
    const auto low = [&](uint8_t &port, uint8_t bit, uint32_t button) {
        if (in.buttons & button) port &= uint8_t(~bit);
    };
    low(k.in0, 0x01, AFCORE_COIN);
    low(k.in0, 0x04, AFCORE_TEST);
    low(k.in0, 0x08, AFCORE_SERVICE);
    low(k.in0, 0x10, AFCORE_START);
    low(k.in0, 0x20, AFCORE_VIEW1);
    low(k.in0, 0x40, AFCORE_VIEW2);
    low(k.in0, 0x80, AFCORE_VIEW3);
    low(k.in1, 0x01, AFCORE_VIEW4);
    low(k.in1, 0x10, AFCORE_TRIGGER1);
    low(k.in1, 0x20, AFCORE_BUTTON2);
    low(k.in1, 0x40, AFCORE_BUTTON3);
    return k;
}

m1::Controls board_controls(afcore &c, const afcore_input &in) {
    if (std::string_view(M1_SET_NAME) == "swa") return swa_controls(in);
    if (std::string_view(M1_SET_NAME) == "vf") return vf_controls(in);
    if (std::string_view(M1_SET_NAME) == "wingwar") return wingwar_controls(in);
    m1::Controls k = m1::Controls::at_rest(M1_SET_NAME);
    const auto pedal = [](float v) { return uint8_t(std::lround(1 + std::clamp(v, 0.f, 1.f) * 0xfe)); };
    k.adc[0] = uint8_t(std::lround(0x80 + std::clamp(in.steer, -1.f, 1.f) * 0x7f));
    k.adc[1] = pedal(in.accel);
    k.adc[2] = pedal(in.brake);
    const auto low = [&](uint8_t &port, uint8_t bit, uint32_t button) {
        if (in.buttons & button) port &= uint8_t(~bit);
    };
    low(k.in0, 0x01, AFCORE_COIN);
    low(k.in0, 0x04, AFCORE_TEST);
    low(k.in0, 0x08, AFCORE_SERVICE);
    low(k.in0, 0x10, AFCORE_START);
    low(k.in0, 0x20, AFCORE_VIEW1);
    low(k.in0, 0x40, AFCORE_VIEW2);
    low(k.in0, 0x80, AFCORE_VIEW3);
    low(k.in1, 0x01, AFCORE_VIEW4);
    low(k.in1, 0x10, AFCORE_SHIFT_DOWN);
    low(k.in1, 0x20, AFCORE_SHIFT_UP);
    if (in.buttons & (AFCORE_SHIFT_UP | AFCORE_SHIFT_DOWN)) c.shift_buttons = true;
    if (c.gear < 0 || c.shift_buttons) c.gear = in.gear;
    if (in.gear != c.gear && c.shift == 0) {
        c.shift = in.gear > c.gear ? 4 : -4; // held 4 frames: the game reads it at its own pace
        c.gear = in.gear;
    }
    if (c.shift > 0) k.in1 &= uint8_t(~0x20), --c.shift;
    else if (c.shift < 0) k.in1 &= uint8_t(~0x10), ++c.shift;
    return k;
}

void core_save(afcore *c) {
    if (!c || c->save_dir.empty()) return;
    save_file(save_path(c, "ioboard_eeprom.bin"), c->board->eeprom().data(), c->board->eeprom().size());
    save_file(save_path(c, "backup_ram.bin"), c->board->backup_ram(), m1::Board::kBackupBytes);
}

afcore *core_create(const char *rom_path, const char *save_dir, char *err, size_t err_size) {
    try {
        auto c = std::make_unique<afcore>();
        const m1::RomSet *set = m1::find_rom_set(M1_SET_NAME);
        if (!set) throw std::runtime_error("no Model 1 ROM set " M1_SET_NAME);
        c->board = std::make_unique<m1::Board>(m1::load_rom_set(*set, rom_path ? rom_path : ""), *set);
        c->board->video().set_hashes(false); // the tools' frame hashes: three screen passes a frame
        if (save_dir && *save_dir) {
            c->save_dir = save_dir;
            load_file(save_path(c.get(), "ioboard_eeprom.bin"), c->board->eeprom().data(), c->board->eeprom().size());
            c->board->reload_settings();
            load_file(save_path(c.get(), "backup_ram.bin"), c->board->backup_ram(), m1::Board::kBackupBytes);
        }
        return c.release();
    } catch (const std::exception &e) {
        set_error(err, err_size, e.what());
        return nullptr;
    }
}

void core_destroy(afcore *c) {
    core_save(c);
    delete c;
}

// Video keeps each frame's 3D for render_eyes only while a front end asks
// for it: on from the first call (that call has nothing yet: flat), off after
// this many frames without one.
constexpr int kStereoIdleFrames = 30;

void core_run_frame(afcore *c, const afcore_input *input) {
    if (c->stopped) return;
    if (c->stereo_idle >= 0 && ++c->stereo_idle > kStereoIdleFrames) {
        c->board->video().set_keep_stereo(false);
        c->stereo_idle = -1;
    }
    c->board->controls() = input ? board_controls(*c, afcore_read_input(input)) : m1::Controls::at_rest(M1_SET_NAME);
    try {
        c->board->run_frame();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "afcore %s: stopped at frame %llu: %s\n", M1_SET_NAME,
                     (unsigned long long)c->board->frame(), e.what());
        c->stopped = true;
        c->error = e.what();
        return;
    }
    snd::SoundBoard &sb = c->board->sound();
    const std::vector<float> fm = sb.take_fm(), pcm = sb.take_pcm();
    c->audio[0].insert(c->audio[0].end(), fm.begin(), fm.end());
    c->audio[1].insert(c->audio[1].end(), pcm.begin(), pcm.end());
    if (m1::Dsb *dsb = c->board->dsb()) {
        const std::vector<float> mpeg = dsb->take();
        c->audio[2].insert(c->audio[2].end(), mpeg.begin(), mpeg.end());
    }
    // Nobody reading (paused output): keep at most a second.
    const size_t cap = 2 * size_t(std::max({sb.fm_rate(), sb.pcm_rate(), double(m1::Dsb::kRate)}));
    for (auto &q : c->audio)
        if (q.size() > cap) q.erase(q.begin(), q.end() - std::ptrdiff_t(cap));
}

const uint32_t *core_screen(afcore *c, int *width, int *height) {
    if (width) *width = m1::Video::W;
    if (height) *height = m1::Video::H;
    return c->board->screen().data();
}

double core_audio_rate(afcore *c, int stream) {
    if (stream == 2) return c->board->dsb() ? c->board->dsb()->rate() : 0;
    if (stream < 0 || stream > 1) return 0;
    return stream == 0 ? c->board->sound().fm_rate() : c->board->sound().pcm_rate();
}

size_t core_audio_read(afcore *c, int stream, float *out, size_t max_frames) {
    if (stream < 0 || stream > 2 || !out) return 0;
    std::deque<float> &q = c->audio[stream];
    const size_t frames = std::min(max_frames, q.size() / 2);
    std::copy_n(q.begin(), 2 * frames, out);
    q.erase(q.begin(), q.begin() + std::ptrdiff_t(2 * frames));
    return frames;
}

int core_link_start(afcore *c, int listen_port, const char *next, int framesync, char *err, size_t err_size) {
    if (listen_port < 1 || listen_port > 65535) {
        set_error(err, err_size, "link: listen port must be 1-65535");
        return -1;
    }
    try {
        auto link = std::make_unique<app::TcpLink>(uint16_t(listen_port), next ? next : "");
        if (!link->error().empty()) {
            set_error(err, err_size, ("link: " + link->error()).c_str());
            return -1;
        }
        c->board->set_link(link.get(), framesync != 0);
        c->link = std::move(link);
        return 0;
    } catch (const std::exception &e) {
        set_error(err, err_size, e.what());
        return -1;
    }
}

int core_link_state(afcore *c, int *id, int *count) {
    if (!c->link) return AFCORE_LINK_OFF;
    const m1::CommBoard &board = c->board->comm_board();
    if (id) *id = board.id();
    if (count) *count = board.count();
    switch (board.link()) {
    case m1::CommBoard::Link::Waiting: return AFCORE_LINK_WAITING;
    case m1::CommBoard::Link::Up: return AFCORE_LINK_UP;
    case m1::CommBoard::Link::Lost: return AFCORE_LINK_LOST;
    default: return AFCORE_LINK_OFF;
    }
}

const char *core_error(afcore *c) { return c->stopped ? c->error.c_str() : nullptr; }

// Metres per game unit, the stereo's default world scale (a guess per game).
#if defined(M1_STEREO_SCALE)
constexpr float kStereoScale = M1_STEREO_SCALE;
#else
constexpr float kStereoScale = 1.0f;
#endif

int core_render_eyes(afcore *c, const afcore_stereo *in) {
    if (!in || c->stopped) return 0;
    const afcore_stereo s = afcore_read_stereo(in);
    rt::StereoView view;
    view.mode = s.mode == AFCORE_STEREO_HEAD ? rt::StereoView::Mode::Head : rt::StereoView::Mode::Pair;
    view.scale = s.world_scale > 0 ? s.world_scale : kStereoScale;
    if (s.ipd > 0) view.ipd = s.ipd;
    if (s.convergence > 0) view.convergence = s.convergence;
    for (int i = 0; i < 2; ++i) {
        rt::StereoEye &e = view.eye[i];
        std::copy(std::begin(s.eye[i].view), std::end(s.eye[i].view), e.view);
        e.tan_left = s.eye[i].tan_left, e.tan_right = s.eye[i].tan_right;
        e.tan_down = s.eye[i].tan_down, e.tan_up = s.eye[i].tan_up;
        e.width = s.eye[i].width, e.height = s.eye[i].height;
    }
    if (c->stereo_idle < 0) c->board->video().set_keep_stereo(true); // kept from the next frame
    c->stereo_idle = 0;
    try {
        return c->stereo.render(c->board->video(), view) ? 1 : 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "afcore %s: stereo: %s\n", M1_SET_NAME, e.what());
        return 0;
    }
}

const uint32_t *core_eye_screen(afcore *c, int eye, int *width, int *height) {
    if (width) *width = c->stereo.eye_width(eye);
    if (height) *height = c->stereo.eye_height(eye);
    return c->stereo.eye(eye).empty() ? nullptr : c->stereo.eye(eye).data();
}

const uint32_t *core_hud_screen(afcore *c, int *width, int *height) {
    if (width) *width = m1::Video::W;
    if (height) *height = m1::Video::H;
    return c->stereo.hud().empty() ? nullptr : c->stereo.hud().data();
}

const afcore_api kApi = {
    AFCORE_API_VERSION, sizeof(afcore_api), AFCORE_CORE_VERSION(M2_ROMSET_ID), M1_SET_NAME, M1_GAME_TITLE, m1::Board::kFrameHz, m1::Video::W, m1::Video::H,
    core_create, core_destroy, core_run_frame, core_screen,
    core_audio_rate, core_audio_read, core_save, core_link_start, core_link_state, core_error,
    core_render_eyes, core_eye_screen, core_hud_screen, kStereoScale,
};

} // namespace

#define AFCORE_ENTRY_(set) afcore_api_##set
#define AFCORE_ENTRY(set) AFCORE_ENTRY_(set)
extern "C" AFCORE_EXPORT const afcore_api *AFCORE_ENTRY(M2_ROMSET_ID)(void) { return &kApi; }
