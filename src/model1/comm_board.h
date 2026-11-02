// license:BSD-3-Clause
// copyright-holders:Ariane Fugmann
//
// The Model 1 communication board: link play between cabinets. Transplanted
// from MAME's m1comm device in its M1COMM_SIMULATION mode
// (src/mame/sega/m1comm.cpp at dddd73680656e355bb2b5beecab1167c9f07bf81;
// BSD-3-Clause, notice above kept as the licence requires; see
// THIRD_PARTY.md): the board's Z80 program (EPR-15112) is not run; the
// protocol it carries is.
//
// Its protocol is not Model 2's (rt::CommBoard): the cabinets form a ring,
// and the master (the game's shared byte 1: 1 master, 2 slave, 0 relay)
// sends an id token (0xff) round it, each slave taking the next id, then the
// count (0xfe). Every frame each cabinet whose ready flag (byte 4) is set
// sends its 0x1c4-byte block (from byte 0x10) on, and each one keeps every
// cabinet's block at 0x10 + id * 0x1c4; the master also sends ten status
// bytes (0xfd, bytes 6-15) and a vsync token (0xfc). The shared RAM is 4 KB
// (the V60 at B00000-B00FFF). The bytes travel through an rt::LinkTransport
// (TCP in the app), which never blocks. With no transport the board is still
// there, as under MAME with no other cabinet: once the game enables it, it
// reports the link as waiting (byte 0 = 5) for ever.
#pragma once

#include "runtime/comm_board.h"

#include <cstdint>
#include <vector>

namespace m1 {

class CommBoard {
public:
    void set_transport(rt::LinkTransport *t) { transport_ = t; }
    // MAME's -comm_framesync: hold each cabinet to the master's frame.
    void set_framesync(bool on) { framesync_ = on ? 1 : 0; }

    uint8_t share_r(uint32_t offset) const { return shared_[offset & 0xfff]; }
    void share_w(uint32_t offset, uint8_t data) { shared_[offset & 0xfff] = data; }
    uint8_t cn_r() const { return uint8_t(cn_ | 0xfe); }
    void cn_w(uint8_t data);
    uint8_t fg_r() const { return uint8_t(fg_ | (~zfg_ << 7) | 0x7e); }
    void fg_w(uint8_t data) {
        if (cn_) fg_ = data & 0x01;
    }
    void vblank() { comm_tick(); } // MAME check_vint_irq: line 384

    // For the host's status line.
    enum class Link { Off, Waiting, Up, Lost };
    Link link() const;
    int id() const { return linkid_; }
    int count() const { return linkcount_; }

private:
    uint8_t shared_[0x1000] = {};
    rt::LinkTransport *transport_ = nullptr;
    uint8_t zfg_ = 0, cn_ = 0, fg_ = 0, framesync_ = 0;
    uint8_t linkenable_ = 0, linkalive_ = 0, linkid_ = 0, linkcount_ = 0;
    uint16_t linktimer_ = 0;
    uint8_t buffer0_[0x200] = {};
    std::vector<uint8_t> rx_; // bytes received toward the next whole frame
    void comm_tick();
    int read_frame(int data_size);
    void send_data(uint8_t frame_type, int frame_start, int frame_size, int data_size);
    void send_frame(int data_size);
    void lost();
};

} // namespace m1
