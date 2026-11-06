// The game core's C interface (common/include/afcore.h) over rt::GameLoop: what the
// desktop app does around the game, without SDL or a window, for front ends
// that bring their own (Arcade Foundry: Insert Coin's SwiftUI and SDL apps).
#include "afcore.h"
#include "audio_ring.h"
#include "board_inputs.h"
#include "core_versions.h"

#include "app/link_socket.h"
#include "runtime/game_loop.h"
#include "runtime/rom_import.h"
#include "runtime/stereo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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
    m2core::AudioRing audio[2]; // FM and PCM, interleaved stereo
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

using m2core::board_inputs;

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
        // Nobody reading (paused output): keep at most a second.
        const size_t cap = 2 * size_t(sb->fm_rate() > sb->pcm_rate() ? sb->fm_rate() : sb->pcm_rate());
        c->audio[0].push(fm.data(), fm.size(), cap);
        c->audio[1].push(pcm.data(), pcm.size(), cap);
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
    return c->audio[stream].read(out, 2 * max_frames) / 2;
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

int core_check_roms(const char *rom_path, char *report, size_t report_size) {
    try {
        const std::string rom = rom_path ? rom_path : "";
        std::error_code ec;
        if (std::filesystem::is_directory(rom, ec)) {
            set_error(report, report_size, "a folder of imported images: nothing to check");
            return -1;
        }
        rt::import_rom_set(rom); // reads every file and checks its size and CRC
        set_error(report, report_size, "");
        return 1;
    } catch (const std::exception &e) {
        set_error(report, report_size, e.what());
        return 0;
    }
}

const afcore_api kApi = {
    AFCORE_API_VERSION, sizeof(afcore_api), AFCORE_CORE_VERSION(M2_ROMSET_ID), M2_ROMSET, rt::kGameTitle, rt::GameLoop::kFrameHz,
    rt::GameLoop::kWidth, rt::GameLoop::kHeight,
    core_create, core_destroy, core_run_frame, core_screen,
    core_audio_rate, core_audio_read, core_save, core_link_start, core_link_state, core_error,
    core_render_eyes, core_eye_screen, core_hud_screen, kStereoScale,
    nullptr, // screen_surface
    core_check_roms,
};

} // namespace

#define AFCORE_ENTRY_(set) afcore_api_##set
#define AFCORE_ENTRY(set) AFCORE_ENTRY_(set)
extern "C" AFCORE_EXPORT const afcore_api *AFCORE_ENTRY(M2_ROMSET_ID)(void) { return &kApi; }
