// Scripted inputs for the headless tools (m2run, m2gpushot): scripts/inputs.
#pragma once

#include "runtime/game_loop.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tools {

// scripts/inputs format: "frames N", "<from>-<to> name=value" or "<at> name=value".
struct Script {
    struct Line {
        uint64_t from, to;
        std::string name;
        unsigned value;
    };
    std::vector<Line> lines;
    void load(const std::string &path) {
        std::ifstream f(path);
        if (!f) throw std::runtime_error("cannot open " + path);
        std::string s;
        while (std::getline(f, s)) {
            if (s.empty() || s[0] == '#' || s.compare(0, 6, "frames") == 0) continue;
            std::istringstream in(s);
            std::string range, kv;
            in >> range >> kv;
            const auto dash = range.find('-'), eq = kv.find('=');
            if (eq == std::string::npos) continue;
            Line l;
            l.from = std::stoull(range.substr(0, dash));
            l.to = dash == std::string::npos ? l.from : std::stoull(range.substr(dash + 1));
            l.name = kv.substr(0, eq);
            l.value = unsigned(std::stoul(kv.substr(eq + 1), nullptr, 0));
            lines.push_back(l);
        }
    }
    // Virtua Fighter 2: punch, kick, guard and the joystick on IN1 (player 1)
    // and IN2 (names ending in 2, player 2); MAME's model2 layout.
    // Dead or Alive: hold, punch and kick on bits 0-2.
    static uint8_t vf2_bit(const std::string &n) {
        if (rt::kDoab) return n == "hold" ? 0x01 : n == "punch" ? 0x02 : n == "kick" ? 0x04 : n == "down" ? 0x10 : n == "up" ? 0x20 : n == "right" ? 0x40 : n == "left" ? 0x80 : 0;
        return n == "punch" ? 0x01 : n == "kick" ? 0x02 : n == "guard" ? 0x04 : n == "down" ? 0x10 : n == "up" ? 0x20 : n == "right" ? 0x40 : n == "left" ? 0x80 : 0;
    }
    rt::Inputs at(uint64_t frame) const {
        rt::Inputs in;
        static const struct {
            const char *name;
            int port; // 0: IN0, 1: IN1
            uint8_t bit;
        } buttons[] = {{"coin", 0, 0x01}, {"test", 0, 0x04}, {"service", 0, 0x08}, {"start", 0, rt::kIn0Start}, {"vr1", 0, 0x20},
                       {"vr2", 0, 0x40},  {"vr3", 0, 0x80},  {"vr4", 1, 0x01}};
        static const uint8_t gearvalue[5] = {0, 2, 1, 6, 5}; // MAME daytona_gearbox_r
        for (const Line &l : lines) {
            if (frame < l.from || frame > l.to) continue;
            if (l.name == "steer") in.steer = uint8_t(l.value);
            // Super GT 24h, Over Rev: scripts give MAME's field values; its pedals read reversed (PORT_REVERSE)
            else if (l.name == "accel" || (l.name == "throttle" && !rt::kWaverunr)) in.accel = uint8_t(rt::kReversedPedals ? 0xff - l.value : l.value); // Manx TT: throttle on ADC 0
            else if (l.name == "bank") in.bank = uint8_t(l.value);
            else if (l.name == "shiftup" && l.value) in.in1 &= uint8_t(~0x10);
            else if (l.name == "shiftdown" && l.value) in.in1 &= uint8_t(~0x20);
            else if (l.name == "view1" && l.value) in.in1 &= uint8_t(~0x01); // INDY 500
            else if (l.name == "view2" && l.value) in.in1 &= uint8_t(~0x02);
            else if (l.name == "start2" && l.value) in.in0 &= uint8_t(~0x20); // Virtua Cop 2
            else if (l.name == "trigger1" && l.value) in.in1 &= uint8_t(~0x01);
            else if (l.name == "trigger2" && l.value) in.in1 &= uint8_t(~0x02);
            // Rail Chase 2: scripts give MAME's field values; its turrets read reversed (PORT_REVERSE)
            else if (l.name == "p1x") in.gun_x[0] = uint16_t(rt::kRchase2 ? rt::kGunXMin[0] + rt::kGunXMax[0] - l.value : l.value);
            else if (l.name == "p1y") in.gun_y[0] = uint16_t(rt::kRchase2 ? rt::kGunYMin[0] + rt::kGunYMax[0] - l.value : l.value);
            else if (l.name == "p2x") in.gun_x[1] = uint16_t(rt::kRchase2 ? rt::kGunXMin[1] + rt::kGunXMax[1] - l.value : l.value);
            else if (l.name == "p2y") in.gun_y[1] = uint16_t(rt::kRchase2 ? rt::kGunYMin[1] + rt::kGunYMax[1] - l.value : l.value);
            else if (rt::kBel && l.value && (l.name == "test" || l.name == "service" || l.name == "missile" || l.name == "missile2")) // Behind Enemy Lines
                (l.name[0] == 'm' ? in.in1 : in.in0) &= uint8_t(~(l.name == "test" ? rt::kIn0Test : l.name == "service" ? rt::kIn0Service : l.name == "missile" ? 0x10 : 0x20));
            else if (l.name == "curving") in.steer = uint8_t(l.value); // Top Skater
            else if (l.name == "slide" && rt::kTopskatr) in.roll = uint8_t(l.value);
            else if (rt::kTopskatr && l.value && (l.name == "selright" || l.name == "selleft" || l.name == "jumpfront" || l.name == "jumptail"))
                (l.name == "jumptail" ? in.in1 : in.in0) &= uint8_t(~(l.name == "selright" ? 0x10 : l.name == "selleft" ? 0x80 : l.name == "jumpfront" ? 0x20 : 0x01));
            else if (l.name == "inclining") in.steer = uint8_t(l.value); // Sega Ski Super G
            else if (l.name == "swing") in.pitch = uint8_t(l.value);
            else if (rt::kSkisuprg && l.value && (l.name == "sel1" || l.name == "sel2" || l.name == "sel3" || l.name == "zoomin" || l.name == "zoomout"))
                (l.name == "zoomout" ? in.in1 : in.in0) &= uint8_t(~(l.name == "sel1" ? 0x40 : l.name == "sel2" ? 0x80 : l.name == "sel3" ? 0x10 : l.name == "zoomin" ? 0x20 : 0x01));
            else if (l.name == "slide") in.steer = uint8_t(l.value); // Sega Water Ski
            else if (rt::kSegawski && l.value && (l.name == "set" || l.name == "selup" || l.name == "seldown" || l.name == "pitchl" || l.name == "pitchr"))
                (l.name == "seldown" ? in.in0 : in.in1) &= uint8_t(~(l.name == "set" ? 0x01 : l.name == "selup" ? 0x02 : l.name == "pitchl" ? 0x04 : l.name == "pitchr" ? 0x08 : 0x40));
            else if (l.name == "handle") in.steer = uint8_t(l.value); // Wave Runner: MAME's field values
            else if (l.name == "roll") in.roll = uint8_t(l.value);
            else if (l.name == "throttle" && rt::kWaverunr) in.throttle = uint8_t(l.value); // the values the input test shows
            else if (l.name == "pitch") in.pitch = uint8_t(l.value);
            else if (l.name == "bat") in.bat[0] = uint8_t(l.value); // Dynamite Baseball 97
            else if (l.name == "bat2") in.bat[1] = uint8_t(l.value);
            else if (l.name == "brake") in.brake = uint8_t(rt::kReversedPedals ? 0xff - l.value : l.value);
            else if (const bool p2 = l.name.back() == '2'; rt::kFighting && vf2_bit(p2 ? l.name.substr(0, l.name.size() - 1) : l.name)) {
                if (l.value) (p2 ? in.in2 : in.in1) &= uint8_t(~vf2_bit(p2 ? l.name.substr(0, l.name.size() - 1) : l.name));
            }
            else if (l.name.compare(0, 4, "gear") == 0 && l.value) {
                const int g = l.name[4] - '0';
                if (g >= 0 && g < 5) in.in1 = uint8_t((in.in1 & ~0x70) | (gearvalue[g] << 4));
            } else
                for (const auto &b : buttons)
                    if (l.name == b.name && l.value) (b.port ? in.in1 : in.in0) &= uint8_t(~b.bit); // active low
        }
        return in;
    }
};

} // namespace tools
