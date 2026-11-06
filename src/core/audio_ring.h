// A fixed-size ring of interleaved stereo samples between the board's sound
// output (written once a frame) and the front end's audio callback (read in
// whatever block size it asks for). When nobody reads (paused output) the
// oldest samples are overwritten, so it holds the newest `capacity` floats.
// Replaces a std::deque<float> whose insert and erase ran every frame.
#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace m2core {

class AudioRing {
public:
    // The first call sets the size; a later different capacity starts over.
    void push(const float *in, size_t n, size_t capacity) {
        if (capacity != buf_.size()) {
            buf_.assign(capacity, 0.0f);
            start_ = size_ = 0;
        }
        if (capacity == 0) return;
        if (n > capacity) { // only the newest fit
            in += n - capacity;
            n = capacity;
        }
        size_t end = (start_ + size_) % capacity;
        const size_t first = std::min(n, capacity - end);
        std::copy_n(in, first, buf_.begin() + std::ptrdiff_t(end));
        std::copy_n(in + first, n - first, buf_.begin());
        const size_t total = size_ + n;
        if (total > capacity) { // overwrote the oldest
            start_ = (start_ + total - capacity) % capacity;
            size_ = capacity;
        } else {
            size_ = total;
        }
    }
    // Copies out up to n of the oldest floats and drops them; returns how many.
    size_t read(float *out, size_t n) {
        n = std::min(n, size_);
        if (n == 0) return 0;
        const size_t capacity = buf_.size();
        const size_t first = std::min(n, capacity - start_);
        std::copy_n(buf_.begin() + std::ptrdiff_t(start_), first, out);
        std::copy_n(buf_.begin(), n - first, out + first);
        start_ = (start_ + n) % capacity;
        size_ -= n;
        return n;
    }
    size_t size() const { return size_; }

private:
    std::vector<float> buf_;
    size_t start_ = 0, size_ = 0;
};

} // namespace m2core
