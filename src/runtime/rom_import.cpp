#include "runtime/rom_import.h"

#include "afcommon/archive.h"
#include "afcommon/zip.h"

#include <cstdio>
#include <cstring>
#include <iterator>
#include <span>

namespace rt {

using afc::crc32;
using afc::Archive;
using afc::open_archive;
using afc::ZipError;

namespace {

enum Region { Program, MainData, CoproData, Polygons, Textures, CoproTables, SoundProgram, Pcm1, Pcm2, Samples, DsbProgram, DsbMpeg, Dsb2Program, M1Program };

struct Load {
    const char *file;
    uint32_t crc;
    Region region;
    uint32_t offset, size; // ROM_LOAD32_WORD: 16-bit words into bytes 0-1 (offset 0) or 2-3 (offset 2) of each dword;
                           // the sound regions load whole files (see import_rom_set)
    uint32_t alt_crc = 0;  // another dump of the same ROM that is accepted (0: none)
};

// MAME model2.cpp ROM_START(daytona93), ROM_START(daytona), ROM_START(srallyc)
// and MODEL2_CPU_BOARD at dddd7368. The parts both Daytona sets share come
// first.
#define SHARED_LOADS                                                         \
    {"mpr-16528.10", 0x9CE591F6, MainData, 0x000000, 0x200000},              \
    {"mpr-16529.11", 0xF7095EAF, MainData, 0x000002, 0x200000},              \
    {"mpr-16537.ic28", 0x36B7C35A, CoproData, 0x000000, 0x200000},           \
    {"mpr-16536.ic29", 0x6D6AFED9, CoproData, 0x000002, 0x200000},           \
    {"mpr-16523.ic16", 0x2F484D42, Polygons, 0x000000, 0x200000},            \
    {"mpr-16518.ic20", 0xDF683BF7, Polygons, 0x000002, 0x200000},            \
    {"mpr-16524.ic17", 0x34658BD7, Polygons, 0x400000, 0x200000},            \
    {"mpr-16519.ic21", 0xFACD1C81, Polygons, 0x400002, 0x200000},            \
    {"mpr-16525.ic18", 0xFB517521, Polygons, 0x800000, 0x200000},            \
    {"mpr-16520.ic22", 0xD66BD9BD, Polygons, 0x800002, 0x200000},            \
    {"mpr-16522.25", 0x55D39A57, Textures, 0x000000, 0x200000},              \
    {"mpr-16521.24", 0xAF1934FB, Textures, 0x000002, 0x200000},              \
    {"opr-14742a.45", 0x90C6B117, CoproTables, 0x000000, 0x020000},          \
    {"opr-14743a.46", 0xAE7F446B, CoproTables, 0x000002, 0x020000},          \
    /* Model 1 sound board (segam1audio): the MultiPCMs' sample ROMs */      \
    {"mpr-16491.32", 0x89920903, Pcm1, 0x000000, 0x200000},                  \
    {"mpr-16492.33", 0x459E701B, Pcm1, 0x200000, 0x200000},                  \
    {"mpr-16493.4", 0x9990DB15, Pcm2, 0x000000, 0x200000},                   \
    {"mpr-16494.5", 0x600E1D6C, Pcm2, 0x200000, 0x200000}

// Daytona USA, Deluxe '93 (daytona93).
const Load kDaytona93[] = {
    {"epr-16530a.12", 0x39E962B5, Program, 0x000000, 0x020000},
    {"epr-16531a.13", 0x693126EB, Program, 0x000002, 0x020000},
    {"mpr-16526.8", 0x5273B8B5, MainData, 0x400000, 0x200000},
    {"mpr-16527.9", 0xFC4CB0EF, MainData, 0x400002, 0x200000},
    {"epr-16534a.6", 0x1BB0D72D, MainData, 0x800000, 0x100000},
    {"epr-16535a.7", 0x459A8BFB, MainData, 0x800002, 0x100000},
    {"epr-16646.ic19", 0x7BA9FD6B, Polygons, 0xC00000, 0x080000},
    {"epr-16645.ic23", 0x78FE0B8A, Polygons, 0xC00002, 0x080000},
    {"mpr-16517.27", 0x4705D3DD, Textures, 0x800000, 0x200000},
    {"mpr-16516.26", 0xA260D45D, Textures, 0x800002, 0x200000},
    // the sound board's 68000 program (ROM_LOAD16_WORD_SWAP)
    {"epr-16489.7", 0xC20E543E, SoundProgram, 0x000000, 0x020000},
    {"epr-16490.8", 0xC24EDAAB, SoundProgram, 0x020000, 0x020000},
    SHARED_LOADS,
};

// Daytona USA, Revision A, 1994 (daytona).
const Load kDaytona[] = {
    {"epr-16722a.12", 0x48B94318, Program, 0x000000, 0x020000},
    {"epr-16723a.13", 0x8AF8B32D, Program, 0x000002, 0x020000},
    {"mpr-16808.8", 0x44F1F5A0, MainData, 0x400000, 0x200000},
    {"mpr-16809.9", 0x37A2DD12, MainData, 0x400002, 0x200000},
    {"epr-16724a.6", 0x469F10FD, MainData, 0x800000, 0x080000},
    {"epr-16725a.7", 0xBA0DF8DB, MainData, 0x800002, 0x080000},
    {"mpr-16772.ic19", 0x770ED912, Polygons, 0xC00000, 0x200000},
    {"mpr-16771.ic23", 0xA2205124, Polygons, 0xC00002, 0x200000},
    {"mpr-16770.27", 0xF9FA7BFB, Textures, 0x800000, 0x200000},
    {"mpr-16769.26", 0xE57429E9, Textures, 0x800002, 0x200000},
    {"epr-16720.7", 0x8E73CFFD, SoundProgram, 0x000000, 0x020000},
    {"epr-16721.8", 0x1BB3B7B7, SoundProgram, 0x020000, 0x020000},
    SHARED_LOADS,
};
#undef SHARED_LOADS

// Sega Rally Championship, Revision C (srallyc), Model 2A-CRX. Its own sound
// board: a 68000 with an SCSP, whose sample ROMs load like the 68000's
// program (ROM_LOAD16_WORD_SWAP into a big-endian region). The video board's
// ROMs (MODEL2A_VID_BOARD, "video_unk" in MAME), the drive board's and the
// comm board's are not loaded, as for Daytona.
const Load kSrallyc[] = {
    {"epr-17888c.12", 0x3D6808AA, Program, 0x000000, 0x080000},
    {"epr-17889c.13", 0xF43C7802, Program, 0x000002, 0x080000},
    {"mpr-17746.10", 0x8FE311F4, MainData, 0x000000, 0x200000},
    {"mpr-17747.11", 0x543593FD, MainData, 0x000002, 0x200000},
    {"mpr-17744.8", 0x71FED098, MainData, 0x400000, 0x200000},
    {"mpr-17745.9", 0x8ECCA705, MainData, 0x400002, 0x200000},
    {"mpr-17884.6", 0x4CFC95E1, MainData, 0x800000, 0x200000},
    {"mpr-17885.7", 0xA08D2467, MainData, 0x800002, 0x200000},
    {"mpr-17754.28", 0x81A84F67, CoproData, 0x000000, 0x200000},
    {"mpr-17755.29", 0x2A6E7DA4, CoproData, 0x000002, 0x200000},
    {"mpr-17748.16", 0x3148A2B2, Polygons, 0x000000, 0x200000},
    {"mpr-17750.20", 0x232AEC29, Polygons, 0x000002, 0x200000},
    {"mpr-17749.17", 0x0838D184, Polygons, 0x400000, 0x200000},
    {"mpr-17751.21", 0xED87AC62, Polygons, 0x400002, 0x200000},
    {"mpr-17753.25", 0x6DB0EB36, Textures, 0x000000, 0x200000},
    {"mpr-17752.24", 0xD6AA86CE, Textures, 0x000002, 0x200000},
    {"opr-14742a.45", 0x90C6B117, CoproTables, 0x000000, 0x020000},
    {"opr-14743a.46", 0xAE7F446B, CoproTables, 0x000002, 0x020000},
    {"epr-17890a.30", 0x5BAC3FA1, SoundProgram, 0x000000, 0x040000},
    {"mpr-17756.31", 0x7725F111, Samples, 0x000000, 0x200000},
    {"mpr-17757.32", 0x1616E649, Samples, 0x200000, 0x200000},
    {"mpr-17886.36", 0x54A72923, Samples, 0x400000, 0x200000},
    {"mpr-17887.37", 0x38C31FDD, Samples, 0x600000, 0x200000},
};

// Dynamite Cop (dynamcop, Export, Model 2A-CRX) with the 315-5881 chip (its
// key in m2_board.h): four 512 KB program ROMs, 32 MB of main_data with no
// mirror, no copro data ROMs; 32 MB of polygons, 16 MB of textures.
const Load kDynamcop[] = {
    {"epr-20930.12", 0xB8FC8FF7, Program, 0x0000000, 0x080000},
    {"epr-20931.13", 0x89D13F88, Program, 0x0000002, 0x080000},
    {"epr-20932.14", 0x618A68BF, Program, 0x0100000, 0x080000},
    {"epr-20933.15", 0x13ABE49C, Program, 0x0100002, 0x080000},
    {"mpr-20797.10", 0x87BAB1E4, MainData, 0x0000000, 0x400000},
    {"mpr-20798.11", 0x40DD752B, MainData, 0x0000002, 0x400000},
    {"mpr-20795.8", 0x0EF85E12, MainData, 0x0800000, 0x400000},
    {"mpr-20796.9", 0x870139CB, MainData, 0x0800002, 0x400000},
    {"mpr-20793.6", 0x42EA08F8, MainData, 0x1000000, 0x400000},
    {"mpr-20794.7", 0x8E5CD1DB, MainData, 0x1000002, 0x400000},
    {"mpr-20791.4", 0x4883D0DF, MainData, 0x1800000, 0x400000},
    {"mpr-20792.5", 0x47BECFA2, MainData, 0x1800002, 0x400000},
    {"mpr-20799.16", 0x424571BF, Polygons, 0x0000000, 0x400000},
    {"mpr-20803.20", 0x61A8AD52, Polygons, 0x0000002, 0x400000},
    {"mpr-20800.17", 0x3C2EE808, Polygons, 0x0800000, 0x400000},
    {"mpr-20804.21", 0x03B35CB8, Polygons, 0x0800002, 0x400000},
    {"mpr-20801.18", 0xC6914173, Polygons, 0x1000000, 0x400000},
    {"mpr-20805.22", 0xF6605EDE, Polygons, 0x1000002, 0x400000},
    {"mpr-20802.19", 0xD11B5267, Polygons, 0x1800000, 0x400000},
    {"mpr-20806.23", 0x0C942073, Polygons, 0x1800002, 0x400000},
    {"mpr-20809.25", 0x3B7B4622, Textures, 0x0000000, 0x400000},
    {"mpr-20807.24", 0x1241E0F2, Textures, 0x0000002, 0x400000},
    {"mpr-20810.27", 0x838A10A7, Textures, 0x0800000, 0x400000},
    {"mpr-20808.26", 0x706BD495, Textures, 0x0800002, 0x400000},
    {"opr-14742a.45", 0x90C6B117, CoproTables, 0x000000, 0x020000},
    {"opr-14743a.46", 0xAE7F446B, CoproTables, 0x000002, 0x020000},
    {"epr-20811.30", 0xA154B83E, SoundProgram, 0x0000000, 0x080000},
    {"mpr-20812.31", 0x695B6088, Samples, 0x0000000, 0x200000},
    {"mpr-20813.32", 0x1908679C, Samples, 0x0200000, 0x200000},
    {"mpr-20814.36", 0xE8EBC74C, Samples, 0x0400000, 0x200000},
    {"mpr-20815.37", 0x1B5AAAE4, Samples, 0x0600000, 0x200000},
};

// Sky Target (skytargt), Model 2A-CRX. Two 512 KB program halves; main_data
// 0x1000000's 1 MB mirrored up to 0x1F00000; 4 MB of copro data; 16 MB of
// polygons, 8 MB of textures.
const Load kSkytargt[] = {
    {"epr-18406.12", 0xFDE9C00A, Program, 0x0000000, 0x080000},
    {"epr-18407.13", 0x35F8B529, Program, 0x0000002, 0x080000},
    {"mpr-18415.10", 0xD7A1BBD7, MainData, 0x0000000, 0x400000},
    {"mpr-18416.11", 0xB77C9243, MainData, 0x0000002, 0x400000},
    {"mpr-18417.8", 0xA0D03F63, MainData, 0x0800000, 0x400000},
    {"mpr-18418.9", 0xC7A6F97F, MainData, 0x0800002, 0x400000},
    {"epr-18404.6", 0xF1407EC4, MainData, 0x1000000, 0x080000},
    {"epr-18405.7", 0x00B40F9E, MainData, 0x1000002, 0x080000},
    {"mpr-18420.28", 0x92B87817, CoproData, 0x0000000, 0x200000},
    {"mpr-18419.29", 0x74542D87, CoproData, 0x0000002, 0x200000},
    {"mpr-18413.16", 0x1C4D416C, Polygons, 0x0000000, 0x400000},
    {"mpr-18409.20", 0x666037EF, Polygons, 0x0000002, 0x400000},
    {"mpr-18414.17", 0x858885BA, Polygons, 0x0800000, 0x400000},
    {"mpr-18410.21", 0xB821A695, Polygons, 0x0800002, 0x400000},
    {"mpr-18411.24", 0x9C2DC40C, Textures, 0x0000002, 0x400000},
    {"mpr-18412.25", 0x4DB52F8B, Textures, 0x0000000, 0x400000},
    {"opr-14742a.45", 0x90C6B117, CoproTables, 0x000000, 0x020000},
    {"opr-14743a.46", 0xAE7F446B, CoproTables, 0x000002, 0x020000},
    {"epr-18408.30", 0x6DEB9657, SoundProgram, 0x0000000, 0x080000},
    {"mpr-18424.31", 0x590A4338, Samples, 0x0000000, 0x200000},
    {"mpr-18423.32", 0xC356D765, Samples, 0x0200000, 0x200000},
    {"mpr-18422.36", 0xB4F3CEA6, Samples, 0x0400000, 0x200000},
    {"mpr-18421.37", 0x00522390, Samples, 0x0600000, 0x200000},
};

// Motor Raid, Twin (motoraid), Model 2A-CRX: Manx TT's board. Two 512 KB
// program halves; main_data's last 1 MB (0x1800000) mirrored up to
// 0x1F00000; 2 MB of copro data; 24 MB of polygons, 8 MB of textures.
const Load kMotoraid[] = {
    {"epr-20007.12", 0xF040C108, Program, 0x0000000, 0x080000},
    {"epr-20008.13", 0x78976E1A, Program, 0x0000002, 0x080000},
    {"mpr-20019.10", 0x49053727, MainData, 0x0000000, 0x400000},
    {"mpr-20020.11", 0xCC5DDB15, MainData, 0x0000002, 0x400000},
    {"mpr-20017.8", 0x4E206ACD, MainData, 0x0800000, 0x400000},
    {"mpr-20018.9", 0xE7ED0E85, MainData, 0x0800002, 0x400000},
    {"mpr-20015.6", 0x23427339, MainData, 0x1000000, 0x400000},
    {"mpr-20016.7", 0xC99A83F4, MainData, 0x1000002, 0x400000},
    {"epr-20013.4", 0xA4478F52, MainData, 0x1800000, 0x080000},
    {"epr-20014.5", 0x1AA541BE, MainData, 0x1800002, 0x080000},
    {"mpr-20023.16", 0x016BE8D6, Polygons, 0x0000000, 0x400000},
    {"mpr-20026.20", 0x20044A30, Polygons, 0x0000002, 0x400000},
    {"mpr-20024.17", 0x62FD2D5B, Polygons, 0x0800000, 0x400000},
    {"mpr-20027.21", 0xB2504EA6, Polygons, 0x0800002, 0x400000},
    {"mpr-20025.18", 0xD4ECD0BE, Polygons, 0x1000000, 0x400000},
    {"mpr-20028.22", 0x3147E0E1, Polygons, 0x1000002, 0x400000},
    {"epr-20011.28", 0x794C026C, CoproData, 0x0000000, 0x100000},
    {"epr-20012.29", 0xF53DB4E3, CoproData, 0x0000002, 0x100000},
    {"mpr-20022.25", 0x9E47B3C2, Textures, 0x0000000, 0x400000},
    {"mpr-20021.24", 0x3CBF36CB, Textures, 0x0000002, 0x400000},
    {"opr-14742a.45", 0x90C6B117, CoproTables, 0x000000, 0x020000},
    {"opr-14743a.46", 0xAE7F446B, CoproTables, 0x000002, 0x020000},
    {"epr-20029.30", 0x927D31B9, SoundProgram, 0x0000000, 0x080000},
    {"mpr-20030.31", 0xB70AB686, Samples, 0x0000000, 0x200000},
    {"mpr-20031.32", 0x84DA70E4, Samples, 0x0200000, 0x200000},
    {"mpr-20032.36", 0x15516D35, Samples, 0x0400000, 0x200000},
    {"mpr-20033.37", 0x8C8ED187, Samples, 0x0600000, 0x200000},
};

// Manx TT Superbike, Revision C (manxttc), Model 2A-CRX, the Twin cabinet's
// sound ROMs (MAME's set for them). The same board as Sega Rally: SCSP sound
// board, i960 program in four 128 KB ROMs; main_data's last 1 MB mirrored
// up to 0xF00000. Video board, comm board and default EEPROM not loaded.
const Load kManxttc[] = {
    {"epr-18822c.12", 0xC7B3E45A, Program, 0x000000, 0x020000},
    {"epr-18823c.13", 0x6B0C1DFB, Program, 0x000002, 0x020000},
    {"epr-18824c.14", 0x352BB817, Program, 0x040000, 0x020000},
    {"epr-18825c.15", 0xF88B036C, Program, 0x040002, 0x020000},
    {"mpr-18751.10", 0x773AD43D, MainData, 0x000000, 0x200000},
    {"mpr-18752.11", 0x4DA3719E, MainData, 0x000002, 0x200000},
    {"mpr-18749.8", 0xC3FE0EEA, MainData, 0x400000, 0x200000},
    {"mpr-18750.9", 0x40B55494, MainData, 0x400002, 0x200000},
    {"mpr-18747.6", 0xA65EC1E8, MainData, 0x800000, 0x200000},
    {"mpr-18748.7", 0x375E3748, MainData, 0x800002, 0x200000},
    {"epr-18862.4", 0x9ADC3A30, MainData, 0xC00000, 0x080000},
    {"epr-18863.5", 0x603742E9, MainData, 0xC00002, 0x080000},
    {"mpr-18753.16", 0x33DDAA0D, Polygons, 0x000000, 0x200000},
    {"mpr-18756.20", 0x28713617, Polygons, 0x000002, 0x200000},
    {"mpr-18754.17", 0x09AABDE5, Polygons, 0x400000, 0x200000},
    {"mpr-18757.21", 0x25FC92E9, Polygons, 0x400002, 0x200000},
    {"mpr-18755.18", 0xBF094D9E, Polygons, 0x800000, 0x200000},
    {"mpr-18758.22", 0x1B5473D0, Polygons, 0x800002, 0x200000},
    {"mpr-18761.28", 0x4E39EC05, CoproData, 0x000000, 0x200000},
    {"mpr-18762.29", 0x4AB165D8, CoproData, 0x000002, 0x200000},
    {"mpr-18760.25", 0x4E3A4A89, Textures, 0x000000, 0x200000},
    {"mpr-18759.24", 0x278D8742, Textures, 0x000002, 0x200000},
    {"opr-14742a.45", 0x90C6B117, CoproTables, 0x000000, 0x020000},
    {"opr-14743a.46", 0xAE7F446B, CoproTables, 0x000002, 0x020000},
    {"epr-18826.30", 0xED9FE4C1, SoundProgram, 0x000000, 0x040000},
    {"mpr-18827.31", 0x58D78CA1, Samples, 0x000000, 0x200000},
    {"mpr-18764.32", 0x0DC6A860, Samples, 0x200000, 0x200000},
    {"mpr-18765.36", 0xCA4A803C, Samples, 0x400000, 0x200000},
    {"mpr-18766.37", 0xE41892EA, Samples, 0x600000, 0x200000},
};

// Manx TT's Model 1 sound board (MAME's manxttdx: segam1audio), which plays
// the engine and road sounds: the Twin set (manxttc) has no copy of its ROMs,
// so they come from the parent set (manxtt) beside it when it is there.
const Load kManxttcM1[] = {
    {"epr-18742.7", 0x1B78DA74, M1Program, 0x000000, 0x020000},
    {"mpr-18743.32", 0x17E84E15, Pcm1, 0x000000, 0x200000},
};

// Virtua Cop 2 (vcop2), Model 2A-CRX: the same board as Sega Rally. The i960
// program in four 512 KB ROMs (0x000000 and 0x100000); main_data's last
// 1 MB mirrored up to 0xF00000; no copro data ROMs; a 512 KB sound program.
const Load kVcop2[] = {
    {"epr-18524.12", 0x1858988B, Program, 0x000000, 0x080000},
    {"epr-18525.13", 0x0C13DF3F, Program, 0x000002, 0x080000},
    {"epr-18518.14", 0x7842951B, Program, 0x100000, 0x080000},
    {"epr-18519.15", 0x31A30EDC, Program, 0x100002, 0x080000},
    {"mpr-18516.10", 0xA3928FF0, MainData, 0x000000, 0x200000},
    {"mpr-18517.11", 0x4BD73DA4, MainData, 0x000002, 0x200000},
    {"mpr-18514.8", 0x791283C5, MainData, 0x400000, 0x200000},
    {"mpr-18515.9", 0x6BA1FFEC, MainData, 0x400002, 0x200000},
    {"mpr-18522.6", 0x61D18536, MainData, 0x800000, 0x200000},
    {"mpr-18523.7", 0x61D08DC4, MainData, 0x800002, 0x200000},
    {"epr-18520.4", 0x1D4EC5E8, MainData, 0xC00000, 0x080000},
    {"epr-18521.5", 0xB8B3781C, MainData, 0xC00002, 0x080000},
    {"mpr-18513.16", 0x777A3633, Polygons, 0x000000, 0x200000},
    {"mpr-18510.20", 0xE83DE997, Polygons, 0x000002, 0x200000},
    {"mpr-18512.25", 0xD9BC7E71, Textures, 0x000000, 0x200000},
    {"mpr-18511.24", 0xCAE77A4F, Textures, 0x000002, 0x200000},
    {"opr-14742a.45", 0x90C6B117, CoproTables, 0x000000, 0x020000},
    {"opr-14743a.46", 0xAE7F446B, CoproTables, 0x000002, 0x020000},
    {"epr-18530.30", 0xAC9C8357, SoundProgram, 0x000000, 0x080000},
    {"mpr-18529.31", 0xF76715B1, Samples, 0x000000, 0x200000},
    {"mpr-18528.32", 0x287A2F9A, Samples, 0x200000, 0x200000},
    {"mpr-18527.36", 0xE6A49314, Samples, 0x400000, 0x200000},
    {"mpr-18526.37", 0x6516D9B5, Samples, 0x600000, 0x200000},
};

// Virtua Cop, Revision B (vcop), on the original Model 2 like Daytona, with
// the same Model 1 sound board. No copro data ROMs (MAME's region is zeroed).
const Load kVcop[] = {
    {"epr-17166b.12", 0xA5647C59, Program, 0x000000, 0x020000},
    {"epr-17167b.13", 0xF5DDE26A, Program, 0x000002, 0x020000},
    {"epr-17160a.14", 0x267F3242, Program, 0x040000, 0x020000},
    {"epr-17161a.15", 0xF7126876, Program, 0x040002, 0x020000},
    {"mpr-17164.10", 0xAC5FC501, MainData, 0x000000, 0x200000},
    {"mpr-17165.11", 0x82296D00, MainData, 0x000002, 0x200000},
    {"mpr-17162.8", 0x60DDD41E, MainData, 0x400000, 0x200000},
    {"mpr-17163.9", 0x8C1F9DC8, MainData, 0x400002, 0x200000},
    {"epr-17168a.6", 0x59091A37, MainData, 0x800000, 0x080000},
    {"epr-17169a.7", 0x0495808D, MainData, 0x800002, 0x080000},
    {"mpr-17159.16", 0xE218727D, Polygons, 0x000000, 0x200000},
    {"mpr-17156.20", 0xC4F4AABF, Polygons, 0x000002, 0x200000},
    {"mpr-17158.25", 0x1108D1EC, Textures, 0x000000, 0x200000},
    {"mpr-17157.24", 0xCF31E33D, Textures, 0x000002, 0x200000},
    {"opr-14742a.45", 0x90C6B117, CoproTables, 0x000000, 0x020000},
    {"opr-14743a.46", 0xAE7F446B, CoproTables, 0x000002, 0x020000},
    {"epr-17170.7", 0x06A38AE2, SoundProgram, 0x000000, 0x020000},
    {"epr-17171.8", 0xB5E436F8, SoundProgram, 0x020000, 0x020000},
    {"mpr-17172.32", 0xAB22CAC3, Pcm1, 0x000000, 0x100000},
    {"mpr-17173.33", 0x3CB4005C, Pcm1, 0x200000, 0x100000},
    {"mpr-17174.4", 0xA50369CC, Pcm2, 0x000000, 0x200000},
    {"mpr-17175.5", 0x9136D43C, Pcm2, 0x200000, 0x200000},
};

// Sonic Championship (schamp, USA), Model 2B-CRX: main_data 0x1000000's
// 1 MB mirrored up to 0x1F00000; 4 MB of copro data; 16 MB of polygons,
// 16 MB of textures.
const Load kSchamp[] = {
    {"epr-19141.15", 0xB942EF21, Program, 0x000000, 0x080000},
    {"epr-19142.16", 0x2D54BD76, Program, 0x000002, 0x080000},
    {"mpr-19007.11", 0x8B8FF751, MainData, 0x0000000, 0x400000},
    {"mpr-19008.12", 0xA94654F5, MainData, 0x0000002, 0x400000},
    {"mpr-19005.9", 0x98CD1127, MainData, 0x0800000, 0x400000},
    {"mpr-19006.10", 0xE79F0A26, MainData, 0x0800002, 0x400000},
    {"epr-19143.7", 0xF97176FD, MainData, 0x1000000, 0x080000},
    {"epr-19144.8", 0xD040202A, MainData, 0x1000002, 0x080000},
    {"mpr-19015.29", 0xC74D99E3, CoproData, 0x000000, 0x200000},
    {"mpr-19016.30", 0x746AE931, CoproData, 0x000002, 0x200000},
    {"mpr-19009.17", 0xFD410350, Polygons, 0x000000, 0x400000},
    {"mpr-19012.21", 0x9BB7B5B6, Polygons, 0x000002, 0x400000},
    {"mpr-19010.18", 0x6FD94187, Polygons, 0x800000, 0x400000},
    {"mpr-19013.22", 0x9E232FE5, Polygons, 0x800002, 0x400000},
    {"mpr-19019.27", 0x59121896, Textures, 0x000000, 0x400000},
    {"mpr-19017.25", 0x7B298379, Textures, 0x000002, 0x400000},
    {"mpr-19020.28", 0x9540DBA0, Textures, 0x800000, 0x400000},
    {"mpr-19018.26", 0x3B7E7A12, Textures, 0x800002, 0x400000},
    {"epr-19021.31", 0x0B9F7583, SoundProgram, 0x000000, 0x080000},
    {"mpr-19022.32", 0x4381869B, Samples, 0x000000, 0x200000},
    {"mpr-19023.33", 0x07C67F88, Samples, 0x200000, 0x200000},
    {"mpr-19024.34", 0x15FF76D3, Samples, 0x400000, 0x200000},
    {"mpr-19025.35", 0x6AD8FB70, Samples, 0x600000, 0x200000},
};

const Load kDynabb97[] = {
    {"epr-19833a.15", 0xD99ED1B2, Program, 0x000000, 0x080000},
    {"epr-19834a.16", 0x24192BB1, Program, 0x000002, 0x080000},
    {"epr-19831a.13", 0x0527EA40, Program, 0x100000, 0x080000},
    {"epr-19832a.14", 0x2F380A40, Program, 0x100002, 0x080000},
    {"mpr-19841.11", 0x989309AF, MainData, 0x0000000, 0x400000},
    {"mpr-19842.12", 0xEEC54070, MainData, 0x0000002, 0x400000},
    {"mpr-19839.9", 0xD5A74CF4, MainData, 0x0800000, 0x400000},
    {"mpr-19840.10", 0x45704E95, MainData, 0x0800002, 0x400000},
    {"mpr-19837.7", 0xC02187D9, MainData, 0x1000000, 0x400000},
    {"mpr-19838.8", 0x546B61CD, MainData, 0x1000002, 0x400000},
    {"mpr-19835.5", 0xA3B0A37C, MainData, 0x1800000, 0x400000},
    {"mpr-19836.6", 0xD70A32AA, MainData, 0x1800002, 0x400000},
    {"mpr-19843.17", 0x019BC583, Polygons, 0x000000, 0x400000},
    {"mpr-19845.21", 0x2D23E73A, Polygons, 0x000002, 0x400000},
    {"mpr-19844.18", 0x150198D6, Polygons, 0x800000, 0x400000},
    {"mpr-19846.22", 0xFE53CD17, Polygons, 0x800002, 0x400000},
    {"mpr-19848.27", 0x4C0526B7, Textures, 0x000000, 0x400000},
    {"mpr-19847.25", 0xFE55EDBD, Textures, 0x000002, 0x400000},
    {"epr-19849.31", 0xB0D5BFF0, SoundProgram, 0x000000, 0x080000},
    {"mpr-19850.32", 0xE1FD27BF, Samples, 0x000000, 0x200000},
    {"mpr-19851.33", 0xDC644077, Samples, 0x200000, 0x200000},
    {"mpr-19852.34", 0xCFDA4EFD, Samples, 0x400000, 0x200000},
    {"mpr-19853.35", 0xCFC64857, Samples, 0x600000, 0x200000},
};

// Wave Runner (waverunr, Revision A, Japan, Model 2C-CRX).
const Load kWaverunr[] = {
    {"epr-19282a.15", 0x5DF58604, Program, 0x000000, 0x080000},
    {"epr-19283a.16", 0xBCA188E1, Program, 0x000002, 0x080000},
    {"mpr-19291.11", 0x54431D67, MainData, 0x0000000, 0x400000},
    {"mpr-19292.12", 0x9152D979, MainData, 0x0000002, 0x400000},
    {"mpr-19293.9", 0xB168BEA9, MainData, 0x0800000, 0x400000},
    {"mpr-19294.10", 0xC731E659, MainData, 0x0800002, 0x400000},
    {"epr-19278.7", 0x29ED421D, MainData, 0x1000000, 0x080000},
    {"epr-19279.8", 0x6AE9F899, MainData, 0x1000002, 0x080000},
    {"epr-19280.29", 0xC6B59FB9, CoproData, 0x000000, 0x080000},
    {"epr-19281.30", 0x5A6110E7, CoproData, 0x000002, 0x080000},
    {"mpr-19287.17", 0x203B9E45, Polygons, 0x0000000, 0x400000},
    {"mpr-19288.21", 0x4A488E4D, Polygons, 0x0000002, 0x400000},
    {"mpr-19289.18", 0x35361CB3, Polygons, 0x0800000, 0x400000},
    {"mpr-19290.22", 0x67300826, Polygons, 0x0800002, 0x400000},
    {"epr-19304.19", 0x1A7D9521, Polygons, 0x1000000, 0x080000},
    {"epr-19305.23", 0x00412412, Polygons, 0x1000002, 0x080000},
    {"mpr-19286.27", 0x3CCC33CB, Textures, 0x000000, 0x400000},
    {"mpr-19285.25", 0xA070FBD0, Textures, 0x000002, 0x400000},
    {"epr-19303.28", 0xFCFFC8A0, Textures, 0x800000, 0x080000},
    {"epr-19302.26", 0xBD00933A, Textures, 0x800002, 0x080000},
    {"epr-19284.31", 0xEFE5F0F3, SoundProgram, 0x000000, 0x040000},
    {"mpr-19295.32", 0xB14EEB09, Samples, 0x0000000, 0x400000},
    {"mpr-19296.34", 0xB4B9FAFF, Samples, 0x0400000, 0x400000},
};

// Sega Water Ski (segawski, Revision A, Japan, Model 2C-CRX).
const Load kSegawski[] = {
    {"epr-19963a.15", 0x89C9CB0D, Program, 0x000000, 0x080000},
    {"epr-19964a.16", 0xC382CEFE, Program, 0x000002, 0x080000},
    {"mpr-19980.11", 0x709804F2, MainData, 0x0000000, 0x400000},
    {"mpr-19981.12", 0xA1E8EC09, MainData, 0x0000002, 0x400000},
    {"mpr-19982.9", 0x41C398BB, MainData, 0x0800000, 0x400000},
    {"mpr-19983.10", 0xE210DEA2, MainData, 0x0800002, 0x400000},
    {"mpr-19984.7", 0x14B967D1, MainData, 0x1000000, 0x400000},
    {"mpr-19985.8", 0x57827677, MainData, 0x1000002, 0x400000},
    {"epr-19961.5", 0xCC34ECAF, MainData, 0x1800000, 0x080000},
    {"epr-19962.6", 0xE8A30E5E, MainData, 0x1800002, 0x080000},
    {"mpr-19986.29", 0x4B8E26F8, CoproData, 0x000000, 0x400000},
    {"mpr-19987.30", 0x8D5B9D38, CoproData, 0x000002, 0x400000},
    {"mpr-19972.17", 0x493C0F8F, Polygons, 0x0000000, 0x400000},
    {"mpr-19973.21", 0x01B6F8E1, Polygons, 0x0000002, 0x400000},
    {"mpr-19974.18", 0x2BD947D3, Polygons, 0x0800000, 0x400000},
    {"mpr-19975.22", 0x07E6B699, Polygons, 0x0800002, 0x400000},
    {"mpr-19976.19", 0xF9496566, Polygons, 0x1000000, 0x400000},
    {"mpr-19977.23", 0xB3DBF54B, Polygons, 0x1000002, 0x400000},
    {"mpr-19978.20", 0xC80F4ED7, Polygons, 0x1800000, 0x400000},
    {"mpr-19979.24", 0x1C0DB4D2, Polygons, 0x1800002, 0x400000},
    {"mpr-19969.27", 0xC0C33D39, Textures, 0x000000, 0x400000},
    {"mpr-19968.25", 0xB8D2F04F, Textures, 0x000002, 0x400000},
    {"mpr-19971.28", 0xC8708096, Textures, 0x800000, 0x400000},
    {"mpr-19970.26", 0xC59D8D36, Textures, 0x800002, 0x400000},
    {"epr-19967.31", 0xC6B8EF3F, SoundProgram, 0x000000, 0x080000},
    {"mpr-19988.32", 0x303732FB, Samples, 0x0000000, 0x400000},
    {"mpr-19989.34", 0x8074A4B3, Samples, 0x0400000, 0x400000},
};

// Top Skater (topskatr, Model 2C-CRX): its Digital Sound Board 2's 68000 program and MPEG data too.
const Load kTopskatr[] = {
    {"epr-19755a.15", 0xB80633B9, Program, 0x000000, 0x080000},
    {"epr-19756a.16", 0x472046A2, Program, 0x000002, 0x080000},
    {"mpr-19735.11", 0x8E509266, MainData, 0x0000000, 0x400000},
    {"mpr-19736.12", 0x094E0A0D, MainData, 0x0000002, 0x400000},
    {"mpr-19737.9", 0x281A7DDE, MainData, 0x0800000, 0x400000},
    {"mpr-19738.10", 0xF688327E, MainData, 0x0800002, 0x400000},
    {"mpr-19743.29", 0xD41A41BF, CoproData, 0x000000, 0x200000},
    {"mpr-19744.30", 0x84F203BF, CoproData, 0x000002, 0x200000},
    {"mpr-19741.17", 0x111A6E29, Polygons, 0x0000000, 0x200000},
    {"mpr-19742.21", 0x28510AFF, Polygons, 0x0000002, 0x200000},
    {"mpr-19740.27", 0xB20F508B, Textures, 0x000000, 0x400000},
    {"mpr-19739.25", 0x8120CFD8, Textures, 0x000002, 0x400000},
    {"mpr-19759.31", 0x573530F2, SoundProgram, 0x000000, 0x080000},
    {"mpr-19745.32", 0x7082A0AF, Samples, 0x0000000, 0x400000},
    {"mpr-19746.34", 0x657B5977, Samples, 0x0400000, 0x400000},
    {"mpr-19760.2s", 0x2E41CA15, Dsb2Program, 0x000000, 0x020000},
    {"mpr-19747.18s", 0x6E895AAA, DsbMpeg, 0x000000, 0x400000},
    {"mpr-19748.20s", 0xFCD74DE3, DsbMpeg, 0x400000, 0x400000},
    {"mpr-19749.22s", 0x842CA1EB, DsbMpeg, 0x800000, 0x400000},
    {"mpr-19750.24s", 0xCD95D0BF, DsbMpeg, 0xC00000, 0x400000},
};

// Rail Chase 2 (rchase2, Model 2B, Revision A). MAME's iocpu (the I/O board's Z80) is not connected, so not loaded.
const Load kRchase2[] = {
    {"epr-18045a.15", 0xBFCA0314, Program, 0x000000, 0x080000},
    {"epr-18046a.16", 0x0B8D3074, Program, 0x000002, 0x080000},
    {"epr-18074a.13", 0xCA4B58DF, Program, 0x100000, 0x080000},
    {"epr-18075a.14", 0xB82672E4, Program, 0x100002, 0x080000},
    {"mpr-18037.11", 0xDEA8F896, MainData, 0x0000000, 0x200000},
    {"mpr-18038.12", 0x441F7709, MainData, 0x0000002, 0x200000},
    {"mpr-18039.9", 0xB98C6F06, MainData, 0x0400000, 0x200000},
    {"mpr-18040.10", 0x0D872667, MainData, 0x0400002, 0x200000},
    {"mpr-18041.7", 0xE511AB0A, MainData, 0x0800000, 0x200000},
    {"mpr-18042.8", 0xE9A04159, MainData, 0x0800002, 0x200000},
    {"mpr-18043.5", 0xFF84DFD6, MainData, 0x0C00000, 0x200000},
    {"mpr-18044.6", 0xAB9B406D, MainData, 0x0C00002, 0x200000},
    {"mpr-18031.17", 0x25D0DEAE, Polygons, 0x0000000, 0x200000},
    {"mpr-18032.21", 0xDBAE35C2, Polygons, 0x0000002, 0x200000},
    {"mpr-18033.18", 0x1E75946C, Polygons, 0x0400000, 0x200000},
    {"mpr-18034.22", 0x215235AD, Polygons, 0x0400002, 0x200000},
    {"mpr-18035.27", 0x4423F66E, Textures, 0x000000, 0x200000},
    {"mpr-18036.25", 0x69221CF5, Textures, 0x000002, 0x200000},
    {"epr-18047.31", 0x4C31D459, SoundProgram, 0x000000, 0x080000},
    {"mpr-18029.32", 0xF6804150, Samples, 0x0000000, 0x200000},
    {"mpr-18030.34", 0x1167615D, Samples, 0x0400000, 0x200000},
};

// Dead or Alive (doab, Revision B, Model 2B). Its main data's last 4 MB are copied above it (MAME's ROM_COPY),
// and the polygon region's gap reads 0xff (ERASEFF). Older romsets carry an earlier dump of mpr-19324
// (CRC d972201f); it is accepted too (a polygon ROM: no code is recompiled from it).
const Load kDoab[] = {
    {"epr-19379b.15", 0x8A10A944, Program, 0x000000, 0x080000},
    {"epr-19380b.16", 0x766C1EC8, Program, 0x000002, 0x080000},
    {"mpr-19318.11", 0xAB431BFE, MainData, 0x0000000, 0x400000},
    {"mpr-19319.12", 0xC5CB694D, MainData, 0x0000002, 0x400000},
    {"mpr-19316.9", 0x2D2D1B1A, MainData, 0x0800000, 0x400000},
    {"mpr-19317.10", 0x96B17BCF, MainData, 0x0800002, 0x400000},
    {"mpr-19314.7", 0xA8D963FB, MainData, 0x1000000, 0x400000},
    {"mpr-19315.8", 0x90AE5682, MainData, 0x1000002, 0x400000},
    {"mpr-19312.5", 0x1DCEDB10, MainData, 0x1800000, 0x200000},
    {"mpr-19313.6", 0x8C63055E, MainData, 0x1800002, 0x200000},
    {"mpr-19322.17", 0xD0E6ECF0, Polygons, 0x0000000, 0x400000},
    {"mpr-19325.21", 0x7CBE432D, Polygons, 0x0000002, 0x400000},
    {"mpr-19323.18", 0x453D3F4A, Polygons, 0x0800000, 0x400000},
    {"mpr-19326.22", 0xB976DA02, Polygons, 0x0800002, 0x400000},
    {"mpr-19324.19", 0x0D6BF454, Polygons, 0x1000000, 0x400000, 0xD972201F},
    {"mpr-19327.23", 0x6A75634C, Polygons, 0x1000002, 0x400000},
    {"mpr-19321.27", 0x9C49E845, Textures, 0x000000, 0x400000},
    {"mpr-19320.25", 0x190C017F, Textures, 0x000002, 0x400000},
    {"epr-19328.31", 0x400BDBFB, SoundProgram, 0x000000, 0x080000},
    {"mpr-19329.32", 0x8FD2708A, Samples, 0x0000000, 0x200000},
    {"mpr-19330.33", 0x0C69787D, Samples, 0x0200000, 0x200000},
    {"mpr-19331.34", 0xC18EA0B8, Samples, 0x0400000, 0x200000},
    {"mpr-19332.35", 0x2877F96F, Samples, 0x0600000, 0x200000},
};

// Behind Enemy Lines (bel, Model 2C-CRX).
const Load kBel[] = {
    {"epr-20225.15", 0x4ABC6B59, Program, 0x000000, 0x020000},
    {"epr-20226.16", 0x43E05B3A, Program, 0x000002, 0x020000},
    {"epr-20223.13", 0x61B1BE98, Program, 0x040000, 0x020000},
    {"epr-20224.14", 0xEB2D7DBF, Program, 0x040002, 0x020000},
    {"mpr-20233.11", 0x3E079A3B, MainData, 0x0000000, 0x200000},
    {"mpr-20234.12", 0x58BDE826, MainData, 0x0000002, 0x200000},
    {"mpr-20231.9", 0xB3393E93, MainData, 0x0400000, 0x200000},
    {"mpr-20232.10", 0xDA4A2E11, MainData, 0x0400002, 0x200000},
    {"mpr-20229.7", 0xCDEC7BF4, MainData, 0x0800000, 0x200000},
    {"mpr-20230.8", 0xA166FA87, MainData, 0x0800002, 0x200000},
    {"mpr-20227.5", 0x1277686E, MainData, 0x0C00000, 0x200000},
    {"mpr-20228.6", 0x49CB5568, MainData, 0x0C00002, 0x200000},
    {"mpr-20236.29", 0x8DE9A3C2, CoproData, 0x000000, 0x200000},
    {"mpr-20235.30", 0x78FA11EF, CoproData, 0x000002, 0x200000},
    {"mpr-20244.17", 0x9D2A8660, Polygons, 0x0000000, 0x200000},
    {"mpr-20240.21", 0x51615908, Polygons, 0x0000002, 0x200000},
    {"mpr-20243.18", 0x48671F7C, Polygons, 0x0400000, 0x200000},
    {"mpr-20239.22", 0x6CD8D8A5, Polygons, 0x0400002, 0x200000},
    {"mpr-20242.19", 0xE7F86AC7, Polygons, 0x0800000, 0x200000},
    {"mpr-20238.23", 0x0A480C7C, Polygons, 0x0800002, 0x200000},
    {"mpr-20241.20", 0x51974B98, Polygons, 0x0C00000, 0x200000},
    {"mpr-20237.24", 0x89B5D8B6, Polygons, 0x0C00002, 0x200000},
    {"mpr-20247.27", 0x00B0417D, Textures, 0x000000, 0x200000},
    {"mpr-20245.25", 0x36490A08, Textures, 0x000002, 0x200000},
    {"mpr-20248.28", 0x0ACE6BEF, Textures, 0x800000, 0x200000},
    {"mpr-20246.26", 0x250D6CA1, Textures, 0x800002, 0x200000},
    {"mpr-20249.31", 0xDC24F13D, SoundProgram, 0x000000, 0x020000},
    {"mpr-20250.32", 0x91B735D3, Samples, 0x0000000, 0x200000},
    {"mpr-20251.33", 0x703A947B, Samples, 0x0200000, 0x200000},
    {"mpr-20252.34", 0x8F48F375, Samples, 0x0400000, 0x200000},
    {"mpr-20253.35", 0xCA6AA17C, Samples, 0x0600000, 0x200000},
};

// Sega Ski Super G (skisuprg, Model 2C-CRX).
const Load kSkisuprg[] = {
    {"epr-19489.15", 0x1DF948A7, Program, 0x000000, 0x080000},
    {"epr-19490.16", 0xE6FC24D3, Program, 0x000002, 0x080000},
    {"epr-19551.13", 0x3EE8F0D5, Program, 0x100000, 0x080000},
    {"epr-19552.14", 0xBAA2E49A, Program, 0x100002, 0x080000},
    {"mpr-19494.11", 0xF19CDB5C, MainData, 0x0000000, 0x400000},
    {"mpr-19495.12", 0xD42E5EF2, MainData, 0x0000002, 0x400000},
    {"mpr-19492.9", 0x4805318F, MainData, 0x0800000, 0x400000},
    {"mpr-19493.10", 0x39DAA909, MainData, 0x0800002, 0x400000},
    {"mpr-19502.29", 0x2212D8D6, CoproData, 0x000000, 0x400000},
    {"mpr-19503.30", 0x3C9CFC73, CoproData, 0x000002, 0x400000},
    {"mpr-19496.17", 0x0E9AEF4E, Polygons, 0x0000000, 0x400000},
    {"mpr-19497.21", 0x5397EFE9, Polygons, 0x0000002, 0x400000},
    {"mpr-19498.18", 0x32E5AE60, Polygons, 0x0800000, 0x400000},
    {"mpr-19499.22", 0x2B9F5B48, Polygons, 0x0800002, 0x400000},
    {"mpr-19501.27", 0x66D7B02E, Textures, 0x000000, 0x400000},
    {"mpr-19500.25", 0x905F5798, Textures, 0x000002, 0x400000},
    {"epr-19491.31", 0x1C9B15FD, SoundProgram, 0x000000, 0x080000},
    {"mpr-19504.32", 0x9419EC08, Samples, 0x0000000, 0x400000},
    {"mpr-19505.34", 0xEBA7F41D, Samples, 0x0400000, 0x400000},
};

// Last Bronx, Revision A, Export (lastbrnx), Model 2B-CRX: no copro data;
// the texture region's gaps read 0xff (MAME's ERASEFF). The SHARC program
// goes through the SHARC's EPB0 register, not the boot FIFO.
const Load kLastbrnx[] = {
    {"epr-19061a.15", 0xC0AEBAB2, Program, 0x000000, 0x080000},
    {"epr-19062a.16", 0xCDF597E8, Program, 0x000002, 0x080000},
    {"mpr-19050.11", 0xE6AF2B61, MainData, 0x000000, 0x400000},
    {"mpr-19051.12", 0x14B88961, MainData, 0x000002, 0x400000},
    {"mpr-19048.9", 0x02180215, MainData, 0x800000, 0x400000},
    {"mpr-19049.10", 0xDB7EECD6, MainData, 0x800002, 0x400000},
    {"mpr-19052.17", 0xD7F27216, Polygons, 0x000000, 0x400000},
    {"mpr-19053.21", 0x1F328465, Polygons, 0x000002, 0x400000},
    {"mpr-19055.27", 0x85A57D49, Textures, 0x000000, 0x200000},
    {"mpr-19054.25", 0x05366277, Textures, 0x000002, 0x200000},
    {"mpr-19056.31", 0x22A22918, SoundProgram, 0x000000, 0x080000},
    {"mpr-19057.32", 0x64809438, Samples, 0x000000, 0x400000},
    {"mpr-19058.34", 0xE237C11C, Samples, 0x400000, 0x400000},
};

// Desert Tank (desert), the original Model 2 like Virtua Cop: Model 1
// sound board, i960 program in four 128 KB ROMs; 1 MB of copro data, 8 MB
// of polygons, 4 MB of textures.
const Load kDesert[] = {
    {"epr-16976.12", 0xD036DFF0, Program, 0x000000, 0x020000},
    {"epr-16977.13", 0xE91194BD, Program, 0x000002, 0x020000},
    {"epr-16970.14", 0x4EA12D1F, Program, 0x040000, 0x020000},
    {"epr-16971.15", 0xD630B220, Program, 0x040002, 0x020000},
    {"mpr-16974.10", 0x2AB491C5, MainData, 0x000000, 0x200000},
    {"mpr-16975.11", 0xE24FE7D3, MainData, 0x000002, 0x200000},
    {"mpr-16972.8", 0x23E53748, MainData, 0x400000, 0x200000},
    {"mpr-16973.9", 0x77D6F509, MainData, 0x400002, 0x200000},
    {"epr-16978.6", 0x38B3E574, MainData, 0x800000, 0x080000},
    {"epr-16979.7", 0xC314EB8B, MainData, 0x800002, 0x080000},
    {"epr-16981.28", 0xAE847571, CoproData, 0x000000, 0x080000},
    {"epr-16980.29", 0x5239B864, CoproData, 0x000002, 0x080000},
    {"mpr-16968.16", 0x4A16F465, Polygons, 0x000000, 0x200000},
    {"mpr-16964.20", 0xD4A769B6, Polygons, 0x000002, 0x200000},
    {"mpr-16969.17", 0x887380AC, Polygons, 0x400000, 0x200000},
    {"mpr-16965.21", 0x9BA7645F, Polygons, 0x400002, 0x200000},
    {"mpr-16967.25", 0xB8B84C9D, Textures, 0x000000, 0x200000},
    {"mpr-16966.24", 0x7484EFE9, Textures, 0x000002, 0x200000},
    {"opr-14742a.45", 0x90C6B117, CoproTables, 0x000000, 0x020000},
    {"opr-14743a.46", 0xAE7F446B, CoproTables, 0x000002, 0x020000},
    {"epr-16985.7", 0x8C4D9056, SoundProgram, 0x000000, 0x020000},
    {"mpr-16986.32", 0x559612F9, Pcm1, 0x000000, 0x200000},
    {"mpr-16988.4", 0xBC705875, Pcm2, 0x000000, 0x200000},
    {"mpr-16989.5", 0x1B616B31, Pcm2, 0x200000, 0x200000},
};

// Virtua Fighter 2, Version 2.1 (vf2), Model 2A-CRX like Sega Rally: SCSP
// sound board, i960 program in four 128 KB ROMs, its code run from ROM. No
// copro data ROMs; the texture region's gaps read 0xff (MAME's ERASEFF).
const Load kVf2[] = {
    {"epr-18385.12", 0x78ED2D41, Program, 0x000000, 0x020000},
    {"epr-18386.13", 0x3418F428, Program, 0x000002, 0x020000},
    {"epr-18387.14", 0x124A8453, Program, 0x040000, 0x020000},
    {"epr-18388.15", 0x8D347980, Program, 0x040002, 0x020000},
    {"mpr-17560.10", 0xD1389864, MainData, 0x000000, 0x200000},
    {"mpr-17561.11", 0xB98D0101, MainData, 0x000002, 0x200000},
    {"mpr-17558.8", 0x4B15F5A6, MainData, 0x400000, 0x200000},
    {"mpr-17559.9", 0xD3264DE6, MainData, 0x400002, 0x200000},
    {"mpr-17566.6", 0xFB41EF98, MainData, 0x800000, 0x200000},
    {"mpr-17567.7", 0xC3396922, MainData, 0x800002, 0x200000},
    {"mpr-17564.4", 0xD8062489, MainData, 0xC00000, 0x200000},
    {"mpr-17565.5", 0x0517C6E9, MainData, 0xC00002, 0x200000},
    {"mpr-17554.16", 0x27896D82, Polygons, 0x000000, 0x200000},
    {"mpr-17548.20", 0xC95FACC2, Polygons, 0x000002, 0x200000},
    {"mpr-17555.17", 0x4DF2810B, Polygons, 0x400000, 0x200000},
    {"mpr-17549.21", 0xE0BCE0E6, Polygons, 0x400002, 0x200000},
    {"mpr-17556.18", 0x41A47616, Polygons, 0x800000, 0x200000},
    {"mpr-17550.22", 0xC36FF3F5, Polygons, 0x800002, 0x200000},
    {"mpr-17553.25", 0x5DA1C5D3, Textures, 0x000000, 0x200000},
    {"mpr-17552.24", 0xE91E7427, Textures, 0x000002, 0x200000},
    {"mpr-17547.27", 0xBE940431, Textures, 0x800000, 0x200000},
    {"mpr-17546.26", 0x042A194B, Textures, 0x800002, 0x200000},
    {"opr-14742a.45", 0x90C6B117, CoproTables, 0x000000, 0x020000},
    {"opr-14743a.46", 0xAE7F446B, CoproTables, 0x000002, 0x020000},
    {"epr-17574.30", 0x4D4C3A55, SoundProgram, 0x000000, 0x080000},
    {"mpr-17573.31", 0xE43557FE, Samples, 0x000000, 0x200000},
    {"mpr-17572.32", 0x4FEBECC8, Samples, 0x200000, 0x200000},
    {"mpr-17571.36", 0x51CAA584, Samples, 0x400000, 0x200000},
    {"mpr-17570.37", 0xBCCD324B, Samples, 0x600000, 0x200000},
};

// INDY 500 Twin (Revision A, Newer; indy500), Model 2B-CRX: no TGP tables
// (its SHARC uploads its program instead, from the i960 program ROM); the
// extra data ROMs at main_data 0x1000000 are copied to every 1 MB above.
const Load kIndy500[] = {
    {"epr-18598a.15", 0x3CDCAC0F, Program, 0x000000, 0x080000},
    {"epr-18599a.16", 0x32BDE9A2, Program, 0x000002, 0x080000},
    {"mpr-18237.11", 0x37E4255A, MainData, 0x0000000, 0x400000},
    {"mpr-18238.12", 0xBF837BAC, MainData, 0x0000002, 0x400000},
    {"mpr-18239.9", 0x9A2DB86E, MainData, 0x0800000, 0x400000},
    {"mpr-18240.10", 0xAB46A35F, MainData, 0x0800002, 0x400000},
    {"epr-18596.7", 0x8BE1A5CD, MainData, 0x1000000, 0x080000},
    {"epr-18597.8", 0x44824E38, MainData, 0x1000002, 0x080000},
    {"epr-18249.29", 0xA399F023, CoproData, 0x000000, 0x080000},
    {"epr-18250.30", 0x7479AD52, CoproData, 0x000002, 0x080000},
    {"mpr-18233.17", 0x48A024D3, Polygons, 0x000000, 0x400000},
    {"mpr-18234.21", 0x1178BFC8, Polygons, 0x000002, 0x400000},
    {"mpr-18235.18", 0xE7D70D59, Polygons, 0x800000, 0x400000},
    {"mpr-18236.22", 0x6CA29E0E, Polygons, 0x800002, 0x400000},
    {"mpr-18232.27", 0xF962347D, Textures, 0x000000, 0x400000},
    {"mpr-18231.25", 0x673D5338, Textures, 0x000002, 0x400000},
    {"epr-18600.31", 0x269EE4A0, SoundProgram, 0x000000, 0x040000},
    {"mpr-18241.32", 0x3A380AE1, Samples, 0x000000, 0x200000},
    {"mpr-18242.33", 0x1CC3DEAE, Samples, 0x200000, 0x200000},
    {"mpr-18243.34", 0xA00A0053, Samples, 0x400000, 0x200000},
    {"mpr-18244.35", 0xBFA75BEB, Samples, 0x600000, 0x200000},
};

// Fighting Vipers (fvipers, Revision D, Model 2B-CRX): main_data has two
// ROM_COPY sources, 0x1000000 (to 0x1700000) and 0x1800000 (to 0x1f00000).
const Load kFvipers[] = {
    {"epr-18606d.15", 0x7334DE7D, Program, 0x0000000, 0x020000},
    {"epr-18607d.16", 0x700D2ADE, Program, 0x0000002, 0x020000},
    {"epr-18604d.13", 0x704FDFCF, Program, 0x0040000, 0x020000},
    {"epr-18605d.14", 0x7DDDF81F, Program, 0x0040002, 0x020000},
    {"mpr-18614.11", 0x0EBC899F, MainData, 0x0000000, 0x400000},
    {"mpr-18615.12", 0x018ABDB7, MainData, 0x0000002, 0x400000},
    {"mpr-18612.9", 0x1F174CD1, MainData, 0x0800000, 0x400000},
    {"mpr-18613.10", 0xF057CDF2, MainData, 0x0800002, 0x400000},
    {"epr-18610d.7", 0xA1871703, MainData, 0x1000000, 0x080000},
    {"epr-18611d.8", 0x39A75FEE, MainData, 0x1000002, 0x080000},
    {"epr-18608d.5", 0x5BC11881, MainData, 0x1800000, 0x080000},
    {"epr-18609d.6", 0xCD426035, MainData, 0x1800002, 0x080000},
    {"mpr-18622.29", 0xC74D99E3, CoproData, 0x0000000, 0x200000},
    {"mpr-18623.30", 0x746AE931, CoproData, 0x0000002, 0x200000},
    {"mpr-18616.17", 0x15A239BE, Polygons, 0x0000000, 0x200000},
    {"mpr-18619.21", 0x9D5E8E2B, Polygons, 0x0000002, 0x200000},
    {"mpr-18617.18", 0xA62CAB7D, Polygons, 0x0400000, 0x200000},
    {"mpr-18620.22", 0x4D432AFD, Polygons, 0x0400002, 0x200000},
    {"mpr-18618.19", 0xADAB589F, Polygons, 0x0800000, 0x200000},
    {"mpr-18621.23", 0xF5EEAA95, Polygons, 0x0800002, 0x200000},
    {"mpr-18626.27", 0x9DF0A961, Textures, 0x0000000, 0x200000},
    {"mpr-18624.25", 0x1D74433E, Textures, 0x0000002, 0x200000},
    {"mpr-18627.28", 0x946175A0, Textures, 0x0800000, 0x200000},
    {"mpr-18625.26", 0x182FD572, Textures, 0x0800002, 0x200000},
    {"epr-18628.31", 0xAA7DD79F, SoundProgram, 0x0000000, 0x080000},
    {"mpr-18629.32", 0x5D0006CC, Samples, 0x0000000, 0x200000},
    {"mpr-18630.33", 0x9D405615, Samples, 0x0200000, 0x200000},
    {"mpr-18631.34", 0x9DAE5B45, Samples, 0x0400000, 0x200000},
    {"mpr-18632.35", 0x39DA6805, Samples, 0x0600000, 0x200000},
};

// Super GT 24h (sgt24h, Jaleco, Model 2B-CRX): no copro data ROMs, an 8 MB
// polygon and a 4 MB texture region (the renderers wrap at the region's
// size, as MAME's masks do); its comm board's program is not needed.
const Load kSgt24h[] = {
    {"epr-19155.15", 0x593952FD, Program, 0x0000000, 0x080000},
    {"epr-19156.16", 0xA91FC4EE, Program, 0x0000002, 0x080000},
    {"mpr-19146.11", 0xD66B5B0F, MainData, 0x0000000, 0x400000},
    {"mpr-19147.12", 0xD5558F48, MainData, 0x0000002, 0x400000},
    {"mpr-19148.9", 0xA14C86DB, MainData, 0x0800000, 0x400000},
    {"mpr-19149.10", 0x94EF5849, MainData, 0x0800002, 0x400000},
    {"mpr-19150.17", 0xE0AD870E, Polygons, 0x0000000, 0x400000},
    {"mpr-19151.21", 0xE2A1B125, Polygons, 0x0000002, 0x400000},
    {"mpr-19153.27", 0x136ADFD0, Textures, 0x0000000, 0x200000},
    {"mpr-19152.25", 0x363769A2, Textures, 0x0000002, 0x200000},
    {"epr-19157.31", 0x8FFEA0CF, SoundProgram, 0x0000000, 0x080000},
    {"mpr-19154.32", 0x7CD9E679, Samples, 0x0000000, 0x400000},
};

// Over Rev (overrevb, Jaleco, Model 2B-CRX, Revision B): Super GT 24h's
// layout (no copro data, 8 MB polygons, 4 MB textures). A clone of the
// Model 2C overrev: the archive must hold every file (a non-merged set).
const Load kOverrevb[] = {
    {"epr-19992b.15", 0x6D3E78D5, Program, 0x0000000, 0x080000},
    {"epr-19993b.16", 0x765DC9CE, Program, 0x0000002, 0x080000},
    {"mpr-19996.11", 0x21928A00, MainData, 0x0000000, 0x400000},
    {"mpr-19997.12", 0x2A169CAB, MainData, 0x0000002, 0x400000},
    {"mpr-19994.9", 0xE691FBD5, MainData, 0x0800000, 0x400000},
    {"mpr-19995.10", 0x82A7828E, MainData, 0x0800002, 0x400000},
    {"mpr-19998.17", 0x6A834574, Polygons, 0x0000000, 0x200000},
    {"mpr-19999.21", 0xFF590A2D, Polygons, 0x0000002, 0x200000},
    {"mpr-20001.27", 0x6CA236AA, Textures, 0x0000000, 0x200000},
    {"mpr-20000.25", 0x894D8DED, Textures, 0x0000002, 0x200000},
    {"epr-20002.31", 0x7EFB069E, SoundProgram, 0x0000000, 0x080000},
    {"mpr-20003.32", 0x149AC22B, Samples, 0x0000000, 0x400000},
    {"mpr-20004.34", 0x0B9C5410, Samples, 0x0400000, 0x400000},
};

// Cyber Troopers Virtual-On (von, Twin, Export, Model 2B-CRX): a 2 MB
// program (the second megabyte from its own pair), 16 MB of main_data with
// no ROM_COPYs; the comm board's program is not needed.
const Load kVon[] = {
    {"epr-18832.15", 0xB900C332, Program, 0x0000000, 0x080000},
    {"epr-18833.16", 0xC793C638, Program, 0x0000002, 0x080000},
    {"epr-18666.13", 0x66EDB432, Program, 0x0100000, 0x080000},
    {"epr-18667.14", 0xB593D31F, Program, 0x0100002, 0x080000},
    {"mpr-18648.11", 0x2EDBE489, MainData, 0x0000000, 0x400000},
    {"mpr-18649.12", 0xE68C5AA6, MainData, 0x0000002, 0x400000},
    {"mpr-18650.9", 0x89A855B9, MainData, 0x0800000, 0x400000},
    {"mpr-18651.10", 0xF4C23107, MainData, 0x0800002, 0x400000},
    {"mpr-18662.29", 0xA33D3335, CoproData, 0x0000000, 0x200000},
    {"mpr-18663.30", 0xEA74A641, CoproData, 0x0000002, 0x200000},
    {"mpr-18654.17", 0x6A0CAF29, Polygons, 0x0000000, 0x400000},
    {"mpr-18655.21", 0xA4293E78, Polygons, 0x0000002, 0x400000},
    {"mpr-18656.18", 0xB4F51E76, Polygons, 0x0800000, 0x400000},
    {"mpr-18657.22", 0xA9BE4674, Polygons, 0x0800002, 0x400000},
    {"mpr-18660.27", 0xE53663E5, Textures, 0x0000000, 0x200000},
    {"mpr-18658.25", 0x3D0FCD01, Textures, 0x0000002, 0x200000},
    {"mpr-18661.28", 0x52B50410, Textures, 0x0800000, 0x200000},
    {"mpr-18659.26", 0x27AA8AE2, Textures, 0x0800002, 0x200000},
    {"epr-18670.31", 0x3E715F76, SoundProgram, 0x0000000, 0x080000},
    {"mpr-18652.32", 0x037EEE53, Samples, 0x0000000, 0x400000},
    {"mpr-18653.34", 0x9EC3E7BF, Samples, 0x0400000, 0x400000},
};

// Gunblade NY (gunblade, Revision A, Model 2B-CRX): a 1 MB program, 16 MB
// of main_data with no ROM_COPYs, 8 MB of textures.
const Load kGunblade[] = {
    {"epr-18988a.15", 0xF63F1AD2, Program, 0x0000000, 0x080000},
    {"epr-18989a.16", 0xC1C84D65, Program, 0x0000002, 0x080000},
    {"mpr-18974.11", 0xE29ECAFF, MainData, 0x0000000, 0x400000},
    {"mpr-18975.12", 0xD8187582, MainData, 0x0000002, 0x400000},
    {"mpr-18976.9", 0xC95C15EB, MainData, 0x0800000, 0x400000},
    {"mpr-18977.10", 0xDB8F5B6F, MainData, 0x0800002, 0x400000},
    {"mpr-18986.29", 0x04820F7B, CoproData, 0x0000000, 0x400000},
    {"mpr-18987.30", 0x2419367F, CoproData, 0x0000002, 0x400000},
    {"mpr-18980.17", 0x22345534, Polygons, 0x0000000, 0x400000},
    {"mpr-18981.21", 0x2544A33D, Polygons, 0x0000002, 0x400000},
    {"mpr-18982.18", 0xD0A92B2A, Polygons, 0x0800000, 0x400000},
    {"mpr-18983.22", 0x1B4AF982, Polygons, 0x0800002, 0x400000},
    {"mpr-18985.27", 0xAD6166E3, Textures, 0x0000000, 0x400000},
    {"mpr-18984.25", 0x756F6F37, Textures, 0x0000002, 0x400000},
    {"epr-18990.31", 0x02B1B0D1, SoundProgram, 0x0000000, 0x080000},
    {"mpr-18978.32", 0x0F78B3E3, Samples, 0x0000000, 0x400000},
    {"mpr-18979.34", 0xF13EA36F, Samples, 0x0400000, 0x400000},
};

// Zero Gunner (zerogun, Export, Model 2B-CRX): two 512 KB program halves,
// 16 MB of main_data with no ROM_COPYs, no copro data ROMs; 8 MB of
// polygons, 4 MB of textures. The 315-5881 security chip's key is in
// m2_board.h (kCrypt5881Key).
const Load kZerogun[] = {
    {"epr-20439.15", 0x10125381, Program, 0x0000000, 0x080000},
    {"epr-20440.16", 0xCE872747, Program, 0x0000002, 0x080000},
    {"mpr-20296.11", 0x072D8A5E, MainData, 0x0000000, 0x400000},
    {"mpr-20297.12", 0xBA6A825B, MainData, 0x0000002, 0x400000},
    {"mpr-20294.9", 0xA0BD1474, MainData, 0x0800000, 0x400000},
    {"mpr-20295.10", 0xC548CCED, MainData, 0x0800002, 0x400000},
    {"mpr-20298.17", 0x8AB782FC, Polygons, 0x0000000, 0x400000},
    {"mpr-20299.21", 0x90E20CDB, Polygons, 0x0000002, 0x400000},
    {"mpr-20301.27", 0x52010FB2, Textures, 0x0000000, 0x200000},
    {"mpr-20300.25", 0x6F042792, Textures, 0x0000002, 0x200000},
    {"epr-20302.31", 0x44FF50D2, SoundProgram, 0x0000000, 0x080000},
    {"mpr-20303.32", 0xC040973F, Samples, 0x0000000, 0x200000},
    {"mpr-20304.33", 0x6DECFE83, Samples, 0x0200000, 0x200000},
};

// Pilot Kids (pltkids, Revision A, Model 2B-CRX): two 512 KB program
// halves, 8 MB of main_data, no copro data ROMs; 32 MB of polygons, 16 MB
// of textures, 8 MB of samples. Its 315-5881 key is in m2_board.h.
const Load kPltkids[] = {
    {"epr-21285a.15", 0xBDDE5B41, Program, 0x0000000, 0x080000},
    {"epr-21286a.16", 0xC8092E0E, Program, 0x0000002, 0x080000},
    {"mpr-21262.da0", 0xAA71353E, MainData, 0x0000000, 0x400000},
    {"mpr-21263.da1", 0xD55D4509, MainData, 0x0000002, 0x400000},
    {"mpr-21264.tp0", 0x6B35204D, Polygons, 0x0000000, 0x400000},
    {"mpr-21268.tp1", 0x16CE2147, Polygons, 0x0000002, 0x400000},
    {"mpr-21265.tp2", 0xF061E639, Polygons, 0x0800000, 0x400000},
    {"mpr-21269.tp3", 0x8C06255E, Polygons, 0x0800002, 0x400000},
    {"mpr-21266.tp4", 0xF9C32021, Polygons, 0x1000000, 0x400000},
    {"mpr-21270.tp5", 0xB61F81C3, Polygons, 0x1000002, 0x400000},
    {"mpr-21267.tp6", 0xC42CC938, Polygons, 0x1800000, 0x400000},
    {"mpr-21271.tp7", 0xA5325C75, Polygons, 0x1800002, 0x400000},
    {"mpr-21274.tx1", 0xF045E3D1, Textures, 0x0000000, 0x400000},
    {"mpr-21272.tx0", 0xDD605C21, Textures, 0x0000002, 0x400000},
    {"mpr-21275.tx3", 0xC4870B7C, Textures, 0x0800000, 0x400000},
    {"mpr-21273.tx2", 0x722EC8A2, Textures, 0x0800002, 0x400000},
    {"epr-21276.sd0", 0x8F415BC3, SoundProgram, 0x0000000, 0x080000},
    {"mpr-21277.sd1", 0xBFBA0FF6, Samples, 0x0000000, 0x200000},
    {"mpr-21278.sd2", 0x27E18E08, Samples, 0x0200000, 0x200000},
    {"mpr-21279.sd3", 0x3A8DCF68, Samples, 0x0400000, 0x200000},
    {"mpr-21280.sd4", 0xAA548124, Samples, 0x0600000, 0x200000},
};

// Virtua Striker (vstriker, Revision A, Model 2B-CRX): two 256 KB program
// halves, 9 MB of main_data with no ROM_COPYs, no copro data ROMs (MAME's
// region is zeroed); 8 MB of polygons, 4 MB of textures.
const Load kVstriker[] = {
    {"epr-18068a.15", 0xAFC69B54, Program, 0x0000000, 0x020000},
    {"epr-18069a.16", 0x0243250C, Program, 0x0000002, 0x020000},
    {"epr-18066a.13", 0xE658B33A, Program, 0x0040000, 0x020000},
    {"epr-18067a.14", 0x49E94047, Program, 0x0040002, 0x020000},
    {"mpr-18055.11", 0x5ABA9FC0, MainData, 0x0000000, 0x200000},
    {"mpr-18056.12", 0x017F0C55, MainData, 0x0000002, 0x200000},
    {"mpr-18053.9", 0x46C770C8, MainData, 0x0400000, 0x200000},
    {"mpr-18054.10", 0x437AF66E, MainData, 0x0400002, 0x200000},
    {"epr-18070a.7", 0x1961E2FC, MainData, 0x0800000, 0x080000},
    {"epr-18071a.8", 0xB2492DCA, MainData, 0x0800002, 0x080000},
    {"mpr-18057.17", 0x890D8806, Polygons, 0x0000000, 0x200000},
    {"mpr-18059.21", 0xC5CDF534, Polygons, 0x0000002, 0x200000},
    {"mpr-18058.18", 0xD4CBDF7C, Polygons, 0x0400000, 0x200000},
    {"mpr-18060.22", 0x93D5C95F, Polygons, 0x0400002, 0x200000},
    {"mpr-18062.27", 0x126E7DE3, Textures, 0x0000000, 0x200000},
    {"mpr-18061.25", 0xC37F1C67, Textures, 0x0000002, 0x200000},
    {"epr-18072.31", 0x73EABB58, SoundProgram, 0x0000000, 0x020000},
    {"mpr-18063.32", 0xB74D7C8A, Samples, 0x0000000, 0x200000},
    {"mpr-18064.33", 0x783B9910, Samples, 0x0200000, 0x200000},
    {"mpr-18065.34", 0x046B55FE, Samples, 0x0400000, 0x200000},
};

// Sega Touring Car Championship (stcc, the newer set, Model 2C-CRX): the
// TGPx4's program and copro_data; the Digital Sound Board's Z80 program and
// MPEG data (ROM_LOAD: whole files); its comm board's and drive board's
// programs are not needed. Its polygon and texture regions have ROM_COPY
// mirrors (kStccCopies).
const Load kStcc[] = {
    {"epr-19570.15", 0x9D6A1965, Program, 0x0000000, 0x080000},
    {"epr-19571.16", 0x97254D16, Program, 0x0000002, 0x080000},
    {"mpr-19257.11", 0xAC28EE24, MainData, 0x0000000, 0x400000},
    {"mpr-19258.12", 0xF5BA7D78, MainData, 0x0000002, 0x400000},
    {"epr-19270.9", 0x7BD1D04E, MainData, 0x0800000, 0x080000},
    {"epr-19271.10", 0xD2D74F85, MainData, 0x0800002, 0x080000},
    {"mpr-19255.29", 0xD78BF030, CoproData, 0x0000000, 0x200000},
    {"mpr-19256.30", 0xCB2B2D9E, CoproData, 0x0000002, 0x200000},
    {"mpr-19251.17", 0xE06FF7BA, Polygons, 0x0000000, 0x400000},
    {"mpr-19252.21", 0x68509993, Polygons, 0x0000002, 0x400000},
    {"epr-19266.18", 0x41464EE2, Polygons, 0x0800000, 0x080000},
    {"epr-19267.22", 0x780F994D, Polygons, 0x0800002, 0x080000},
    {"mpr-19254.27", 0x1EC49C02, Textures, 0x0000000, 0x200000},
    {"mpr-19253.25", 0x41BA79FB, Textures, 0x0000002, 0x200000},
    {"epr-19269.28", 0x01881121, Textures, 0x0800000, 0x080000},
    {"epr-19268.26", 0xBC4E081C, Textures, 0x0800002, 0x080000},
    {"epr-19274.31", 0x2DCC08AE, SoundProgram, 0x0000000, 0x020000},
    {"mpr-19259.32", 0x4D55DBFC, Samples, 0x0000000, 0x400000},
    {"mpr-19261.34", 0xB88878FF, Samples, 0x0400000, 0x400000},
    {"epr-19275.2s", 0xEE809D3F, DsbProgram, 0x0000000, 0x020000},
    {"mpr-19262.57s", 0x922AED7A, DsbMpeg, 0x0000000, 0x200000},
    {"mpr-19263.58s", 0xA256F4CD, DsbMpeg, 0x0200000, 0x200000},
    {"mpr-19264.59s", 0xB6C51D0F, DsbMpeg, 0x0400000, 0x200000},
    {"mpr-19265.60s", 0x7D98700A, DsbMpeg, 0x0600000, 0x200000},
};

// The House of the Dead (hotdo, the original set, Model 2C-CRX): a 2 MB
// program, main_data's last 1 MB mirrored up to 0x1F00000, a 32 MB polygon
// region; no Digital Sound Board.
const Load kHotdo[] = {
    {"epr-19696.15", 0x03DA5623, Program, 0x0000000, 0x080000},
    {"epr-19697.16", 0xA9722D87, Program, 0x0000002, 0x080000},
    {"epr-19694.13", 0xE85CA1A3, Program, 0x0100000, 0x080000},
    {"epr-19695.14", 0xCD52B461, Program, 0x0100002, 0x080000},
    {"mpr-19704.11", 0xAA80DBB0, MainData, 0x0000000, 0x400000},
    {"mpr-19705.12", 0xF906843B, MainData, 0x0000002, 0x400000},
    {"mpr-19702.9", 0xFC8AA3B7, MainData, 0x0800000, 0x400000},
    {"mpr-19703.10", 0x208D993D, MainData, 0x0800002, 0x400000},
    {"mpr-19700.7", 0x0558CFD3, MainData, 0x1000000, 0x400000},
    {"mpr-19701.8", 0x224A8929, MainData, 0x1000002, 0x400000},
    {"epr-19698.5", 0xE7A7B6EA, MainData, 0x1800000, 0x080000},
    {"epr-19699.6", 0x8160B3D9, MainData, 0x1800002, 0x080000},
    {"epr-19707.29", 0x384FD133, CoproData, 0x0000000, 0x080000},
    {"epr-19706.30", 0x1277531C, CoproData, 0x0000002, 0x080000},
    {"mpr-19715.17", 0x3FF7DDA7, Polygons, 0x0000000, 0x400000},
    {"mpr-19711.21", 0x080D13F1, Polygons, 0x0000002, 0x400000},
    {"mpr-19714.18", 0x3E55AB49, Polygons, 0x0800000, 0x400000},
    {"mpr-19710.22", 0x80DF1036, Polygons, 0x0800002, 0x400000},
    {"mpr-19713.19", 0x4D092CD3, Polygons, 0x1000000, 0x400000},
    {"mpr-19709.23", 0xD08937BF, Polygons, 0x1000002, 0x400000},
    {"mpr-19712.20", 0x41577943, Polygons, 0x1800000, 0x400000},
    {"mpr-19708.24", 0x5CB790F2, Polygons, 0x1800002, 0x400000},
    {"mpr-19718.27", 0xA9DE5924, Textures, 0x0000000, 0x400000},
    {"mpr-19716.25", 0x45C7DCCE, Textures, 0x0000002, 0x400000},
    {"mpr-19719.28", 0x838F8343, Textures, 0x0800000, 0x400000},
    {"mpr-19717.26", 0x393E440B, Textures, 0x0800002, 0x400000},
    {"epr-19720.31", 0xB367D21D, SoundProgram, 0x0000000, 0x080000},
    {"mpr-19721.32", 0xF5D8FA9A, Samples, 0x0000000, 0x400000},
    {"mpr-19722.34", 0xA56FA539, Samples, 0x0400000, 0x400000},
};

// ROM_COPY in the polygon and texture regions, in MAME's order.
struct Copy {
    Region region;
    uint32_t from, to, size;
};
const Copy kStccCopies[] = {
    {Polygons, 0x800000, 0x900000, 0x100000}, {Polygons, 0x800000, 0xa00000, 0x200000},
    {Polygons, 0x800000, 0xc00000, 0x400000}, {Textures, 0x000000, 0x400000, 0x400000},
    {Textures, 0x800000, 0x900000, 0x100000}, {Textures, 0x800000, 0xa00000, 0x100000},
    {Textures, 0x800000, 0xb00000, 0x100000}, {Textures, 0x800000, 0xc00000, 0x100000},
    {Textures, 0x800000, 0xd00000, 0x100000}, {Textures, 0x800000, 0xe00000, 0x100000},
    {Textures, 0x800000, 0xf00000, 0x100000},
};
const Copy kDoabCopies[] = {{MainData, 0x1800000, 0x1c00000, 0x400000}};
const Copy kWaverunrCopies[] = {
    {Polygons, 0x1000000, 0x1100000, 0x100000}, {Polygons, 0x1000000, 0x1200000, 0x100000},
    {Polygons, 0x1000000, 0x1300000, 0x100000}, {Polygons, 0x1000000, 0x1400000, 0x100000},
    {Polygons, 0x1000000, 0x1500000, 0x100000}, {Polygons, 0x1000000, 0x1600000, 0x100000},
    {Polygons, 0x1000000, 0x1700000, 0x100000}, {Polygons, 0x1000000, 0x1800000, 0x100000},
    {Polygons, 0x1000000, 0x1900000, 0x100000}, {Polygons, 0x1000000, 0x1a00000, 0x100000},
    {Polygons, 0x1000000, 0x1b00000, 0x100000}, {Polygons, 0x1000000, 0x1c00000, 0x100000},
    {Polygons, 0x1000000, 0x1d00000, 0x100000}, {Polygons, 0x1000000, 0x1e00000, 0x100000},
    {Textures, 0x800000, 0x900000, 0x100000}, {Textures, 0x800000, 0xa00000, 0x100000},
    {Textures, 0x800000, 0xb00000, 0x100000}, {Textures, 0x800000, 0xc00000, 0x100000},
    {Textures, 0x800000, 0xd00000, 0x100000}, {Textures, 0x800000, 0xe00000, 0x100000},
    {Textures, 0x800000, 0xf00000, 0x100000},
};

// Per set: its files; main_data's ROM_COPY source (copied to every 1 MB
// from there up to 0xF00000; 0: none); where the TGP program the i960
// uploads at boot sits, its length and CRC. Daytona's is in main_data (found
// by matching MAME's upload; the same 2,024 words in both sets). Sega
// Rally's is in the i960 program ROM: found by its opening jump table, the
// same as Daytona's, and matched word for word with MAME's upload (1,665
// words; the game runs this ROM from its copy in work RAM, see the design
// document). Indy 500's is the SHARC's: 12,288 16-bit words (the 48-bit
// instructions, 16 bits at a time), in the program ROM (matched with MAME's
// upload); tgp_words counts its 16-bit words. Fighting Vipers' SHARC upload is
// in main_data at 0x100000c (13,053 words, after a 12-byte header).
struct Set {
    const char *name;
    const Load *loads;
    size_t count;
    uint32_t mirror_from;
    Region tgp_region;
    uint32_t tgp_offset, tgp_words, tgp_crc;
    uint32_t mirror2_from = 0; // a second ROM_COPY source (to 0x1F00000); mirror_from's then end below it
    uint32_t polygons_size = 0x1000000, textures_size = 0x1000000; // MAME's region sizes
    const Copy *copies = nullptr; // main data, polygon and texture ROM_COPYs
    size_t copy_count = 0;
    // Optional ROMs, from this archive or else the named set's beside it
    // (without them the game runs without what they hold)
    const Load *extra = nullptr;
    size_t extra_count = 0;
    const char *extra_set = nullptr;
};
const Set kSets[] = {
    {"daytona93", kDaytona93, std::size(kDaytona93), 0x900000, MainData, 0x860020, 2024, 0xD6D611DDu},
    {"daytona", kDaytona, std::size(kDaytona), 0x800000, MainData, 0x800020, 2024, 0xD6D611DDu},
    {"srallyc", kSrallyc, std::size(kSrallyc), 0, Program, 0x05AE94, 1665, 0x072484B1u},
    {"manxttc", kManxttc, std::size(kManxttc), 0xC00000, Program, 0x0787E8, 1782, 0xBB000CC3u, 0, 0x1000000, 0x1000000,
     nullptr, 0, kManxttcM1, std::size(kManxttcM1), "manxtt"},
    {"vcop2", kVcop2, std::size(kVcop2), 0xC00000, Program, 0x05D480, 1778, 0x6B49E475u},
    {"schamp", kSchamp, std::size(kSchamp), 0x1000000, Program, 0x0B6450, 14862, 0x11E00028u},
    {"lastbrnx", kLastbrnx, std::size(kLastbrnx), 0, Program, 0x0099F8, 19698, 0x14FE3004u, 0, 0x800000, 0x800000},
    {"dynabb97", kDynabb97, std::size(kDynabb97), 0, Program, 0x05F6C0, 12288, 0xBFF412C3u, 0, 0x1000000, 0x800000},
    {"waverunr", kWaverunr, std::size(kWaverunr), 0x1000000, Program, 0x01EC58, 8192, 0x046739DDu, 0, 0x2000000, 0x1000000,
     kWaverunrCopies, std::size(kWaverunrCopies)},
    {"segawski", kSegawski, std::size(kSegawski), 0x1800000, Program, 0x023FA8, 8192, 0x046739DDu, 0, 0x2000000, 0x1000000},
    {"skisuprg", kSkisuprg, std::size(kSkisuprg), 0, Program, 0x08D480, 8192, 0x7952BA24u},
    {"topskatr", kTopskatr, std::size(kTopskatr), 0, Program, 0x02262C, 8192, 0xBCA1C375u, 0, 0x400000, 0x800000},
    {"doab", kDoab, std::size(kDoab), 0, Program, 0x009710, 12288, 0x7956B2AFu, 0, 0x2000000, 0x800000, kDoabCopies, std::size(kDoabCopies)},
    {"rchase2", kRchase2, std::size(kRchase2), 0, Program, 0x0040C0, 9336, 0xEB753AEBu},
    {"bel", kBel, std::size(kBel), 0, Program, 0x027090, 8192, 0x046EC3CEu, 0, 0x1000000, 0xC00000},
    {"desert", kDesert, std::size(kDesert), 0, Program, 0x0016A0, 2024, 0xD6D611DDu, 0, 0x800000, 0x1000000},
    {"vcop", kVcop, std::size(kVcop), 0, Program, 0x0070C0, 2024, 0x77D05CE6u},
    {"vf2", kVf2, std::size(kVf2), 0, Program, 0x003E90, 3802, 0xFD8D0383u},
    {"indy500", kIndy500, std::size(kIndy500), 0x1000000, Program, 0x014650, 12288, 0xFA3C4230u},
    {"fvipers", kFvipers, std::size(kFvipers), 0x1000000, MainData, 0x100000C, 13053, 0x92DCEC90u, 0x1800000},
    {"sgt24h", kSgt24h, std::size(kSgt24h), 0, Program, 0x05F4E0, 12288, 0x65AD115Cu, 0, 0x800000, 0x400000},
    {"overrevb", kOverrevb, std::size(kOverrevb), 0, Program, 0x01FF80, 12288, 0x097E80BBu, 0, 0x800000, 0x400000},
    {"von", kVon, std::size(kVon), 0, Program, 0x16B58C, 11038, 0x5DC298DAu},
    {"gunblade", kGunblade, std::size(kGunblade), 0, Program, 0x05B4F0, 20874, 0x847C576Cu, 0, 0x1000000, 0x800000},
    {"vstriker", kVstriker, std::size(kVstriker), 0, MainData, 0x825F3C, 6855, 0x0A5218D2u, 0, 0x800000, 0x400000},
    {"zerogun", kZerogun, std::size(kZerogun), 0, Program, 0x01BC30, 12288, 0x65D7DE7Bu, 0, 0x800000, 0x400000},
    {"dynamcop", kDynamcop, std::size(kDynamcop), 0, Program, 0x0B6084, 3712, 0x6685FDEEu, 0, 0x2000000, 0x1000000},
    {"skytargt", kSkytargt, std::size(kSkytargt), 0x1000000, MainData, 0xE7114C, 2859, 0xFB1BDB4Eu, 0, 0x2000000, 0x1000000},
    {"motoraid", kMotoraid, std::size(kMotoraid), 0x1800000, Program, 0x01BEB8, 3147, 0x201540F1u, 0, 0x2000000, 0x1000000},
    {"pltkids", kPltkids, std::size(kPltkids), 0, Program, 0x02CB70, 12288, 0xDA0D95DAu, 0, 0x2000000, 0x1000000},
    // the TGPx4's: 8,192 32-bit words (4,096 64-bit instructions), in the program ROM (matched with MAME's upload)
    {"stcc", kStcc, std::size(kStcc), 0x800000, Program, 0x077EE0, 8192, 0x2EF111DFu, 0, 0x1000000, 0x1000000,
     kStccCopies, std::size(kStccCopies)},
    {"hotdo", kHotdo, std::size(kHotdo), 0x1800000, MainData, 0xF48000, 8192, 0x49EDF71Fu, 0, 0x2000000, 0x1000000},
};

const Set &this_set() {
    for (const Set &s : kSets)
        if (!std::strcmp(s.name, M2_ROMSET)) return s;
    throw ZipError(std::string("unknown ROM set ") + M2_ROMSET);
}

std::string hex8(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "%08x", v);
    return b;
}

// The archive's name for a ROM: MAME's, or else the file with its size and
// CRC (older romsets name the same dumps differently: ep18068a.15 for
// epr-18068a.15).
std::string file_in(const Archive &z, const Load &l) {
    if (z.entries().count(l.file)) return l.file;
    for (const auto &[name, e] : z.entries())
        if (e.has_crc && e.size == l.size && (e.crc == l.crc || (l.alt_crc && e.crc == l.alt_crc))) return name;
    return l.file;
}

// The set's optional ROMs (Set::extra), all from one archive: this one, or
// the extra set's .zip or .7z in the same folder. None found: left empty.
void load_extra(const Set &set, Archive &z, const std::string &zip_path, M2Board::Images &img) {
    if (!set.extra) return;
    const auto has_all = [&](Archive &a) {
        for (const Load &l : std::span(set.extra, set.extra_count)) {
            const auto it = a.entries().find(file_in(a, l));
            if (it == a.entries().end() || it->second.size != l.size) return false;
        }
        return true;
    };
    std::unique_ptr<Archive> other;
    Archive *from = has_all(z) ? &z : nullptr;
    if (!from) {
        const size_t slash = zip_path.find_last_of("/\\");
        const std::string dir = slash == std::string::npos ? "" : zip_path.substr(0, slash + 1);
        for (const char *ext : {".zip", ".7z"}) {
            try {
                other = open_archive(dir + set.extra_set + ext);
            } catch (const std::exception &) {
                continue;
            }
            if (has_all(*other)) {
                from = other.get();
                break;
            }
        }
    }
    if (!from) return;
    std::vector<uint8_t> m1_program(0x40000, 0), pcm1(0x400000, 0);
    for (const Load &l : std::span(set.extra, set.extra_count)) {
        const std::vector<uint8_t> data = from->read(file_in(*from, l));
        if (data.size() != l.size || crc32(data.data(), data.size()) != l.crc) return; // another dump: go without
        if (l.region == M1Program) // 68000, big-endian: byte-swapped words
            for (uint32_t i = 0; i < l.size; i += 2) m1_program[l.offset + i] = data[i + 1], m1_program[l.offset + i + 1] = data[i];
        else std::copy(data.begin(), data.end(), pcm1.begin() + l.offset);
    }
    img.m1_program = std::move(m1_program);
    img.pcm1 = std::move(pcm1);
}

} // namespace

std::vector<RomCheck> check_rom_set(const std::string &zip_path) {
    std::vector<RomCheck> out;
    const auto z = open_archive(zip_path);
    const Set &set = this_set();
    for (const Load &l : std::span(set.loads, set.count)) {
        RomCheck c;
        c.file = l.file;
        const auto it = z->entries().find(file_in(*z, l));
        if (it == z->entries().end()) c.problem = "missing";
        else if (it->second.size != l.size) c.problem = "wrong size";
        else if (it->second.has_crc && it->second.crc != l.crc && !(l.alt_crc && it->second.crc == l.alt_crc))
            c.problem = "wrong CRC " + hex8(it->second.crc) + " (expected " + hex8(l.crc) + ")";
        else c.ok = true;
        out.push_back(c);
    }
    return out;
}

M2Board::Images import_rom_set(const std::string &zip_path) {
    const auto z = open_archive(zip_path);
    M2Board::Images img;
    img.program.assign(0x200000, 0);
    img.main_data.assign(0x2000000, 0);
    img.copro_data.assign(0x800000, 0);
    const Set &set = this_set();
    img.polygons.assign(set.polygons_size, std::strcmp(set.name, "doab") ? 0x00 : 0xff); // doab's region is ERASEFF
    img.copro_tables.assign(0x40000, 0);
    img.sound_program.assign(0x40000, 0);
    img.textures.assign(set.textures_size, std::strcmp(set.name, "vf2") && std::strcmp(set.name, "lastbrnx") ? 0x00 : 0xff); // vf2's and lastbrnx's regions are ERASEFF
    for (const Load &l : std::span(set.loads, set.count)) // Virtua Cop 2's sound program fills 512 KB
        if (l.region == SoundProgram && l.offset + l.size > img.sound_program.size()) img.sound_program.assign(l.offset + l.size, 0);
    for (const Load &l : std::span(set.loads, set.count)) {
        if (l.region == Pcm1 || l.region == Pcm2) { // the Model 1 sound board's MultiPCMs
            img.pcm1.assign(0x400000, 0);
            img.pcm2.assign(0x400000, 0);
        }
        if (l.region == Samples) img.samples.assign(0x800000, 0); // the SCSP's sample ROM
        if (l.region == DsbProgram) img.dsb_program.assign(0x20000, 0); // the Digital Sound Board
        if (l.region == Dsb2Program) img.dsb_program.assign(0x20000, 0); // the Digital Sound Board 2 (a 68000)
        if (l.region == DsbMpeg && img.dsb_mpeg.size() < std::max<size_t>(0x800000, l.offset + l.size))
            img.dsb_mpeg.assign(std::max<size_t>(0x800000, l.offset + l.size), 0); // Top Skater's DSB2: 16 MB
    }
    for (const Load &l : std::span(set.loads, set.count)) {
        const std::vector<uint8_t> data = z->read(file_in(*z, l)); // CRC against the archive's own
        const uint32_t crc = data.size() == l.size ? crc32(data.data(), data.size()) : 0;
        if (data.size() != l.size || (crc != l.crc && !(l.alt_crc && crc == l.alt_crc)))
            throw ZipError(std::string(l.file) + ": not the " + set.name + " ROM this build was recompiled from");
        std::vector<uint8_t> *r = nullptr;
        switch (l.region) {
        case Program: r = &img.program; break;
        case MainData: r = &img.main_data; break;
        case CoproData: r = &img.copro_data; break;
        case Polygons: r = &img.polygons; break;
        case Textures: r = &img.textures; break;
        case CoproTables: r = &img.copro_tables; break;
        case SoundProgram: // 68000, big-endian: the files hold byte-swapped words
        case Dsb2Program:
        case Samples: {
            std::vector<uint8_t> &dst = l.region == Samples ? img.samples : l.region == Dsb2Program ? img.dsb_program : img.sound_program;
            for (uint32_t i = 0; i < l.size; i += 2) {
                dst[l.offset + i] = data[i + 1];
                dst[l.offset + i + 1] = data[i];
            }
            continue;
        }
        case Pcm1: std::copy(data.begin(), data.end(), img.pcm1.begin() + l.offset); continue;
        case Pcm2: std::copy(data.begin(), data.end(), img.pcm2.begin() + l.offset); continue;
        case DsbProgram: std::copy(data.begin(), data.end(), img.dsb_program.begin() + l.offset); continue;
        case DsbMpeg: std::copy(data.begin(), data.end(), img.dsb_mpeg.begin() + l.offset); continue;
        case M1Program: continue; // only in Set::extra
        }
        for (uint32_t w = 0; w < l.size / 2; w++) {
            (*r)[l.offset + w * 4] = data[w * 2];
            (*r)[l.offset + w * 4 + 1] = data[w * 2 + 1];
        }
    }
    load_extra(set, *z, zip_path, img);
    const uint32_t mirror_end = set.mirror2_from ? set.mirror2_from - 0x100000
                                : set.mirror_from >= 0x1000000 ? 0x1F00000 : 0xF00000; // the extra data's 16 MB, or the first
    for (uint32_t dst = set.mirror_from + 0x100000; set.mirror_from && dst <= mirror_end; dst += 0x100000) // ROM_COPY mirrors
        std::copy_n(img.main_data.begin() + set.mirror_from, 0x100000, img.main_data.begin() + dst);
    for (uint32_t dst = set.mirror2_from + 0x100000; set.mirror2_from && dst <= 0x1F00000; dst += 0x100000)
        std::copy_n(img.main_data.begin() + set.mirror2_from, 0x100000, img.main_data.begin() + dst);
    for (const Copy &c : std::span(set.copies, set.copy_count)) {
        std::vector<uint8_t> &r = c.region == MainData ? img.main_data : c.region == Polygons ? img.polygons : img.textures;
        std::copy_n(r.begin() + c.from, c.size, r.begin() + c.to);
    }
    // MAME's init_zerogun and init_pltkids: the program destroys its
    // interrupt table and never fixes it (a work RAM sum stores over it);
    // that store, at 0x700 or 0x730, becomes a branch to the next instruction.
    const uint32_t patch = !std::strcmp(set.name, "zerogun") ? 0x700 : !std::strcmp(set.name, "pltkids") ? 0x730 : 0;
    if (patch) {
        const uint32_t b = 0x08000004;
        for (int i = 0; i < 4; i++) img.program[patch + i] = uint8_t(b >> (8 * i));
    }
    return img;
}

const char *rom_set_name() { return M2_ROMSET; }

std::vector<uint8_t> tgp_program(const M2Board::Images &img) {
    const Set &set = this_set();
    const uint8_t *p = (set.tgp_region == Program ? img.program : img.main_data).data() + set.tgp_offset;
    const size_t bytes = set.tgp_words * (kModel2b && !kModel2c ? 2 : 4); // the SHARC's upload is in 16-bit words
    if (crc32(p, bytes) != set.tgp_crc) throw ZipError("TGP program not where expected");
    return {p, p + bytes};
}

} // namespace rt
