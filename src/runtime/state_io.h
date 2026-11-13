// Save states (afcore 5.4, docs/design/MODEL2.md "Save states"): one pass
// over the board's devices that writes their state or reads it back. Each
// device has a state(StateIO &) that names what it keeps between frames, in
// order, with the same calls for both directions:
//
//   void Thing::state(StateIO &s) {
//       s.mark("THNG");     // a check: a state read out of step stops here
//       s(count_);          // trivially copyable values, arrays of them
//       s(queue_);          // vectors and deques of them (resized on load)
//       s.fixed(ram_);      // vectors whose data a page table points into: same size, kept in place
//   }
//
// The bytes are the build's own layout (plain structs copied whole): a state
// loads in the same core version on the same kind of machine (both checked
// by afcommon/state.h's header and the core version in it).
#pragma once

#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <type_traits>
#include <vector>

namespace rt {

class StateIO {
public:
    explicit StateIO(std::vector<uint8_t> &out) : out_(&out) {}
    StateIO(const uint8_t *data, size_t size) : in_(data), size_(size) {}

    bool saving() const { return out_ != nullptr; }
    bool loading() const { return out_ == nullptr; }
    bool ok() const { return error_.empty(); }
    const std::string &error() const { return error_; }
    void fail(const std::string &why) {
        if (error_.empty()) error_ = why;
    }
    size_t left() const { return loading() ? size_ - pos_ : 0; }

    void bytes(void *p, size_t n) {
        if (saving()) {
            const uint8_t *b = static_cast<const uint8_t *>(p);
            out_->insert(out_->end(), b, b + n);
        } else if (!ok() || n > size_ - pos_) {
            fail("the save state ends early");
        } else {
            std::memcpy(p, in_ + pos_, n);
            pos_ += n;
        }
    }

    template <typename T> void operator()(T &v) {
        static_assert(std::is_trivially_copyable_v<T>, "a device's state: give it a state(StateIO &)");
        bytes(&v, sizeof v);
    }
    template <typename T> void operator()(std::vector<T> &v) {
        static_assert(std::is_trivially_copyable_v<T>);
        uint64_t n = v.size();
        (*this)(n);
        if (loading()) {
            if (!ok() || n > left() / (sizeof(T) ? sizeof(T) : 1)) return fail("the save state ends early");
            v.resize(size_t(n));
        }
        if (n) bytes(v.data(), size_t(n) * sizeof(T));
    }
    template <typename T> void operator()(std::deque<T> &d) {
        static_assert(std::is_trivially_copyable_v<T>);
        std::vector<T> v(d.begin(), d.end());
        (*this)(v);
        if (loading() && ok()) d.assign(v.begin(), v.end());
    }
    // A vector the board's page table (or a device) points into: the state
    // must have it at the same size, and it is read in place.
    template <typename T> void fixed(std::vector<T> &v) {
        static_assert(std::is_trivially_copyable_v<T>);
        uint64_t n = v.size();
        (*this)(n);
        if (loading() && ok() && n != v.size()) return fail("the save state's memory sizes are another build's");
        if (n) bytes(v.data(), v.size() * sizeof(T));
    }
    // A pointer into `owner` itself (a table, a buffer): saved as its offset.
    template <typename Owner, typename T> void inner(const Owner *owner, T *&p) {
        const char *base = reinterpret_cast<const char *>(owner);
        int64_t o = p ? int64_t(reinterpret_cast<const char *>(p) - base) : -1;
        (*this)(o);
        if (saving() || !ok()) return;
        if (o < 0) p = nullptr;
        else if (uint64_t(o) + sizeof(T) > sizeof(Owner)) fail("the save state's pointers are damaged");
        else p = reinterpret_cast<T *>(const_cast<char *>(base) + o);
    }
    // A four-character check between devices.
    void mark(const char tag[4]) {
        char t[4];
        std::memcpy(t, tag, 4);
        bytes(t, 4);
        if (loading() && ok() && std::memcmp(t, tag, 4) != 0)
            fail(std::string("the save state is out of step at ") + std::string(tag, 4));
    }

private:
    std::vector<uint8_t> *out_ = nullptr;
    const uint8_t *in_ = nullptr;
    size_t size_ = 0, pos_ = 0;
    std::string error_;
};

// A plain struct with pointers the owner set (a bus, a table, a ROM): read
// whole into a copy, then keep(copy) puts back the live object's pointers
// and the copy replaces it. (The copy is on the heap: some are large.)
template <typename T, typename Keep> void state_keeping(StateIO &s, T &v, Keep keep) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (s.saving()) return s(v);
    std::vector<uint8_t> copy(sizeof(T));
    T *c = reinterpret_cast<T *>(copy.data());
    std::memcpy(static_cast<void *>(c), &v, sizeof v);
    s(*c);
    if (!s.ok()) return;
    keep(*c);
    std::memcpy(static_cast<void *>(&v), c, sizeof v);
}

} // namespace rt
