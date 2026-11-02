// The Model 1 bus with every device replayed from a MAME trace (M1 of the
// Model 1 plan): RAM and ROM are native (model1/memory), each device access
// must be the next one MAME made (same instruction, address, size, space;
// writes the same data) and reads return MAME's data. The data ROM bank
// register is also applied natively. At each write to the interrupt control
// register the registers and RAM hashes are compared with MAME's sample.
#pragma once

#include "model1/memory.h"
#include "model1/trace.h"
#include "v60/core.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace m1 {

// A board device modelled natively behind the replay (m1::Copro).
struct DeviceModel {
    virtual ~DeviceModel() = default;
    virtual uint32_t read(uint32_t a, unsigned size) = 0;
    virtual void write(uint32_t a, uint32_t v, unsigned size) = 0;
    // A timed device (m1::Copro): the ticks (80 MHz) its last read waited.
    virtual uint64_t wait_ticks() const { return 0; }
};

struct Divergence : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class ReplayBus : public v60::Bus {
public:
    ReplayBus(Memory &mem, std::vector<TraceEvent> events);
    void attach(const v60::Core *core) { core_ = core; }

    // Instructions completed: the harness's counter, which it advances
    void set_counter(const uint64_t *count) { count_ = count; }
    uint64_t count() const { return *count_; }
    // The instruction count of MAME's next interrupt (UINT64_MAX if none)
    uint64_t next_irq() const;

    uint32_t dev_read(uint32_t a, unsigned size, unsigned space) override;
    void dev_write(uint32_t a, uint32_t v, unsigned size, unsigned space) override;

    // The interrupt MAME took before instruction `count`, if any (consumed).
    bool take_irq(int &vector, uint32_t &pc);
    // MAME's register file at the first instruction (consumed), if recorded.
    bool take_init(uint32_t regs[68]);
    bool finished() const;
    uint64_t end_count() const { return end_; }
    uint64_t samples() const { return samples_; }
    uint64_t events() const { return events_; }
    uint64_t irqs() const { return irqs_; }
    std::string where() const;

    // The coprocessor board native (milestone 2): the V60's accesses to it
    // still have to be MAME's, but they also drive our TGP. A word the TGP
    // hands the V60 through its FIFO must be MAME's (strict); copro RAM reads
    // depend on how far the TGP has run, so they are measured and MAME's
    // value is used.
    void attach_copro(DeviceModel *c) { copro_ = c; }
    // The display list control (680000) native: reads measured, MAME's used.
    void attach_listctl(DeviceModel *v) { listctl_ = v; }
    uint64_t listctl_reads = 0, listctl_mismatch = 0;
    std::string first_listctl_mismatch;
    // The I/O board (C00000) native: reads measured, MAME's used. A read of
    // the command byte where MAME's board was still busy is a poll ours did
    // not need (it finishes commands at once).
    void attach_io(DeviceModel *io) { io_ = io; }
    // The sound board's UART (C40000-C40003): writes go to it too.
    void attach_sound(DeviceModel *s) { sound_ = s; }
    uint64_t io_reads = 0, io_busy = 0, io_mismatch = 0;
    std::string first_io_mismatch;
    // Timed TGP: our wait at each FIFO read against MAME's retries (8 V60
    // cycles each): reads where both agree, ours shorter or longer, and the
    // retries off in all.
    uint64_t wait_same = 0, wait_short = 0, wait_long = 0, wait_off = 0, wait_mame = 0;
    uint64_t copro_fifo_words = 0, copro_ram_reads = 0, copro_ram_busy = 0, copro_ram_mismatch = 0;
    std::string first_ram_mismatch;

private:
    const TraceEvent &next(const char *what);
    void sample();
    Memory &mem_;
    std::vector<TraceEvent> ev_;
    size_t pos_ = 0;
    uint64_t end_ = UINT64_MAX;
    const v60::Core *core_ = nullptr;
    DeviceModel *copro_ = nullptr, *listctl_ = nullptr, *io_ = nullptr, *sound_ = nullptr;
    uint64_t own_count_ = 0;
    const uint64_t *count_ = &own_count_;
    std::vector<size_t> irq_pos_;
    mutable size_t irq_next_ = 0;
    uint64_t samples_ = 0, events_ = 0, irqs_ = 0;
};

} // namespace m1
