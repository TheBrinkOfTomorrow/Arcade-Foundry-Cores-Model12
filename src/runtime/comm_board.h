// license:BSD-3-Clause
// copyright-holders:Ariane Fugmann
//
// The Model 2 communication board (837-10537 on Daytona USA Revision A):
// link play between cabinets. Transplanted from MAME's m2comm device in its
// M2COMM_SIMULATION mode (src/mame/sega/m2comm.cpp at
// dddd73680656e355bb2b5beecab1167c9f07bf81; BSD-3-Clause, notice above kept
// as the licence requires; see THIRD_PARTY.md): the board's Z80 program
// (EPR-16726) is not run; the protocol it carries is.
//
// Cabinets form a ring: each sends to the next and receives from the one
// before. At start the master (set in the game's test mode: LINK ID) sends a
// numbering token round the ring, so every cabinet learns its id and the
// count; then every frame each one passes its block of race data on, and the
// master sends a vsync token. The shared RAM is the game's (M2Board maps it
// at 0x01a00000); the bytes travel through a LinkTransport the host provides
// (TCP in the app), which never blocks. Without a transport the board is not
// here at all: M2Board keeps its plain registers and the game sees no link,
// except on the sets that wait for the board (kCommAlways): there its cable
// is a LoopbackTransport, a ring of one, as MAME's defaults make it (the
// board connects to its own port, 15112).
#pragma once

#include <algorithm>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <vector>

namespace rt {

// Lockstep's slack: with frame sync on, how many frames the master may run
// ahead of the sync token (vsync) it sent coming back round the ring. 0 is
// MAME's -comm_framesync, a whole lap of the ring every frame: on Wi-Fi a lap
// often takes longer than a frame, so every cabinet slows and stutters. With
// slack, a slow lap only delays what the cabinets see of each other by up to
// that many frames. AF_LINK_SLACK overrides it (0 for MAME's).
constexpr int kDefaultLinkSlack = 4;
inline int link_slack() {
    static const int slack = [] {
        const char *s = std::getenv("AF_LINK_SLACK");
        return s && *s ? std::clamp(std::atoi(s), 0, 60) : kDefaultLinkSlack;
    }();
    return slack;
}

// The two cables: to the next cabinet and from the one before. Non-blocking.
class LinkTransport {
public:
    virtual ~LinkTransport() = default;
    virtual void poll() = 0;        // accept / connect as needed
    virtual bool rx_open() const = 0; // a cabinet is sending to us
    virtual bool tx_open() const = 0; // we are connected to the next cabinet
    // Bytes received: > 0 how many, 0 none yet, < 0 the connection closed.
    virtual int read(uint8_t *buf, int max) = 0;
    // All of buf, or false: the connection is lost.
    virtual bool write(const uint8_t *buf, int n) = 0;
};

// A ring of one: what is sent is received.
class LoopbackTransport : public LinkTransport {
public:
    void poll() override {}
    bool rx_open() const override { return true; }
    bool tx_open() const override { return true; }
    int read(uint8_t *buf, int max) override {
        const int n = std::min<int>(max, int(q_.size()));
        std::copy(q_.begin(), q_.begin() + n, buf);
        q_.erase(q_.begin(), q_.begin() + n);
        return n;
    }
    bool write(const uint8_t *buf, int n) override {
        q_.insert(q_.end(), buf, buf + n);
        return true;
    }

    template <typename S> void state(S &s) { s(q_); } // save states

private:
    std::deque<uint8_t> q_;
};

class CommBoard {
public:
    // shared: the board's 16 KB shared RAM. frame_offset: where in the
    // received block a frame lands (0x1c0 for most games).
    explicit CommBoard(uint8_t *shared, uint16_t frame_offset = 0x1c0) : shared_(shared), frame_offset_(frame_offset) {}
    void set_transport(LinkTransport *t) { transport_ = t; }
    // Hold each cabinet to the master's frame (MAME's -comm_framesync), within
    // `slack` frames (link_slack); off by default, as in MAME: cabinets run
    // free and exchange data as it comes.
    void set_framesync(bool on, int slack = link_slack()) {
        framesync_ = on ? 1 : 0;
        slack_ = slack;
    }

    uint8_t cn_r() const { return uint8_t(cn_ | 0xfe); }
    void cn_w(uint8_t data);
    uint8_t fg_r();
    void fg_w(uint8_t data) { fg_ = data & 1; }
    void vblank(); // MAME check_vint_irq: once per frame

    // For the host's status line.
    enum class Link { Off, Waiting, Up, Lost };
    Link link() const;
    int id() const { return linkid_; }
    int count() const { return linkcount_; }
    // Save states (the loopback board only: a link isn't saved).
    template <typename S> void state(S &s) {
        s(zfg_), s(cn_), s(fg_), s(framesync_), s(linkenable_), s(linkalive_), s(linkid_), s(linkcount_), s(zfg_delay_);
        s(linktimer_), s(slack_), s(outstanding_), s(buffer0_), s(rx_);
    }

private:
    uint8_t *shared_;
    uint16_t frame_offset_;
    LinkTransport *transport_ = nullptr;
    uint8_t zfg_ = 0, cn_ = 0, fg_ = 0, framesync_ = 0;
    uint8_t linkenable_ = 0, linkalive_ = 0, linkid_ = 0, linkcount_ = 0, zfg_delay_ = 0;
    uint16_t linktimer_ = 0;
    int slack_ = 0;       // frames the master may run ahead (set_framesync)
    int outstanding_ = 0; // the master's vsyncs not yet back round the ring
    uint8_t buffer0_[0x1000] = {};
    std::vector<uint8_t> rx_; // bytes received toward the next whole frame
    void comm_tick();
    void read_fg();
    int read_frame(int data_size);
    void send_data(uint8_t frame_type, int frame_start, int frame_size, int data_size);
    void send_frame(int data_size);
    void lost();
    bool held(bool is_master) const;
};

} // namespace rt
