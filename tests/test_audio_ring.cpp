// The core's audio ring (src/core/audio_ring.h) against the std::deque it
// replaced: random pushes and reads, including overflow while nobody reads,
// must give the same samples in the same order.
#include "core/audio_ring.h"

#include <cstdio>
#include <deque>
#include <random>
#include <vector>

int main() {
    std::mt19937 rng(1234);
    for (size_t cap : {size_t(2), size_t(64), size_t(2 * 44100)}) {
        m2core::AudioRing ring;
        std::deque<float> model;
        float next = 0;
        for (int step = 0; step < 20000; ++step) {
            if (rng() % 2) { // a frame of output: up to twice the capacity
                std::vector<float> in(2 * (rng() % (cap + 1)));
                for (float &v : in) v = next++;
                ring.push(in.data(), in.size(), cap);
                model.insert(model.end(), in.begin(), in.end());
                if (model.size() > cap) model.erase(model.begin(), model.end() - std::ptrdiff_t(cap));
            } else { // the audio callback
                std::vector<float> out(2 * (rng() % (cap / 2 + 2)));
                const size_t n = ring.read(out.data(), out.size());
                const size_t want = std::min(out.size(), model.size());
                if (n != want) {
                    std::fprintf(stderr, "FAIL: capacity %zu step %d: read %zu, want %zu\n", cap, step, n, want);
                    return 1;
                }
                for (size_t i = 0; i < n; ++i)
                    if (out[i] != model[i]) {
                        std::fprintf(stderr, "FAIL: capacity %zu step %d: sample %zu is %g, want %g\n", cap, step, i,
                                     out[i], model[i]);
                        return 1;
                    }
                model.erase(model.begin(), model.begin() + std::ptrdiff_t(n));
            }
            if (ring.size() != model.size()) {
                std::fprintf(stderr, "FAIL: capacity %zu step %d: size %zu, want %zu\n", cap, step, ring.size(), model.size());
                return 1;
            }
        }
    }
    std::puts("audio ring: OK");
    return 0;
}
