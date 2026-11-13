#include "model1/io_board.h"

#include <algorithm>
#include <cstring>

namespace m1 {

IoBoard::IoBoard(const std::vector<uint8_t> &e) {
    eeprom.fill(0xff);
    // The 93C46's words are stored big-endian in MAME's image; the BIOS puts
    // each word's high byte first in the RAM.
    for (size_t i = 0; i + 1 < std::min(e.size(), eeprom.size()); i += 2) {
        eeprom[i] = e[i + 1];
        eeprom[i + 1] = e[i];
    }
    std::memcpy(&ram_[0x100], eeprom.data(), eeprom.size());
    copy_ = eeprom;
}

void IoBoard::reload_settings() {
    std::memcpy(&ram_[0x100], eeprom.data(), eeprom.size());
    copy_ = eeprom;
}

namespace {
// Microseconds (to a quarter) in 80 MHz ticks
constexpr uint64_t us(double v) { return uint64_t(v * 80 + 0.5); }
// The times the 315-5338A reads a byte for the Z80 (command 0C) or writes
// one (command 07), from MAME's
constexpr uint64_t kStatusToCommand = us(303.25), kIdlePass = us(221.75);
// Command 1: bytes 0-14, two reads of the outputs, byte 0x11, then the command cleared
constexpr uint64_t kLatch[19] = {us(742), us(1086.5), us(1431), us(1775.5), us(2539.75), us(2884.25), us(3228.75),
                                 us(3573.25), us(4221), us(4449), us(4677), us(5324.75), us(5552.75), us(5780.75),
                                 us(6008.75), us(6330.5), us(6623), us(6828.5), us(7059)};
constexpr uint64_t kLatchPass = us(7289.5);
constexpr uint64_t kStoreFirst = us(282.5), kStoreEach = us(286.25), kStoreDone = us(36851.5), kStorePass = us(37082);
constexpr uint64_t kLoadDone = us(210), kLoadFirst = us(444.5), kLoadEach = us(237.25), kLoadPass = us(31022.5);
// EPR-14869, timed: the command clears this long after the V60 wrote it.
// Measured under MAME by the game's polls of it. A store (2) on Virtua
// Formula's first boot (blank EEPROM): 33,539 polls, 35,635 us. A latch (1):
// 6,412 and 7,066 polls in its test mode (the only latches a game polls),
// 7,490 and 7,510 us; the BIOS's main loop sets the phase, not modelled.
constexpr uint64_t kStoreA = us(35635), kLatchA = us(7500);
// An idle pass writing an EEPROM word: the bits take 1.75 us more each one
constexpr uint64_t kWordPass = us(2619), kWordOne = us(1.75);
} // namespace

void IoBoard::bios_b_to(uint64_t t) {
    const uint64_t seen = t - t % 80;
    while (at_ < seen) bios_b_step();
}

void IoBoard::bios_b_step() {
    switch (step_) {
    case Step::Status:
        ram_[0x21] = written_;
        step_ = Step::Command;
        at_ += kStatusToCommand;
        return;
    case Step::Command:
        base_ = at_;
        i_ = 0;
        switch (ram_[0x20]) {
        case 1: step_ = Step::Latch, at_ = base_ + kLatch[0]; return;
        case 2: step_ = Step::Store, at_ = base_ + kStoreFirst; return;
        case 3: step_ = Step::Load, at_ = base_ + kLoadDone; return;
        default: break;
        }
        step_ = Step::Status;
        if (written_ < 0x40) {
            const unsigned ones = unsigned(__builtin_popcount(copy_[2 * written_] | copy_[2 * written_ + 1] << 8));
            eeprom[2 * written_] = copy_[2 * written_];
            eeprom[2 * written_ + 1] = copy_[2 * written_ + 1];
            eeprom_dirty = true;
            ++written_;
            at_ = base_ + kWordPass + kWordOne * ones;
        } else {
            at_ = base_ + kIdlePass;
        }
        return;
    case Step::Latch:
        if (i_ < 8) ram_[i_] = controls.adc[i_];
        else if (i_ == 8) ram_[8] = controls.in0;
        else if (i_ == 9) ram_[9] = controls.in1;
        else if (i_ == 10) ram_[10] = controls.in2;
        else if (i_ < 15) ram_[i_] = 0xff;
        else if (i_ == 17) ram_[0x11] = 0xff; // the drive board's reply
        else if (i_ == 18) ram_[0x20] = 0;
        if (++i_ < 19) at_ = base_ + kLatch[i_];
        else step_ = Step::Status, at_ = base_ + kLatchPass;
        return;
    case Step::Store:
        if (i_ < 128) {
            copy_[i_] = ram_[0x100 + i_];
            at_ = ++i_ < 128 ? base_ + kStoreFirst + kStoreEach * i_ : base_ + kStoreDone;
        } else {
            written_ = 0;
            ram_[0x20] = 0;
            step_ = Step::Status, at_ = base_ + kStorePass;
        }
        return;
    case Step::Load:
        if (i_ == 0) ram_[0x20] = 0;
        else ram_[0x100 + i_ - 1] = copy_[i_ - 1];
        if (++i_ <= 128) at_ = base_ + kLoadFirst + kLoadEach * (i_ - 1);
        else step_ = Step::Status, at_ = base_ + kLoadPass;
        return;
    }
}

void IoBoard::ram_w(uint32_t index, uint8_t v) {
    index &= 0x7ff;
    ram_[index] = v;
    if (index == 0x11 && drive_commands.size() < kMaxDrive) drive_commands.push_back(v);
    if (index != 0x20 || (bios_b_ && now_)) return;
    if (now_ && !ready_) {
        if (ready_at_ == UINT64_MAX) { // the first command: the BIOS takes it at its next pass
            const uint64_t t = now_(), r = (t + kPass - kPassPhase % kPass) % kPass;
            ready_at_ = t + (r ? kPass - r : 0) + kReadyAfter;
        }
        settle();
        if (!ready_) {
            pending_ = v;
            return;
        }
    }
    command(v);
}

Controls Controls::at_rest(const std::string &set) {
    Controls c;
    if (set == "swa") c.adc = {0x7f, 0x7f, 0xc8, 0xff, 0x7f, 0x7f, 0xff, 0xff}; // sticks centred, throttle closed (reversed: 0xc8)
    if (set == "vf") c.adc.fill(0xff); // no analogue controls
    if (set == "wingwar") c.adc = {0x80, 0x80, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff}; // stick centred, throttle closed
    return c;
}

void IoBoard::settle() {
    if (ready_ || now_() < ready_at_) return;
    if (pending_ >= 0) command(uint8_t(std::exchange(pending_, -1))); // completes now, untimed
    ready_ = true;
}

void IoBoard::command(uint8_t v) {
    switch (v) {
    case 1: // latch the controls
        std::copy(controls.adc.begin(), controls.adc.end(), ram_.begin());
        ram_[8] = controls.in0;
        ram_[9] = controls.in1;
        ram_[10] = controls.in2;
        for (int i = 11; i < 15; i++) ram_[size_t(i)] = 0xff;
        if (now_ && ready_) { // timed (not the first command, which completes when the BIOS is up)
            busy_until_ = now_() + kLatchA;
            return;
        }
        break;
    case 2: // store the settings
        std::memcpy(eeprom.data(), &ram_[0x100], eeprom.size());
        eeprom_dirty = true;
        if (now_ && ready_) { // timed: the command clears once the BIOS has written the EEPROM
            busy_until_ = now_() + kStoreA;
            return;
        }
        break;
    case 3: // load the settings
        std::memcpy(&ram_[0x100], eeprom.data(), eeprom.size());
        break;
    default: return;
    }
    ram_[0x20] = 0;
}

uint32_t IoBoard::read(uint32_t a, unsigned size) {
    if (bios_b_ && now_) bios_b_to(now_());
    else if (now_ && !ready_) settle();
    else if (busy_until_ && now_() >= busy_until_) busy_until_ = 0, ram_[0x20] = 0;
    const uint32_t i = (a & 0xfff) >> 1;
    if (size == 4) return ram_r(i) | uint32_t(ram_r(i + 1)) << 16;
    if (size == 1 && (a & 1)) return 0; // the high byte is not connected
    return ram_r(i);
}

void IoBoard::write(uint32_t a, uint32_t v, unsigned size) {
    if (bios_b_ && now_) bios_b_to(now_());
    const uint32_t i = (a & 0xfff) >> 1;
    if (size == 4) {
        ram_w(i, uint8_t(v));
        ram_w(i + 1, uint8_t(v >> 16));
    } else if (size == 2 || !(a & 1)) {
        ram_w(i, uint8_t(v));
    }
}

} // namespace m1
