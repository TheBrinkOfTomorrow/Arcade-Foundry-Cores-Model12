// The Model 1 I/O board (837-8950, MAME's model1io with BIOS epr14869c) as
// the V60 sees it: the MB8421 dual-port RAM at C00000-C00FFF, its low byte
// on each 16-bit word (index = (address - C00000) / 2). The board's Z80 is
// not run; its BIOS's side of the RAM is modelled from Virtua Racing's
// traffic under MAME:
//   - at reset the BIOS copies the 128-byte settings EEPROM to bytes
//     0x100-0x17f (the game reads them before it sends any command);
//   - the game writes a command to byte 0x20 and polls it until it is 0.
//     1: latch the controls: bytes 0-7 the ADC channels (Controls::adc), 8-14
//     the switch ports (IN.0, IN.1, the rest unused, 0xff). 2: store bytes 0x100-0x17f to the EEPROM. 3: load
//     them from it (Daytona's board, the same BIOS family, has both);
//   - byte 0x11 is the drive board's command (force feedback), queued for
//     the host.
// Untimed, ours completes a command at once. Timed (set_clock), the board
// answers nothing before its BIOS has started: under MAME the first command
// (written about 1.5 frames earlier) completes at kReadyTicks after reset;
// later ones complete before the game looks.
//
// EPR-14869B (Virtua Fighter, Star Wars Arcade; set_bios_b) is timed
// differently, and ours runs its main loop (Z80 code at 004D) as accesses on
// the board clock, at the times MAME's Z80 makes them (measured, to a
// quarter microsecond; the Z80 runs at 4 MHz with no waits, so they never
// vary): each pass writes byte 0x21 (the EEPROM words written, 0x40 when
// all are) and reads the command. 1 writes the controls over 7 ms; 2 reads
// bytes 0x100-0x17f into the BIOS's copy over 37 ms, then the idle passes
// write one EEPROM word each (2.6 ms); 3 clears the command at once and then
// writes the copy back over 31 ms. The V60 sees a write once MAME's
// scheduler would: from the next one-microsecond slice (the V60 runs first
// in each).
#pragma once

#include "model1/replay_bus.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace m1 {

struct Controls {
    // The ADC channels (an0-an7): Virtua Racing's wheel, accelerator and
    // brake; Star Wars Arcade's stick X and Y, throttle, (an3 unused) and
    // player 2's stick X and Y. An unconnected channel reads 0xff.
    std::array<uint8_t, 8> adc{0x80, 0x01, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff}; // MAME's vr ports at rest
    uint8_t in0 = 0xff, in1 = 0xff, in2 = 0xff; // switches, active low (IN.0-2; IN.2: Virtua Fighter's player 2)
    // The set's controls at rest (MAME's port defaults)
    static Controls at_rest(const std::string &set);
};

class IoBoard : public DeviceModel {
public:
    explicit IoBoard(const std::vector<uint8_t> &eeprom);
    // MAME: the first read of the finished command comes 2,507,400 V60
    // cycles (9.01 frames) after reset; in 80 MHz ticks.
    static constexpr uint64_t kReadyTicks = 2507400ull * 5;
    void set_clock(std::function<uint64_t()> now) { now_ = std::move(now); }
    // The board runs EPR-14869B (timed only)
    void set_bios_b() { bios_b_ = true; }

    // After the host replaced `eeprom` (a saved copy): the BIOS's copy at reset.
    void reload_settings();

    uint8_t ram_r(uint32_t index) const { return ram_[index & 0x7ff]; }
    void ram_w(uint32_t index, uint8_t v);

    // DeviceModel: the V60's accesses to C00000-C00FFF (a 32-bit one is two
    // 16-bit ones, low half first)
    uint32_t read(uint32_t a, unsigned size) override;
    void write(uint32_t a, uint32_t v, unsigned size) override;

    Controls controls;
    std::array<uint8_t, 128> eeprom; // in the RAM's byte order
    bool eeprom_dirty = false;
    std::vector<uint8_t> drive_commands; // written to byte 0x11 since the host last took them
    static constexpr size_t kMaxDrive = 256;

private:
    void command(uint8_t v);
    void settle(); // timed: a command written before the BIOS was up completes once it is
    std::array<uint8_t, 0x800> ram_{};
    std::function<uint64_t()> now_;
    bool ready_ = false;
    int pending_ = -1;

    // EPR-14869B's main loop: where it is, and when its next access is
    void bios_b_to(uint64_t t); // its accesses MAME's V60 at time t would see done
    void bios_b_step();
    enum class Step : uint8_t { Status, Command, Latch, Store, Load };
    bool bios_b_ = false;
    Step step_ = Step::Status;
    uint64_t at_ = 9659560, base_ = 0; // ticks: the next access (first, the status write at 120.7445 ms); the command's read
    unsigned i_ = 0;             // the access within a command
    uint8_t written_ = 0x40;     // the BIOS's 4080: EEPROM words written
    std::array<uint8_t, 128> copy_{}; // the BIOS's copy of the settings (4000-407F)
};

} // namespace m1
