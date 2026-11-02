// The game core's C interface (include/afcore.h) over rt::GameLoop: what the
// desktop app does around the game, without SDL or a window, for front ends
// that bring their own (Arcade Foundry: Insert Coin's SwiftUI and SDL apps).
#include "afcore.h"
#include "core_versions.h"

#include "app/link_socket.h"
#include "runtime/game_loop.h"
#include "runtime/rom_import.h"
#include "runtime/stereo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

struct afcore {
    std::unique_ptr<app::TcpLink> link; // declared first: outlives the game, which holds it
    std::unique_ptr<rt::GameLoop> game;
    std::string save_dir;
    std::deque<float> audio[2];
    std::string error; // the board hit something the core cannot run: it stays stopped
    rt::Stereo stereo;
    int head_idle = -1; // frames since the last head-tracked render_eyes, -1 with the clip planes closed
};

namespace {

void set_error(char *err, size_t size, const char *what) {
    if (err && size) std::snprintf(err, size, "%s", what);
}

std::string save_path(const afcore *c, const char *name) { return c->save_dir + "/" + name; }

template <typename C> void load_file(const std::string &path, C &into) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return;
    std::vector<uint8_t> d{std::istreambuf_iterator<char>(f), {}};
    if (d.size() == into.size()) std::copy(d.begin(), d.end(), into.begin());
}
template <typename C> void save_file(const std::string &path, const C &from) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char *>(from.data()), std::streamsize(from.size()));
}

// The app's mapping (app::Controls::sample) from controls to the board's ports.
rt::Inputs board_inputs(const afcore_input &c) {
    rt::Inputs in;
    const float lock = rt::kModel2a ? 0x7f : 0x60;
    const auto pedal = [](float v) {
        return uint8_t(std::lround(rt::kPedalMin + std::clamp(v, 0.f, 1.f) * (rt::kPedalMax - rt::kPedalMin)));
    };
    in.steer = uint8_t(std::lround(0x80 + std::clamp(c.steer, -1.f, 1.f) * lock));
    in.accel = pedal(c.accel);
    in.brake = pedal(c.brake);
    const auto low = [&](uint8_t &port, uint8_t bit, uint32_t button) {
        if (c.buttons & button) port &= uint8_t(~bit);
    };
    if (rt::kGuns) { // two light guns; start 1 and 2 on IN0 (MAME's model2 ports), triggers on IN1
        low(in.in0, 0x01, AFCORE_COIN);
        low(in.in0, rt::kIn0Test, AFCORE_TEST);
        low(in.in0, rt::kIn0Service, AFCORE_SERVICE);
        low(in.in0, 0x10, AFCORE_START);
        low(in.in0, 0x20, AFCORE_START2);
        low(in.in1, 0x01, AFCORE_TRIGGER1);
        low(in.in1, 0x02, AFCORE_TRIGGER2);
        if (rt::kBel) { // player 1's missile: VIEW3 (the front ends' West or X; B reloads there) or BUTTON2
            low(in.in1, 0x10, AFCORE_VIEW3);
            low(in.in1, 0x10, AFCORE_BUTTON2);
        }
        for (int n = 0; n < 2; n++) rt::set_gun(in, n, c.gun_x[n], c.gun_y[n]);
        return in;
    }
    if (rt::kFighting) { // two players' punch, kick, guard and joystick on IN1 and IN2 (MAME's model2 ports)
        low(in.in0, 0x01, AFCORE_COIN);
        low(in.in0, 0x04, AFCORE_TEST);
        low(in.in0, 0x08, AFCORE_SERVICE);
        low(in.in0, 0x10, AFCORE_START);
        low(in.in0, 0x20, AFCORE_START2);
        static const struct { uint8_t bit; uint32_t button; } fight[] = {
            {uint8_t(rt::kDoab ? 0x02 : 0x01), AFCORE_PUNCH}, {uint8_t(rt::kDoab ? 0x04 : 0x02), AFCORE_KICK},
            {uint8_t(rt::kDoab ? 0x01 : 0x04), AFCORE_GUARD}, {0x10, AFCORE_DOWN}, // Dead or Alive: GUARD holds
            {0x20, AFCORE_UP},    {0x40, AFCORE_RIGHT}, {0x80, AFCORE_LEFT}};
        for (const auto &f : fight) {
            if (rt::kDynabb97 && f.bit == 0x04) { // no third button: guard swings the bat, fully
                in.bat[0] = c.buttons & f.button ? 0xff : 0;
                in.bat[1] = c.buttons2 & f.button ? 0xff : 0;
                continue;
            }
            low(in.in1, f.bit, f.button);
            if (c.buttons2 & f.button) in.in2 &= uint8_t(~f.bit);
        }
        return in;
    }
    if (rt::kDesert) { // MAME's desert ports: VR1-3 on IN0, the shift lever, machine gun and cannon on IN1
        in.steer = uint8_t(std::lround(0x80 + std::clamp(c.steer, -1.f, 1.f) * 0x7f));
        low(in.in0, 0x01, AFCORE_COIN);
        low(in.in0, 0x04, AFCORE_TEST);
        low(in.in0, 0x08, AFCORE_SERVICE);
        low(in.in0, rt::kIn0Start, AFCORE_START);
        low(in.in0, 0x20, AFCORE_VIEW1);
        low(in.in0, 0x40, AFCORE_VIEW2);
        low(in.in0, 0x80, AFCORE_VIEW3);
        low(in.in1, 0x01, AFCORE_VIEW4);
        low(in.in1, 0x10, AFCORE_SHIFT_UP);
        low(in.in1, 0x20, AFCORE_SHIFT_DOWN);
        return in;
    }
    if (rt::kSkytargt) { // the flight stick: STICKX reads lower pushed right (MAME's PORT_REVERSE), STICKY lower pushed forward
        in.stick_x = uint8_t(std::lround(0x80 - std::clamp(c.steer, -1.f, 1.f) * 0x7f));
        in.stick_y = uint8_t(std::lround(0x80 + std::clamp(c.stick_y, -1.f, 1.f) * 0x7f));
        low(in.in0, 0x01, AFCORE_COIN);
        low(in.in0, 0x04, AFCORE_TEST);
        low(in.in0, 0x08, AFCORE_SERVICE);
        low(in.in0, 0x20, AFCORE_BUTTON3); // Star Wars Arcade's buttons: view change
        low(in.in0, rt::kIn0Start, AFCORE_START);
        low(in.in1, 0x10, AFCORE_TRIGGER1); // machine gun
        low(in.in1, 0x20, AFCORE_BUTTON2);  // missile
        return in;
    }
    if (rt::kTopskatr) { // the curving follows the steering, the slide the stick's forward and back
        in.steer = uint8_t(std::lround(0x80 + std::clamp(c.steer, -1.f, 1.f) * 0x7f));
        in.roll = uint8_t(std::lround(0x80 + std::clamp(c.stick_y, -1.f, 1.f) * 0x7f));
        low(in.in0, 0x01, AFCORE_COIN);
        low(in.in0, 0x04, AFCORE_TEST);
        low(in.in0, 0x08, AFCORE_SERVICE);
        low(in.in0, 0x40, AFCORE_START);
        low(in.in0, 0x20, AFCORE_VIEW1);      // jump front
        low(in.in1, 0x01, AFCORE_VIEW2);      // jump tail
        low(in.in0, 0x80, AFCORE_SHIFT_DOWN); // select left
        low(in.in0, 0x10, AFCORE_SHIFT_UP);   // select right
        low(in.in0, 0x80, AFCORE_LEFT);
        low(in.in0, 0x10, AFCORE_RIGHT);
        return in;
    }
    if (rt::kSkisuprg) { // the inclining follows the steering, the swing the stick's forward and back; no start button: select 1 (START too)
        in.steer = uint8_t(std::lround(0x80 + std::clamp(c.steer, -1.f, 1.f) * 0x7f));
        in.pitch = uint8_t(std::lround(0x80 + std::clamp(c.stick_y, -1.f, 1.f) * 0x7f));
        low(in.in0, 0x01, AFCORE_COIN);
        low(in.in0, 0x04, AFCORE_TEST);
        low(in.in0, 0x08, AFCORE_SERVICE);
        low(in.in0, 0x40, AFCORE_START);      // select 1
        low(in.in0, 0x40, AFCORE_VIEW1);
        low(in.in0, 0x80, AFCORE_VIEW2);      // select 2
        low(in.in0, 0x10, AFCORE_VIEW3);      // select 3
        low(in.in0, 0x20, AFCORE_SHIFT_UP);   // zoom in
        low(in.in1, 0x01, AFCORE_SHIFT_DOWN); // zoom out
        return in;
    }
    if (rt::kSegawski) { // the slide follows the steering; no start button: set (START or VIEW1) starts and confirms
        in.steer = uint8_t(std::lround(0x80 + std::clamp(c.steer, -1.f, 1.f) * 0x7f));
        low(in.in0, 0x01, AFCORE_COIN);
        low(in.in0, 0x04, AFCORE_TEST);
        low(in.in0, 0x08, AFCORE_SERVICE);
        low(in.in0, 0x40, AFCORE_DOWN);    // select down
        low(in.in1, 0x01, AFCORE_START);   // set
        low(in.in1, 0x01, AFCORE_VIEW1);
        low(in.in1, 0x02, AFCORE_UP);      // select up
        low(in.in1, 0x04, AFCORE_VIEW3);   // pitch left (the front ends' West button)
        low(in.in1, 0x08, AFCORE_VIEW2);   // pitch right (East)
        return in;
    }
    if (rt::kWaverunr) { // the handle bar and the rider's lean both follow the steering; the throttle lever reads 0xff released, 0 gripped (the input test's THROTTLE LEVER)
        in.steer = uint8_t(std::lround(0x80 + std::clamp(c.steer, -1.f, 1.f) * 0x7f));
        in.roll = in.steer;
        in.throttle = uint8_t(std::lround(0xff - std::clamp(c.accel, 0.f, 1.f) * 0xff));
        in.pitch = uint8_t(std::lround(0x80 + std::clamp(c.stick_y, -1.f, 1.f) * 0x7f));
        low(in.in0, 0x01, AFCORE_COIN);
        low(in.in0, 0x04, AFCORE_TEST);
        low(in.in0, 0x08, AFCORE_SERVICE);
        low(in.in0, rt::kIn0Start, AFCORE_START);
        low(in.in1, 0x01, AFCORE_VIEW1);
        return in;
    }
    if (rt::kManxControls) { // the bank sensor reads lower leaning right (MAME's PORT_REVERSE)
        in.bank = uint8_t(std::lround(0x80 - std::clamp(c.steer, -1.f, 1.f) * 0x7f));
        low(in.in0, 0x01, AFCORE_COIN);
        low(in.in0, 0x04, AFCORE_TEST);
        low(in.in0, 0x08, AFCORE_SERVICE);
        low(in.in0, rt::kIn0Start, AFCORE_START);
        low(in.in1, 0x10, rt::kMotoraid ? AFCORE_PUNCH : AFCORE_SHIFT_UP); // Motor Raid: punch and kick
        low(in.in1, 0x20, rt::kMotoraid ? AFCORE_KICK : AFCORE_SHIFT_DOWN);
        return in;
    }
    if (rt::kIndyControls) { // shift up and down, two view buttons on IN1 (MAME's indy500 ports; Super GT 24h has view 1 only)
        low(in.in0, 0x01, AFCORE_COIN);
        low(in.in0, 0x04, AFCORE_TEST);
        low(in.in0, 0x08, AFCORE_SERVICE);
        low(in.in0, rt::kIn0Start, AFCORE_START);
        low(in.in1, 0x01, AFCORE_VIEW1);
        low(in.in1, 0x02, AFCORE_VIEW2);
        low(in.in1, 0x10, AFCORE_SHIFT_UP);
        low(in.in1, 0x20, AFCORE_SHIFT_DOWN);
        return in;
    }
    static const uint8_t gearvalue[5] = {0, 2, 1, 6, 5}; // MAME daytona_gearbox_r: neutral, 1-4
    in.in1 = uint8_t((in.in1 & ~0x70) | (gearvalue[std::clamp(c.gear, 0, 4)] << 4));
    low(in.in0, 0x01, AFCORE_COIN);
    low(in.in0, 0x04, AFCORE_TEST);
    low(in.in0, 0x08, AFCORE_SERVICE);
    low(in.in0, rt::kIn0Start, AFCORE_START);
    low(in.in0, 0x20, AFCORE_VIEW1);
    if (rt::kModel2a) { // IN2 is the hand brake (analogue, 0 = off)
        in.handbrake = uint8_t(std::lround(std::clamp(c.handbrake, 0.f, 1.f) * 0xff));
        return in;
    }
    low(in.in0, 0x40, AFCORE_VIEW2);
    low(in.in0, 0x80, AFCORE_VIEW3);
    low(in.in1, 0x01, AFCORE_VIEW4);
    return in;
}

void core_save(afcore *c) {
    if (!c || c->save_dir.empty()) return;
    save_file(save_path(c, "ioboard_eeprom.bin"), c->game->board().io().eeprom);
    save_file(save_path(c, "backup_ram.bin"), c->game->board().backup_ram());
}

afcore *core_create(const char *rom_path, const char *save_dir, char *err, size_t err_size) {
    try {
        auto c = std::make_unique<afcore>();
        const std::string rom = rom_path ? rom_path : "";
        std::error_code ec;
        c->game = std::filesystem::is_directory(rom, ec) ? std::make_unique<rt::GameLoop>(rom)
                                                         : std::make_unique<rt::GameLoop>(rt::import_rom_set(rom));
        if (save_dir && *save_dir) {
            c->save_dir = save_dir;
            load_file(save_path(c.get(), "ioboard_eeprom.bin"), c->game->board().io().eeprom);
            load_file(save_path(c.get(), "backup_ram.bin"), c->game->board().backup_ram());
        }
        c->game->set_cabinet_setup(true); // Manx TT, first run: TWIN cabinet
        c->game->board().video().set_keep_layers(true); // for render_eyes: one copy of the back layers a frame
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

// Head-tracked stereo: how far the geometrizer's clip planes open (Geo::
// set_open_frustum), and how many frames without a head-tracked render_eyes
// close them again.
constexpr float kHeadOpenFrustum = 6.0f;
constexpr int kHeadIdleFrames = 30;
// The racing games mix their SCSP output far below the others (Sega Rally peaks near 0.07 of full
// scale through a coin, start and race, VF2 near 0.7), which the cabinet's amplifier made up for.
// Raise those to about the same level, clamped so a louder moment can't wrap. Rail Chase 2 (INDY
// 500's sound driver) peaks higher but is as quiet over a game (RMS 0.04, Daytona 0.23).
constexpr float kOutputGain = rt::kSegaRally || rt::kSgt24h ? 8.0f : rt::kIndy500 ? 6.0f : rt::kRchase2 ? 2.5f : 1.0f;

void core_run_frame(afcore *c, const afcore_input *input) {
    if (!c->error.empty()) return;
    try {
        rt::Inputs in = input ? board_inputs(afcore_read_input(input)) : rt::Inputs{};
        // Sega Ski Super G stops at boot on DRIVE BOARD TROUBLE (no drive board answers it, as in
        // MAME) until test is pressed: press it once for the player while that screen shows.
        if (rt::kSkisuprg && c->game->frames() >= 900 && c->game->frames() < 906) in.in0 &= uint8_t(~0x04);
        c->game->run_frame(in);
    } catch (const std::exception &e) {
        c->error = e.what();
        std::fprintf(stderr, "afcore %s: stopped: %s\n", M2_ROMSET, e.what());
        return;
    }
    if (c->head_idle >= 0 && ++c->head_idle > kHeadIdleFrames) { // head-tracked stereo stopped
        c->game->board().set_open_frustum(1.0f);
        c->head_idle = -1;
    }
    if (snd::SoundBoard *sb = c->game->sound()) {
        std::vector<float> fm = sb->take_fm();
        const std::vector<float> pcm = sb->take_pcm();
        if constexpr (kOutputGain != 1.0f)
            for (float &v : fm) v = std::clamp(v * kOutputGain, -1.0f, 1.0f);
        c->audio[0].insert(c->audio[0].end(), fm.begin(), fm.end());
        c->audio[1].insert(c->audio[1].end(), pcm.begin(), pcm.end());
        // Nobody reading (paused output): keep at most a second.
        for (int s = 0; s < 2; ++s) {
            const size_t cap = 2 * size_t(sb->fm_rate() > sb->pcm_rate() ? sb->fm_rate() : sb->pcm_rate());
            if (c->audio[s].size() > cap) c->audio[s].erase(c->audio[s].begin(), c->audio[s].end() - cap);
        }
    }
}

const uint32_t *core_screen(afcore *c, int *width, int *height) {
    if (width) *width = c->game->screen_width();
    if (height) *height = rt::GameLoop::kHeight;
    return c->game->screen().data();
}

double core_audio_rate(afcore *c, int stream) {
    snd::SoundBoard *sb = c->game->sound();
    if (!sb || stream < 0 || stream > 1) return 0;
    return stream == 0 ? sb->fm_rate() : sb->pcm_rate();
}

size_t core_audio_read(afcore *c, int stream, float *out, size_t max_frames) {
    if (stream < 0 || stream > 1 || !out) return 0;
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
        c->game->board().set_link(link.get(), framesync != 0);
        c->link = std::move(link);
        return 0;
    } catch (const std::exception &e) {
        set_error(err, err_size, e.what());
        return -1;
    }
}

int core_link_state(afcore *c, int *id, int *count) {
    const rt::CommBoard *board = c->link ? c->game->board().comm_board() : nullptr;
    if (!board) return AFCORE_LINK_OFF;
    if (id) *id = board->id();
    if (count) *count = board->count();
    switch (board->link()) {
    case rt::CommBoard::Link::Waiting: return AFCORE_LINK_WAITING;
    case rt::CommBoard::Link::Up: return AFCORE_LINK_UP;
    case rt::CommBoard::Link::Lost: return AFCORE_LINK_LOST;
    default: return AFCORE_LINK_OFF;
    }
}

const char *core_error(afcore *c) { return c->error.empty() ? nullptr : c->error.c_str(); }

// Metres per game unit: what makes the cars car-sized. Sega Rally's car in
// its attract race spans about 3 units front to back seen from a quarter
// view, 5 to 8 units away: roughly a metre a unit. The other games are
// assumed the same until measured.
constexpr float kStereoScale = 1.0f;

int core_render_eyes(afcore *c, const afcore_stereo *in) {
    if (!in || !c->error.empty()) return 0;
    const afcore_stereo s = afcore_read_stereo(in);
    rt::StereoView view;
    view.mode = s.mode == AFCORE_STEREO_HEAD ? rt::StereoView::Mode::Head : rt::StereoView::Mode::Pair;
    view.scale = s.world_scale > 0 ? s.world_scale : kStereoScale > 0 ? kStereoScale : 1.0f;
    if (s.ipd > 0) view.ipd = s.ipd;
    if (s.convergence > 0) view.convergence = s.convergence;
    for (int i = 0; i < 2; ++i) {
        std::copy_n(s.eye[i].view, 16, view.eye[i].view);
        view.eye[i].tan_left = s.eye[i].tan_left, view.eye[i].tan_right = s.eye[i].tan_right;
        view.eye[i].tan_down = s.eye[i].tan_down, view.eye[i].tan_up = s.eye[i].tan_up;
        view.eye[i].width = s.eye[i].width, view.eye[i].height = s.eye[i].height;
    }
    if (view.mode == rt::StereoView::Mode::Head) { // open the clip planes from the next frame
        if (c->head_idle < 0) c->game->board().set_open_frustum(kHeadOpenFrustum);
        c->head_idle = 0;
    }
    try {
        return c->stereo.render(c->game->board().video(), view) ? 1 : 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "afcore %s: stereo: %s\n", M2_ROMSET, e.what());
        return 0;
    }
}

const uint32_t *core_eye_screen(afcore *c, int eye, int *width, int *height) {
    if (width) *width = c->stereo.eye_width(eye);
    if (height) *height = c->stereo.eye_height(eye);
    return c->stereo.eye(eye).empty() ? nullptr : c->stereo.eye(eye).data();
}

const uint32_t *core_hud_screen(afcore *c, int *width, int *height) {
    if (width) *width = rt::Video::W;
    if (height) *height = rt::Video::H;
    return c->stereo.hud().empty() ? nullptr : c->stereo.hud().data();
}

const afcore_api kApi = {
    AFCORE_API_VERSION, sizeof(afcore_api), AFCORE_CORE_VERSION(M2_ROMSET_ID), M2_ROMSET, rt::kGameTitle, rt::GameLoop::kFrameHz,
    rt::GameLoop::kWidth, rt::GameLoop::kHeight,
    core_create, core_destroy, core_run_frame, core_screen,
    core_audio_rate, core_audio_read, core_save, core_link_start, core_link_state, core_error,
    core_render_eyes, core_eye_screen, core_hud_screen, kStereoScale,
};

} // namespace

#define AFCORE_ENTRY_(set) afcore_api_##set
#define AFCORE_ENTRY(set) AFCORE_ENTRY_(set)
extern "C" AFCORE_EXPORT const afcore_api *AFCORE_ENTRY(M2_ROMSET_ID)(void) { return &kApi; }
