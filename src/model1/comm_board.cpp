// license:BSD-3-Clause
// copyright-holders:Ariane Fugmann
//
// MAME's m1comm_device (M1COMM_SIMULATION), transplanted: see comm_board.h.
// Changes: the sockets are a non-blocking rt::LinkTransport, so read_frame
// keeps partial frames until whole instead of waiting; the frame-sync wait
// gives up after kSyncTimeout (MAME waits forever), and the master may run
// up to slack_ frames ahead of its vsync (rt::link_slack; MAME: none);
// messages through osd_printf_verbose are dropped.

#include "model1/comm_board.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace m1 {

namespace {
constexpr auto kSyncTimeout = std::chrono::seconds(3);
}

CommBoard::Link CommBoard::link() const {
    if (!linkenable_) return Link::Off;
    if (linkalive_ == 1) return Link::Up;
    if (linkalive_ == 2) return Link::Lost;
    return Link::Waiting;
}

void CommBoard::cn_w(uint8_t data) {
    cn_ = data & 0x01;
    if (!cn_) {
        // reset command
        linkenable_ = 0x00;
        zfg_ = 0x00;
    } else {
        // init command
        linkenable_ = 0x01;
        linkid_ = 0x00;
        linkalive_ = 0x00;
        linkcount_ = 0x00;
        linktimer_ = 0x00e8; // 58 fps * 4s
        outstanding_ = 0;
        rx_.clear();
    }
}

void CommBoard::lost() {
    linkalive_ = 0x02;
    linktimer_ = 0x00;
    outstanding_ = 0;
    shared_[0] = 0xff;
}

// Whether this frame still waits on the ring. A slave (or relay) waits for
// a vsync from the master (when several came, it takes them all, so a slave
// held up by a slow hop catches up). The master waits only while more than
// slack_ of its vsyncs are still on their way round: with slack 0, a lap
// every frame, as in MAME.
bool CommBoard::held(bool is_master) const {
    if (is_master) return framesync_ && outstanding_ > slack_;
    return linktimer_ == 0x01;
}

void CommBoard::comm_tick() {
    if (linkenable_ != 0x01) return;
    if (transport_) transport_->poll();
    const int frame_start = 0x0010;
    int frame_offset = 0x0000;
    const int frame_size = 0x01c4;
    const int data_size = frame_size + 1;
    int recv = 0;
    int idx = 0;

    const bool is_master = (shared_[1] == 0x01);
    const bool is_slave = (shared_[1] == 0x02);
    const bool is_relay = (shared_[1] == 0x00);

    if (linkalive_ == 0x02) {
        // link failed...
        shared_[0] = 0xff;
        return;
    } else if (linkalive_ == 0x00) {
        // link not yet established...
        shared_[0] = 0x05;

        // if both lines are there check ring
        if (transport_ && transport_->rx_open() && transport_->tx_open()) {
            // try to read one message
            recv = read_frame(data_size);
            while (recv > 0) {
                // check message id
                idx = buffer0_[0];

                // 0xFF - link id
                if (idx == 0xff) {
                    if (is_master) {
                        // master gets first id and starts next state
                        linkid_ = 0x01;
                        linkcount_ = buffer0_[1];
                        linktimer_ = 0x00;
                    } else {
                        // slave get own id, relay does nothing
                        if (is_slave) {
                            buffer0_[1]++;
                            linkid_ = buffer0_[1];
                        }

                        // forward message to other nodes
                        send_frame(data_size);
                    }
                }

                // 0xFE - link size
                else if (idx == 0xfe) {
                    if (is_slave || is_relay) {
                        linkcount_ = buffer0_[1];

                        // forward message to other nodes
                        send_frame(data_size);
                    }

                    // consider it done
                    linkalive_ = 0x01;
                    zfg_ = 0x01;

                    // write to shared mem
                    shared_[0] = 0x01;
                    shared_[2] = linkid_;
                    shared_[3] = linkcount_;
                }

                if (linkalive_ == 0x00) recv = read_frame(data_size);
                else recv = 0;
            }

            // if we are master and link is not yet established
            if (is_master && (linkalive_ == 0x00)) {
                if (linktimer_ == 0x01) {
                    // send first packet
                    buffer0_[0] = 0xff;
                    buffer0_[1] = 0x01;
                    send_frame(data_size);
                } else if (linktimer_ == 0x00) {
                    // send second packet
                    buffer0_[0] = 0xfe;
                    buffer0_[1] = linkcount_;
                    send_frame(data_size);

                    // consider it done
                    linkalive_ = 0x01;
                    zfg_ = 0x01;

                    // write to shared mem
                    shared_[0] = 0x01;
                    shared_[2] = linkid_;
                    shared_[3] = linkcount_;
                } else if (linktimer_ > 0x01) {
                    // decrease delay timer
                    linktimer_--;
                }
            }
        }
    }

    if (linkalive_ == 0x01) {
        // link established
        const auto deadline = std::chrono::steady_clock::now() + kSyncTimeout;
        do {
            // try to read a message
            recv = read_frame(data_size);
            while (recv > 0) {
                // check if valid id
                idx = buffer0_[0];
                if (idx > 0 && idx <= linkcount_) {
                    // if not own message
                    if (idx != linkid_) {
                        // save message to "ring buffer"
                        frame_offset = frame_start + (idx * frame_size);
                        for (int j = 0x00; j < frame_size; j++) shared_[(frame_offset + j) & 0xfff] = buffer0_[1 + j];

                        // forward message to other nodes
                        send_frame(data_size);
                    }
                } else {
                    if (idx == 0xfc) {
                        // 0xFC - VSYNC: back at the master after a lap; a
                        // slave takes it and passes it on
                        if (is_master) {
                            if (outstanding_ > 0) outstanding_--;
                        } else {
                            linktimer_ = 0x00;
                            send_frame(data_size);
                        }
                    }
                    if (idx == 0xfd) {
                        // 0xFD - master addional bytes
                        if (!is_master) {
                            // save message to "ring buffer"
                            frame_offset = 0x06;
                            for (int j = 0x00; j < 0x0a; j++) shared_[frame_offset + j] = buffer0_[1 + j];

                            // forward message to other nodes
                            send_frame(data_size);
                        }
                    }
                }

                // try to read another message
                recv = read_frame(data_size);
            }
            if (held(is_master)) {
                if (std::chrono::steady_clock::now() > deadline) { lost(); break; } // MAME waits forever
                std::this_thread::yield();
                if (transport_) transport_->poll();
            }
        } while (held(is_master));

        if (linkalive_ != 0x01) return;

        // enable wait for vsync
        linktimer_ = framesync_;

        // update "ring buffer" if link established
        // live relay does not send data
        if (linkid_ != 0x00) {
            // check ready-to-send flag
            if (shared_[4] != 0x00) {
                send_data(linkid_, frame_start, frame_size, data_size);

                // save message to "ring buffer"
                frame_offset = frame_start + (linkid_ * frame_size);
                for (int j = 0x00; j < frame_size; j++) shared_[(frame_offset + j) & 0xfff] = buffer0_[1 + j];
            }

            if (is_master) {
                // master sends additional status bytes
                send_data(0xfd, 0x06, 0x0a, data_size);

                // send vsync
                buffer0_[0] = 0xfc;
                buffer0_[1] = 0x01;
                send_frame(data_size);
                if (framesync_) outstanding_++;
            }
        }

        // clear 05
        shared_[5] = 0x00;
    }
}

// One whole frame into buffer0_: data_size, or 0 while it is not all here
// (what has come is kept). The previous cabinet closing the line loses the
// link.
int CommBoard::read_frame(int data_size) {
    if (!transport_) return 0;
    if (!transport_->rx_open()) {
        if (linkalive_ == 0x01) lost(); // rx connection lost
        return 0;
    }
    uint8_t chunk[sizeof buffer0_];
    while (int(rx_.size()) < data_size) {
        const int n = transport_->read(chunk, data_size - int(rx_.size()));
        if (n < 0) {
            if (linkalive_ == 0x01) lost(); // rx connection lost
            return 0;
        }
        if (n == 0) return 0;
        rx_.insert(rx_.end(), chunk, chunk + n);
    }
    std::memcpy(buffer0_, rx_.data(), size_t(data_size));
    rx_.erase(rx_.begin(), rx_.begin() + data_size);
    return data_size;
}

void CommBoard::send_data(uint8_t frame_type, int frame_start, int frame_size, int data_size) {
    buffer0_[0] = frame_type;
    for (int i = 0x00; i < frame_size; i++) buffer0_[1 + i] = shared_[(frame_start + i) & 0xfff];
    send_frame(data_size);
}

void CommBoard::send_frame(int data_size) {
    if (!transport_) return;
    if (!transport_->tx_open() || !transport_->write(buffer0_, data_size)) {
        if (linkalive_ == 0x01) lost(); // tx connection lost
    }
}

} // namespace m1
