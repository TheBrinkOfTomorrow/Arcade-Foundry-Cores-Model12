// m1rom: one region of a Model 1 ROM set, as the board sees it (after
// de-interleaving), written to a file. For tools that take a raw image, such
// as m2tgprecomp --model1 (the TGP program, region tgp_copro). The output is
// ROM data: keep it under build/, never commit it.
//
//   m1rom ROMSET REGION OUT.bin [--set vr]
#include "model1/rom_set.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: m1rom ROMSET REGION OUT.bin [--set NAME]\n");
        return 2;
    }
    std::string set_name = "vr";
    for (int i = 4; i + 1 < argc; i++)
        if (!std::strcmp(argv[i], "--set")) set_name = argv[i + 1];
    try {
        const m1::RomSet *set = m1::find_rom_set(set_name);
        if (!set) throw std::runtime_error("unknown set " + set_name);
        const m1::Images img = m1::load_rom_set(*set, argv[1]);
        const auto it = img.find(argv[2]);
        if (it == img.end()) throw std::runtime_error(std::string("no region ") + argv[2]);
        std::ofstream o(argv[3], std::ios::binary);
        o.write(reinterpret_cast<const char *>(it->second.data()), std::streamsize(it->second.size()));
        if (!o) throw std::runtime_error(std::string("cannot write ") + argv[3]);
        std::printf("m1rom: %s, %zu bytes\n", argv[2], it->second.size());
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "m1rom: %s\n", e.what());
        return 1;
    }
}
