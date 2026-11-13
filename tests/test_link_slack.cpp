// Lockstep with slack (src/runtime/comm_board.cpp, link_slack): three
// boards in a ring, each on its own thread at the board's 57.5 Hz, over
// cables that take 12 ms a hop, so a lap of the ring (36 ms) is longer than a
// frame (17.4 ms), as on Wi-Fi. With frame sync and no slack (MAME's
// -comm_framesync) every frame waits a lap: about 26 frames a second. With
// the default slack the master runs ahead and every cabinet keeps the
// board's rate. No ROM, no network.

#include "runtime/comm_board.h"

#include <atomic>
#include <memory>
#include <chrono>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// One direction of cable: each byte can be read `delay` after it was written.
struct Wire {
    std::mutex m;
    std::deque<std::pair<Clock::time_point, uint8_t>> bytes;
    std::chrono::milliseconds delay{12};
};

class Cable : public rt::LinkTransport {
public:
    Cable(Wire &in, Wire &out) : in_(in), out_(out) {}
    void poll() override {}
    bool rx_open() const override { return true; }
    bool tx_open() const override { return true; }
    int read(uint8_t *buf, int max) override {
        std::lock_guard<std::mutex> lock(in_.m);
        const auto now = Clock::now();
        int n = 0;
        while (n < max && !in_.bytes.empty() && in_.bytes.front().first <= now) {
            buf[n++] = in_.bytes.front().second;
            in_.bytes.pop_front();
        }
        return n;
    }
    bool write(const uint8_t *buf, int n) override {
        std::lock_guard<std::mutex> lock(out_.m);
        const auto at = Clock::now() + out_.delay;
        for (int i = 0; i < n; i++) out_.bytes.emplace_back(at, buf[i]);
        return true;
    }

private:
    Wire &in_, &out_;
};

struct Result {
    bool linked = true;
    double fps[3] = {};
};

// Three cabinets, 1 -> 2 -> 3 -> 1, run for `seconds` once linked.
Result race(int slack, double seconds) {
    Wire w[3]; // w[i]: into cabinet i
    std::vector<std::vector<uint8_t>> ram(3, std::vector<uint8_t>(0x4000));
    std::vector<std::unique_ptr<Cable>> cables;
    std::vector<std::unique_ptr<rt::CommBoard>> boards;
    for (int i = 0; i < 3; i++) {
        cables.push_back(std::make_unique<Cable>(w[i], w[(i + 1) % 3]));
        boards.push_back(std::make_unique<rt::CommBoard>(ram[size_t(i)].data()));
        boards[size_t(i)]->set_transport(cables[size_t(i)].get());
        boards[size_t(i)]->set_framesync(true, slack);
        boards[size_t(i)]->fg_w(i == 0 ? 1 : 0); // cabinet 1 is the master
        boards[size_t(i)]->cn_w(1);
    }
    std::atomic<int> up{0};
    std::atomic<bool> lost{false};
    Result r;
    const auto frame = std::chrono::nanoseconds(int64_t(1e9 / 57.524));
    std::vector<std::thread> threads;
    for (int i = 0; i < 3; i++) {
        threads.emplace_back([&, i] {
            auto &b = *boards[size_t(i)];
            auto next = Clock::now();
            const auto give_up = next + std::chrono::seconds(10);
            // Until every cabinet is linked.
            bool counted = false;
            while (up.load() < 3 && Clock::now() < give_up) {
                b.vblank();
                (void)b.fg_r();
                if (!counted && b.link() == rt::CommBoard::Link::Up) {
                    counted = true;
                    up++;
                }
                next += frame;
                std::this_thread::sleep_until(next);
            }
            if (up.load() < 3) return;
            // Then frames for `seconds`, at the board's rate or as the ring allows.
            const auto start = Clock::now();
            const auto end = start + std::chrono::duration<double>(seconds);
            next = start;
            int frames = 0;
            while (Clock::now() < end) {
                b.vblank();
                (void)b.fg_r();
                frames++;
                if (b.link() != rt::CommBoard::Link::Up) lost = true;
                next += frame;
                if (next > Clock::now()) std::this_thread::sleep_until(next);
                else next = Clock::now(); // late: no catching up, as the front ends do
            }
            r.fps[i] = frames / std::chrono::duration<double>(Clock::now() - start).count();
        });
    }
    for (auto &t : threads) t.join();
    r.linked = up.load() == 3 && !lost;
    return r;
}
} // namespace

int main() {
    const Result strict = race(0, 1.5);
    const Result slack = race(rt::kDefaultLinkSlack, 1.5);
    std::printf("test_link_slack: 36 ms laps: slack 0 %.1f %.1f %.1f fps, slack %d %.1f %.1f %.1f fps\n",
                strict.fps[0], strict.fps[1], strict.fps[2], rt::kDefaultLinkSlack, slack.fps[0], slack.fps[1], slack.fps[2]);
    check(strict.linked, "slack 0: the three cabinets link and stay linked");
    check(slack.linked, "slack: the three cabinets link and stay linked");
    for (int i = 0; i < 3; i++) {
        check(strict.fps[i] < 40, "slack 0: a lap every frame holds every cabinet below the board's rate");
        check(slack.fps[i] > 54, "slack: every cabinet keeps the board's rate");
    }
    if (failures) return 1;
    std::printf("test_link_slack: ok\n");
    return 0;
}
