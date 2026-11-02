// license:BSD-3-Clause
// copyright-holders:Dirk Best, Aaron Giles, R. Belmont, Olivier Galibert, ElSemi, Angelo Salese
//
// Model 2A-CRX I/O (see io_315_5649.h). Semantics follow MAME at
// dddd73680656e355bb2b5beecab1167c9f07bf81 (BSD-3-Clause; notice above
// kept): src/mame/sega/315_5649.cpp (read/write), src/devices/machine/
// eepromser.cpp (the 93Cxx serial protocol), and model2.cpp's model2a
// wiring (eeprom_w, in0_r, srallyc's ports and analogue channels). See
// THIRD_PARTY.md.

#include "runtime/io_315_5649.h"

#include "runtime/m2_board.h"

namespace rt {

// ---------------------------------------------------------------------------
// 93C46, 16-bit: 6 address bits, 2 command bits after the start bit.

namespace {
constexpr uint32_t kAddressBits = 6, kDataBits = 16;
}

void Eeprom93c46::set_word(uint32_t a, uint16_t v) {
    a &= 63;
    if (word(a) == v) return;
    mem_[2 * a] = uint8_t(v);
    mem_[2 * a + 1] = uint8_t(v >> 8);
    dirty_ = true;
}

void Eeprom93c46::cs_write(bool state) {
    if (state == cs_) return;
    cs_ = state;
    if (cs_) {
        if (state_ == InReset) state_ = WaitStart;
    } else {
        state_ = InReset; // a falling CS ends every state
    }
}

void Eeprom93c46::clk_write(bool state) {
    if (state == clk_) return;
    clk_ = state;
    if (clk_) clk_rising();
}

void Eeprom93c46::clk_rising() {
    switch (state_) {
    case WaitStart:
        if (di_ && ready()) {
            accum_ = bits_ = 0;
            state_ = WaitCommand;
        }
        break;
    case WaitCommand:
        accum_ = (accum_ << 1) | (di_ ? 1 : 0);
        if (++bits_ == 2 + kAddressBits) execute_command();
        break;
    case Reading: {
        // a dummy 0, then the word, MSB first; then 1s (no streaming)
        const uint32_t bit = bits_++;
        if (bit == 0) shift_ = uint32_t(word(address_)) << (32 - kDataBits);
        else shift_ = (shift_ << 1) | 1;
        break;
    }
    case WaitData:
        shift_ = (shift_ << 1) | (di_ ? 1 : 0);
        if (++bits_ == kDataBits) {
            if (!locked_) {
                if (command_ == Write) set_word(address_, uint16_t(shift_)), busy_for(1750);
                else { // WriteAll: ANDed into the cells (ERASEALL comes first)
                    for (uint32_t a = 0; a < 64; a++) set_word(a, uint16_t(word(a) & shift_));
                    busy_for(8000);
                }
            }
            state_ = locked_ ? InReset : WaitCompletion;
        }
        break;
    default: break;
    }
}

void Eeprom93c46::execute_command() {
    address_ = accum_ & ((1u << kAddressBits) - 1);
    switch (accum_ >> kAddressBits) {
    case 0:
        switch (address_ >> (kAddressBits - 2)) {
        case 0: command_ = Lock; break;
        case 1: command_ = WriteAll; break;
        case 2: command_ = EraseAll; break;
        default: command_ = Unlock; break;
        }
        address_ = 0;
        break;
    case 1: command_ = Write; break;
    case 2: command_ = Read; break;
    default: command_ = Erase; break;
    }
    bits_ = 0;
    switch (command_) {
    case Read: shift_ = 0; state_ = Reading; break;
    case Write:
    case WriteAll: shift_ = 0; state_ = WaitData; break;
    case Erase:
        if (locked_) { state_ = InReset; break; }
        set_word(address_, 0xffff);
        busy_for(1000);
        state_ = WaitCompletion;
        break;
    case EraseAll:
        if (locked_) { state_ = InReset; break; }
        for (uint32_t a = 0; a < 64; a++) set_word(a, 0xffff);
        busy_for(8000);
        state_ = WaitCompletion;
        break;
    case Lock: locked_ = true; state_ = InReset; break;
    case Unlock: locked_ = false; state_ = InReset; break;
    }
}

bool Eeprom93c46::do_read() const {
    // MAME: READY/BUSY while waiting for a start bit; the shifter while
    // reading; otherwise the tristated line reads 1
    if (state_ == Reading) return (shift_ & 0x80000000u) != 0;
    if (state_ == WaitStart) return ready();
    return true;
}

// ---------------------------------------------------------------------------
// 315-5649

Io5649::Io5649(IoBoard &io) : io_(io), eeprom_(io.eeprom, io.eeprom_dirty) {}

uint8_t Io5649::port_in(uint32_t port) {
    const Inputs &in = io_.inputs;
    switch (port) {
    case 1: // model2 in0_r: with port A bit 0 set, bit 5 is the EEPROM's DO
        if (ctrlmode_) return uint8_t(0xc0 | (eeprom_.do_read() ? 0x20 : 0) | 0x10 | (in.in0 & 0x0f));
        return in.in0;
    case 2: return in.in1;       // IN1: gears in bits 4-6 (Manx TT: shift up 4, down 5)
    case 3: // rchase2: the drive board's answer to its command (MAME rchase2_drive_board_r)
        if (kRchase2) {
            const uint8_t c = drive_command_;
            return uint8_t(0xff & ~(c == 0xe0 || c == 0x0e ? 1 : 0) & ~(c == 0xd0 || c == 0x0d ? 2 : 0) &
                           ~(c == 0xb0 || c == 0x0b ? 4 : 0) & ~(c == 0x70 || c == 0x07 ? 8 : 0));
        }
        return kSegaRally ? in.handbrake : in.in2; // IN2: srallyc's hand brake (analogue, 0 = off)
    case 6: return 0xff;         // SW: DIP switches, all off
    default: return 0xff;        // no input wired
    }
}

void Io5649::port_out(uint32_t port, uint8_t data) {
    switch (port) {
    case 0: // model2 eeprom_w
        ctrlmode_ = data & 1;
        eeprom_.di_write((data >> 5) & 1);
        eeprom_.clk_write((data >> 7) & 1);
        eeprom_.cs_write((data >> 6) & 1);
        break;
    case 4: // srallyc: the drive board's command (MAME drive_board_w); rchase2's (rchase2_drive_board_w)
        if (kRchase2) { drive_command_ = data; break; }
        if (io_.drive_commands.size() < IoBoard::kMaxDrive) io_.drive_commands.push_back(data);
        break;
    default: break; // port F: lamps
    }
}

uint8_t Io5649::input_bits(uint32_t offset) const {
    offset &= 0x1f;
    if (offset == 0x0f) return 0xff; // the analogue inputs
    if (offset == 0x0c && kGuns5649) return 0xff; // the guns
    if (offset > 6 || !((port_config_ >> offset) & 1)) return 0;
    switch (offset) {
    case 1: return ctrlmode_ ? 0x0f : 0xff; // in0_r: DO and fixed bits in control mode
    case 3: return kRchase2 ? 0 : 0xff;    // IN2 (rchase2: the drive board, no controls)
    case 2: return 0xff;                    // IN1
    default: return 0;                      // DIP switches, unwired ports
    }
}

// MAME model2_state::lightgun_mux_r: bytes 0-7 the positions (P1_Y, P1_X,
// P2_Y, P2_X, low byte first); 8 and up the off-screen bits (lightgun_
// offscreen_r's low byte: bit 0 player 1, bit 1 player 2, within 5% of the
// ports' range of an edge, the margin truncated to whole steps).
uint8_t Io5649::gun_r() const {
    const Inputs &in = io_.inputs;
    const uint16_t v[4] = {in.gun_y[0], in.gun_x[0], in.gun_y[1], in.gun_x[1]};
    if (gun_mux_ < 8) {
        const uint16_t d = v[gun_mux_ >> 1];
        return uint8_t(gun_mux_ & 1 ? d >> 8 : d);
    }
    return gun_offscreen(in);
}

uint8_t Io5649::read(uint32_t offset) {
    offset &= 0x1f;
    switch (offset) {
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06:
        // (port G's counter mode, mode bit 7, is not used by Sega Rally)
        return (port_config_ >> offset) & 1 ? port_in(offset) : port_value_[offset];
    case 0x0c: if (kGuns5649) return gun_r(); return 0; // Virtua Cop 2: the guns (serial channel 2)
    case 0x0b: return 0;    // serial input: nothing connected (MAME's callbacks default to 0)
    case 0x0d: return 0x0c;            // RS-422 status (MAME's: receive buffers full, transmit empty)
    case 0x0f: {                       // analogue input, auto-increments
        const Inputs &in = io_.inputs;
        // srallyc: steering, accelerator, brake; manxtt: throttle, brake, bank
        uint8_t v = 0xff;
        if (kSegawski) v = analog_channel_ == 0 ? in.steer : 0xff; // MAME's SLIDE
        else if (kTopskatr) v = analog_channel_ == 0 ? in.steer : analog_channel_ == 1 ? in.roll : 0xff; // MAME's CURVING, SLIDE
        else if (kSkisuprg) v = analog_channel_ == 0 ? in.pitch : analog_channel_ == 1 ? in.steer : 0xff; // MAME's SWING, INCLINING
        else if (kWaverunr) { // MAME's HANDLE, ROLL, THROTTLE, PITCH
            const uint8_t a[4] = {in.steer, in.roll, in.throttle, in.pitch};
            v = analog_channel_ < 4 ? a[analog_channel_] : 0xff;
        } else if (kDynabb97) v = analog_channel_ < 2 ? in.bat[analog_channel_] : 0xff; // MAME's BAT1, BAT2
        else if (kFighting) v = 0xff; // vf2, fvipers: nothing wired (MAME's callbacks default to 0xff)
        else if (kRchase2) { // MAME's rchase2: P2_X, P1_X, P2_Y, P1_Y
            const uint16_t g[4] = {in.gun_x[1], in.gun_x[0], in.gun_y[1], in.gun_y[0]};
            v = analog_channel_ < 4 ? uint8_t(g[analog_channel_]) : 0xff;
        }
        else if (kTurrets) { // MAME's gunblade, bel: P1_X, P2_X, P1_Y, P2_Y
            const uint16_t g[4] = {in.gun_x[0], in.gun_x[1], in.gun_y[0], in.gun_y[1]};
            v = analog_channel_ < 4 ? uint8_t(g[analog_channel_]) : 0xff;
        }
        else if (kSkytargt) v = analog_channel_ == 0 ? in.stick_y : analog_channel_ == 2 ? in.stick_x : 0xff;
        else if (kManxControls) v = analog_channel_ == 0 ? in.accel : analog_channel_ == 1 ? in.brake : analog_channel_ == 2 ? in.bank : 0xff;
        else v = analog_channel_ == 0 ? in.steer : analog_channel_ == 1 ? in.accel : analog_channel_ == 2 ? in.brake : 0xff;
        analog_channel_ = (analog_channel_ + 1) & 7;
        return v;
    }
    default: return 0xff;
    }
}

void Io5649::write(uint32_t offset, uint8_t data) {
    offset &= 0x1f;
    switch (offset) {
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06:
        port_value_[offset] = data;
        port_out(offset, data);
        break;
    case 0x08: port_config_ = data; break; // direction: 1 = input
    case 0x0e: mode_ = data; break;
    case 0x0f: analog_channel_ = data & 7; break;
    case 0x0a: gun_mux_ = data; break; // Virtua Cop 2: which gun byte serial channel 2 reads
    default: break; // RS-422 output: nothing connected
    }
}

} // namespace rt
