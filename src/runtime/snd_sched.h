// Event schedule for the recompiled sound 68000, counted in completed
// instructions (the same model as the i960's rt::Lockstep). Recompiled code
// calls boundary() before each instruction; when the count reaches the next
// event the due callbacks run and, in free run, a pending interrupt the mask
// allows is taken. The lockstep checker instead takes interrupts where MAME's
// log says, from its own callbacks.
#pragma once

#include "runtime/snd_cpu.h"
#include "runtime/state_io.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>

namespace snd {

class Sched {
public:
    explicit Sched(Cpu68k &c) : c_(c) {}

    uint64_t count = 0;              // completed instructions
    uint64_t end_count = UINT64_MAX; // recompiled code returns once this is reached
    uint64_t next_count = UINT64_MAX;
    bool auto_irq = true;            // free run: take c.irq_line when the mask allows
    std::function<void(int)> on_irq; // after a free-run interrupt entry, its level (a HOLD_LINE's acknowledge)

    bool boundary() {
        if (count < next_count && count < end_count) return false;
        return apply();
    }
    bool finished() const { return count >= end_count; }

    // Run fn once `at` instructions have completed (at >= count). tag and
    // arg: what it is, for save states (the owner's numbers; tag 0: one a
    // save state can't hold).
    void at(uint64_t when, std::function<void()> fn, uint32_t tag = 0, uint64_t arg = 0) {
        events_.emplace(std::max(when, count), Event{std::move(fn), tag, arg});
        next_count = std::min(next_count, events_.begin()->first);
    }
    // Save states: the counts and the events to come, each made again on
    // load by make(tag, arg).
    void state(rt::StateIO &s, const std::function<std::function<void()>(uint32_t, uint64_t)> &make) {
        s.mark("SCHD");
        s(count), s(end_count), s(next_count), s(auto_irq);
        struct Saved {
            uint64_t when, arg;
            uint32_t tag, pad;
        };
        std::vector<Saved> saved;
        if (s.saving())
            for (const auto &[when, e] : events_) {
                if (!e.tag) return s.fail("a sound board event a save state can't hold");
                saved.push_back({when, e.arg, e.tag, 0});
            }
        s(saved);
        if (s.saving() || !s.ok()) return;
        events_.clear();
        for (const Saved &v : saved) {
            std::function<void()> fn = make(v.tag, v.arg);
            if (!fn) return s.fail("a sound board event this build doesn't have");
            events_.emplace(v.when, Event{std::move(fn), v.tag, v.arg});
        }
    }
    // The interrupt line or mask changed: re-check at the next boundary.
    void poke() { next_count = std::min(next_count, count + 1); }

private:
    bool apply() {
        const uint32_t pc = c_.pc;
        while (!events_.empty() && events_.begin()->first <= count) {
            auto fn = std::move(events_.begin()->second.fn);
            events_.erase(events_.begin());
            fn();
        }
        if (auto_irq && c_.irq_line > c_.mask()) {
            const int level = c_.irq_line;
            c_.take_irq(level);
            if (on_irq) on_irq(level);
        }
        next_count = events_.empty() ? UINT64_MAX : events_.begin()->first;
        return c_.pc != pc || finished();
    }

    Cpu68k &c_;
    struct Event {
        std::function<void()> fn;
        uint32_t tag = 0;
        uint64_t arg = 0;
    };
    std::multimap<uint64_t, Event> events_;
};

} // namespace snd
