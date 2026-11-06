// The app's mapping (app::Controls::sample) from the front end's controls to
// the board's ports. Switches are tables: one row per (port, bit, button),
// the bit pulled low while the button is held. A row whose button is 0 never
// fires, which is how a set leaves out a switch its cabinet doesn't have.
// The analogue inputs (wheel, pedals, sticks, guns) follow the tables.
#pragma once

#include "afcore.h"
#include "runtime/m2_board.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace m2core {

struct Switch {
    uint8_t rt::Inputs::*port;
    uint8_t bit;
    uint32_t button;
    bool player2 = false; // read from afcore_input::buttons2
};

using rt::Inputs;
constexpr uint32_t only(bool on, uint32_t button) { return on ? button : 0; }

// Two light guns: start 1 and 2 on IN0 (MAME's model2 ports), triggers on IN1.
// Behind Enemy Lines: player 1's missile on VIEW3 (the front ends' West or X; B reloads there) or BUTTON2.
inline constexpr Switch kGunSwitches[] = {
    {&Inputs::in0, 0x01, AFCORE_COIN},     {&Inputs::in0, rt::kIn0Test, AFCORE_TEST},
    {&Inputs::in0, rt::kIn0Service, AFCORE_SERVICE},
    {&Inputs::in0, 0x10, AFCORE_START},    {&Inputs::in0, 0x20, AFCORE_START2},
    {&Inputs::in1, 0x01, AFCORE_TRIGGER1}, {&Inputs::in1, 0x02, AFCORE_TRIGGER2},
    {&Inputs::in1, 0x10, only(rt::kBel, AFCORE_VIEW3)}, {&Inputs::in1, 0x10, only(rt::kBel, AFCORE_BUTTON2)},
};

// Two players' punch, kick, guard and joystick on IN1 and IN2 (MAME's model2 ports).
// Dead or Alive: GUARD holds, its bits differ. Dynamite Baseball 97 has no
// third button: guard swings the bat instead (see board_inputs).
inline constexpr uint8_t kPunchBit = rt::kDoab ? 0x02 : 0x01, kKickBit = rt::kDoab ? 0x04 : 0x02,
                         kGuardBit = rt::kDoab ? 0x01 : 0x04;
#define M2_FIGHT_PLAYER(port, p2)                                                                       \
    {&Inputs::port, kPunchBit, AFCORE_PUNCH, p2}, {&Inputs::port, kKickBit, AFCORE_KICK, p2},           \
        {&Inputs::port, kGuardBit, only(!rt::kDynabb97, AFCORE_GUARD), p2},                            \
        {&Inputs::port, 0x10, AFCORE_DOWN, p2}, {&Inputs::port, 0x20, AFCORE_UP, p2},                   \
        {&Inputs::port, 0x40, AFCORE_RIGHT, p2}, {&Inputs::port, 0x80, AFCORE_LEFT, p2}
inline constexpr Switch kFightingSwitches[] = {
    {&Inputs::in0, 0x01, AFCORE_COIN},  {&Inputs::in0, 0x04, AFCORE_TEST},   {&Inputs::in0, 0x08, AFCORE_SERVICE},
    {&Inputs::in0, 0x10, AFCORE_START}, {&Inputs::in0, 0x20, AFCORE_START2},
    M2_FIGHT_PLAYER(in1, false),        M2_FIGHT_PLAYER(in2, true),
};
#undef M2_FIGHT_PLAYER

// Desert Tank (MAME's desert ports): VR1-3 on IN0, the shift lever, machine gun and cannon on IN1.
inline constexpr Switch kDesertSwitches[] = {
    {&Inputs::in0, 0x01, AFCORE_COIN},           {&Inputs::in0, 0x04, AFCORE_TEST},
    {&Inputs::in0, 0x08, AFCORE_SERVICE},        {&Inputs::in0, rt::kIn0Start, AFCORE_START},
    {&Inputs::in0, 0x20, AFCORE_VIEW1},          {&Inputs::in0, 0x40, AFCORE_VIEW2},
    {&Inputs::in0, 0x80, AFCORE_VIEW3},          {&Inputs::in1, 0x01, AFCORE_VIEW4},
    {&Inputs::in1, 0x10, AFCORE_SHIFT_UP},       {&Inputs::in1, 0x20, AFCORE_SHIFT_DOWN},
};

// Sky Target: Star Wars Arcade's buttons.
inline constexpr Switch kSkytargtSwitches[] = {
    {&Inputs::in0, 0x01, AFCORE_COIN},     {&Inputs::in0, 0x04, AFCORE_TEST},
    {&Inputs::in0, 0x08, AFCORE_SERVICE},  {&Inputs::in0, 0x20, AFCORE_BUTTON3}, // view change
    {&Inputs::in0, rt::kIn0Start, AFCORE_START},
    {&Inputs::in1, 0x10, AFCORE_TRIGGER1}, // machine gun
    {&Inputs::in1, 0x20, AFCORE_BUTTON2},  // missile
};

// Top Skater.
inline constexpr Switch kTopskatrSwitches[] = {
    {&Inputs::in0, 0x01, AFCORE_COIN},       {&Inputs::in0, 0x04, AFCORE_TEST},
    {&Inputs::in0, 0x08, AFCORE_SERVICE},    {&Inputs::in0, 0x40, AFCORE_START},
    {&Inputs::in0, 0x20, AFCORE_VIEW1},      // jump front
    {&Inputs::in1, 0x01, AFCORE_VIEW2},      // jump tail
    {&Inputs::in0, 0x80, AFCORE_SHIFT_DOWN}, // select left
    {&Inputs::in0, 0x10, AFCORE_SHIFT_UP},   // select right
    {&Inputs::in0, 0x80, AFCORE_LEFT},       {&Inputs::in0, 0x10, AFCORE_RIGHT},
};

// Sega Ski Super G: no start button, select 1 (START too).
inline constexpr Switch kSkisuprgSwitches[] = {
    {&Inputs::in0, 0x01, AFCORE_COIN},     {&Inputs::in0, 0x04, AFCORE_TEST},
    {&Inputs::in0, 0x08, AFCORE_SERVICE},
    {&Inputs::in0, 0x40, AFCORE_START},    // select 1
    {&Inputs::in0, 0x40, AFCORE_VIEW1},
    {&Inputs::in0, 0x80, AFCORE_VIEW2},    // select 2
    {&Inputs::in0, 0x10, AFCORE_VIEW3},    // select 3
    {&Inputs::in0, 0x20, AFCORE_SHIFT_UP}, // zoom in
    {&Inputs::in1, 0x01, AFCORE_SHIFT_DOWN}, // zoom out
};

// Sega Water Ski: no start button, set (START or VIEW1) starts and confirms.
inline constexpr Switch kSegawskiSwitches[] = {
    {&Inputs::in0, 0x01, AFCORE_COIN},  {&Inputs::in0, 0x04, AFCORE_TEST}, {&Inputs::in0, 0x08, AFCORE_SERVICE},
    {&Inputs::in0, 0x40, AFCORE_DOWN},  // select down
    {&Inputs::in1, 0x01, AFCORE_START}, // set
    {&Inputs::in1, 0x01, AFCORE_VIEW1},
    {&Inputs::in1, 0x02, AFCORE_UP},    // select up
    {&Inputs::in1, 0x04, AFCORE_VIEW3}, // pitch left (the front ends' West button)
    {&Inputs::in1, 0x08, AFCORE_VIEW2}, // pitch right (East)
};

// Wave Runner.
inline constexpr Switch kWaverunrSwitches[] = {
    {&Inputs::in0, 0x01, AFCORE_COIN},           {&Inputs::in0, 0x04, AFCORE_TEST},
    {&Inputs::in0, 0x08, AFCORE_SERVICE},        {&Inputs::in0, rt::kIn0Start, AFCORE_START},
    {&Inputs::in1, 0x01, AFCORE_VIEW1},
};

// Manx TT and Motor Raid (punch and kick where Manx TT shifts).
inline constexpr Switch kManxSwitches[] = {
    {&Inputs::in0, 0x01, AFCORE_COIN},     {&Inputs::in0, 0x04, AFCORE_TEST},
    {&Inputs::in0, 0x08, AFCORE_SERVICE},  {&Inputs::in0, rt::kIn0Start, AFCORE_START},
    {&Inputs::in1, 0x10, rt::kMotoraid ? AFCORE_PUNCH : AFCORE_SHIFT_UP},
    {&Inputs::in1, 0x20, rt::kMotoraid ? AFCORE_KICK : AFCORE_SHIFT_DOWN},
};

// INDY 500, Super GT 24h, Over Rev, Touring Car: shift up and down, two view
// buttons on IN1 (MAME's indy500 ports; Super GT 24h has view 1 only).
inline constexpr Switch kIndySwitches[] = {
    {&Inputs::in0, 0x01, AFCORE_COIN},     {&Inputs::in0, 0x04, AFCORE_TEST},
    {&Inputs::in0, 0x08, AFCORE_SERVICE},  {&Inputs::in0, rt::kIn0Start, AFCORE_START},
    {&Inputs::in1, 0x01, AFCORE_VIEW1},    {&Inputs::in1, 0x02, AFCORE_VIEW2},
    {&Inputs::in1, 0x10, AFCORE_SHIFT_UP}, {&Inputs::in1, 0x20, AFCORE_SHIFT_DOWN},
};

// Daytona USA and Sega Rally: four view buttons (Sega Rally has VIEW1 only;
// its IN2 is the hand brake). The gearbox is set in board_inputs.
inline constexpr Switch kRacingSwitches[] = {
    {&Inputs::in0, 0x01, AFCORE_COIN},    {&Inputs::in0, 0x04, AFCORE_TEST},
    {&Inputs::in0, 0x08, AFCORE_SERVICE}, {&Inputs::in0, rt::kIn0Start, AFCORE_START},
    {&Inputs::in0, 0x20, AFCORE_VIEW1},   {&Inputs::in0, 0x40, only(!rt::kModel2a, AFCORE_VIEW2)},
    {&Inputs::in0, 0x80, only(!rt::kModel2a, AFCORE_VIEW3)},
    {&Inputs::in1, 0x01, only(!rt::kModel2a, AFCORE_VIEW4)},
};

// This set's table.
constexpr std::span<const Switch> switches() {
    if constexpr (rt::kGuns) return kGunSwitches;
    else if constexpr (rt::kFighting) return kFightingSwitches;
    else if constexpr (rt::kDesert) return kDesertSwitches;
    else if constexpr (rt::kSkytargt) return kSkytargtSwitches;
    else if constexpr (rt::kTopskatr) return kTopskatrSwitches;
    else if constexpr (rt::kSkisuprg) return kSkisuprgSwitches;
    else if constexpr (rt::kSegawski) return kSegawskiSwitches;
    else if constexpr (rt::kWaverunr) return kWaverunrSwitches;
    else if constexpr (rt::kManxControls) return kManxSwitches;
    else if constexpr (rt::kIndyControls) return kIndySwitches;
    else return kRacingSwitches;
}

// -1..1 to an ADC value centred on 0x80; reversed reads lower to the right
// (MAME's PORT_REVERSE).
inline uint8_t centred(float v, bool reversed = false) {
    const float d = std::clamp(v, -1.f, 1.f) * 0x7f;
    return uint8_t(std::lround(reversed ? 0x80 - d : 0x80 + d));
}

inline rt::Inputs board_inputs(const afcore_input &c) {
    rt::Inputs in;
    for (const Switch &s : switches())
        if ((s.player2 ? c.buttons2 : c.buttons) & s.button) in.*s.port &= uint8_t(~s.bit);

    // Analogue inputs: the wheel and pedals for every set (as the app's
    // mapping always did), then what each cabinet has instead or as well.
    const float lock = rt::kModel2a ? 0x7f : 0x60;
    const auto pedal = [](float v) {
        return uint8_t(std::lround(rt::kPedalMin + std::clamp(v, 0.f, 1.f) * (rt::kPedalMax - rt::kPedalMin)));
    };
    in.steer = uint8_t(std::lround(0x80 + std::clamp(c.steer, -1.f, 1.f) * lock));
    in.accel = pedal(c.accel);
    in.brake = pedal(c.brake);
    if constexpr (rt::kGuns) {
        for (int n = 0; n < 2; n++) rt::set_gun(in, n, c.gun_x[n], c.gun_y[n]);
    } else if constexpr (rt::kFighting) {
        if constexpr (rt::kDynabb97) { // guard swings the bat, fully
            in.bat[0] = c.buttons & AFCORE_GUARD ? 0xff : 0;
            in.bat[1] = c.buttons2 & AFCORE_GUARD ? 0xff : 0;
        }
    } else if constexpr (rt::kSkytargt) { // the flight stick: STICKX reads lower pushed right, STICKY lower pushed forward
        in.stick_x = centred(c.steer, true);
        in.stick_y = centred(c.stick_y);
    } else if constexpr (rt::kManxControls) { // the bank sensor reads lower leaning right
        in.bank = centred(c.steer, true);
    } else if constexpr (rt::kDesert || rt::kTopskatr || rt::kSkisuprg || rt::kSegawski || rt::kWaverunr) {
        in.steer = centred(c.steer);
        if constexpr (rt::kTopskatr) in.roll = centred(c.stick_y);  // the slide: the stick's forward and back
        if constexpr (rt::kSkisuprg) in.pitch = centred(c.stick_y); // the swing
        if constexpr (rt::kWaverunr) { // the rider's lean follows the steering; the throttle lever reads 0xff released, 0 gripped
            in.roll = in.steer;
            in.throttle = uint8_t(std::lround(0xff - std::clamp(c.accel, 0.f, 1.f) * 0xff));
            in.pitch = centred(c.stick_y);
        }
    } else if constexpr (!rt::kIndyControls) { // Daytona USA, Sega Rally
        static constexpr uint8_t gearvalue[5] = {0, 2, 1, 6, 5}; // MAME daytona_gearbox_r: neutral, 1-4
        in.in1 = uint8_t((in.in1 & ~0x70) | (gearvalue[std::clamp(c.gear, 0, 4)] << 4));
        if constexpr (rt::kModel2a) // IN2 is the hand brake (analogue, 0 = off)
            in.handbrake = uint8_t(std::lround(std::clamp(c.handbrake, 0.f, 1.f) * 0xff));
    }
    return in;
}

} // namespace m2core
