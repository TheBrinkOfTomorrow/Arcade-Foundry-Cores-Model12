// The Model 2 board as the recompiled game runs on it outside lockstep: the
// whole i960 memory map with every device answered natively (no trace, no
// MAME, no emulated CPUs). Frame pacing is the only clock: the game's native
// code runs until it waits for vblank (its idle loop), then the board starts
// vblank: the geometrizer parses the display list, the vblank interrupt is
// raised, and at vblank end the screen is composed.
//
// The board is the one M2_BOARD names: model2o (Daytona) or model2a (Model
// 2A-CRX, Sega Rally: 256 KB of RAM at 0x00200000, the 315-5649 I/O chip and
// its EEPROM at 0x01c00000 in place of the I/O board's dual-port RAM).
//
// Devices, each native: RAM/ROM and texture RAM (as the replay bus), the TGP
// and geometry ports (TgpBoard, recompiled TGP), the geometrizer (Geo), the
// screen (Video: tilemaps, palette, 3D layer), the interrupt controller and
// timers, video/render mode registers, the I/O board (IoBoard: its dual-port
// RAM mailbox protocol, inputs, settings EEPROM), the sound UART (bytes to
// the sound runtime), the comm board's shared RAM and backup RAM.
#pragma once

#include "runtime/cpu.h"
#include "runtime/geo.h"
#include "runtime/lockstep.h"
#include "runtime/m2_tgp_board.h"
#include "runtime/video.h"
#include "runtime/scenery.h"
#include "runtime/state_io.h"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "runtime/comm_board.h"
#include "runtime/crypt5881/crypt5881.h"
#include "runtime/io_315_5649.h"

namespace rt {

// The board this build is for (CMake's M2_BOARD). Model 2B-CRX (model2b) is
// Model 2A-CRX with a SHARC in place of the TGP, so kModel2a holds there too
// and kModel2b marks what differs. Model 2C-CRX (model2c) is Model 2B-CRX
// with a TGPx4 in place of the SHARC: kModel2b holds there too (its map, its
// texture and luma RAM), and kModel2c marks what differs (no SHARC IOP
// registers, the sound UART at 0x01c80000 as on Model 2A).
#ifdef M2_BOARD
inline constexpr bool kModel2c = std::string_view(M2_BOARD) == "model2c";
inline constexpr bool kModel2b = std::string_view(M2_BOARD) == "model2b" || kModel2c;
inline constexpr bool kModel2a = std::string_view(M2_BOARD) == "model2a" || kModel2b;
#else
inline constexpr bool kModel2c = false;
inline constexpr bool kModel2b = false;
inline constexpr bool kModel2a = false;
#endif

// The ROM set this build is recompiled from (CMake's M2_ROMSET), and the
// games on the model2a board.
#ifdef M2_ROMSET
inline constexpr std::string_view kRomSet = M2_ROMSET;
#else
inline constexpr std::string_view kRomSet = "daytona93";
#endif
inline constexpr bool kManxTT = kRomSet == "manxttc";
inline constexpr bool kMotoraid = kRomSet == "motoraid";
// Manx TT and Motor Raid: MAME's manxtt ports (throttle, brake and bank; IN1 0x10/0x20 shift on Manx TT, punch and kick on Motor Raid).
inline constexpr bool kManxControls = kManxTT || kMotoraid;
inline constexpr bool kVcop2 = kRomSet == "vcop2";
inline constexpr bool kVcop = kRomSet == "vcop";
inline constexpr bool kVf2 = kRomSet == "vf2";
inline constexpr bool kIndy500 = kRomSet == "indy500";
inline constexpr bool kFvipers = kRomSet == "fvipers";
inline constexpr bool kSgt24h = kRomSet == "sgt24h";
inline constexpr bool kOverrevb = kRomSet == "overrevb";
inline constexpr bool kStcc = kRomSet == "stcc";
inline constexpr bool kVon = kRomSet == "von";
inline constexpr bool kHotd = kRomSet == "hotdo";
inline constexpr bool kGunblade = kRomSet == "gunblade";
inline constexpr bool kVstriker = kRomSet == "vstriker";
inline constexpr bool kZerogun = kRomSet == "zerogun";
inline constexpr bool kPltkids = kRomSet == "pltkids";
inline constexpr bool kDynamcop = kRomSet == "dynamcop";
// Sky Target: a flight stick on the 315-5649's ADC (MAME's skytargt ports: STICKY channel 0, STICKX 2),
// machine gun and missile on IN1 bits 4-5, view change on IN0 bit 5.
inline constexpr bool kSkytargt = kRomSet == "skytargt";
inline constexpr bool kDynabb97 = kRomSet == "dynabb97";
// Wave Runner (Model 2C): the handle bar, the rider's roll, the throttle lever and the pitch on the
// 315-5649's ADC channels 0-3 (MAME's HANDLE, ROLL, THROTTLE, PITCH, all 0x80 at rest), view on IN1
// bit 0, start on IN0 bit 6; IN2 bit 3 the safety sensor, reading 0 (MAME's unconnected IPT_CUSTOM).
inline constexpr bool kWaverunr = kRomSet == "waverunr";
// Sega Water Ski (Model 2C): the slide on the 315-5649's ADC channel 0 (MAME's SLIDE, 0x80 at rest);
// set, select up and pitch left and right on IN1 bits 0-3, select down on IN0 bit 6 (MAME's segawski ports).
inline constexpr bool kSegawski = kRomSet == "segawski";
// Sega Ski Super G (Model 2C): the swing and the inclining on the 315-5649's ADC channels 0 and 1
// (MAME's SWING, INCLINING, 0x80 at rest); select 1-3 on IN0 bits 6, 7 and 4, zoom in on IN0 bit 5,
// zoom out on IN1 bit 0; IN2 the foot sensors, active high (0 at rest: right bits 0-3, left 4-7).
inline constexpr bool kSkisuprg = kRomSet == "skisuprg";
// Behind Enemy Lines (Model 2C): Gunblade NY's turrets and triggers (MAME's bel ports, from gunblade), test and
// service swapped (test IN0 bit 3, service bit 2), the missile buttons on IN1 bits 4 (player 1) and 5 (player 2).
inline constexpr bool kBel = kRomSet == "bel";
// Top Skater (Model 2C): the curving and the slide on the 315-5649's ADC channels 0 and 1 (MAME's CURVING,
// SLIDE, 0x80 at rest); select right and left on IN0 bits 4 and 7, jump front on IN0 bit 5, start on bit 6,
// jump tail on IN1 bit 0. Its music is on the Digital Sound Board 2 (a 68000 and MPEG audio).
inline constexpr bool kTopskatr = kRomSet == "topskatr";
// Rail Chase 2 (Model 2B): two gun turrets on the 315-5649's ADC, in the order P2_X, P1_X, P2_Y, P1_Y and
// reversed (MAME's rchase2 ports: PORT_REVERSE), triggers on IN1 bits 0 and 1. Its drive board (the
// seat's motion) answers on port D the command written to port E (MAME's rchase2_drive_board_r).
inline constexpr bool kRchase2 = kRomSet == "rchase2";
// Dead or Alive (Model 2B, Revision B; Tecmo): vf2's ports with hold, punch and kick on bits 0-2 (MAME's doa
// ports), and the 317-0229 compression chip, which it asks once at boot for a string (MAME's HACK_MODE_DOA).
inline constexpr bool kDoab = kRomSet == "doab";
// Desert Tank (original Model 2, Daytona's model1io board): STEER, ACCEL and BRAKE on the ADC (pedals
// resting at 0), VR1-3 on IN0 bits 5-7, the shift lever on IN1 bit 0, machine gun and cannon on bits 4-5.
inline constexpr bool kDesert = kRomSet == "desert";
inline constexpr bool kLastbrnx = kRomSet == "lastbrnx";
inline constexpr bool kSchamp = kRomSet == "schamp";
// Virtua Cop 2 and The House of the Dead: guns on the 315-5649's serial channel 2 (MAME's vcop2 ports).
inline constexpr bool kGuns5649 = kVcop2 || kHotd;
// Gunblade NY, Behind Enemy Lines and Rail Chase 2: two gun turrets, read as analogue sticks on the 315-5649's ADC (MAME's gunblade ports).
inline constexpr bool kTurrets = kGunblade || kBel || kRchase2;
// Every game with guns: they aim with the mouse or a pointer.
inline constexpr bool kGuns = kGuns5649 || kVcop || kTurrets;
// INDY 500, Super GT 24h, Over Rev and Touring Car: MAME's indy500 ports (two shift buttons and views on IN1).
inline constexpr bool kIndyControls = kIndy500 || kSgt24h || kOverrevb || kStcc;
// Super GT 24h and Over Rev: the pedals read reversed (MAME's PORT_REVERSE: 0xff up).
inline constexpr bool kReversedPedals = kSgt24h || kOverrevb;
// These wait at boot ("CHECKING NETWORK NOW") for the comm board to answer;
// MAME always fits one, so the board is there with no link (no transport).
inline constexpr bool kCommAlways = kSgt24h || kOverrevb;
// The games with the 315-5881 security chip (MAME model2_5881_mem), and its key (MAME's ROM_PARAMETER ":315_5881:key").
inline constexpr bool kCrypt5881 = kZerogun || kPltkids || kDynamcop;
inline constexpr uint32_t kCrypt5881Key = kZerogun ? 0x042c0d13u : kPltkids ? 0x042e2dc1u : kDynamcop ? 0x2c2a4a93u : 0;
// Virtua Fighter 2 and Fighting Vipers: MAME's vf2 ports (two players' buttons and joysticks on IN1 and IN2).
// Virtual-On's are laid out the same: its two sticks, each with shot and dash, on IN1 and IN2.
// Virtua Striker's too: long pass, shoot and short pass on bits 0-2, the joysticks on 4-7.
// Zero Gunner's and Pilot Kids' too: shot and special on bits 0-1 (MAME's zerogun and pltkids ports: 2-3 unused).
// Last Bronx's too (MAME's vf2 ports), and Sonic Championship's (punch, kick, barrier).
// Dynamite Cop's: punch, kick and jump on bits 0-2 (MAME's dynamcop ports).
// Dead or Alive's: hold, punch and kick on bits 0-2 (MAME's doa ports).
// Dynamite Baseball 97's: two buttons and the joystick (MAME's dynabb ports; the bats on the ADC).
inline constexpr bool kFighting = kDoab || kVf2 || kFvipers || kLastbrnx || kSchamp || kVon || kVstriker || kZerogun || kPltkids || kDynamcop || kDynabb97;
inline constexpr bool kSegaRally = kModel2a && !kManxTT && !kMotoraid && !kVcop2 && !kVf2 && !kIndy500 && !kFvipers && !kSgt24h && !kOverrevb && !kStcc && !kVon && !kHotd && !kGunblade && !kVstriker && !kZerogun && !kPltkids && !kDynamcop && !kSkytargt && !kLastbrnx && !kSchamp && !kDynabb97 && !kWaverunr && !kSegawski && !kSkisuprg && !kBel && !kTopskatr && !kRchase2 && !kDoab;

// The game this build runs, for window titles and messages.
inline constexpr const char *kGameTitle = kManxTT ? "Manx TT Superbike" : kMotoraid ? "Motor Raid" : kVcop2 ? "Virtua Cop 2" : kVcop ? "Virtua Cop" : kVf2 ? "Virtua Fighter 2" : kIndy500 ? "Indy 500" : kFvipers ? "Fighting Vipers" : kSgt24h ? "Super GT 24h" : kOverrevb ? "Over Rev" : kStcc ? "Sega Touring Car Championship" : kVon ? "Cyber Troopers Virtual-On" : kHotd ? "The House of the Dead" : kGunblade ? "Gunblade NY" : kVstriker ? "Virtua Striker" : kZerogun ? "Zero Gunner" : kPltkids ? "Pilot Kids" : kDynamcop ? "Dynamite Cop" : kSkytargt ? "Sky Target" : kDesert ? "Desert Tank" : kLastbrnx ? "Last Bronx" : kSchamp ? "Sonic Championship" : kDynabb97 ? "Dynamite Baseball 97" : kWaverunr ? "Wave Runner" : kSegawski ? "Sega Water Ski" : kSkisuprg ? "Sega Ski Super G" : kBel ? "Behind Enemy Lines" : kTopskatr ? "Top Skater" : kRchase2 ? "Rail Chase 2" : kDoab ? "Dead or Alive" : kModel2a ? "Sega Rally Championship" : "Daytona USA";

// IN0's test and service buttons (Behind Enemy Lines has them the other way round).
inline constexpr uint8_t kIn0Test = kBel ? 0x08 : 0x04, kIn0Service = kBel ? 0x04 : 0x08;

// IN0's start button: Daytona's I/O board has it at bit 4, Sega Rally's
// IN0 at bit 6 (MAME's srallyc ports; bit 5 is VR on both).
inline constexpr uint8_t kIn0Start = kModel2a && !kGuns && !kFighting ? 0x40 : 0x10; // Virtua Cop 2, Virtua Fighter 2, Fighting Vipers: MAME's model2 IN0 (start 2 at 0x20)
// The pedals' range: Daytona's ADC 0x20-0xe0, Sega Rally's (and Manx TT's
// throttle and brake) 0x00-0xff, Super GT 24h's and Over Rev's reversed.
inline constexpr uint8_t kPedalMin = kReversedPedals ? 0xff : kModel2a || kDesert ? 0x00 : 0x20, kPedalMax = kReversedPedals ? 0x00 : kModel2a || kDesert ? 0xff : 0xe0;

// Board time without a cycle clock: MAME's i960 runs 11,381,000 of Sega
// Rally's instructions a second (its 2 ms timer 0 period, 50,000 ticks of
// the 25 MHz timer clock, is 22,762 instructions in MAME's run).
inline constexpr uint64_t kI960Ips = 11381000, kTimerHz = 25000000;
inline uint64_t ticks_for_instructions(uint64_t n) { return n * (kTimerHz / 1000) / (kI960Ips / 1000); }

// The guns' ranges (MAME's P1_X/P2_X/P1_Y/P2_Y PORT_MINMAX): Virtua Cop 2's,
// Virtua Cop's, The House of the Dead's, Rail Chase 2's or Gunblade NY's.
inline constexpr int kGunXMin[2] = {kRchase2 ? 0x3a : kTurrets ? 0x69 : kVcop ? 0x83 : kHotd ? 173 : 137, kRchase2 ? 0x34 : kTurrets ? 0x00 : kVcop ? 0x80 : kHotd ? 163 : 134},
                     kGunXMax[2] = {kRchase2 ? 0xca : kTurrets ? 0xff : kVcop ? 0x276 : kHotd ? 596 : 630, kRchase2 ? 0xc7 : kTurrets ? 0x96 : kVcop ? 0x273 : kHotd ? 596 : 627},
                     kGunYMin[2] = {kRchase2 ? 0x1c : kTurrets ? 0x11 : kHotd ? 87 : 36, kRchase2 ? 0x1c : kTurrets ? 0x11 : kVcop ? 0x27 : kHotd ? 87 : 36},
                     kGunYMax[2] = {kRchase2 ? 0xcb : kTurrets ? 0xae : kHotd ? 380 : 425, kRchase2 ? 0xcb : kTurrets ? 0xae : kHotd ? 380 : 425};

struct Inputs {
    uint8_t steer = 0x80, accel = kPedalMin, brake = kPedalMin; // ADC values (see kPedalMin)
    uint8_t in0 = 0xff, in1 = kManxControls || kGuns || kFighting || kIndyControls || kSkytargt || kDesert || kWaverunr || kSegawski || kSkisuprg || kTopskatr ? 0xff : 0x8f, in2 = kWaverunr ? 0xf7 : kSkisuprg ? 0x00 : 0xff; // switches, active low (MAME IN0/IN1/IN2 layout)
    uint8_t handbrake = 0;                            // Model 2A (Sega Rally): IN2 is the hand brake, 0 = off
    uint8_t bank = 0x80;                              // Manx TT: the bike's lean (ADC 2, 0x80 upright)
    uint8_t stick_x = 0x80, stick_y = 0x80;           // Sky Target: the flight stick (MAME's STICKX, STICKY)
    uint8_t bat[2] = {0, 0};                          // Dynamite Baseball 97: the two bat swings (MAME's BAT1, BAT2)
    uint8_t roll = 0x80, throttle = 0x80, pitch = 0x80; // Wave Runner: ADC 1-3 (the handle bar is steer, ADC 0)
    // Virtua Cop 2: the guns' positions in MAME's P1_X/P1_Y/P2_X/P2_Y ranges
    // (gun_x/gun_y_min/max), centred by default; triggers on IN1 bits 0 and 1.
    // Gunblade NY: the turrets' ADC values, at MAME's defaults (Rail Chase 2's reversed).
    uint16_t gun_x[2] = {uint16_t(kRchase2 ? 0x82 : kTurrets ? 0xb1 : kVcop ? 0x17c : 0x17f), uint16_t(kRchase2 ? 0x7e : kTurrets ? 0x50 : kVcop ? 0x179 : 0x17c)},
             gun_y[2] = {uint16_t(kRchase2 ? 0x74 : kTurrets ? 0x5f : 0x0e6), uint16_t(kRchase2 ? 0x74 : kTurrets ? 0x5f : kVcop ? 0x0e8 : 0x0e6)};
};

// Virtua Cop 2 and Virtua Cop: player n's gun at (x, y), 0..1 across and down the screen;
// outside it, the gun points off screen (at the range's low edge: shooting
// there reloads).
inline void set_gun(Inputs &in, int n, float x, float y) {
    if (kTurrets) { x = x < 0 ? 0 : x > 1 ? 1 : x; y = y < 0 ? 0 : y > 1 ? 1 : y; } // a turret never leaves the screen
    if (kRchase2) { x = 1 - x; y = 1 - y; } // its ports are reversed (MAME's PORT_REVERSE)
    const bool on = x >= 0 && x <= 1 && y >= 0 && y <= 1;
    in.gun_x[n] = uint16_t(on ? kGunXMin[n] + int(x * (kGunXMax[n] - kGunXMin[n]) + 0.5f) : kGunXMin[n]);
    in.gun_y[n] = uint16_t(on ? kGunYMin[n] + int(y * (kGunYMax[n] - kGunYMin[n]) + 0.5f) : kGunYMin[n]);
}

// The guns' off-screen flags as MAME's I/O boards give them (315_5649 serial
// channel 2 byte 8, model1io2 FPGA byte 8): 0xfc, bit 0 when player 1's gun
// is within 5% of an edge of its range, bit 1 for player 2.
inline uint8_t gun_offscreen(const Inputs &in) {
    auto off = [](uint16_t p, int lo, int hi) { const int m = int((hi - lo) * 0.05f); return p <= lo + m || p >= hi - m; };
    uint8_t d = 0xfc;
    for (int n = 0; n < 2; n++)
        if (off(in.gun_x[n], kGunXMin[n], kGunXMax[n]) || off(in.gun_y[n], kGunYMin[n], kGunYMax[n])) d |= uint8_t(1 << n);
    return d;
}

// The I/O board's side of the MB8421 dual-port RAM (2 KB): the game writes a
// command to byte 0x20 and the board answers. 1: latch the inputs into bytes
// 0-10. 3: copy the 128-byte settings EEPROM to bytes 0x100-0x17f. 2: store
// bytes 0x100-0x17f to the EEPROM. The byte returns to 0 when done. Byte 0x11
// is the force feedback drive board's command (rt::DriveBoard): each byte
// written is queued for the host (found by logging the game's writes: it
// carries the drive board's command set). Virtua Cop's board (model1io2)
// answers the same commands; its latch leaves the ADC bytes at 0xff, sets
// bytes 0x0b-0x0e to 0xff, 0xff, 0xff, 0x0f and puts the guns at 0x80-0x87
// (P1_Y, P1_X, P2_Y, P2_X, low byte first) and their off-screen bits at
// 0x88 (all read from MAME's traces of the game). Byte 0x0f is the game's
// own (its lamps), read back.
class IoBoard {
public:
    uint8_t read(uint32_t index) const { return ram_[index & 0x7ff]; }
    void write(uint32_t index, uint8_t v);
    Inputs inputs;
    std::array<uint8_t, 128> eeprom;
    bool eeprom_dirty = false;
    std::vector<uint8_t> drive_commands; // written to byte 0x11 since last taken (at most kMaxDrive kept)
    static constexpr size_t kMaxDrive = 256;
    IoBoard() { ram_.fill(0); eeprom.fill(0xff); }
    template <typename S> void state(S &s) { s(inputs), s(eeprom), s(eeprom_dirty), s(drive_commands), s(ram_); }

private:
    std::array<uint8_t, 0x800> ram_;
};

class M2Board : public Bus {
public:
    struct Images {
        std::vector<uint8_t> program, main_data, copro_tables, copro_data, polygons, textures;
        std::vector<uint8_t> sound_program, pcm1, pcm2; // sound board (68000 program, MultiPCM samples)
        std::vector<uint8_t> samples; // Model 2A's SCSP sound board: its sample ROM (empty on Daytona)
        std::vector<uint8_t> dsb_program, dsb_mpeg; // the Digital Sound Board (Model 2C's Touring Car): Z80 program, MPEG data
        std::vector<uint8_t> m1_program; // Manx TT's Model 1 sound board beside the SCSP (its pcm1): 68000 program
#ifdef M2_DC_MEMORY
        // The Dreamcast: program, main_data, polygons, textures and copro_data
        // are read through rom (those images stay empty); texture RAM (tex0
        // then tex1, 2 MB each; the game's writes only reach the first 1 MB
        // of each) and frame buffer RAM (A then B, 512 KB each; optional, the
        // game does not write it) are the frontend's, in video RAM.
        RomSource *rom = nullptr;
        uint8_t *texture_ram = nullptr, *frame_buffer_ram = nullptr;
#endif
    };
    explicit M2Board(Images images);

    // The CPU and scheduler (free-run Lockstep) the board raises interrupts on.
    void attach(Cpu &cpu, Lockstep &ls);

    // Bus
    uint32_t fetch(uint32_t addr) override;
    uint8_t read_byte(uint32_t addr) override;
    uint16_t read_word(uint32_t addr) override;
    uint32_t read_dword(uint32_t addr) override;
    void write_byte(uint32_t addr, uint8_t data) override;
    void write_word(uint32_t addr, uint16_t data) override;
    void write_dword(uint32_t addr, uint32_t data) override;
    uint16_t flags(uint32_t addr) override { return page(addr).burst ? Cpu::BURST : 0; }
    SceneryState *scenery() override { return &scenery_; }

    // Frame control, called by the runner.
    void vblank_start();       // MAME screen_vblank: geometrizer, vblank IRQ
    void vblank_end();         // screen_update
    bool in_idle_loop() const; // the game is waiting for vblank
    uint64_t frame() const { return frame_; }

    // Board time (Model 2A: Sega Rally runs on the board's timers), in ticks
    // of the 25 MHz timer clock: it advances with the i960's instructions
    // (kI960Ips) and the runner moves it forward over the game's waits.
    uint64_t now_ticks() const;
    void skip_to(uint64_t ticks);
    // MAME model2_timer_cb for each timer that has run out by now.
    void fire_timers();
    // When the next running timer whose interrupt is enabled runs out
    // (UINT64_MAX: none).
    uint64_t next_timer_due() const;

    Video &video() { return *video_; }
    const Geo *geo() const { return geo_.get(); }
    // Widescreen (enhancement): pixels added to each side; 0 = the original screen.
    void set_wide_margin(int pixels);
    void set_open_frustum(float scale); // stereo, head-tracked: Geo::set_open_frustum
    // Draw mode (enhancement): draw the screen every (1 + skip)th frame; the
    // frames between keep the last picture. 0 = every frame, as the game does
    // (double buffered, a new 3D picture each frame); 1 = every 2nd; 2 = every
    // 3rd. The game logic and the geometrizer still run every frame.
#ifdef M2_DC_SPEED
    // The Dreamcast draws every 4th frame: 3 is allowed too.
    void set_frame_skip(int skip) { frame_skip_ = skip < 0 ? 0 : skip > 3 ? 3 : skip; }
#else
    void set_frame_skip(int skip) { frame_skip_ = skip < 0 ? 0 : skip > 2 ? 2 : skip; }
#endif
    IoBoard &io() { return io_; }
    TgpBoard &tgp() { return tgp_; }
    // Bytes sent to the sound board since the last take.
    std::vector<uint8_t> take_sound_bytes() { sound_total_ += uart_out_.size(); return std::exchange(uart_out_, {}); }
    uint64_t sound_bytes_total() const { return sound_total_ + uart_out_.size(); }
    std::vector<uint8_t> &backup_ram() { return backup_; }
    // Link play: the communication board on a host transport (CommBoard;
    // nullptr: no link, the registers stay plain). Set before the game runs.
    void set_link(LinkTransport *transport, bool framesync = false);
    const CommBoard *comm_board() const { return comm_board_.get(); }
    // Save states: every device's state and RAM (ROM images are not saved).
    void state(StateIO &s);
    // The board's Lockstep callback with this tag (add_callback), for a load.
    std::function<void()> lockstep_callback(int tag);
    static constexpr int kUartTag = 1;
#ifdef M2_DC_SPIN_SKIP
    uint64_t spin_skipped() const { return spin_skipped_; } // instructions skipped in the frame wait (read_byte)
#endif

private:
    SceneryState scenery_;
    enum Kind : uint8_t { Unmapped, Rom, Ram, Tex, Dev };
    struct Page {
        Kind kind = Unmapped;
        bool burst = false;
        uint8_t *base = nullptr;
#ifdef M2_DC_MEMORY
        RomRegion region = RomRegion::Program; // Rom pages: read through img_.rom, base is null
        uint32_t rom_offset = 0;
#endif
    };
    static constexpr unsigned kPageBits = 12;
#ifdef M2_DC_MEMORY
    // Two levels: 1 MB chunks of 256 pages, allocated when mapped (one table
    // entry per 4 KB of the 4 GB space is 12 MB here).
    Page &page(uint32_t addr) {
        auto &chunk = chunks_[addr >> 20];
        return chunk ? chunk[(addr >> kPageBits) & 255] : unmapped_;
    }
    Page &map_page(uint32_t addr);
    void map_rom(uint32_t start, uint32_t end, RomRegion region, uint32_t offset);
    const uint8_t *rom_page(const Page &p) { return img_.rom->page_fast(p.region, p.rom_offset >> kPageBits); }
#else
    Page &page(uint32_t addr) { return pages_[addr >> kPageBits]; }
#endif
    void map(uint32_t start, uint32_t end, Kind k, uint8_t *base, uint32_t mirror = 0, bool burst = true);

    // Device dword access: data in its byte lanes, mask = lanes accessed.
    uint32_t dev_read(uint32_t addr, uint32_t mask);
    void dev_write(uint32_t addr, uint32_t data, uint32_t mask);
    void ram_written(uint32_t addr, uint32_t data, uint32_t mask); // video registers kept in RAM pages
    void tex_write(const Page &p, uint32_t addr, uint32_t lane_data);

    void irq_update();
    void uart_write_data(uint8_t v);
    void uart_txrdy(bool state);

    Images img_;
    std::vector<uint8_t> ram_, work_, cpuctl_, backup_, tile_, chr_, palette_, xlat_, tex0_, tex1_, luma_, fb_a_, fb_b_,
        comm_;
    std::vector<uint8_t> luma_dw_ = std::vector<uint8_t>(kModel2b ? 0x20000 : 0); // Model 2B: luma RAM in VideoMem's layout
#ifdef M2_DC_MEMORY
    std::vector<std::unique_ptr<Page[]>> chunks_;
    static Page unmapped_; // what page() gives outside the map; never written
#else
    std::vector<Page> pages_;
#endif

    TgpBoard tgp_;
    std::unique_ptr<Geo> geo_;
    std::unique_ptr<Video> video_;
    IoBoard io_;
    Io5649 io5649_{io_}; // model2a
    std::unique_ptr<CommBoard> comm_board_; // link play only (and kCommAlways)
    std::vector<uint8_t> crypt_ram_ = std::vector<uint8_t>(kCrypt5881 ? 0x10000 : 0); // 0x01d80000: the encrypted stream
    std::unique_ptr<Crypt5881> crypt_;                                                // kCrypt5881
    // kDoab: the 317-0229's RAM window (0x01d80000) and its state (MAME's sega_315_5838_comp_device, HACK_MODE_DOA)
    std::vector<uint8_t> comp_ram_ = std::vector<uint8_t>(kDoab ? 0x8000 : 0);
    uint32_t comp_src_ = 0;   // m_srcoffset
    bool comp_busy_ = false;  // m_prot_a (doa_unk_r)
    uint8_t comp_byte();      // get_decompressed_byte
    LoopbackTransport loopback_;            // kCommAlways with no link
    Cpu *cpu_ = nullptr;
    int frame_skip_ = 0;
    uint64_t tex_generation_ = 0; // texture RAM writes so far
    Lockstep *ls_ = nullptr;

    uint32_t intreq_ = 0, intena_ = 0;
    int8_t lines_[4] = {-1, -1, -1, -1};
    uint32_t timervals_[4] = {0xfffff, 0xfffff, 0xfffff, 0xfffff}; // MAME machine_reset
    // Model 2A timers (MAME timers_w/timers_r): start, length, running
    uint64_t timer_start_[4] = {}, timer_due_[4] = {};
    uint32_t timer_orig_[4] = {};
    bool timer_run_[4] = {};
    uint64_t tick_base_ = 0, count_base_ = 0;
    uint64_t count_now() const;
    uint32_t videocontrol_ = 0, zclip_ = 0;
    bool render_mode_ = false, render_test_ = false, render_unk_ = false;
    uint8_t comm_cn_ = 0, comm_fg_ = 0;
    uint64_t frame_ = 0;
#ifdef M2_DC_SPIN_SKIP
    uint64_t spin_skipped_ = 0;
#endif
#ifdef M2_FAST_BUS
    // The last page read from plain memory (RAM, texture RAM) and the last
    // plain RAM page written (no video register behind it: ram_written does
    // nothing there), straight through their base. The map never changes
    // after the constructor, so neither goes stale.
    uint32_t fast_read_page_ = ~0u, fast_write_page_ = ~0u;
    uint8_t *fast_read_base_ = nullptr, *fast_write_base_ = nullptr;
    static bool plain_ram(uint32_t addr) {
        return !((addr >= 0x01000000 && addr <= 0x011fffff) || (addr >= 0x01800000 && addr <= 0x0181bfff) ||
                 (kModel2b && addr >= 0x11000000 && addr <= 0x113fffff));
    }
    void fast_read(uint32_t addr, const Page &p) { fast_read_page_ = addr >> kPageBits, fast_read_base_ = p.base; }
    void fast_write(uint32_t addr, const Page &p) {
        if (plain_ram(addr)) fast_write_page_ = addr >> kPageBits, fast_write_base_ = p.base;
    }
#endif

    // i8251 UART to the sound board (transmit side)
    bool uart_txrdy_ = true, uart_shift_busy_ = false, uart_have_hold_ = false;
    uint8_t uart_hold_ = 0;
    std::vector<uint8_t> uart_out_;
    uint64_t sound_total_ = 0;
    void uart_shift_done();
};

} // namespace rt
