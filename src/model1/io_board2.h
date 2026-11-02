// The Model 1 advanced I/O board (837-10859, MAME's model1io2 with BIOS
// EPR-16891: Wing War). Its Z80 (a TMPZ84C015 at 9.8304 MHz) runs the BIOS,
// statically recompiled (tools/dsbrecomp --io2, io2_z80.h), on the board as
// MAME's model1io2.cpp builds it (BSD-3-Clause, copyright-holders: Dirk
// Best): the 315-5338A I/O controller (its serial link to the MB8421 dual-
// port RAM, the switch ports, the outputs, the EEPROM's and the board's
// control lines), the MSM6253 ADC, the 93C46 settings EEPROM (MAME's
// eepromser.cpp), and the TMPZ84C015's CTC (MAME's z80ctc.cpp: channel 1
// is the BIOS's 1 ms interrupt, channel 2 the SIO's baud clock). The V60
// sees the dual-port RAM at C00000-C00FFF, the low byte of each word.
//
// Time: the V60's in 80 MHz ticks, the Z80's in T-states, each MAME's
// attoseconds exactly (kAttoPerCycle a T-state). MAME runs the V60 first in
// each timeslice and the Z80 after it, so before a V60 access the Z80 runs
// up to the start of the V60's slice (slice_start, from the board): the V60
// sees the Z80's accesses of earlier slices and the Z80 sees the V60's of
// the slice it runs in. The board's timers end MAME's timeslices (each CTC
// tick and the zero-count pulse a T-state later, the EEPROM's write cycle,
// the watchdog): latest_cut gives the board the last one before a time.
//
// See THIRD_PARTY.md.
#pragma once

#include "model1/io_board.h"
#include "model1/io2_z80.h"
#include "model1/replay_bus.h"

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace m1 {

class IoBoard2 : public DeviceModel, io2::Io {
public:
    // bios: EPR-16891 (64 KiB; the Z80 sees its first 32 KiB); eeprom: the
    // 93C46's 64 words as MAME's NVRAM keeps them (big-endian), or empty
    IoBoard2(const std::vector<uint8_t> &bios, const std::vector<uint8_t> &eeprom);
    IoBoard2(const IoBoard2 &) = delete;
    IoBoard2 &operator=(const IoBoard2 &) = delete;

    static constexpr int64_t kAttoPerCycle = 101725260416; // MAME: 1 s / 9,830,400 Hz, rounded down
    static constexpr int64_t kAttoPerTick = 12500000000;
    // The V60's time (ticks), and the start of its timeslice at a time (attoseconds)
    void set_clock(std::function<uint64_t()> now) { now_ = std::move(now); }
    void set_slices(std::function<__int128(uint64_t)> slice_start) { slice_start_ = std::move(slice_start); }

    // DeviceModel: the V60's accesses to C00000-C00FFF
    uint32_t read(uint32_t a, unsigned size) override;
    void write(uint32_t a, uint32_t v, unsigned size) override;

    // The last timeslice end the board's timers make at or before time t
    // (attoseconds; 0: none)
    __int128 latest_cut(__int128 t);
    // Runs the Z80 up to time t (a time no later than the V60's slice start)
    void run_to(__int128 t);

    Controls controls;              // adc an0-an7, in0/in1/in2 (the 315-5338A's ports A-C)
    std::array<uint8_t, 128> eeprom; // MAME's NVRAM image (big-endian words)
    bool eeprom_dirty = false;
    uint8_t outputs = 0; // port D (lamps, coin counter)
    uint64_t cycles() const { return z_.cycles; }
    // Tools: each of the Z80's accesses to the dual-port RAM ('R' or 'W', the byte, the data, its T-state)
    std::function<void(char, unsigned, uint8_t, uint64_t)> on_left;
    // and the V60's ('R' or 'W', the byte, the data, its time in ticks)
    std::function<void(char, unsigned, uint8_t, uint64_t)> on_right;

private:
    // io2::Io
    uint8_t mem_r(uint16_t a, uint64_t cycles) override;
    void mem_w(uint16_t a, uint8_t v, uint64_t cycles) override;
    uint8_t in(uint16_t port, uint64_t cycles) override;
    void out(uint16_t port, uint8_t v, uint64_t cycles) override;
    bool irq(uint64_t cycles) override;
    uint8_t irq_ack(uint64_t cycles) override;
    void reti(uint64_t cycles) override;

    static __int128 at(uint64_t cycles) { return (__int128)cycles * kAttoPerCycle; }

    // 315-5338A
    uint8_t io_r(unsigned offset, uint64_t cycles);
    void io_w(unsigned offset, uint8_t v, uint64_t cycles);
    void port_out(unsigned port, uint8_t v, uint64_t cycles);
    std::array<uint8_t, 7> port_value_{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    uint8_t port_config_ = 0, serial_ = 0, cmd_ = 0;
    uint16_t address_ = 0;
    bool secondary_ = false; // port G bit 6: the ADC reads an4-an7
    uint8_t adc_shift_ = 0;
    unsigned fpga_count_ = 0;

    // 93C46 (16-bit words, 6 address bits): MAME's eeprom_serial_93cxx_device
    enum class Ee : uint8_t { Reset, StartBit, Command, Reading, WaitData, Completion };
    void ee_cs(int state, uint64_t cycles);
    void ee_clk(int state, uint64_t cycles);
    void ee_event_clk_rise(uint64_t cycles);
    void ee_set_state(Ee s, uint64_t cycles);
    int ee_do(uint64_t cycles) const;
    bool ee_ready(uint64_t cycles) const { return at(cycles) >= ee_completion_; }
    uint16_t ee_word(unsigned a) const { return uint16_t(eeprom[2 * a] << 8 | eeprom[2 * a + 1]); }
    void ee_store(unsigned a, uint16_t v);
    Ee ee_state_ = Ee::Reset;
    int ee_cs_ = 0, ee_clk_ = 0, ee_di_ = 0;
    bool ee_locked_ = true;
    unsigned ee_bits_ = 0, ee_accum_ = 0, ee_address_ = 0;
    int ee_command_ = 0; // 1 read, 2 write, 3 erase, 4 lock, 5 unlock, 6 write all, 7 erase all
    uint32_t ee_shift_ = 0;
    __int128 ee_completion_ = 0, ee_cs_rise_ = 0, ee_cut_ = 0;

    // The CTC: per channel MAME's mode, time constant and timer (the next
    // tick's T-state and its period), and the daisy chain's interrupt state
    struct Channel {
        uint8_t mode = 0x02, tconst = 0; // RESET_ACTIVE
        uint64_t next = 0, period = 0;    // period 0: stopped
        uint64_t start = 0;               // the T-state the running timer started on
        uint8_t int_state = 0;            // 1 INT (pending), 2 IEO (in service)
    };
    void ctc_w(unsigned ch, uint8_t v, uint64_t cycles);
    void ctc_to(uint64_t cycles); // the ticks at or before a T-state
    std::array<Channel, 4> ctc_{};
    uint8_t ctc_vector_ = 0;
    uint64_t wdt_due_ = 0; // the TMPZ84C015's watchdog: its timeout's T-state (0: none)

    io2::Z80 z_;
    std::array<uint8_t, 0x800> ram_{}; // the MB8421
    std::function<uint64_t()> now_;
    std::function<__int128(uint64_t)> slice_start_;
    void sync(); // the Z80 up to the V60's slice
};

} // namespace m1
