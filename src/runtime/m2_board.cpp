// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese
//
// The Model 2 board as native runtime (see m2_board.h). Register semantics
// follow MAME's src/mame/sega/model2.cpp at
// dddd73680656e355bb2b5beecab1167c9f07bf81 (BSD-3-Clause; notice above kept):
// irq_request_r/irq_ack_w/irq_enable_w/irq_update, timers_w, videoctl_r/w,
// render_mode_r/w, tgpid_r, the model2o memory map. The I/O board's mailbox
// protocol was read from the game's own traffic (see IoBoard). See
// THIRD_PARTY.md.

#include "runtime/m2_board.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rt {

// ---------------------------------------------------------------------------
// I/O board

void IoBoard::write(uint32_t index, uint8_t v) {
    index &= 0x7ff;
    ram_[index] = v;
    if (index == 0x11 && drive_commands.size() < kMaxDrive) drive_commands.push_back(v); // to the drive board
    if (index != 0x20) return;
    switch (v) {
    case 1: // latch inputs
        ram_[0] = inputs.steer;
        ram_[1] = inputs.accel;
        ram_[2] = inputs.brake;
        for (int i = 3; i < 8; i++) ram_[size_t(i)] = 0xff; // unused ADC channels
        ram_[8] = inputs.in0;
        ram_[9] = inputs.in1;
        ram_[10] = inputs.in2;
        if (kDesert) // Desert Tank reads bytes 0x0b-0x0e too (MAME's model1io: 0xff, its DIP switches all off)
            for (int i = 0x0b; i <= 0x0e; i++) ram_[size_t(i)] = 0xff;
        if (kVcop) { // Virtua Cop's board (model1io2, epr-17181): no ADC; the guns from its FPGA
            for (int i = 0; i < 3; i++) ram_[size_t(i)] = 0xff;
            ram_[0x0b] = ram_[0x0c] = ram_[0x0d] = 0xff;
            ram_[0x0e] = 0x0f;
            for (int n = 0; n < 2; n++) { // P1_Y, P1_X, P2_Y, P2_X, low byte first
                ram_[size_t(0x80 + 4 * n)] = uint8_t(inputs.gun_y[n]);
                ram_[size_t(0x81 + 4 * n)] = uint8_t(inputs.gun_y[n] >> 8);
                ram_[size_t(0x82 + 4 * n)] = uint8_t(inputs.gun_x[n]);
                ram_[size_t(0x83 + 4 * n)] = uint8_t(inputs.gun_x[n] >> 8);
            }
            ram_[0x88] = gun_offscreen(inputs) & 3; // bit 0: P1 off screen, bit 1: P2
        }
        ram_[0x21] = 0x40; // board status the game checks at boot
        ram_[0x20] = 0;
        break;
    case 2: // store settings
        std::memcpy(eeprom.data(), &ram_[0x100], eeprom.size());
        eeprom_dirty = true;
        ram_[0x20] = 0;
        break;
    case 3: // load settings
        std::memcpy(&ram_[0x100], eeprom.data(), eeprom.size());
        ram_[0x20] = 0;
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Board

#ifdef M2_DC_MEMORY
// The Dreamcast: no texture RAM vectors (the frontend's, in video RAM), a
// two-level page table, ROM through img_.rom.
M2Board::M2Board(Images images)
    : img_(std::move(images)), ram_(0x20000), work_(0x100000), cpuctl_(0x1000), backup_(0x4000, 0xff), tile_(0x10000),
      chr_(0x80000), palette_(0x4000), xlat_(0xc000), luma_(0x20000), comm_(0x4000),
      chunks_(size_t(1) << 12), tgp_(img_.copro_tables, *img_.rom) {
    if (!img_.rom || !img_.texture_ram) throw Fatal("M2_DC_MEMORY: no ROM source or texture RAM");
    if (kModel2a) throw Fatal("M2_DC_MEMORY: model2o only");
    img_.copro_tables = {}; // the TGP board keeps its own copy (as words)
    map_rom(0x00000000, 0x001fffff, RomRegion::Program, 0);
    map(0x00200000, 0x0021ffff, Ram, ram_.data());
    map_rom(0x00220000, 0x0023ffff, RomRegion::Program, 0x20000);
#else
M2Board::M2Board(Images images)
    : img_(std::move(images)), ram_(kModel2a ? 0x40000 : 0x20000), work_(0x100000), cpuctl_(0x1000), backup_(0x4000, 0xff),
      tile_(0x10000), chr_(0x80000), palette_(0x4000), xlat_(0xc000), tex0_(0x200000), tex1_(0x200000), luma_(0x20000),
      fb_a_(0x80000), fb_b_(0x80000), comm_(0x4000), pages_(size_t(1) << (32 - kPageBits)),
      tgp_(img_.copro_tables, img_.copro_data) {
    if (kCommAlways) set_link(nullptr, false);
    // model2o memory map (MAME model2_base_mem, model2_tgp_mem, model2o_mem);
    // model2a_crx_mem: 256 KB of RAM, no ROM mirror
    map(0x00000000, 0x001fffff, Rom, img_.program.data());
    if (kModel2a) {
        map(0x00200000, 0x0023ffff, Ram, ram_.data());
    } else {
        map(0x00200000, 0x0021ffff, Ram, ram_.data());
        map(0x00220000, 0x0023ffff, Rom, img_.program.data() + 0x20000);
    }
#endif
    map(0x00500000, 0x005fffff, Ram, work_.data());
    map(0x00800000, 0x00807fff, Dev, nullptr, 0, false);
    map(0x00880000, 0x00887fff, Dev, nullptr, 0, false);
    if (kModel2b && !kModel2c) {
        map(0x008c0000, 0x008c0fff, Dev, nullptr, 0, false); // the SHARC's IOP registers
        map(0x009c0000, 0x009c0fff, Dev, nullptr, 0, false); // i8251 (model2b_crx_mem)
    }
    map(0x00900000, 0x0091ffff, Dev, nullptr, 0x60000, true); // buffer RAM (TgpBoard)
    map(0x00980000, 0x00980fff, Dev, nullptr, 0, false);
    map(0x00e00000, 0x00e00fff, Ram, cpuctl_.data(), 0, false);
    map(0x00e80000, 0x00e80fff, Dev, nullptr, 0, false);
    map(0x00f00000, 0x00f00fff, Dev, nullptr, 0, false);
    map(0x01000000, 0x0100ffff, Ram, tile_.data(), 0x110000);
    map(0x01040000, 0x01040fff, Dev, nullptr, 0x100000, false);
    map(0x01060000, 0x01060fff, Dev, nullptr, 0x100000, false);
    map(0x01080000, 0x010fffff, Ram, chr_.data(), 0x100000);
    map(0x01800000, 0x01803fff, Ram, palette_.data());
    map(0x01810000, 0x0181bfff, Ram, xlat_.data());
    map(0x0181c000, 0x0181cfff, Dev, nullptr, 0, false);
    map(0x01a00000, 0x01a03fff, Ram, comm_.data(), 0x10000);
    map(0x01a04000, 0x01a04fff, Dev, nullptr, 0x10000, false);
    map(0x01c00000, 0x01c00fff, Dev, nullptr, 0, false);
    if (!kModel2b || kModel2c) map(0x01c80000, 0x01c80fff, Dev, nullptr, 0, false); // i8251 (not model2b_crx_mem's)
    map(0x01d00000, 0x01d03fff, Ram, backup_.data());
    if (kCrypt5881) { // model2_5881_mem: the stream's RAM, the chip's registers
        map(0x01d80000, 0x01d8ffff, Ram, crypt_ram_.data());
        map(0x01d90000, 0x01d9ffff, Dev, nullptr, 0, false);
        crypt_ = std::make_unique<Crypt5881>(kCrypt5881Key, [this](uint32_t a) { // crypt_read_callback
            const uint32_t o = (2 * a) & 0xfffe;
            return uint16_t(crypt_ram_[o] << 8 | crypt_ram_[o + 1]);
        });
    }
    if (kDoab) { // model2_0229_mem: the chip's RAM window; its registers in the pages at 0x01d84000 and 0x01d87000
        map(0x01d80000, 0x01d87fff, Ram, comp_ram_.data());
        map(0x01d84000, 0x01d84fff, Dev, nullptr, 0, false);
        map(0x01d87000, 0x01d87fff, Dev, nullptr, 0, false);
    }
#ifdef M2_DC_MEMORY
    map_rom(0x02000000, 0x03ffffff, RomRegion::MainData, 0);
    map_rom(0x06000000, 0x06ffffff, RomRegion::MainData, 0x1000000);
#else
    map(0x02000000, 0x03ffffff, Rom, img_.main_data.data());
    map(0x06000000, 0x06ffffff, Rom, img_.main_data.data() + 0x1000000);
#endif
    map(0x10000000, 0x105fffff, Dev, nullptr, 0, false);
#ifdef M2_DC_MEMORY
    // Frame buffer RAM is optional: Daytona never writes it (0 of 256 pages
    // in 9,000 attract frames and a whole race); without it the range is
    // unmapped, reading 0 as the zeroed RAM would.
    if (img_.frame_buffer_ram) {
        map(0x11600000, 0x1167ffff, Ram, img_.frame_buffer_ram);
        map(0x11680000, 0x116fffff, Ram, img_.frame_buffer_ram + 0x80000);
    }
#else
    map(0x11600000, 0x1167ffff, Ram, fb_a_.data());
    map(0x11680000, 0x116fffff, Ram, fb_b_.data());
#endif
#ifdef M2_DC_MEMORY
    map(0x12000000, 0x121fffff, Tex, img_.texture_ram, 0x200000);
    map(0x12400000, 0x125fffff, Tex, img_.texture_ram + 0x200000, 0x200000);
#else
    if (kModel2b) {
        // model2b_crx_mem: texture RAM 0 and 1 as plain RAM (1 MB each, each
        // mirrored once above), the luma RAM at 0x11400000.
        map(0x11000000, 0x110fffff, Ram, tex0_.data(), 0x100000);
        map(0x11200000, 0x112fffff, Ram, tex1_.data(), 0x100000);
    } else {
        map(0x12000000, 0x121fffff, Tex, tex0_.data(), 0x200000);
        map(0x12400000, 0x125fffff, Tex, tex1_.data(), 0x200000);
    }
#endif
    if (kModel2b)
        map(0x11400000, 0x1140ffff, Ram, luma_.data());
    else
        map(0x12800000, 0x1281ffff, Ram, luma_.data());

#ifdef M2_DC_MEMORY
    geo_ = std::make_unique<Geo>(*img_.rom, tgp_.buffer_data());
#else
    geo_ = std::make_unique<Geo>(img_.polygons, img_.textures, tgp_.buffer_data());
#endif
    video_ = std::make_unique<Video>(tile_.data(), chr_.data());
    video_->enable_write_tracking();
}

#ifdef M2_DC_MEMORY
M2Board::Page M2Board::unmapped_;

M2Board::Page &M2Board::map_page(uint32_t addr) {
    auto &chunk = chunks_[addr >> 20];
    if (!chunk) chunk = std::make_unique<Page[]>(256);
    return chunk[(addr >> kPageBits) & 255];
}

void M2Board::map_rom(uint32_t start, uint32_t end, RomRegion region, uint32_t offset) {
    for (uint64_t a = start; a <= end; a += (1u << kPageBits)) {
        Page &p = map_page(uint32_t(a));
        p.kind = Rom;
        p.burst = true;
        p.base = nullptr;
        p.region = region;
        p.rom_offset = offset + uint32_t(a - start);
    }
}
#endif

void M2Board::map(uint32_t start, uint32_t end, Kind k, uint8_t *base, uint32_t mirror, bool burst) {
    for (uint32_t m = 0;; m = (m - mirror) & mirror) {
        for (uint64_t a = start; a <= end; a += (1u << kPageBits)) {
#ifdef M2_DC_MEMORY
            Page &p = map_page(uint32_t(a | m));
#else
            Page &p = pages_[uint32_t(a | m) >> kPageBits];
#endif
            p.kind = k;
            p.burst = burst;
            p.base = base ? base + (a - start) : nullptr;
        }
        if (((m - mirror) & mirror) == 0) break;
    }
}

void M2Board::attach(Cpu &cpu, Lockstep &ls) {
    cpu_ = &cpu;
    ls_ = &ls;
    if (kModel2a) io5649_.set_clock([this] { return now_ticks(); }); // the EEPROM's write time
#ifdef M2_FAST_GEN
    cpu.work_ram = work_.data(); // plain RAM: nothing watches its writes (ram_written)
#endif
}

// --- board time and the timers (Model 2A) -----------------------------------

uint64_t M2Board::count_now() const {
#ifdef M2_FAST_GEN
    return ls_->now();
#else
    return ls_->count;
#endif
}

uint64_t M2Board::now_ticks() const { return tick_base_ + ticks_for_instructions(count_now() - count_base_); }

void M2Board::skip_to(uint64_t ticks) {
    if (ticks <= now_ticks()) return;
    tick_base_ = ticks;
    count_base_ = count_now();
}

void M2Board::fire_timers() {
    const uint64_t now = now_ticks();
    for (int n = 0; n < 4; ++n) {
        if (!timer_run_[n] || now < timer_due_[n]) continue;
        timer_run_[n] = false;
        timervals_[n] = 0xfffff;
        const uint32_t line = 1u << (n + 2);
        if (intena_ & line) {
            intreq_ |= line;
            irq_update();
        }
    }
}

uint64_t M2Board::next_timer_due() const {
    uint64_t due = UINT64_MAX;
    for (int n = 0; n < 4; ++n)
        if (timer_run_[n] && (intena_ & (1u << (n + 2)))) due = std::min(due, timer_due_[n]);
    return due;
}

// --- interrupts (MAME irq_update) --------------------------------------------

void M2Board::irq_update() {
    const int8_t want[4] = {int8_t((intreq_ & 0x001) ? 1 : 0), int8_t((intreq_ & 0x002) ? 1 : 0),
                            int8_t((intreq_ & 0x3fc) ? 1 : 0), int8_t((intreq_ & 0xc00) ? 1 : 0)};
    for (int l = 0; l < 4; l++)
        if (want[l] != lines_[l]) {
            lines_[l] = want[l];
            cpu_->execute_set_input(l, want[l]);
            ls_->poke();
        }
}

void M2Board::vblank_start() {
    // 60 Hz mode or an even frame: the geometrizer starts a new frame
    if ((videocontrol_ & 1) == 0 || (frame_ & 1) == 0) {
#ifdef M2_DC_SPEED
        // Draw mode: this frame's polygons are shown if it is drawn, or, in
        // 30 Hz mode (no parse next frame), if the next one is.
        if (frame_skip_) {
            const uint64_t n = uint64_t(frame_skip_ + 1);
            geo_->skip_objects = frame_ % n != 0 && ((videocontrol_ & 1) == 0 || (frame_ + 1) % n != 0);
        }
#endif
        geo_->zclip_w(zclip_);
        tgp_.sync(); // the TGP writes display-list data into buffer RAM (Manx TT)
        geo_->parse(tgp_.geo_read_start());
        video_->frame_start();
    }
    if (intena_ & 1) {
        intreq_ |= 1;
        irq_update();
    }
    if (comm_board_) comm_board_->vblank(); // MAME check_vint_irq
}

void M2Board::set_link(LinkTransport *transport, bool framesync) {
    if (!transport) {
        comm_board_.reset();
        if (kCommAlways) {
            comm_board_ = std::make_unique<CommBoard>(comm_.data());
            comm_board_->set_transport(&loopback_);
        }
        return;
    }
    comm_board_ = std::make_unique<CommBoard>(comm_.data());
    comm_board_->set_transport(transport);
    comm_board_->set_framesync(framesync);
}

void M2Board::set_open_frustum(float scale) { geo_->set_open_frustum(scale); }

void M2Board::set_wide_margin(int pixels) {
    geo_->set_wide_margin(pixels);
    video_->set_wide_margin(pixels);
    scenery_.width = video_->width();
}

void M2Board::vblank_end() {
    if (frame_skip_ && frame_ % uint64_t(frame_skip_ + 1) != 0) { // draw mode: keep the last picture
        ++frame_;
        return;
    }
    VideoMem m;
    m.palram = palette_.data();
    m.colorxlat = xlat_.data();
    if (kModel2b) {
        luma_2b_to_dwords(luma_.data(), luma_dw_.data());
        m.lumaram = luma_dw_.data();
    } else {
        m.lumaram = luma_.data();
    }
#ifdef M2_DC_MEMORY
    m.tex0 = reinterpret_cast<const uint32_t *>(img_.texture_ram);
    m.tex1 = reinterpret_cast<const uint32_t *>(img_.texture_ram + 0x200000);
#else
    m.tex0 = reinterpret_cast<const uint32_t *>(tex0_.data());
    m.tex1 = reinterpret_cast<const uint32_t *>(tex1_.data());
#endif
    m.tex_generation = tex_generation_;
    video_->screen_update(geo_->polys, geo_->windows(), m);
    ++frame_;
}

bool M2Board::in_idle_loop() const {
    const uint32_t ip = cpu_->m_IP;
    // Sega Rally: polls videoctl_r's frame bit until it changes (0x5a379c-
    // 0x5a37b0, in the RAM copy of its code); MAME takes 99% of the vblank
    // interrupts of a 3,000-frame attract run there.
    if (kSegaRally) return ip >= 0x005a379c && ip <= 0x005a37b3;
    // Manx TT: the same poll, from ROM (0x3f1c-0x3f30); 97% of MAME's
    // vblank interrupts in a 3,000-frame attract run land there.
    if (kManxTT) return ip >= 0x3f1c && ip <= 0x3f33;
    // Virtua Cop 2: until the frame counter at 0x00500000 changes (0x36fc-
    // 0x3704); 95% of MAME's interrupts in 3,000 frames of attract.
    if (kVcop2) return ip >= 0x36fc && ip <= 0x3707;
    // Virtua Cop: polls the frame bit (0x0098000c bit 2) at 0x9124-0x9134;
    // 91% of MAME's interrupts in 3,000 frames of attract.
    if (kVcop) return ip >= 0x9124 && ip <= 0x9137;
    // Virtua Fighter 2: until the frame counter at 0x00500000 changes
    // (0x10f98-0x10fa0); 77% of MAME's vblank interrupts in 3,000 frames of attract.
    if (kVf2) return ip >= 0x10f98 && ip <= 0x10fa3;
    // INDY 500: polls the frame bit (0x0098000c bit 2) at 0x34914-0x34924;
    // all of MAME's vblank interrupts in 3,000 frames of attract land there.
    if (kIndy500) return ip >= 0x34914 && ip <= 0x34927;
    // Super GT 24h: the frame bit poll (0x0098000c bit 2) at 0x5ae08-0x5ae18
    // and the wait for a flag at 0x00532f30 (0x5b3e0-0x5b3e8, set by the
    // vblank handler); MAME takes most of its interrupts in attract there.
    if (kSgt24h) return (ip >= 0x5ae08 && ip <= 0x5ae1b) || (ip >= 0x5b3e0 && ip <= 0x5b3eb);
    // Over Rev: the frame bit poll (0x0098000c bit 2) at 0x12c44-0x12c54;
    // all of MAME's vblank interrupts in 3,000 frames of attract land there.
    if (kOverrevb) return ip >= 0x12c44 && ip <= 0x12c57;
    // Fighting Vipers: until the frame counter at 0x00500000 changes
    // (0x118dc-0x118e4); most of MAME's vblank interrupts in 3,000 frames of attract.
    if (kFvipers) return ip >= 0x118dc && ip <= 0x118e7;
    // The game's wait-for-vblank loops (0x12b0: until the frame counter at
    // 0x00500000 changes; 0x12f0: until it reaches 2). MAME takes 99% of
    // vblank interrupts here; the rest land in CPU-bound code such as the
    // boot-time texture upload at 0x1388.
    return (ip >= 0x12b0 && ip <= 0x12bb) || (ip >= 0x12f0 && ip <= 0x12ff);
}

// --- sound UART (i8251, transmit side) ------------------------------------------
// The game sends bytes from its IRQ3 handler whenever TxRDY is set. With no
// clock, a byte moves to the shifter and out at once; the sound runtime
// receives it immediately.

void M2Board::uart_txrdy(bool state) {
    if (state == uart_txrdy_) return;
    uart_txrdy_ = state;
    // MAME sound_ready_w: TxRDY (or RxRDY) sets request bit 10 if enabled
    if (state && (intena_ & (1u << 10))) {
        intreq_ |= 1u << 10;
        irq_update();
    }
}

void M2Board::uart_write_data(uint8_t v) {
    uart_hold_ = v;
    uart_have_hold_ = true;
    uart_txrdy(false);
    if (!uart_shift_busy_) {
        uart_shift_busy_ = true;
#ifdef M2_FAST_GEN
        ls_->add_callback(ls_->now() + 1, [this] { uart_shift_done(); }); // (now(): Lockstep::pending)
#else
        ls_->add_callback(ls_->count + 1, [this] { uart_shift_done(); });
#endif
    }
}

void M2Board::uart_shift_done() {
    uart_shift_busy_ = false;
    if (uart_have_hold_) {
        uart_out_.push_back(uart_hold_);
        uart_have_hold_ = false;
    }
    uart_txrdy(true);
}

// --- device registers --------------------------------------------------------

uint32_t M2Board::dev_read(uint32_t addr, uint32_t mask) {
    if (addr >= 0x00900000 && addr <= 0x0097ffff) {
        tgp_.sync(); // the TGP answers through its mailbox here (0x0091fff0-8)
        return tgp_.buffer_r(addr & 0x1ffff);
    }
    if (addr >= 0x00800000 && addr <= 0x00803fff) return tgp_.geo_r((addr - 0x00800000) >> 2);
    if (addr >= 0x00804000 && addr <= 0x00807fff) return 0xffffffffu; // geo_prg_r
    if (addr >= 0x00884000 && addr <= 0x00887fff) return tgp_.fifo_r();
    switch (addr) {
    case 0x00980000: return tgp_.coproctl_r();
#if M2_MODEL2B || M2_MODEL2C
    case 0x00980014: return tgp_.status_r();
#endif
#if M2_MODEL2B
    case 0x009c0004: return (mask & 0xff) ? uint32_t(0x84 | (uart_txrdy_ ? 1 : 0)) : 0; // i8251 status, umask32 0xff
#endif
    case 0x00980004: {
        const uint32_t v = tgp_.fifo_out_empty() ? 1 : 0;
#ifndef M2_FAST_BUS // (a desktop debugging print: getenv on every status read)
        if (std::getenv("M2RUN_VERBOSE")) std::fprintf(stderr, "fifo status read -> %u (frame %llu)\n", v, (unsigned long long)frame_);
#endif
        return v;
    }
    case 0x0098000c: { // videoctl_r
        const uint32_t framenum = render_mode_ ? uint32_t((frame_ & 1) << 2) : uint32_t((frame_ & 2) << 1);
        return framenum | (videocontrol_ & 3);
    }
    case 0x00e80000: return intreq_;
    case 0x00e80004: return intena_;
    default: break;
    }
    if (addr >= 0x00980030 && addr <= 0x0098003f) { // tgpid_r, byte-wide
        static const uint8_t id[] = {0, 'T', 'A', 'H', 0, 'A', 'K', 'O', 0, 'Z', 'A', 'K', 0, 'M', 'T', 'K'};
        const uint32_t o = addr - 0x00980030;
        return uint32_t(id[o]) | uint32_t(id[o + 1]) << 8 | uint32_t(id[o + 2]) << 16 | uint32_t(id[o + 3]) << 24;
    }
    if (addr >= 0x00f00000 && addr <= 0x00f0000f) {
        const uint32_t n = (addr >> 2) & 3;
        if (kModel2a && timer_run_[n]) // MAME timers_r: the start value less the elapsed ticks
            timervals_[n] = timer_orig_[n] - uint32_t(now_ticks() - timer_start_[n]);
        return timervals_[n];
    }
    if ((addr & ~0x10000u) == 0x01a04000) { // cn_r, fg_r
        if (comm_board_) // fg_r takes frames in: only when that lane is read
            return uint32_t(comm_board_->cn_r()) | ((mask & 0xff0000) ? uint32_t(comm_board_->fg_r()) << 16 : 0);
        return uint32_t(comm_cn_ | 0xfe) | uint32_t(comm_fg_) << 16;
    }
    if (kModel2a && addr >= 0x01c00000 && addr <= 0x01c0001f) { // 315-5649 through umask 0x00ff00ff
        const uint32_t i = ((addr & 0x1f) >> 2) * 2;
        return ((mask & 0x000000ff) ? uint32_t(io5649_.read(i)) : 0) | ((mask & 0x00ff0000) ? uint32_t(io5649_.read(i + 1)) << 16 : 0);
    }
    if (kModel2a && (!kModel2b || kModel2c) && addr == 0x01c80000) // i8251 status in lane 2: DSR, TxEMPTY, TxRDY (nothing is received)
        return (mask & 0x00ff0000) ? uint32_t(0x84 | (uart_txrdy_ ? 1 : 0)) << 16 : 0;
    if (!kModel2a && addr >= 0x01c00000 && addr <= 0x01c00fff) { // MB8421 through umask 0x00ff00ff
        const uint32_t i = ((addr & 0xfff) >> 2) * 2;
        return uint32_t(io_.read(i)) | uint32_t(io_.read(i + 1)) << 16;
    }
    if (kDoab && addr >= 0x01d80000 && addr <= 0x01d87fff) {
        if (addr == 0x01d8400c) { // doa_unk_r: a busy flag, toggled on every read
            comp_busy_ = !comp_busy_;
            return comp_busy_ ? 0xffff : 0xfff0;
        }
        if (addr == 0x01d87ff8) { // doa_prot_r: two decompressed bytes per 16-bit lane, the upper lane first
            uint32_t v = 0;
            if (mask & 0xffff0000) { const uint32_t hi = comp_byte(); v |= (hi << 8 | comp_byte()) << 16; }
            if (mask & 0x0000ffff) { const uint32_t hi = comp_byte(); v |= hi << 8 | comp_byte(); }
            return v;
        }
        uint32_t v;
        std::memcpy(&v, &comp_ram_[addr & 0x7ffc], 4);
        return v & mask;
    }
    if (kCrypt5881 && addr >= 0x01d90000 && addr <= 0x01d9ffff) { // iomap_le: 16-bit registers, both lanes
        const uint32_t o = addr & 0xfffc;
        return ((mask & 0x0000ffff) ? uint32_t(crypt_->read16(o)) : 0) | ((mask & 0xffff0000) ? uint32_t(crypt_->read16(o + 2)) << 16 : 0);
    }
    if (addr >= 0x10000000 && addr <= 0x101fffff) return uint32_t(render_unk_) << 14 | uint32_t(render_mode_) << 2 | uint32_t(render_test_);
    if (addr >= 0x10400000 && addr <= 0x105fffff) return uint32_t(geo_->polys.size()); // polygon_count_r
    (void)mask;
    return 0;
}

void M2Board::dev_write(uint32_t addr, uint32_t data, uint32_t mask) {
    if (addr >= 0x00900000 && addr <= 0x0097ffff) { tgp_.buffer_w(addr & 0x1ffff, data, mask); return; }
    if (addr == 0x00980000) { tgp_.coproctl_w(data, mask); return; }
    const uint32_t d = data & mask; // handlers without a mask see unwritten lanes as 0
    if (addr >= 0x00884000 && addr <= 0x00887fff) { tgp_.fifo_w(d); return; }
    if (addr >= 0x00880000 && addr <= 0x00883fff) { tgp_.function_port_w((addr - 0x00880000) >> 2, d); return; }
#if M2_MODEL2B
    if (addr >= 0x008c0000 && addr <= 0x008c0fff) { tgp_.iop_w((addr - 0x008c0000) >> 2, d); return; }
    if (addr == 0x009c0000) { // i8251, umask32 0x000000ff: data at offset 0, mode/command at 4
        if (mask & 0x000000ff) uart_write_data(uint8_t(data));
        return;
    }
#endif
    if (addr >= 0x00804000 && addr <= 0x00807fff) { tgp_.geo_prg_w(d); return; }
    if (addr >= 0x00800000 && addr <= 0x00803fff) { tgp_.geo_w((addr - 0x00800000) >> 2, d); return; }
    switch (addr) {
    case 0x00980008: tgp_.geoctl_w(d); return;
    case 0x0098000c: videocontrol_ = (videocontrol_ & ~mask) | (data & mask); return;
    case 0x00e80000: intreq_ &= data | ~mask; irq_update(); return; // irq_ack_w
    case 0x00e80004: {                                                // irq_enable_w (MAME delays 80 ns)
        intena_ = (intena_ & ~mask) | (data & mask);
        if (uart_txrdy_ && (intena_ & (1u << 10))) intreq_ |= 1u << 10; // irq_mask_delayed_update
        irq_update();
        return;
    }
    case 0x0181c000: zclip_ = d; return;
    default: break;
    }
    if (addr >= 0x00f00000 && addr <= 0x00f0000f) {
        // timers_w: count down at 25 MHz and raise request bit 2+n. Daytona
        // reloads timer 0 each frame and never enables its interrupt, so the
        // value is kept without a clock.
        const uint32_t n = (addr >> 2) & 3;
        uint32_t &t = timervals_[n];
        t = (t & ~mask) | (data & mask);
        if (kModel2a) { // Sega Rally runs its game timing on timers 0 and 2
            timer_orig_[n] = t;
            timer_start_[n] = now_ticks();
            timer_due_[n] = timer_start_[n] + t;
            timer_run_[n] = true;
            // A 0 runs out at once: MAME's timeslice ends at the write and
            // the timer fires under the interrupt mask written just before
            // (Over Rev stops timers 1 and 2 with their interrupts masked,
            // then unmasks them; fired later, they would interrupt again and
            // again). Free running only: under lockstep MAME's log brings the
            // interrupts.
            if (t == 0 && ls_->free_run()) fire_timers();
        }
        return;
    }
    if ((addr & ~0x100000u) == 0x01040000) { if (mask & 0xffff) video_->xhout_w(uint16_t(data)); return; }
    if ((addr & ~0x100000u) == 0x01060000) { if (mask & 0xffff) video_->xvout_w(uint16_t(data)); return; }
    if ((addr & ~0x10000u) == 0x01a04000) {
        if (comm_board_) {
            if (mask & 0xff) comm_board_->cn_w(uint8_t(data));
            if (mask & 0xff0000) comm_board_->fg_w(uint8_t(data >> 16));
            return;
        }
        if (mask & 0xff) comm_cn_ = uint8_t(data & 1);
        if (mask & 0xff0000) comm_fg_ = uint8_t(data >> 16);
        return;
    }
    if (kModel2a) {
        if (addr >= 0x01c00000 && addr <= 0x01c0001f) {
            const uint32_t i = ((addr & 0x1f) >> 2) * 2;
            if (mask & 0x000000ff) io5649_.write(i, uint8_t(data));
            if (mask & 0x00ff0000) io5649_.write(i + 1, uint8_t(data >> 16));
            return;
        }
        if (addr >= 0x01c00020 && addr <= 0x01c00fff) return; // 0x01c00040: nopw
    } else if (addr >= 0x01c00000 && addr <= 0x01c00fff) {
        const uint32_t i = ((addr & 0xfff) >> 2) * 2;
        if (mask & 0x000000ff) io_.write(i, uint8_t(data));
        if (mask & 0x00ff0000) io_.write(i + 1, uint8_t(data >> 16));
        return;
    }
    if (addr >= 0x01c80000 && addr <= 0x01c80003) { // i8251, umask16 0x00ff: lane 0 data, lane 2 control
        if (mask & 0x000000ff) uart_write_data(uint8_t(data));
        return; // mode/command bytes: transmitter enabled by the game at boot
    }
    if (kDoab && addr >= 0x01d80000 && addr <= 0x01d87fff) {
        if (addr == 0x01d87ff0) { comp_src_ = ((comp_src_ & ~mask) | (data & mask)) & 0x7fffff; return; } // srcaddr_w
        if (addr == 0x01d87ff4) return; // data_w_doa: the decompression tables (unused by MAME's DOA mode)
        uint32_t v;
        std::memcpy(&v, &comp_ram_[addr & 0x7ffc], 4);
        v = (v & ~mask) | (data & mask);
        std::memcpy(&comp_ram_[addr & 0x7ffc], &v, 4);
        return;
    }
    if (kCrypt5881 && addr >= 0x01d90000 && addr <= 0x01d9ffff) {
        const uint32_t o = addr & 0xfffc;
        if (mask & 0x0000ffff) crypt_->write16(o, uint16_t(data), uint16_t(mask));
        if (mask & 0xffff0000) crypt_->write16(o + 2, uint16_t(data >> 16), uint16_t(mask >> 16));
        return;
    }
    if (addr >= 0x10000000 && addr <= 0x101fffff) { // render_mode_w
        render_test_ = data & 1;
        render_mode_ = (data >> 2) & 1;
        render_unk_ = (data >> 14) & 1;
        return;
    }
}

// MAME's HACK_MODE_DOA: the one string the game reads from the chip at boot, then zeros.
uint8_t M2Board::comp_byte() {
    static constexpr char kString[] = "  TECMO LTD.  DEAD OR ALIVE  1996.10.22  VER. 1.00";
    const uint8_t b = comp_src_ < 50 ? uint8_t(kString[comp_src_]) : 0;
    comp_src_ = (comp_src_ + 1) & 0x7fffff;
    return b;
}

// Palette and colour translation live in RAM pages; the video output tracks
// what MAME's handlers do on each write.
void M2Board::ram_written(uint32_t addr, uint32_t data, uint32_t mask) {
    (void)data;
    if (kModel2b && addr >= 0x11000000 && addr <= 0x113fffff) ++tex_generation_; // the hardware renderer re-uploads
    if ((addr >= 0x01000000 && addr <= 0x0100ffff) ||
        (addr >= 0x01110000 && addr <= 0x0111ffff)) video_->tile_memory_w();
    else if ((addr >= 0x01080000 && addr <= 0x010fffff) ||
             (addr >= 0x01180000 && addr <= 0x011fffff)) video_->character_memory_w();
    if (addr >= 0x01800000 && addr <= 0x01803fff) {
        for (uint32_t lane = 0; lane < 2; lane++)
            if ((mask >> (16 * lane)) & 0xffff) video_->palette_w(((addr & 0x3fff) >> 1) + lane, palette_.data(), xlat_.data());
    } else if (addr >= 0x01810000 && addr <= 0x0181bfff) {
        for (uint32_t lane = 0; lane < 2; lane++)
            if ((mask >> (16 * lane)) & 0xffff) video_->colorxlat_w(((addr - 0x01810000) >> 1) + lane);
    }
}

void M2Board::tex_write(const Page &p, uint32_t addr, uint32_t lane_data) {
    uint8_t *const base = p.base - ((addr & 0x1fffff) & ~((1u << kPageBits) - 1));
    const uint32_t o = (addr & 0x1fffff) >> 2;
    uint8_t *const w = base + (o >> 1) * 4 + (o & 1) * 2;
    w[0] = uint8_t(lane_data);
    w[1] = uint8_t(lane_data >> 8);
    ++tex_generation_; // the hardware renderer re-uploads texture RAM when this moves
}

// --- bus ---------------------------------------------------------------------

uint32_t M2Board::fetch(uint32_t addr) {
    const Page &p = page(addr);
    if (p.kind != Rom && p.kind != Ram) throw Fatal("instruction fetch from a device");
    uint32_t v;
#ifdef M2_DC_MEMORY
    if (p.kind == Rom) {
        std::memcpy(&v, M2_AL(rom_page(p) + (addr & 0xffc), 4), 4);
        return v;
    }
#endif
    std::memcpy(&v, M2_AL(p.base + (addr & 0xffc), 4), 4);
    return v;
}

uint8_t M2Board::read_byte(uint32_t addr) {
#ifdef M2_FAST_BUS
    if ((addr >> kPageBits) == fast_read_page_ && addr != 0x00500000u) return fast_read_base_[addr & 0xfff];
#endif
    const Page &p = page(addr);
    const unsigned sh = (addr & 3) * 8;
#ifdef M2_DC_SPIN_SKIP
    // The game's wait for the next frame (0x1394: ldob 0x500000,r3; 0x139c:
    // cmpibe r3,g0,0x1394), about 3/4 of its instructions in a race: every
    // frame runs to the instruction cap (in_idle_loop does not list it, and
    // that timing is the game's). Each pass reads the same RAM byte, compares
    // equal and branches back, and nothing changes the byte until the next
    // lockstep event (an interrupt, a callback). So when this read will
    // compare equal, the passes up to just before that event are skipped:
    // the count moves on by whole passes, to the same state the passes would
    // have left. This read is under way (its boundary passed at `count`), so
    // the last skipped pass ends at a count below the event's.
    if (addr == 0x00500000u && cpu_->m_IP == 0x1394u && p.kind == Ram) {
        const uint8_t v = p.base[addr & 0xfff];
        if (v == cpu_->m_r[16]) {
#ifdef M2_FAST_GEN
            const uint64_t limit = std::min(ls_->next_count, ls_->end_count), count = ls_->now();
#else
            const uint64_t limit = std::min(ls_->next_count, ls_->end_count), count = ls_->count;
#endif
            if (limit > count + 2) {
                const uint64_t skip = (limit - 1 - count) / 2 * 2;
                ls_->count += skip;
                spin_skipped_ += skip;
#ifdef M2_FAST_GEN
                ++ls_->epoch; // the count jumped: the fast generated code checks in full next
#endif
            }
        }
        return v;
    }
#endif
#ifdef M2_DC_MEMORY
    if (p.kind == Rom) return rom_page(p)[addr & 0xfff];
#endif
    switch (p.kind) {
#ifdef M2_FAST_BUS
    case Ram: case Tex: fast_read(addr, p); return p.base[addr & 0xfff];
    case Rom: return p.base[addr & 0xfff];
#else
    case Rom: case Ram: case Tex: return p.base[addr & 0xfff];
#endif
    case Dev: return uint8_t(dev_read(addr & ~3u, 0xffu << sh) >> sh);
    default: return 0;
    }
}

uint16_t M2Board::read_word(uint32_t addr) {
    addr &= ~1u;
#ifdef M2_FAST_BUS
    if ((addr >> kPageBits) == fast_read_page_) {
        uint16_t v;
        std::memcpy(&v, M2_AL(fast_read_base_ + (addr & 0xfff), 2), 2);
        return v;
    }
#endif
    const Page &p = page(addr);
    const unsigned sh = (addr & 2) * 8;
#ifdef M2_DC_MEMORY
    if (p.kind == Rom) {
        uint16_t v;
        std::memcpy(&v, M2_AL(rom_page(p) + (addr & 0xfff), 2), 2);
        return v;
    }
#endif
    switch (p.kind) {
#ifdef M2_FAST_BUS
    case Ram: case Tex: fast_read(addr, p); [[fallthrough]];
    case Rom: {
#else
    case Rom: case Ram: case Tex: {
#endif
        uint16_t v;
        std::memcpy(&v, M2_AL(p.base + (addr & 0xfff), 2), 2);
        return v;
    }
    case Dev: return uint16_t(dev_read(addr & ~3u, 0xffffu << sh) >> sh);
    default: return 0;
    }
}

uint32_t M2Board::read_dword(uint32_t addr) {
    addr &= ~3u;
#ifdef M2_FAST_BUS
    // The TGP's FIFO, status and buffer RAM (dev_read's answers), without the
    // page table and the dispatch chain: tens of millions of reads a race.
    if ((addr & 0xffffc000u) == 0x00884000u) return tgp_.fifo_r();
    if (addr == 0x00980004u) return tgp_.fifo_out_empty() ? 1 : 0;
    if (addr - 0x00900000u < 0x80000u) {
        tgp_.sync();
        return tgp_.buffer_r(addr & 0x1ffff);
    }
#endif
#ifdef M2_FAST_BUS
    if ((addr >> kPageBits) == fast_read_page_) {
        uint32_t v;
        std::memcpy(&v, M2_AL(fast_read_base_ + (addr & 0xfff), 4), 4);
        return v;
    }
#endif
    const Page &p = page(addr);
#ifdef M2_DC_MEMORY
    if (p.kind == Rom) {
        uint32_t v;
        std::memcpy(&v, M2_AL(rom_page(p) + (addr & 0xfff), 4), 4);
        return v;
    }
#endif
    switch (p.kind) {
#ifdef M2_FAST_BUS
    case Ram: case Tex: fast_read(addr, p); [[fallthrough]];
    case Rom: {
#else
    case Rom: case Ram: case Tex: {
#endif
        uint32_t v;
        std::memcpy(&v, M2_AL(p.base + (addr & 0xfff), 4), 4);
        return v;
    }
    case Dev: return dev_read(addr, 0xffffffffu);
    default: return 0;
    }
}

void M2Board::write_byte(uint32_t addr, uint8_t data) {
#ifdef M2_FAST_BUS
    if ((addr >> kPageBits) == fast_write_page_) {
        fast_write_base_[addr & 0xfff] = data;
        return;
    }
#endif
    const Page &p = page(addr);
    const unsigned sh = (addr & 3) * 8;
    switch (p.kind) {
    case Ram: {
#ifdef M2_FAST_BUS
        fast_write(addr, p);
#endif
        uint8_t &dst = p.base[addr & 0xfff];
        const bool changed = dst != data;
        dst = data;
        if (changed) ram_written(addr & ~3u, uint32_t(data) << sh, 0xffu << sh);
        return;
    }
    case Tex: tex_write(p, addr, uint32_t(data) << sh); return;
    case Dev: dev_write(addr & ~3u, uint32_t(data) << sh, 0xffu << sh); return;
    default: return;
    }
}

void M2Board::write_word(uint32_t addr, uint16_t data) {
    addr &= ~1u;
#ifdef M2_FAST_BUS
    if ((addr >> kPageBits) == fast_write_page_) {
        std::memcpy(M2_AL(fast_write_base_ + (addr & 0xfff), 2), &data, 2);
        return;
    }
#endif
    const Page &p = page(addr);
    const unsigned sh = (addr & 2) * 8;
    switch (p.kind) {
    case Ram: {
#ifdef M2_FAST_BUS
        fast_write(addr, p);
#endif
        uint16_t old; std::memcpy(&old, M2_AL(p.base + (addr & 0xfff), 2), 2);
        std::memcpy(M2_AL(p.base + (addr & 0xfff), 2), &data, 2);
        if (old != data) ram_written(addr & ~3u, uint32_t(data) << sh, 0xffffu << sh);
        return;
    }
    case Tex: tex_write(p, addr, uint32_t(data) << sh); return;
    case Dev: dev_write(addr & ~3u, uint32_t(data) << sh, 0xffffu << sh); return;
    default: return;
    }
}

void M2Board::write_dword(uint32_t addr, uint32_t data) {
    addr &= ~3u;
#ifdef M2_FAST_BUS
    // The TGP's function port and FIFO (dev_write's, all lanes written),
    // without the page table and the dispatch chain.
    if ((addr & 0xffff8000u) == 0x00880000u) {
        if (addr & 0x4000) tgp_.fifo_w(data);
        else tgp_.function_port_w((addr - 0x00880000u) >> 2, data);
        return;
    }
#endif
#ifdef M2_FAST_BUS
    if ((addr >> kPageBits) == fast_write_page_) {
        std::memcpy(M2_AL(fast_write_base_ + (addr & 0xfff), 4), &data, 4);
        return;
    }
#endif
    const Page &p = page(addr);
    switch (p.kind) {
    case Ram: {
#ifdef M2_FAST_BUS
        fast_write(addr, p);
#endif
        uint32_t old; std::memcpy(&old, M2_AL(p.base + (addr & 0xfff), 4), 4);
        std::memcpy(M2_AL(p.base + (addr & 0xfff), 4), &data, 4);
        if (old != data) ram_written(addr, data, 0xffffffffu);
        return;
    }
    case Tex: tex_write(p, addr, data); return;
    case Dev: dev_write(addr, data, 0xffffffffu); return;
    default: return;
    }
}

} // namespace rt
