// The controls script MAME's scripted runs use (scripts/m1_inputs.lua), for
// the Model 1 tools that set the same controls on our I/O board.
#pragma once

#include "model1/io_board.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace m1tools {

// scripts/m1_inputs.lua's script: per frame, the controls held.
struct InputLine {
    uint64_t from, to;
    std::string name;
    unsigned value;
};

inline std::vector<InputLine> read_inputs(const std::string &path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot read " + path);
    std::vector<InputLine> out;
    std::string line;
    while (std::getline(f, line)) {
        line = line.substr(0, line.find('#'));
        std::istringstream s(line);
        std::string range, kv;
        if (!(s >> range)) continue;
        const size_t dash = range.find('-');
        const uint64_t from = std::stoull(range.substr(0, dash));
        const uint64_t to = dash == std::string::npos ? from : std::stoull(range.substr(dash + 1));
        while (s >> kv) {
            const size_t eq = kv.find('=');
            out.push_back({from, to, kv.substr(0, eq), unsigned(std::stoul(kv.substr(eq + 1), nullptr, 0))});
        }
    }
    return out;
}

inline m1::Controls controls_at(const std::vector<InputLine> &script, uint64_t frame, const std::string &set = "vr") {
    m1::Controls c = m1::Controls::at_rest(set);
    static const struct {
        const char *name;
        uint8_t m1::Controls::*port;
        uint8_t mask;
    } bits[] = {{"coin", &m1::Controls::in0, 0x01},      {"test", &m1::Controls::in0, 0x04},
                {"service", &m1::Controls::in0, 0x08},   {"start", &m1::Controls::in0, 0x10},
                {"vr1", &m1::Controls::in0, 0x20},
                {"vr2", &m1::Controls::in0, 0x40},       {"vr3", &m1::Controls::in0, 0x80},
                {"vr4", &m1::Controls::in1, 0x01},       {"shiftdown", &m1::Controls::in1, 0x10},
                {"shiftup", &m1::Controls::in1, 0x20},
                // Star Wars Arcade
                {"start2", &m1::Controls::in0, 0x20},    {"trigger", &m1::Controls::in1, 0x01},
                {"button2", &m1::Controls::in1, 0x02},   {"button3", &m1::Controls::in1, 0x10},
                {"trigger2p", &m1::Controls::in1, 0x04}, {"button2p", &m1::Controls::in1, 0x08},
                // Virtua Fighter (and player 2's on IN.2)
                {"punch", &m1::Controls::in1, 0x01},     {"kick", &m1::Controls::in1, 0x02},
                {"guard", &m1::Controls::in1, 0x04},     {"down", &m1::Controls::in1, 0x10},
                {"up", &m1::Controls::in1, 0x20},        {"right", &m1::Controls::in1, 0x40},
                {"left", &m1::Controls::in1, 0x80},      {"punch2p", &m1::Controls::in2, 0x01},
                {"kick2p", &m1::Controls::in2, 0x02},    {"guard2p", &m1::Controls::in2, 0x04},
                {"down2p", &m1::Controls::in2, 0x10},    {"up2p", &m1::Controls::in2, 0x20},
                {"right2p", &m1::Controls::in2, 0x40},   {"left2p", &m1::Controls::in2, 0x80},
                // Wing War (its views are vr1-vr4)
                {"gun", &m1::Controls::in1, 0x10},       {"missile", &m1::Controls::in1, 0x20},
                {"smoke", &m1::Controls::in1, 0x40}};
    // the ADC channels by name (vr's, then swa's)
    static const struct {
        const char *name;
        int channel;
    } adcs[] = {{"wheel", 0}, {"accel", 1}, {"brake", 2},     {"stickx", 0},
                {"sticky", 1}, {"throttle", 2}, {"stick2x", 4}, {"stick2y", 5}};
    // MAME's script (scripts/m1_inputs.lua) sets the fields at its frame
    // callback: an analogue field reads the new value at once, a switch
    // takes it at the next frame's input update, a frame later.
    for (const InputLine &l : script) {
        bool known = false;
        for (const auto &a : adcs)
            if (l.name == a.name) {
                known = true;
                if (frame >= l.from && frame <= l.to) c.adc[size_t(a.channel)] = uint8_t(l.value);
            }
        if (!known) {
            for (const auto &b : bits)
                if (l.name == b.name) {
                    known = true;
                    if (frame < l.from + 1 || frame > l.to + 1) continue;
                    if (l.value) c.*b.port &= uint8_t(~b.mask);
                    else c.*b.port |= b.mask;
                }
            if (!known) throw std::runtime_error("unknown control " + l.name);
        }
    }
    return c;
}
} // namespace m1tools
