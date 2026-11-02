#include "model1/replay_bus.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace m1 {

ReplayBus::ReplayBus(Memory &mem, std::vector<TraceEvent> events) : mem_(mem), ev_(std::move(events)) {
    for (size_t i = 0; i < ev_.size(); i++) {
        if (ev_[i].type == TraceEvent::End) end_ = ev_[i].count;
        if (ev_[i].type == TraceEvent::Irq) irq_pos_.push_back(i);
    }
    mem_.map(*this);
}

std::string ReplayBus::where() const {
    char b[160];
    std::snprintf(b, sizeof b, "instruction %" PRIu64 ", PC %08x; MAME's next event: %s", count(),
                  core_ ? core_->m_PPC : 0, pos_ < ev_.size() ? describe(ev_[pos_]).c_str() : "none");
    return b;
}

const TraceEvent &ReplayBus::next(const char *what) {
    if (pos_ >= ev_.size() || ev_[pos_].type == TraceEvent::End)
        throw Divergence(std::string("the trace ended, but ours made a ") + what + "; " + where());
    return ev_[pos_];
}

uint32_t ReplayBus::dev_read(uint32_t a, unsigned size, unsigned space) {
    const TraceEvent &e = next("read");
    if (e.type != TraceEvent::Read || e.count != count() || e.addr != a || e.size != size || e.space != space) {
        char b[128];
        std::snprintf(b, sizeof b, "ours read %s%06x size %u; ", space ? "I/O " : "", a, size);
        throw Divergence(b + where());
    }
    ++pos_;
    ++events_;
    if (listctl_ && space == 0 && (a & ~3u) == 0x680000) {
        const uint32_t ours = listctl_->read(a, size);
        ++listctl_reads;
        if (ours != e.data && !listctl_mismatch++) {
            char b[96];
            std::snprintf(b, sizeof b, "%06x: ours %08x, MAME %08x; ", a, ours, e.data);
            first_listctl_mismatch = b + where();
        }
    }
    if (io_ && space == 0 && (a & 0xfff000) == 0xc00000) {
        const uint32_t ours = io_->read(a, size);
        ++io_reads;
        if (ours != e.data && a == 0xc00040 && ours == 0) ++io_busy;
        else if (ours != e.data && !io_mismatch++) {
            char b[96];
            std::snprintf(b, sizeof b, "%06x: ours %08x, MAME %08x; ", a, ours, e.data);
            first_io_mismatch = b + where();
        }
    }
    if (copro_ && (a & 0xf00000) == 0xd00000) {
        const uint32_t ours = copro_->read(a, size);
        if ((a & 0xfe0000) == 0xd80000) {
            ++copro_fifo_words;
            if (!(a & 2)) {
                const uint64_t ours = copro_->wait_ticks() / 40;
                wait_mame += e.stalls;
                if (ours == e.stalls) ++wait_same;
                else if (ours < e.stalls) ++wait_short, wait_off += e.stalls - ours;
                else ++wait_long, wait_off += ours - e.stalls;
            }
            if (ours != e.data) {
                char b[128];
                std::snprintf(b, sizeof b, "TGP FIFO word %" PRIu64 ": ours %08x, MAME %08x; ", copro_fifo_words, ours,
                              e.data);
                throw Divergence(b + where());
            }
        } else {
            ++copro_ram_reads;
            // The game polls a word the TGP sets to FFFFFFFF while it works;
            // ours has always finished by then (it runs before the read).
            if (ours != e.data && e.data == 0xffffffff) ++copro_ram_busy;
            else if (ours != e.data && !copro_ram_mismatch++) {
                char b[96];
                std::snprintf(b, sizeof b, "%06x: ours %08x, MAME %08x; ", a, ours, e.data);
                first_ram_mismatch = b + where();
            }
        }
    }
    return e.data;
}

void ReplayBus::dev_write(uint32_t a, uint32_t v, unsigned size, unsigned space) {
    const TraceEvent &e = next("write");
    if (e.type != TraceEvent::Write || e.count != count() || e.addr != a || e.size != size || e.space != space ||
        e.data != v) {
        char b[128];
        std::snprintf(b, sizeof b, "ours wrote %s%06x size %u data %08x; ", space ? "I/O " : "", a, size, v);
        throw Divergence(b + where());
    }
    ++pos_;
    ++events_;
    if (copro_ && (a & 0xf00000) == 0xd00000) copro_->write(a, v, size);
    if (listctl_ && space == 0 && (a & ~3u) == 0x680000) listctl_->write(a, v, size);
    if (io_ && space == 0 && (a & 0xfff000) == 0xc00000) io_->write(a, v, size);
    if (sound_ && space == 0 && (a & ~3u) == 0xc40000) sound_->write(a, v, size);
    if (space == 0 && a == 0xe00004) mem_.bank_write(*this, v);
    if (space == 0 && a == 0xe00000) sample();
}

void ReplayBus::sample() {
    const TraceEvent &e = next("sample");
    if (e.type != TraceEvent::Sample) throw Divergence("expected MAME's sample; " + where());
    static const char *const names[34] = {"r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",  "r8",
                                          "r9",  "r10", "r11", "r12", "r13", "r14", "r15", "r16", "r17",
                                          "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25", "r26",
                                          "r27", "r28", "ap",  "fp",  "sp",  "pc",  "psw"};
    for (int i = 0; i < 34; i++) {
        const uint32_t ours = i < 32 ? core_->m_reg[i] : i == 32 ? core_->m_reg[32] : core_->psw();
        if (ours != e.regs[i]) {
            char b[96];
            std::snprintf(b, sizeof b, "sample %" PRIu64 ": %s ours %08x, MAME %08x; ", samples_, names[i], ours,
                          e.regs[i]);
            throw Divergence(b + where());
        }
    }
    for (size_t i = 0; i < e.hashes.size() && i < std::size(kRamRegions); i++)
        if (mem_.hash(kRamRegions[i]) != e.hashes[i]) {
            char b[96];
            std::snprintf(b, sizeof b, "sample %" PRIu64 ": RAM %06x-%06x differs; ", samples_, kRamRegions[i].base,
                          kRamRegions[i].base + kRamRegions[i].bytes - 1);
            throw Divergence(b + where());
        }
    ++pos_;
    ++samples_;
}

bool ReplayBus::take_irq(int &vector, uint32_t &pc) {
    if (pos_ < ev_.size() && ev_[pos_].type == TraceEvent::Irq && ev_[pos_].count == count()) {
        vector = int(ev_[pos_].addr);
        pc = ev_[pos_].data;
        ++pos_;
        ++irqs_;
        return true;
    }
    if (pos_ < ev_.size() && ev_[pos_].type == TraceEvent::Irq && ev_[pos_].count < count())
        throw Divergence("MAME took an interrupt we passed; " + where());
    return false;
}

bool ReplayBus::take_init(uint32_t regs[68]) {
    if (pos_ < ev_.size() && ev_[pos_].type == TraceEvent::Init) {
        std::memcpy(regs, ev_[pos_].regs, sizeof ev_[pos_].regs);
        ++pos_;
        return true;
    }
    return false;
}

bool ReplayBus::finished() const { return count() >= end_ || pos_ >= ev_.size(); }

uint64_t ReplayBus::next_irq() const {
    while (irq_next_ < irq_pos_.size() && irq_pos_[irq_next_] < pos_) ++irq_next_;
    return irq_next_ < irq_pos_.size() ? ev_[irq_pos_[irq_next_]].count : UINT64_MAX;
}

} // namespace m1
