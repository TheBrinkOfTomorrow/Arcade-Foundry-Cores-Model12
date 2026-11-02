// Model 1 ROM sets: every file CRC-checked against MAME's ROM_START
// (sega/model1.cpp at the pinned commit) and laid out in the regions MAME
// loads it into (ROM_LOAD16_BYTE / ROM_LOAD32_WORD / ROM_LOAD32_BYTE
// interleave). Input: a MAME zip or 7z, or a folder of the loose files.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace m1 {

struct RomFile {
    const char *name;
    uint32_t crc;
    uint32_t size;
    const char *region;
    uint32_t offset;
    enum Kind : uint8_t { Plain, Byte16, Word32, Byte32, WordSwap16 } kind;
    // A device's ROM (MAME keeps it in the device's own archive, e.g.
    // model1io2.zip, next to the set's; a non-merged set has it inside)
    const char *device = nullptr;
};

// Code the program copies from ROM into RAM and runs there (for the recompiler)
struct CodeCopy {
    uint32_t from, to, bytes;
};

struct RomSet {
    const char *name;   // MAME short name
    const char *title;
    std::vector<RomFile> files;
    // Where the V60 runs the program ROM from (for the recompiler's guesses)
    uint32_t code_lo, code_hi;
    // The sound 68000's instructions a second under MAME (the board runs on
    // instruction counts): its log's count over the run's emulated time
    uint64_t sound_ips;
    // The I/O board's BIOS is EPR-14869B (m1::IoBoard::set_bios_b)
    bool io_bios_b = false;
    // The advanced I/O board (model1io2, m1::IoBoard2): its BIOS in io2_bios
    bool io2 = false;
    std::vector<CodeCopy> code_copies = {};
};

const RomSet *find_rom_set(const std::string &name);

// Region sizes and fill (MAME's ROM_REGION, ROMREGION_ERASEFF)
struct Region {
    const char *name;
    uint32_t size;
    uint8_t fill;
};
const std::vector<Region> &regions();

using Images = std::map<std::string, std::vector<uint8_t>>;

// Loads the set from a zip, a 7z or a folder (a parent's files are looked up
// in the same place; a device's in the set's, then in the device's archive or
// folder beside it). A file is found by its name or else by its size and
// CRC. Throws std::runtime_error naming the first problem.
Images load_rom_set(const RomSet &set, const std::string &path);

} // namespace m1
