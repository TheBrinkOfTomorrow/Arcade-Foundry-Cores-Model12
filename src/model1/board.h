// The Model 1 board free-running: the recompiled V60 on the board's memory
// and devices, the TGP board, the video, the I/O board and the sound board,
// a video frame at a time. This is what the core library (src/core/m1core)
// runs; the trace harnesses (m1native) hold each part to MAME.
//
// Time is MAME's: the V60 at 8 cycles an instruction (16 MHz), plus the
// cycles it waits for the TGP (m1::Copro's timed model), in ticks of 80 MHz.
// A frame is 656 x 424 pixels at 16 MHz (278,144 V60 cycles): the vblank
// interrupt (level 1) at line 384, the screen composed at line 0 (MAME's
// VIDEO_UPDATE_AFTER_VBLANK update), 40 lines later.
#pragma once

#include "model1/comm_board.h"
#include "model1/dsb.h"
#include "model1/io_board.h"
#include "model1/io_board2.h"
#include "model1/memory.h"
#include "model1/native.h"
#include "model1/rom_set.h"
#include "model1/tgp.h"
#include "model1/video.h"
#include "runtime/sound_board.h"
#include "v60/core.h"

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

namespace m1 {

class Board : public v60::Bus {
public:
    static constexpr uint64_t kTicksPerInstruction = 8 * Copro::kTicksPerV60Cycle;
    static constexpr uint64_t kFrameTicks = 278144 * Copro::kTicksPerV60Cycle, kUpdateTicks = 40 * 656 * Copro::kTicksPerV60Cycle;
    static constexpr double kTickHz = 80e6;
    static constexpr double kFrameHz = 16000000.0 / (656 * 424); // MAME's screen: 57.5242 Hz
    // the set's images, consumed
    Board(Images img, const RomSet &set);

    // Runs to the next composed screen.
    void run_frame();
    const std::vector<uint32_t> &screen() const { return video_.screen(); }
    Video &video() { return video_; }
    const Video &video() const { return video_; }
    uint64_t frame() const { return frame_; }
    uint64_t instructions() const { return env_.count; }
    // Of those, run by the fallback interpreter (code with no recompiled version)
    uint64_t interpreted() const { return env_.interpreted; }
    size_t fallback_entries() const { return env_.seen.size(); }
    uint64_t stall_ticks() const { return stall_ticks_; }
    uint32_t pc() const { return cpu_.pc() & 0xffffff; }

    IoBoard &io() { return io_; }
    // Wing War's I/O board (nullptr: the set has the first one, io())
    IoBoard2 *io2() { return io2_.get(); }
    // The set's I/O board: its controls and its settings EEPROM (128 bytes,
    // the board's own layout: the RAM's byte order on the first, MAME's
    // big-endian 93C46 image on Wing War's)
    Controls &controls() { return io2_ ? io2_->controls : io_.controls; }
    std::array<uint8_t, 128> &eeprom() { return io2_ ? io2_->eeprom : io_.eeprom; }
    bool &eeprom_dirty() { return io2_ ? io2_->eeprom_dirty : io_.eeprom_dirty; }
    // After the host replaced eeprom() (a saved copy), before the game runs
    void reload_settings() {
        if (!io2_) io_.reload_settings(); // Wing War's BIOS reads the EEPROM itself
    }
    // Link play: the comm board on a host transport (nullptr: no other
    // cabinet, as under MAME with none). Set before the game runs.
    void set_link(rt::LinkTransport *transport, bool framesync = false) {
        comm_.set_transport(transport);
        comm_.set_framesync(framesync);
    }
    const CommBoard &comm_board() const { return comm_; }
    snd::SoundBoard &sound() { return sound_; }
    // Star Wars Arcade's Digital Sound Board (nullptr: the set has none)
    Dsb *dsb() { return dsb_.get(); }
    // The battery-backed RAM (400000, 64 KiB)
    uint8_t *backup_ram() { return mem_.ram(0x400000); }
    static constexpr size_t kBackupBytes = 0x10000;

    uint32_t dev_read(uint32_t a, unsigned size, unsigned space) override;
    void dev_write(uint32_t a, uint32_t v, unsigned size, unsigned space) override;

private:
    uint64_t now() const { return env_.count * kTicksPerInstruction + stall_ticks_; }
    void run_to(uint64_t ticks);
    void copro_waited();
    void irq_raise(int level);
    void fire_timers();
    void timer_period_w(int t, uint16_t period);
    void sound_to_now();
    void uart_data_w(uint8_t v);
    void uart_control_w(uint8_t v);
    void uart_update();
    void uart_schedule(uint64_t after);
    void uart_ready(); // sound_ready_w: level 3 while TxRDY and unmasked
    bool uart_txrdy();
    uint64_t uart_next_bit(uint64_t after) const; // the first bit time after `after`

    Memory mem_;
    Copro copro_;
    Video video_;
    IoBoard io_;
    std::unique_ptr<IoBoard2> io2_;
    snd::SoundBoard sound_;
    std::unique_ptr<Dsb> dsb_;
    v60::Core cpu_;
    // Code the recompiler did not reach: MAME's V60 interpreter (v60::Interp)
    // runs it until control is back in recompiled code, the run's limit or a
    // pending interrupt the PSW lets in. Each new address is logged (a seed
    // for the next recompile). M2_STRICT=1: a hard error, as before.
    struct FallbackEnv : NativeEnv {
        FallbackEnv();
        void miss(v60::Core &c) override;
        bool strict = false;
        uint64_t interpreted = 0;
        std::unordered_set<uint32_t> seen;
    };
    FallbackEnv env_;
    uint64_t frame_ = 0, next_vblank_ = kFrameTicks; // ticks
    uint64_t stall_ticks_ = 0, target_ = 0;
    double sound_done_ = 0; // seconds of board time the sound board has run
    // interrupt controller (model1_state irq_*)
    uint8_t irq_status_ = 0, irq_mask_ = 0xff;
    int last_irq_ = 0;
    // the two timers (E00008/A: the period in 0x800 V60 cycles; E0000C/E:
    // the count left), which raise level 0: swa's UART queue pump
    static constexpr uint64_t kNever = ~uint64_t(0);
    static constexpr uint64_t kTimerUnit = 0x800 * Copro::kTicksPerV60Cycle;
    uint16_t timer_period_[2] = {}, timer_val_[2] = {};
    uint64_t timer_due_[2] = {kNever, kNever};
    // The main board's i8251 to the sound board (C40000 data, C40002 mode,
    // command and status). It shifts a bit every 32 us (16 MHz / 2 / 16 / 16:
    // the 500 kHz TxC at x16). TxRDY (buffer empty) raises level 3 when
    // unmasked, at a write, a mask write and each idle bit time (Virtua
    // Fighter's sound queue pump; VR and swa keep it masked).
    static constexpr uint64_t kUartBitTicks = 2560, kTxcTicks = 160; // 80 MHz / 31,250; TxC's period
    uint64_t uart_phase_ = 0; // ticks: a bit time's phase (set by the mode write)
    bool uart_expect_mode_ = true, uart_txen_ = false, uart_full_ = false;
    uint64_t uart_due_ = kNever, uart_shift_end_ = 0; // ticks: the next bit time that acts; the shift register empty
    // the comm board: shared RAM at b00000-b00fff, CN at b01000, FG at b01002
    CommBoard comm_;
    uint8_t comm_read(uint32_t a);
    void comm_write(uint32_t a, uint8_t v);
};

} // namespace m1
