#include "model1/rom_set.h"

#include "afcommon/archive.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace m1 {
namespace {

uint32_t crc32(const std::vector<uint8_t> &d) {
    static uint32_t table[256];
    if (!table[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    uint32_t c = 0xffffffffu;
    for (uint8_t b : d) c = table[(c ^ b) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

// MODEL1_CPU_BOARD (shared by every Model 1 set)
#define M1_CPU_BOARD                                                                                  \
    {"opr14742.bin", 0x446a1085, 0x20000, "copro_tables", 0x000000, RomFile::Word32},                 \
        {"opr14743.bin", 0xe8953554, 0x20000, "copro_tables", 0x000002, RomFile::Word32},             \
        {"opr-14744.58", 0x730ea9e0, 0x20000, "other_data", 0x000000, RomFile::Word32},               \
        {"opr-14745.59", 0x4c934d96, 0x20000, "other_data", 0x000002, RomFile::Word32},               \
        {"opr-14746.62", 0x2a266cbd, 0x20000, "other_data", 0x040000, RomFile::Word32},               \
        {"opr-14747.63", 0xa4ad5e19, 0x20000, "other_data", 0x040002, RomFile::Word32},               \
        {"opr14748.bin", 0x4a532cb8, 0x20000, "other_other_data", 0x000000, RomFile::Plain},          \
        {"315-5571.bin", 0x1233db2a, 0x2000, "315_5571", 0, RomFile::Plain},                          \
        {"315-5572.bin", 0x66785906, 0x2000, "315_5572", 0, RomFile::Plain}

const std::vector<RomSet> kSets = {
    {"vr",
     "Virtua Racing",
     {M1_CPU_BOARD,
      {"epr-14882.14", 0x547d75ad, 0x80000, "maincpu", 0x200000, RomFile::Byte16},
      {"epr-14883.15", 0x6bfad8b1, 0x80000, "maincpu", 0x200001, RomFile::Byte16},
      {"epr-14878a.4", 0x6d69e695, 0x20000, "maincpu", 0xfc0000, RomFile::Plain},
      {"epr-14879a.5", 0xd45af9dd, 0x20000, "maincpu", 0xfe0000, RomFile::Plain},
      {"mpr-14880.6", 0xadc7c208, 0x80000, "maincpu", 0x1000000, RomFile::Byte16},
      {"mpr-14881.7", 0xe5ab89df, 0x80000, "maincpu", 0x1000001, RomFile::Byte16},
      {"mpr-14884.8", 0x6cf9c026, 0x80000, "maincpu", 0x1100000, RomFile::Byte16},
      {"mpr-14885.9", 0xf65c9262, 0x80000, "maincpu", 0x1100001, RomFile::Byte16},
      {"mpr-14886.10", 0x92868734, 0x80000, "maincpu", 0x1200000, RomFile::Byte16},
      {"mpr-14887.11", 0x10c7c636, 0x80000, "maincpu", 0x1200001, RomFile::Byte16},
      {"mpr-14888.12", 0x04bfdc5b, 0x80000, "maincpu", 0x1300000, RomFile::Byte16},
      {"mpr-14889.13", 0xc49f0486, 0x80000, "maincpu", 0x1300001, RomFile::Byte16},
      {"315-5573.bin", 0x3335a19b, 0x2000, "tgp_copro", 0, RomFile::Plain},
      {"epr-14870a.7", 0x919d9b75, 0x20000, "m1audio", 0x00000, RomFile::WordSwap16},
      {"mpr-14873.32", 0xb1965190, 0x200000, "mpcm1", 0x000000, RomFile::Plain},
      {"mpr-14876.4", 0xba6b2327, 0x200000, "mpcm2", 0x000000, RomFile::Plain},
      {"mpr-14890.26", 0xdcbe006b, 0x200000, "polygons", 0x000000, RomFile::Word32},
      {"mpr-14891.27", 0x25832b38, 0x200000, "polygons", 0x000002, RomFile::Word32},
      {"mpr-14892.28", 0x5136f3ba, 0x200000, "polygons", 0x400000, RomFile::Word32},
      {"mpr-14893.29", 0x1c531ada, 0x200000, "polygons", 0x400002, RomFile::Word32},
      {"mpr-14894.30", 0x830a71bc, 0x200000, "polygons", 0x800000, RomFile::Word32},
      {"mpr-14895.31", 0xaf027ac5, 0x200000, "polygons", 0x800002, RomFile::Word32},
      {"mpr-14896.32", 0x382091dc, 0x200000, "polygons", 0xc00000, RomFile::Word32},
      {"mpr-14897.33", 0x74873195, 0x200000, "polygons", 0xc00002, RomFile::Word32},
      {"mpr-14898.39", 0x61da2bb6, 0x80000, "copro_data", 0x000000, RomFile::Byte32},
      {"mpr-14899.40", 0x2cd58bee, 0x80000, "copro_data", 0x000001, RomFile::Byte32},
      {"mpr-14900.41", 0xaa7c017d, 0x80000, "copro_data", 0x000002, RomFile::Byte32},
      {"mpr-14901.42", 0x175b7a9a, 0x80000, "copro_data", 0x000003, RomFile::Byte32},
      {"93c45.bin", 0x65aac303, 0x80, "eeprom", 0x00, RomFile::Plain}},
     0xfc0000, 0x1000000,
     1041300}, // 127.04M sound instructions in 122 s
    {"swa",
     "Star Wars Arcade (US)",
     {M1_CPU_BOARD,
      {"epr-16669.14", 0x52e5e7e4, 0x80000, "maincpu", 0x200000, RomFile::Byte16},
      {"epr-16670.15", 0x1e7ecabd, 0x80000, "maincpu", 0x200001, RomFile::Byte16},
      {"epr-16668.5", 0x9e112425, 0x80000, "maincpu", 0xf80000, RomFile::Plain},
      {"epr-16668.5", 0x9e112425, 0x80000, "maincpu", 0x000000, RomFile::Plain}, // ROM_RELOAD
      {"epr-16668.5", 0x9e112425, 0x80000, "maincpu", 0x080000, RomFile::Plain}, // ROM_RELOAD
      // MAME's CRC is marked BAD_DUMP; this is the dump every set has
      {"315-5711.bin", 0xc5ddb8fc, 0x2000, "tgp_copro", 0, RomFile::Plain},
      {"epr-16470.7", 0x7da18cf7, 0x20000, "m1audio", 0x00000, RomFile::WordSwap16},
      {"epr-16470.7", 0x7da18cf7, 0x20000, "m1audio", 0x80000, RomFile::WordSwap16}, // ROM_RELOAD
      {"mpr-16486.32", 0x7df50533, 0x200000, "mpcm1", 0x000000, RomFile::Plain},
      {"mpr-16487.33", 0x31b28dfa, 0x200000, "mpcm1", 0x200000, RomFile::Plain},
      {"mpr-16484.4", 0x9d4c334d, 0x200000, "mpcm2", 0x000000, RomFile::Plain},
      {"mpr-16485.5", 0x95aadcad, 0x200000, "mpcm2", 0x200000, RomFile::Plain},
      {"epr-16471.2", 0xf4ee84a4, 0x20000, "dsb_cpu", 0x000000, RomFile::Plain},
      {"mpr-16514.57", 0x3175b0be, 0x200000, "dsb_mpeg", 0x000000, RomFile::Plain},
      {"mpr-16515.58", 0x3114d748, 0x200000, "dsb_mpeg", 0x200000, RomFile::Plain},
      {"mpr-16476.26", 0xd48609ae, 0x200000, "polygons", 0x000000, RomFile::Word32},
      {"mpr-16477.27", 0xb979b082, 0x200000, "polygons", 0x000002, RomFile::Word32},
      {"mpr-16478.28", 0x80c780f7, 0x200000, "polygons", 0x400000, RomFile::Word32},
      {"mpr-16479.29", 0xe43183b3, 0x200000, "polygons", 0x400002, RomFile::Word32},
      {"mpr-16480.30", 0x3185547a, 0x200000, "polygons", 0x800000, RomFile::Word32},
      {"mpr-16481.31", 0xce8d76fe, 0x200000, "polygons", 0x800002, RomFile::Word32},
      {"mpr-16472.39", 0x5a0d7553, 0x80000, "copro_data", 0x000000, RomFile::Byte32},
      {"mpr-16473.40", 0x876c5399, 0x80000, "copro_data", 0x000001, RomFile::Byte32},
      {"mpr-16474.41", 0x5864a26f, 0x80000, "copro_data", 0x000002, RomFile::Byte32},
      {"mpr-16475.42", 0xb9266be9, 0x80000, "copro_data", 0x000003, RomFile::Byte32}},
     0x080000, 0x100000, // the program's copy at 080000 (it is also at F80000, where reset starts)
     762760}, // 94.58M sound instructions in 124.0 s (7,133 frames)
    {"vf",
     "Virtua Fighter",
     {M1_CPU_BOARD,
      {"epr-16082.14", 0xb23f22ee, 0x80000, "maincpu", 0x200000, RomFile::Byte16},
      {"epr-16083.15", 0xd12c77f8, 0x80000, "maincpu", 0x200001, RomFile::Byte16},
      {"epr-16080.4", 0x3662e1a5, 0x20000, "maincpu", 0xfc0000, RomFile::Plain},
      {"epr-16081.5", 0x6dec06ce, 0x20000, "maincpu", 0xfe0000, RomFile::Plain},
      {"mpr-16084.6", 0x483f453b, 0x80000, "maincpu", 0x1000000, RomFile::Byte16},
      {"mpr-16085.7", 0x5fa01277, 0x80000, "maincpu", 0x1000001, RomFile::Byte16},
      {"mpr-16086.8", 0xdeac47a1, 0x80000, "maincpu", 0x1100000, RomFile::Byte16},
      {"mpr-16087.9", 0x7a64daac, 0x80000, "maincpu", 0x1100001, RomFile::Byte16},
      {"mpr-16088.10", 0xfcda2d1e, 0x80000, "maincpu", 0x1200000, RomFile::Byte16},
      {"mpr-16089.11", 0x39befbe0, 0x80000, "maincpu", 0x1200001, RomFile::Byte16},
      {"mpr-16090.12", 0x90c76831, 0x80000, "maincpu", 0x1300000, RomFile::Byte16},
      {"mpr-16091.13", 0x53115448, 0x80000, "maincpu", 0x1300001, RomFile::Byte16},
      // MAME marks it BAD_DUMP; this is the dump every set has
      {"315-5724.bin", 0x4b4f330e, 0x2000, "tgp_copro", 0, RomFile::Plain},
      {"epr-16120.7", 0x2bff8378, 0x20000, "m1audio", 0x00000, RomFile::WordSwap16},
      {"epr-16121.8", 0xff6723f9, 0x20000, "m1audio", 0x20000, RomFile::WordSwap16},
      {"epr-16121.8", 0xff6723f9, 0x20000, "m1audio", 0x80000, RomFile::WordSwap16}, // ROM_RELOAD
      {"mpr-16122.32", 0x568bc64e, 0x200000, "mpcm1", 0x000000, RomFile::Plain},
      {"mpr-16123.33", 0x15d78844, 0x200000, "mpcm1", 0x200000, RomFile::Plain},
      {"mpr-16124.4", 0x45520ba1, 0x200000, "mpcm2", 0x000000, RomFile::Plain},
      {"mpr-16125.5", 0x9b4998b6, 0x200000, "mpcm2", 0x200000, RomFile::Plain},
      {"mpr-16096.26", 0xa92b0bf3, 0x200000, "polygons", 0x000000, RomFile::Word32},
      {"mpr-16097.27", 0x0232955a, 0x200000, "polygons", 0x000002, RomFile::Word32},
      {"mpr-16098.28", 0xcf2e1b84, 0x200000, "polygons", 0x400000, RomFile::Word32},
      {"mpr-16099.29", 0x20e46854, 0x200000, "polygons", 0x400002, RomFile::Word32},
      {"mpr-16100.30", 0xe13e983d, 0x200000, "polygons", 0x800000, RomFile::Word32},
      {"mpr-16101.31", 0x0dbed94d, 0x200000, "polygons", 0x800002, RomFile::Word32},
      {"mpr-16102.32", 0x4cb41fb6, 0x200000, "polygons", 0xc00000, RomFile::Word32},
      {"mpr-16103.33", 0x526d1c76, 0x200000, "polygons", 0xc00002, RomFile::Word32}},
     0xfc0000, 0x1000000,
     765065, // 45.90M sound instructions in 60 s of attract
     true}, // the I/O board's BIOS is EPR-14869B (MAME's vf machine)
    {"wingwar",
     "Wing War (World)",
     {M1_CPU_BOARD,
      {"epr-16729.14", 0x7edec2cc, 0x80000, "maincpu", 0x200000, RomFile::Byte16},
      {"epr-16730.15", 0xbab24dee, 0x80000, "maincpu", 0x200001, RomFile::Byte16},
      {"epr-16953.4", 0xc821a920, 0x20000, "maincpu", 0xfc0000, RomFile::Plain},
      {"epr-16953.4", 0xc821a920, 0x20000, "maincpu", 0x000000, RomFile::Plain}, // ROM_RELOAD
      {"epr-16952.5", 0x03a3ecc5, 0x20000, "maincpu", 0xfe0000, RomFile::Plain},
      {"epr-16952.5", 0x03a3ecc5, 0x20000, "maincpu", 0x020000, RomFile::Plain}, // ROM_RELOAD
      {"mpr-16738.6", 0x51518ffa, 0x80000, "maincpu", 0x1000000, RomFile::Byte16},
      {"mpr-16737.7", 0x37b1379c, 0x80000, "maincpu", 0x1000001, RomFile::Byte16},
      {"mpr-16736.8", 0x10b6a025, 0x80000, "maincpu", 0x1100000, RomFile::Byte16},
      {"mpr-16735.9", 0xc82fd198, 0x80000, "maincpu", 0x1100001, RomFile::Byte16},
      {"mpr-16734.10", 0xf76371c1, 0x80000, "maincpu", 0x1200000, RomFile::Byte16},
      {"mpr-16733.11", 0xe105847b, 0x80000, "maincpu", 0x1200001, RomFile::Byte16},
      // MAME's CRC is marked BAD_DUMP; this is the dump every set has
      {"315-5711.bin", 0xc5ddb8fc, 0x2000, "tgp_copro", 0, RomFile::Plain},
      {"epr-17126.7", 0x50178e40, 0x20000, "m1audio", 0x00000, RomFile::WordSwap16},
      {"epr-16752.8", 0x6541c48f, 0x20000, "m1audio", 0x20000, RomFile::WordSwap16},
      {"epr-16752.8", 0x6541c48f, 0x20000, "m1audio", 0x80000, RomFile::WordSwap16}, // ROM_RELOAD
      {"mpr-16753.32", 0x324a8333, 0x200000, "mpcm1", 0x000000, RomFile::Plain},
      {"mpr-16754.33", 0x144f3cf5, 0x200000, "mpcm1", 0x200000, RomFile::Plain},
      {"mpr-16755.4", 0x4baaf878, 0x200000, "mpcm2", 0x000000, RomFile::Plain},
      {"mpr-16756.5", 0xd9c40672, 0x200000, "mpcm2", 0x200000, RomFile::Plain},
      {"mpr-16743.26", 0xa710d33c, 0x200000, "polygons", 0x000000, RomFile::Word32},
      {"mpr-16744.27", 0xde796e1f, 0x200000, "polygons", 0x000002, RomFile::Word32},
      {"mpr-16745.28", 0x905b689c, 0x200000, "polygons", 0x400000, RomFile::Word32},
      {"mpr-16746.29", 0x163b312e, 0x200000, "polygons", 0x400002, RomFile::Word32},
      {"mpr-16747.30", 0x7353bb12, 0x200000, "polygons", 0x800000, RomFile::Word32},
      {"mpr-16748.31", 0x8ce98d3a, 0x200000, "polygons", 0x800002, RomFile::Word32},
      {"mpr-16749.32", 0x0e36dc1a, 0x200000, "polygons", 0xc00000, RomFile::Word32},
      {"mpr-16750.33", 0xe4f0b98d, 0x200000, "polygons", 0xc00002, RomFile::Word32},
      {"mpr-16741.39", 0x84b2ffd8, 0x80000, "copro_data", 0x000000, RomFile::Byte32},
      {"mpr-16742.40", 0xe9cc12bb, 0x80000, "copro_data", 0x000001, RomFile::Byte32},
      {"mpr-16739.41", 0x6c73e98f, 0x80000, "copro_data", 0x000002, RomFile::Byte32},
      {"mpr-16740.42", 0x44b31007, 0x80000, "copro_data", 0x000003, RomFile::Byte32},
      // the I/O board (837-10859): MAME's model1io2 device, BIOS EPR-16891
      {"epr-16891.6", 0xa33f84d1, 0x10000, "io2_bios", 0, RomFile::Plain, "model1io2"}},
     0x000000, 0x040000, // the program's copies at 000000 (also at FC0000, where reset starts)
     765525, // 45.93M sound instructions in 60 s of attract
     false,
     true, // the advanced I/O board
     // the routines it runs from backup RAM, copied there from the program
     {
         // Wing War's code in backup RAM: its boot table (ROM 003668) and
         // the per-mode tables (00365C: 0036CC, 003730) copy these (copy loops
         // at 002538 and 002560); run_native picks the one RAM holds
         {0x00258e, 0x40d000, 0x015}, {0x0025a3, 0x40d000, 0x07c}, {0x002701, 0x40d200, 0x0a7},
         {0x0027a8, 0x40d200, 0x10e}, {0x002a1a, 0x40d400, 0x0bb}, {0x002ad5, 0x40d400, 0x122},
         {0x002de4, 0x40d600, 0x0cb}, {0x002eaf, 0x40d600, 0x132}, {0x002bf7, 0x40d800, 0x0c3},
         {0x002cba, 0x40d800, 0x12a}, {0x002fe1, 0x40da00, 0x13a}, // (and 00311B, the same bytes)
         {0x003255, 0x40dc00, 0x0cf}, {0x003324, 0x40dc00, 0x0c0}, {0x003456, 0x40dc00, 0x0c0},
         {0x003e2c, 0x40de00, 0x044}, {0x003e70, 0x40de00, 0x090}}},
};

const std::vector<Region> kRegions = {
    {"maincpu", 0x2000000, 0xff},  {"tgp_copro", 0x2000, 0},      {"m1audio", 0xc0000, 0},
    {"mpcm1", 0x400000, 0},        {"mpcm2", 0x400000, 0},        {"polygons", 0x1000000, 0},
    {"copro_data", 0x200000, 0},   {"copro_tables", 0x40000, 0},  {"other_data", 0x80000, 0},
    {"other_other_data", 0x20000, 0}, {"315_5571", 0x2000, 0},    {"315_5572", 0x2000, 0},
    {"eeprom", 0x80, 0xff},         {"dsb_cpu", 0x20000, 0},       {"dsb_mpeg", 0x800000, 0},
    {"io2_bios", 0x10000, 0},
};

class Source {
public:
    explicit Source(const std::string &path) : path_(path) {
        if (!std::filesystem::is_directory(path)) archive_ = afc::open_archive(path);
    }
    // By name, else by size and CRC (sets name some files differently); empty if absent
    std::vector<uint8_t> find(const std::string &name, uint32_t size, uint32_t crc) {
        if (archive_) {
            if (archive_->entries().count(name)) return archive_->read(name);
            for (const auto &[n, e] : archive_->entries())
                if (e.size == size && e.has_crc && e.crc == crc) return archive_->read(n);
            return {};
        }
        std::ifstream f(path_ + "/" + name, std::ios::binary);
        if (f) return {std::istreambuf_iterator<char>(f), {}};
        std::error_code ec;
        for (const auto &e : std::filesystem::directory_iterator(path_, ec))
            if (e.is_regular_file() && e.file_size() == size) {
                std::ifstream g(e.path(), std::ios::binary);
                std::vector<uint8_t> d{std::istreambuf_iterator<char>(g), {}};
                if (crc32(d) == crc) return d;
            }
        return {};
    }
    std::vector<uint8_t> read(const std::string &name, uint32_t size, uint32_t crc) {
        std::vector<uint8_t> d = find(name, size, crc);
        if (d.empty()) throw std::runtime_error(path_ + ": missing " + name);
        return d;
    }

private:
    std::string path_;
    std::unique_ptr<afc::Archive> archive_;
};

} // namespace

const RomSet *find_rom_set(const std::string &name) {
    for (const auto &s : kSets)
        if (name == s.name) return &s;
    return nullptr;
}

const std::vector<Region> &regions() { return kRegions; }

// A device's archive (or folder) beside the set's: DIR/model1io2.zip, .7z or DIR/model1io2/
std::vector<uint8_t> device_file(const std::string &set_path, const RomFile &f) {
    const std::filesystem::path dir = std::filesystem::path(set_path).parent_path();
    for (const char *ext : {".zip", ".7z", ""}) {
        const std::filesystem::path p = dir / (std::string(f.device) + ext);
        std::error_code ec;
        if (!std::filesystem::exists(p, ec)) continue;
        Source s(p.string());
        std::vector<uint8_t> d = s.find(f.name, f.size, f.crc);
        if (!d.empty()) return d;
    }
    throw std::runtime_error(set_path + ": missing " + f.name + " (MAME's " + f.device + " set: put " + f.device +
                             ".zip beside it)");
}

Images load_rom_set(const RomSet &set, const std::string &path) {
    Source src(path);
    Images img;
    for (const auto &r : kRegions) img[r.name].assign(r.size, r.fill);
    for (const auto &f : set.files) {
        std::vector<uint8_t> d = f.device ? src.find(f.name, f.size, f.crc) : src.read(f.name, f.size, f.crc);
        if (d.empty()) d = device_file(path, f);
        if (d.size() != f.size)
            throw std::runtime_error(std::string(f.name) + ": wrong size " + std::to_string(d.size()));
        if (crc32(d) != f.crc) throw std::runtime_error(std::string(f.name) + ": wrong CRC");
        std::vector<uint8_t> &r = img.at(f.region);
        for (uint32_t i = 0; i < f.size; i++) {
            uint32_t o = 0;
            switch (f.kind) {
            case RomFile::Plain: o = f.offset + i; break;
            case RomFile::Byte16: o = f.offset + i * 2; break;
            case RomFile::Word32: o = f.offset + (i / 2) * 4 + (i & 1); break;
            case RomFile::Byte32: o = f.offset + i * 4; break;
            case RomFile::WordSwap16: o = f.offset + (i ^ 1); break;
            }
            r.at(o) = d[i];
        }
    }
    return img;
}

} // namespace m1
