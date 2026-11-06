#include "app/controls.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace app {

namespace {
const struct {
    const char *name, *key;
} kActions[kNumActions] = {
    {"Steer left", "steer_left"}, {"Steer right", "steer_right"}, {"Accelerate", "accelerate"}, {"Brake", "brake"},
    {"Gear 1", "gear1"},          {"Gear 2", "gear2"},            {"Gear 3", "gear3"},           {"Gear 4", "gear4"},
    {"Shift up", "gear_up"},      {"Shift down", "gear_down"},    {"View 1 (red)", "view1"},    {"View 2 (blue)", "view2"},
    {"View 3 (yellow)", "view3"}, {"View 4 (green)", "view4"},    {"Coin", "coin"},              {"Start", "start"},
    {"Test", "test"},             {"Service", "service"},
};
} // namespace

const char *action_name(Action a) {
    // Virtua Cop 2 and Virtua Cop: the mouse is player 1's gun
    if (rt::kGuns) {
        if (a == Accelerate) return "Trigger (also the left mouse button)";
        if (rt::kBel && a == View1) return "Missile";
        if (a != Coin && a != Start && a != Test && a != Service) return "(unused)";
    }
    // Virtua Fighter 2: player 1's joystick on the steering and pedal keys, punch, kick and guard on views 1-3
    if (rt::kFighting) {
        static const char *const names[] = {"Left", "Right", "Up", "Down"};
        if (a <= Brake) return names[a];
        if (a == View1) return "Punch";
        if (a == View2) return "Kick";
        if (a == View3) return rt::kDynabb97 ? "Bat swing" : rt::kDoab ? "Hold" : "Guard";
        if (a != Coin && a != Start && a != Test && a != Service) return "(unused)";
    }
    // Sky Target: the stick on the steering keys (left and right) and the pedals (accelerator forward,
    // brake back); machine gun, missile and view change (MAME's skytargt ports)
    if (rt::kSkytargt) {
        if (a == SteerLeft) return "Stick left";
        if (a == SteerRight) return "Stick right";
        if (a == Accelerate) return "Stick forward";
        if (a == Brake) return "Stick back";
        if (a == View1) return "Machine gun";
        if (a == View2) return "Missile";
        if (a == View3) return "View change";
        if (a != Coin && a != Start && a != Test && a != Service) return "(unused)";
    }
    // Desert Tank: steering, accelerator and the brake channel; VR1-3, the shift lever (held),
    // machine gun and cannon on the shift buttons (MAME's desert ports)
    if (rt::kDesert) {
        if (a == GearUp) return "Machine gun";
        if (a == GearDown) return "Cannon";
        if (a == View1) return "VR1 (blue)";
        if (a == View2) return "VR2 (green)";
        if (a == View3) return "VR3 (red)";
        if (a == View4) return "Shift (held)";
        if (a >= Gear1 && a <= Gear4) return "(unused)";
    }
    // Manx TT: lean, throttle, brake and two shift buttons; Start is also VR (MAME's manxtt ports).
    // Motor Raid: the same, punch and kick for the shift buttons (MAME's motoraid ports).
    if (rt::kManxControls) {
        if (rt::kMotoraid && a == GearUp) return "Punch";
        if (rt::kMotoraid && a == GearDown) return "Kick";
        if (a == SteerLeft) return "Lean left";
        if (a == SteerRight) return "Lean right";
        if (a == Accelerate) return "Throttle";
        if ((a >= Gear1 && a <= Gear4) || (a >= View1 && a <= View4)) return "(unused)";
    }
    // INDY 500: two shift buttons and two view buttons (MAME's indy500 ports)
    if (rt::kIndyControls) {
        if (a == View1) return "View 1 (zoom in)";
        if (a == View2) return "View 2 (zoom out)";
        if ((a >= Gear1 && a <= Gear4) || a == View3 || a == View4) return "(unused)";
    }
    // Sega Ski Super G: the inclining on the steering keys, the swing on the pedals; select 1-3, zoom in and out
    // Top Skater: the curving on the steering keys, the slide on the pedals; jump front and tail, select left and right
    if (rt::kTopskatr) {
        if (a == Accelerate) return "Slide forward";
        if (a == Brake) return "Slide back";
        if (a == View1) return "Jump front";
        if (a == View2) return "Jump tail";
        if (a == GearUp) return "Select right";
        if (a == GearDown) return "Select left";
        if ((a >= Gear1 && a <= Gear4) || a == View3 || a == View4) return "(unused)";
    }
    if (rt::kSkisuprg) {
        if (a == Accelerate) return "Swing forward";
        if (a == Brake) return "Swing back";
        if (a == View1) return "Select 2";
        if (a == View2) return "Select 3";
        if (a == GearUp) return "Zoom in";
        if (a == GearDown) return "Zoom out";
        if ((a >= Gear1 && a <= Gear4) || a == View3 || a == View4) return "(unused)";
    }
    // Sega Water Ski: the slide on the steering keys; set (Start), select up and down, pitch left and right
    if (rt::kSegawski) {
        if (a == Accelerate) return "Select up";
        if (a == Brake) return "Select down";
        if (a == View1) return "Pitch left";
        if (a == View2) return "Pitch right";
        if ((a >= Gear1 && a <= Gear4) || a == View3 || a == View4) return "(unused)";
    }
    // Wave Runner: the handle bar on the steering keys, the throttle lever on the accelerator, one view button
    if (rt::kWaverunr) {
        if (a == Accelerate) return "Throttle";
        if (a == View1) return "View change";
        if (a == Brake || (a >= Gear1 && a <= Gear4) || (a >= View2 && a <= View4)) return "(unused)";
    }
    // Sega Rally: one view button (VR) and the hand brake (MAME's srallyc ports)
    if (rt::kModel2a && a == View1) return "View change";
    if (rt::kModel2a && a == View2) return "Hand brake";
    if (rt::kModel2a && (a == View3 || a == View4)) return "(unused)";
    return kActions[a].name;
}
const char *action_key(Action a) { return kActions[a].key; }

std::string PadInput::describe() const {
    switch (kind) {
    case Button: {
        const char *n = SDL_GetGamepadStringForButton(SDL_GamepadButton(index));
        return std::string("Button ") + (n ? n : "?");
    }
    case Axis: {
        const char *n = SDL_GetGamepadStringForAxis(SDL_GamepadAxis(index));
        const bool trigger = index == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || index == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        return std::string(trigger ? "Trigger " : "Axis ") + (n ? n : "?") + (trigger ? "" : (dir > 0 ? " +" : " -"));
    }
    default: return "-";
    }
}

std::string PadInput::save() const {
    switch (kind) {
    case Button: return std::string("button:") + SDL_GetGamepadStringForButton(SDL_GamepadButton(index));
    case Axis: return std::string("axis:") + SDL_GetGamepadStringForAxis(SDL_GamepadAxis(index)) + (dir > 0 ? ":+" : ":-");
    default: return "none";
    }
}

PadInput PadInput::parse(const std::string &s) {
    PadInput p;
    if (s.compare(0, 7, "button:") == 0) {
        const SDL_GamepadButton b = SDL_GetGamepadButtonFromString(s.substr(7).c_str());
        if (b != SDL_GAMEPAD_BUTTON_INVALID) p.kind = Button, p.index = b;
    } else if (s.compare(0, 5, "axis:") == 0) {
        const auto c = s.find(':', 5);
        const SDL_GamepadAxis a = SDL_GetGamepadAxisFromString(s.substr(5, c - 5).c_str());
        if (a != SDL_GAMEPAD_AXIS_INVALID) p.kind = Axis, p.index = a, p.dir = (c != std::string::npos && s[c + 1] == '-') ? -1 : 1;
    }
    return p;
}

void Devices::handle_event(const SDL_Event &e) {
    switch (e.type) {
    case SDL_EVENT_JOYSTICK_ADDED:
        if (SDL_Joystick *j = SDL_OpenJoystick(e.jdevice.which)) { // our own reference (a gamepad holds another)
            if (std::find(joys.begin(), joys.end(), j) == joys.end()) joys.push_back(j);
            else SDL_CloseJoystick(j);
        }
        break;
    case SDL_EVENT_JOYSTICK_REMOVED:
        for (auto it = joys.begin(); it != joys.end(); ++it)
            if (SDL_GetJoystickID(*it) == e.jdevice.which) {
                SDL_CloseJoystick(*it);
                joys.erase(it);
                break;
            }
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        if (!pad) pad = SDL_OpenGamepad(e.gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (pad && e.gdevice.which == SDL_GetGamepadID(pad)) SDL_CloseGamepad(pad), pad = nullptr;
        break;
    default: break;
    }
}

void Devices::close_all() {
    if (pad) SDL_CloseGamepad(pad), pad = nullptr;
    for (SDL_Joystick *j : joys) SDL_CloseJoystick(j);
    joys.clear();
}

namespace {
std::string guid_string(SDL_Joystick *j) {
    char b[64];
    SDL_GUIDToString(SDL_GetJoystickGUID(j), b, sizeof b);
    return b;
}
} // namespace

SDL_Joystick *Devices::find(const std::string &guid) const {
    for (SDL_Joystick *j : joys)
        if (guid_string(j) == guid) return j;
    return nullptr;
}

float JoyInput::value(const Devices &d, float deadzone) const {
    if (kind == None) return 0.f;
    SDL_Joystick *j = d.find(guid);
    if (!j) return 0.f;
    switch (kind) {
    case Button: return SDL_GetJoystickButton(j, index) ? 1.f : 0.f;
    case Hat: return (SDL_GetJoystickHat(j, index) & mask) ? 1.f : 0.f;
    case Axis: {
        if (full == rest) return 0.f;
        const float x = std::clamp(float(SDL_GetJoystickAxis(j, index) - rest) / float(full - rest), 0.f, 1.f);
        return x <= deadzone ? 0.f : (x - deadzone) / (1.f - deadzone);
    }
    default: return 0.f;
    }
}

std::string JoyInput::describe(const Devices &d) const {
    if (kind == None) return "-";
    SDL_Joystick *j = d.find(guid);
    std::string what;
    if (kind == Button) what = "Button " + std::to_string(index + 1);
    else if (kind == Hat) what = std::string("Hat ") + (mask == SDL_HAT_UP ? "up" : mask == SDL_HAT_DOWN ? "down" : mask == SDL_HAT_LEFT ? "left" : "right");
    else what = "Axis " + std::to_string(index + 1) + (full > rest ? " +" : " -");
    const char *name = j ? SDL_GetJoystickName(j) : nullptr;
    return what + (j ? std::string(" (") + (name ? name : "joystick") + ")" : " (not connected)");
}

std::string JoyInput::save() const {
    switch (kind) {
    case Button: return "button:" + guid + ":" + std::to_string(index);
    case Hat: return "hat:" + guid + ":" + std::to_string(index) + ":" + std::to_string(mask);
    case Axis: return "axis:" + guid + ":" + std::to_string(index) + ":" + std::to_string(rest) + ":" + std::to_string(full);
    default: return "none";
    }
}

JoyInput JoyInput::parse(const std::string &s) {
    JoyInput j;
    std::vector<std::string> f;
    for (size_t at = 0;;) {
        const size_t c = s.find(':', at);
        f.push_back(s.substr(at, c - at));
        if (c == std::string::npos) break;
        at = c + 1;
    }
    auto num = [&](size_t i) { return i < f.size() ? std::atoi(f[i].c_str()) : 0; };
    if (f.size() >= 3 && f[0] == "button") j.kind = Button, j.guid = f[1], j.index = num(2);
    else if (f.size() >= 4 && f[0] == "hat") j.kind = Hat, j.guid = f[1], j.index = num(2), j.mask = num(3);
    else if (f.size() >= 5 && f[0] == "axis") j.kind = Axis, j.guid = f[1], j.index = num(2), j.rest = num(3), j.full = num(4);
    return j;
}

void Controls::set_defaults() {
    auto b = [&](Action a, SDL_Scancode k, PadInput::Kind kind, int index, int dir = 1) {
        bind[a].key = k;
        bind[a].pad.kind = kind;
        bind[a].pad.index = index;
        bind[a].pad.dir = dir;
        bind[a].joy = JoyInput{};
    };
    b(SteerLeft, SDL_SCANCODE_LEFT, PadInput::Axis, SDL_GAMEPAD_AXIS_LEFTX, -1);
    b(SteerRight, SDL_SCANCODE_RIGHT, PadInput::Axis, SDL_GAMEPAD_AXIS_LEFTX, +1);
    b(Accelerate, SDL_SCANCODE_UP, PadInput::Axis, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    b(Brake, SDL_SCANCODE_DOWN, PadInput::Axis, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    b(Gear1, SDL_SCANCODE_1, PadInput::None, 0);
    b(Gear2, SDL_SCANCODE_2, PadInput::None, 0);
    b(Gear3, SDL_SCANCODE_3, PadInput::None, 0);
    b(Gear4, SDL_SCANCODE_4, PadInput::None, 0);
    b(GearUp, SDL_SCANCODE_W, PadInput::Button, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    b(GearDown, SDL_SCANCODE_Q, PadInput::Button, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    b(View1, SDL_SCANCODE_A, PadInput::Button, SDL_GAMEPAD_BUTTON_SOUTH);
    b(View2, SDL_SCANCODE_S, PadInput::Button, SDL_GAMEPAD_BUTTON_EAST);
    b(View3, SDL_SCANCODE_D, PadInput::Button, SDL_GAMEPAD_BUTTON_WEST);
    b(View4, SDL_SCANCODE_F, PadInput::Button, SDL_GAMEPAD_BUTTON_NORTH);
    b(Coin, SDL_SCANCODE_5, PadInput::Button, SDL_GAMEPAD_BUTTON_BACK);
    b(Start, SDL_SCANCODE_RETURN, PadInput::Button, SDL_GAMEPAD_BUTTON_START);
    b(Test, SDL_SCANCODE_F2, PadInput::None, 0);
    b(Service, SDL_SCANCODE_F3, PadInput::None, 0);
    deadzone = 0.08f;
    joy_deadzone = 0.02f;
    wheel_sensitivity = 1.0f;
    steer_invert = false;
}

float Controls::value(Action a, const bool *keys, const Devices &d) const {
    const Binding &b = bind[a];
    SDL_Gamepad *pad = d.pad;
    float v = (b.key != SDL_SCANCODE_UNKNOWN && keys && keys[b.key]) ? 1.f : 0.f;
    if (pad && b.pad.kind == PadInput::Button && SDL_GetGamepadButton(pad, SDL_GamepadButton(b.pad.index))) v = 1.f;
    if (pad && b.pad.kind == PadInput::Axis) {
        float x = SDL_GetGamepadAxis(pad, SDL_GamepadAxis(b.pad.index)) / 32767.f * float(b.pad.dir);
        x = std::clamp(x, 0.f, 1.f);
        x = x <= deadzone ? 0.f : (x - deadzone) / (1.f - deadzone); // rescale past the dead zone: full travel still reaches 1
        v = std::max(v, x);
    }
    float joy = b.joy.value(d, joy_deadzone);
    if ((a == SteerLeft || a == SteerRight) && b.joy.kind == JoyInput::Axis) {
        const float sensitivity = std::isfinite(wheel_sensitivity)
            ? std::clamp(wheel_sensitivity, 0.25f, 3.0f) : 1.0f;
        // Apply after calibration/dead zone, preserving centre and full lock.
        if (sensitivity != 1.0f) joy = std::pow(joy, 1.0f / sensitivity);
    }
    return std::max(v, joy);
}

bool Controls::analog_source(Action a, const Devices &d) const {
    return (d.pad && bind[a].pad.kind == PadInput::Axis) || (bind[a].joy.kind == JoyInput::Axis && d.find(bind[a].joy.guid));
}

rt::Inputs Controls::sample(const bool *keys, const Devices &d) {
    rt::Inputs in;
    if (rt::kGuns) { // the guns aim with the mouse (main.cpp); Accelerate also fires player 1's
        auto low = [&](uint8_t &port, uint8_t bit, Action a) { if (value(a, keys, d) > 0.5f) port &= uint8_t(~bit); };
        low(in.in0, 0x01, Coin);
        low(in.in0, rt::kIn0Test, Test);
        low(in.in0, rt::kIn0Service, Service);
        low(in.in0, rt::kIn0Start, Start);
        low(in.in1, 0x01, Accelerate);
        if (rt::kBel) low(in.in1, 0x10, View1); // the missile
        return in;
    }
    if (rt::kFighting) { // player 1 (MAME's model2 IN1: punch, kick, guard, then down, up, right, left)
        auto low = [&](uint8_t &port, uint8_t bit, Action a) { if (value(a, keys, d) > 0.5f) port &= uint8_t(~bit); };
        low(in.in0, 0x01, Coin);
        low(in.in0, 0x04, Test);
        low(in.in0, 0x08, Service);
        low(in.in0, rt::kIn0Start, Start);
        low(in.in1, rt::kDoab ? 0x02 : 0x01, View1); // Dead or Alive: hold, punch, kick on bits 0-2
        low(in.in1, rt::kDoab ? 0x04 : 0x02, View2);
        if (rt::kDoab) low(in.in1, 0x01, View3);
        else if (rt::kDynabb97) in.bat[0] = value(View3, keys, d) > 0.5f ? 0xff : 0; // MAME's BAT1
        else low(in.in1, 0x04, View3);
        low(in.in1, 0x10, Brake);
        low(in.in1, 0x20, Accelerate);
        low(in.in1, 0x40, SteerRight);
        low(in.in1, 0x80, SteerLeft);
        return in;
    }
    // Steering: an analogue stick sets the position directly; keys ramp toward full lock and back.
    const float l = value(SteerLeft, keys, d), r = value(SteerRight, keys, d);
    float target = r - l;
    if (steer_invert) target = -target;
    const bool analog = (analog_source(SteerLeft, d) || analog_source(SteerRight, d)) &&
                        !(keys && (keys[bind[SteerLeft].key] || keys[bind[SteerRight].key]));
    steer = analog ? target : steer + std::clamp(target - steer, -0.12f, 0.12f);
    accel = value(Accelerate, keys, d);
    brake = value(Brake, keys, d);
    // ADC ranges (MAME's daytona ports): steering 0x20-0xe0 centred on 0x80, pedals 0x20 (up) to 0xe0 (floored);
    // srallyc's: steering 0x01-0xff, pedals 0x00-0xff (rt::kPedalMin/kPedalMax)
    const float lock = rt::kModel2a ? 0x7f : 0x60;
    in.steer = uint8_t(std::lround(0x80 + std::clamp(steer, -1.f, 1.f) * lock));
    in.accel = uint8_t(std::lround(rt::kPedalMin + std::clamp(accel, 0.f, 1.f) * (rt::kPedalMax - rt::kPedalMin)));
    in.brake = uint8_t(std::lround(rt::kPedalMin + std::clamp(brake, 0.f, 1.f) * (rt::kPedalMax - rt::kPedalMin)));

    if (rt::kDesert) { // MAME's desert ports: the paddle's full range; VR1-3 on IN0, shift, machine gun, cannon on IN1
        in.steer = uint8_t(std::lround(0x80 + std::clamp(steer, -1.f, 1.f) * 0x7f));
        auto low = [&](uint8_t &port, uint8_t bit, Action a) { if (value(a, keys, d) > 0.5f) port &= uint8_t(~bit); };
        low(in.in0, 0x01, Coin);
        low(in.in0, 0x04, Test);
        low(in.in0, 0x08, Service);
        low(in.in0, rt::kIn0Start, Start);
        low(in.in0, 0x20, View1);
        low(in.in0, 0x40, View2);
        low(in.in0, 0x80, View3);
        low(in.in1, 0x01, View4);
        low(in.in1, 0x10, GearUp);
        low(in.in1, 0x20, GearDown);
        return in;
    }
    if (rt::kSkytargt) { // STICKX reads lower pushed right (MAME's PORT_REVERSE), STICKY lower pushed forward
        in.stick_x = uint8_t(std::lround(0x80 - std::clamp(steer, -1.f, 1.f) * 0x7f));
        in.stick_y = uint8_t(std::lround(0x80 + std::clamp(brake - accel, -1.f, 1.f) * 0x7f));
        auto low = [&](uint8_t &port, uint8_t bit, Action a) { if (value(a, keys, d) > 0.5f) port &= uint8_t(~bit); };
        low(in.in0, 0x01, Coin);
        low(in.in0, 0x04, Test);
        low(in.in0, 0x08, Service);
        low(in.in0, 0x20, View3);
        low(in.in0, rt::kIn0Start, Start);
        low(in.in1, 0x10, View1);
        low(in.in1, 0x20, View2);
        return in;
    }
    if (rt::kTopskatr) { // the curving follows the steering, the slide the pedals (MAME's topskatr ports)
        in.steer = uint8_t(std::lround(0x80 + std::clamp(steer, -1.f, 1.f) * 0x7f));
        in.roll = uint8_t(std::lround(0x80 + std::clamp(accel - brake, -1.f, 1.f) * 0x7f));
        auto low = [&](uint8_t &port, uint8_t bit, Action a) { if (value(a, keys, d) > 0.5f) port &= uint8_t(~bit); };
        low(in.in0, 0x01, Coin);
        low(in.in0, 0x04, Test);
        low(in.in0, 0x08, Service);
        low(in.in0, 0x40, Start);
        low(in.in0, 0x20, View1);
        low(in.in1, 0x01, View2);
        low(in.in0, 0x10, GearUp);
        low(in.in0, 0x80, GearDown);
        return in;
    }
    if (rt::kSkisuprg) { // the inclining follows the steering, the swing the pedals; Start is select 1 (MAME's skisuprg ports)
        in.steer = uint8_t(std::lround(0x80 + std::clamp(steer, -1.f, 1.f) * 0x7f));
        in.pitch = uint8_t(std::lround(0x80 + std::clamp(accel - brake, -1.f, 1.f) * 0x7f));
        auto low = [&](uint8_t &port, uint8_t bit, Action a) { if (value(a, keys, d) > 0.5f) port &= uint8_t(~bit); };
        low(in.in0, 0x01, Coin);
        low(in.in0, 0x04, Test);
        low(in.in0, 0x08, Service);
        low(in.in0, 0x40, Start);
        low(in.in0, 0x80, View1);
        low(in.in0, 0x10, View2);
        low(in.in0, 0x20, GearUp);
        low(in.in1, 0x01, GearDown);
        return in;
    }
    if (rt::kSegawski) { // the slide follows the steering; Start is set (MAME's segawski ports)
        in.steer = uint8_t(std::lround(0x80 + std::clamp(steer, -1.f, 1.f) * 0x7f));
        auto low = [&](uint8_t &port, uint8_t bit, Action a) { if (value(a, keys, d) > 0.5f) port &= uint8_t(~bit); };
        low(in.in0, 0x01, Coin);
        low(in.in0, 0x04, Test);
        low(in.in0, 0x08, Service);
        low(in.in0, 0x40, Brake);
        low(in.in1, 0x01, Start);
        low(in.in1, 0x02, Accelerate);
        low(in.in1, 0x04, View1);
        low(in.in1, 0x08, View2);
        return in;
    }
    if (rt::kWaverunr) { // the handle bar and the lean follow the steering; the throttle lever reads 0xff released, 0 gripped
        in.steer = uint8_t(std::lround(0x80 + std::clamp(steer, -1.f, 1.f) * 0x7f));
        in.roll = in.steer;
        in.throttle = uint8_t(std::lround(0xff - std::clamp(accel, 0.f, 1.f) * 0xff));
        auto low = [&](uint8_t &port, uint8_t bit, Action a) { if (value(a, keys, d) > 0.5f) port &= uint8_t(~bit); };
        low(in.in0, 0x01, Coin);
        low(in.in0, 0x04, Test);
        low(in.in0, 0x08, Service);
        low(in.in0, rt::kIn0Start, Start);
        low(in.in1, 0x01, View1);
        return in;
    }
    if (rt::kManxControls) { // the bank sensor reads lower leaning right (MAME's PORT_REVERSE)
        in.bank = uint8_t(std::lround(0x80 - std::clamp(steer, -1.f, 1.f) * 0x7f));
        auto low = [&](uint8_t &port, uint8_t bit, Action a) { if (value(a, keys, d) > 0.5f) port &= uint8_t(~bit); };
        low(in.in0, 0x01, Coin);
        low(in.in0, 0x04, Test);
        low(in.in0, 0x08, Service);
        low(in.in0, rt::kIn0Start, Start);
        low(in.in1, 0x10, GearUp);
        low(in.in1, 0x20, GearDown);
        return in;
    }
    if (rt::kIndyControls) { // MAME's indy500 IN1: views 1 and 2, shift up and down
        auto low = [&](uint8_t &port, uint8_t bit, Action a) { if (value(a, keys, d) > 0.5f) port &= uint8_t(~bit); };
        low(in.in0, 0x01, Coin);
        low(in.in0, 0x04, Test);
        low(in.in0, 0x08, Service);
        low(in.in0, rt::kIn0Start, Start);
        low(in.in1, 0x01, View1);
        low(in.in1, 0x02, View2);
        low(in.in1, 0x10, GearUp);
        low(in.in1, 0x20, GearDown);
        return in;
    }

    // Gears: direct selection, or sequential shifts on the press
    for (int g = 0; g < 4; g++)
        if (value(Action(Gear1 + g), keys, d) > 0.5f) gear = g + 1;
    for (Action a : {GearUp, GearDown}) {
        const bool now = value(a, keys, d) > 0.5f;
        if (now && !held_[a]) gear = std::clamp(gear + (a == GearUp ? 1 : -1), 1, 4);
        held_[a] = now;
    }
    static const uint8_t gearvalue[5] = {0, 2, 1, 6, 5}; // MAME daytona_gearbox_r: neutral, 1-4
    in.in1 = uint8_t((in.in1 & ~0x70) | (gearvalue[gear] << 4));

    auto low = [&](uint8_t &port, uint8_t bit, Action a) { if (value(a, keys, d) > 0.5f) port &= uint8_t(~bit); };
    low(in.in0, 0x01, Coin);
    low(in.in0, 0x04, Test);
    low(in.in0, 0x08, Service);
    low(in.in0, rt::kIn0Start, Start);
    low(in.in0, 0x20, View1);
    if (rt::kModel2a) { // IN2 is the hand brake (analogue, 0 = off)
        in.handbrake = uint8_t(std::lround(std::clamp(value(View2, keys, d), 0.f, 1.f) * 0xff));
        return in;
    }
    low(in.in0, 0x40, View2);
    low(in.in0, 0x80, View3);
    low(in.in1, 0x01, View4);
    return in;
}

} // namespace app
