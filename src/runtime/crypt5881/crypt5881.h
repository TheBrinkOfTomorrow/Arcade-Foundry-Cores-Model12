// The Sega 315-5881 security chip (Zero Gunner, Pilot Kids, Dynamite Cop):
// the game writes an encrypted stream into RAM at 0x01d80000 and reads it
// back decrypted through the chip's registers at 0x01d90000 (MAME
// model2_5881_mem, the chip's iomap_le). The decryption is MAME's
// sega_315_5881_crypt_device, transplanted by scripts/transplant_5881.py
// (mame/decls.inc, mame/crypt.inc); this class stands in for its device
// framework.
#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>

namespace rt {

class Crypt5881 {
public:
    // key: the game's (MAME's ROM_PARAMETER ":315_5881:key"); read: a 16-bit
    // word of the encrypted stream at word address addr (MAME's
    // crypt_read_callback: RAM 0x01d80000 + 2 * addr, byte-swapped).
    Crypt5881(uint32_t key, std::function<uint16_t(uint32_t)> read) : m_read(std::move(read)), key(key) {
        buffer = std::make_unique<uint8_t[]>(BUFFER_SIZE);
        line_buffer = std::make_unique<uint8_t[]>(LINE_SIZE);
        line_buffer_prev = std::make_unique<uint8_t[]>(LINE_SIZE);
        reset();
    }
    // MAME device_reset
    void reset() {
        std::memset(buffer.get(), 0, BUFFER_SIZE);
        std::memset(line_buffer.get(), 0, LINE_SIZE);
        std::memset(line_buffer_prev.get(), 0, LINE_SIZE);
        prot_cur_address = 0;
        subkey = 0;
        dec_hist = 0;
        dec_header = 0;
        enc_ready = false;
        first_read = false;
        buffer_pos = 0;
        line_buffer_pos = 0;
        line_buffer_size = 0;
        buffer_bit = 0;
    }

    // iomap_le, by byte offset in the 64 KB window: 0 ready, 8 address low,
    // 0xa address high, 0xc subkey, 0xe the decrypted stream.
    uint16_t read16(uint32_t offset) {
        switch (offset & 0xfffe) {
        case 0x0: return ready_r();
        case 0xe: return decrypt_le_r();
        default: return 0;
        }
    }
    void write16(uint32_t offset, uint16_t data, uint16_t mem_mask = 0xffff) {
        switch (offset & 0xfffe) {
        case 0x8: addrlo_w(data); break;
        case 0xa: addrhi_w(0, data, mem_mask); break;
        case 0xc: subkey_le_w(0, data, mem_mask); break;
        default: break;
        }
    }

    uint16_t ready_r();
    void subkey_le_w(uint32_t offset, uint16_t data, uint16_t mem_mask);
    void subkey_be_w(uint32_t offset, uint16_t data, uint16_t mem_mask);
    void addrlo_w(uint16_t data);
    void addrhi_w(uint32_t offset, uint16_t data, uint16_t mem_mask);
    uint16_t decrypt_le_r();
    uint16_t decrypt_be_r();
    uint16_t do_decrypt(uint8_t *&base);
    void set_addr_low(uint16_t data);
    void set_addr_high(uint16_t data);
    void set_subkey(uint16_t data);

private:
    template <typename T> static constexpr int BIT(T x, int n) { return (x >> n) & 1; }
    std::function<uint16_t(uint32_t)> m_read;
#include "runtime/crypt5881/mame/decls.inc"
};

} // namespace rt
