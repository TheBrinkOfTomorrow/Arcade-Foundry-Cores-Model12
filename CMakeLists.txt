cmake_minimum_required(VERSION 3.20)
project(daytona_recomp LANGUAGES C CXX)
# Single-config generators build unoptimised without a build type, and the
# SoftFloat build below relies on its inline helpers being inlined.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
# M2_CORE: also build the game as a library (m2core, common/include/afcore.h). Its
# static dependencies are then position-independent.
option(M2_CORE "Build the game core library (C interface)" OFF)
# M1_CORE: the same for a Model 1 game (cmake/model1.cmake).
option(M1_CORE "Build the Model 1 game core library (C interface)" OFF)
if(M2_CORE OR M1_CORE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
set(M2_GEN_ROOT "${CMAKE_BINARY_DIR}/gen" CACHE PATH
    "Directory containing recompiled daytona93/daytona93_tgp/daytona93_snd sources")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Plain char is unsigned on arm64 (and 32-bit ARM) Linux, signed on x86-64
# and on Apple's arm64, where the recompiled games are checked against MAME.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm)")
    add_compile_options(-fsigned-char)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # /bigobj: the generated game code has many labels per object file
    add_compile_options(/W4 /fp:strict /bigobj /utf-8)
    # The Visual Studio generator compiles one file at a time per target
    # without /MP; setup's --jobs only sets how many targets build at once.
    if(CMAKE_GENERATOR MATCHES "Visual Studio" AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/MP)
    endif()
else()
    # No -ffast-math, ever: exact rounding is the point. No FP
    # contraction either: a fused multiply-add (ARM64 always has one) rounds
    # once where the TGP's code rounds twice.
    add_compile_options(-Wall -Wextra -fno-fast-math -ffp-contract=off)
endif()

# Berkeley SoftFloat 3e (scripts/fetch_softfloat.sh): the extF80 reference
# model for i960 FP. 8086-SSE specialisation, FAST_INT64 build.
set(SOFTFLOAT_DIR ${CMAKE_SOURCE_DIR}/extern/softfloat)
include(cmake/softfloat_sources.cmake)
add_library(softfloat STATIC ${SOFTFLOAT_SOURCES})
# SoftFloat's GCC platform header needs __int128, __builtin_clz and GNU inline
# rules, which MSVC rejects (and clang-cl's are not GCC's). Those compilers get
# our portable one (cmake/softfloat-portable): same results, plain functions.
# M2_SOFTFLOAT_PORTABLE forces it anywhere, to test that path off Windows.
option(M2_SOFTFLOAT_PORTABLE "Build SoftFloat without GCC extensions" OFF)
if(MSVC OR M2_SOFTFLOAT_PORTABLE)
    set(SOFTFLOAT_PLATFORM ${CMAKE_SOURCE_DIR}/cmake/softfloat-portable)
    set(SOFTFLOAT_INLINE)
else()
    set(SOFTFLOAT_PLATFORM ${SOFTFLOAT_DIR}/build/Linux-x86_64-GCC)
    set(SOFTFLOAT_INLINE INLINE_LEVEL=5)
endif()
target_include_directories(softfloat
    PUBLIC ${SOFTFLOAT_DIR}/source/include
    PRIVATE ${SOFTFLOAT_PLATFORM} ${SOFTFLOAT_DIR}/source/8086-SSE)
target_compile_definitions(softfloat PRIVATE
    SOFTFLOAT_ROUND_ODD ${SOFTFLOAT_INLINE} SOFTFLOAT_FAST_DIV32TO16 SOFTFLOAT_FAST_DIV64TO32)
# FAST_INT64 selects the by-value extF80 API and LITTLEENDIAN the field order of
# extFloat80_t in the public headers, so users must see both (a mismatch silently
# swaps signExp and signif across the call boundary). Little-endian hosts only.
# Rounding mode and flags are globals in SoftFloat: make them per thread.
# C++ users get __thread, not thread_local: on macOS an extern thread_local is
# reached through a C++ wrapper function the C definitions never emit. MSVC and
# clang-cl (Windows TLS has no such wrapper) use __declspec(thread) in C, which
# both accept where MSVC's C may not take _Thread_local, and thread_local in C++.
if(MSVC)
    set(SOFTFLOAT_TLS_C "__declspec(thread)")
    set(SOFTFLOAT_TLS_CXX thread_local)
else()
    set(SOFTFLOAT_TLS_C _Thread_local)
    set(SOFTFLOAT_TLS_CXX __thread)
endif()
target_compile_definitions(softfloat PUBLIC SOFTFLOAT_FAST_INT64 LITTLEENDIAN=1
    $<$<COMPILE_LANGUAGE:C>:THREAD_LOCAL=${SOFTFLOAT_TLS_C}>
    $<$<COMPILE_LANGUAGE:CXX>:THREAD_LOCAL=${SOFTFLOAT_TLS_CXX}>)
set_target_properties(softfloat PROPERTIES C_STANDARD 11)
if(NOT MSVC)
    target_compile_options(softfloat PRIVATE -w) # upstream code, not ours
endif()

add_library(i960 STATIC
    src/i960/isa.cpp
    src/i960/decode.cpp
    src/i960/reach.cpp
    src/i960/fp.cpp
)
target_include_directories(i960 PUBLIC src)
target_link_libraries(i960 PUBLIC softfloat)

add_library(trace STATIC src/trace/trace.cpp)
target_include_directories(trace PUBLIC src)

# Runtime the recompiled game runs on: i960 context and services (call/ret,
# interrupts, memory; MAME's semantics, transplanted, BSD-3), replay bus and
# lockstep, and MAME's i960 interpreter (src/refcore) as the fallback for
# code the recompiler did not reach (GameLoop; M2_STRICT=1 turns it off).
# The ROM set the game is recompiled from: one of M2_SETS (games.toml, which
# scripts/gen_games.py writes into cmake/games.cmake), daytona93 by default.
# One set per build directory: its generated code, seeds and saves are its own.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/games.cmake)
string(REPLACE ";" ", " M2_SETS_TEXT "${M2_SETS}")
set(M2_ROMSET daytona93 CACHE STRING "MAME ROM set to recompile: ${M2_SETS_TEXT}")
set_property(CACHE M2_ROMSET PROPERTY STRINGS ${M2_SETS})
if(NOT M2_ROMSET IN_LIST M2_SETS)
    message(FATAL_ERROR "M2_ROMSET must be one of ${M2_SETS_TEXT} (games.toml), not ${M2_ROMSET}")
endif()
# The board the set runs on, as MAME's driver classes name them: model2o (the
# original Model 2, Daytona) or model2a (Model 2A-CRX, Sega Rally, Manx TT
# Virtua Cop 2 and Virtua Fighter 2), both in src/runtime/m2_board.cpp. Sega Rally's build
# names its game segarally, Manx TT's manxtt, Virtua Cop 2's vcop2. Virtua
# Cop is on the original board, its build named vcop. Virtua Fighter 2's is vf2.
# Indy 500 is on Model 2B-CRX (model2b: a SHARC geometry processor in place
# of the TGP), its build named indy500; Fighting Vipers, Super GT 24h and Over Rev
# too (builds fvipers, sgt24h, overrevb), Virtual-On, Gunblade NY, Virtua Striker, Zero Gunner and Pilot Kids (von, gunblade, vstriker, zerogun, pltkids). Sega Touring Car Championship is on
# Model 2C-CRX (model2c: Model 2B's board with a TGPx4 for the SHARC), its
# build named stcc; The House of the Dead too (build hotd).
if(DEFINED M2_BOARD_${M2_ROMSET})
    set(M2_BOARD ${M2_BOARD_${M2_ROMSET}})
else()
    set(M2_BOARD model2o)
endif()

# The Digital Sound Board (Star Wars Arcade on Model 1, Sega Touring Car
# Championship on Model 2C): its i8251, MPEG registers and decoder around the
# recompiled Z80 program (dsbrecomp's dsb_gen.cpp, linked by the game).
add_library(dsb STATIC src/model1/dsb.cpp src/model1/mpeg_audio.cpp)
target_include_directories(dsb PUBLIC src)
if(NOT MSVC)
    set_source_files_properties(src/model1/mpeg_audio.cpp PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter")
endif()

add_library(runtime STATIC
    src/runtime/cpu.cpp
    src/runtime/tgp.cpp
    src/runtime/m2_tgp_board.cpp
    src/runtime/sharc/sharc.cpp
    src/runtime/io_315_5649.cpp
    src/runtime/geo.cpp
    src/runtime/raster.cpp
    src/runtime/video.cpp
    src/runtime/m2_board.cpp
    src/runtime/game_loop.cpp
    src/runtime/rom_import.cpp
    src/runtime/m2_replay_bus.cpp
    src/runtime/lockstep.cpp
    src/runtime/snd_cpu.cpp
    src/runtime/multipcm.cpp
    src/runtime/sound_board.cpp
    src/runtime/scsp.cpp
    src/runtime/scsp_board.cpp
    src/runtime/dsb2.cpp
    src/runtime/native_sample_mixer.cpp
    src/runtime/native_sound_sequencer.cpp
    src/runtime/native_sound_engine.cpp
    src/runtime/enhance.cpp
    src/runtime/comm_board.cpp
    src/runtime/crypt5881/crypt5881.cpp
    src/refcore/i960_ref.cpp
)
target_include_directories(runtime PUBLIC src)
target_compile_definitions(runtime PUBLIC M2_FALLBACK_INTERP=1)
# M2_MODEL2A: the Model 2A-CRX family (model2a, model2b and model2c: SCSP
# sound, the 315-5649 I/O chip); M2_MODEL2B: the SHARC board; M2_MODEL2C:
# the TGPx4 board.
if(M2_BOARD MATCHES "^model2[abc]$")
    set(M2_MODEL2A 1)
else()
    set(M2_MODEL2A 0)
endif()
if(M2_BOARD STREQUAL "model2b")
    set(M2_MODEL2B 1)
else()
    set(M2_MODEL2B 0)
endif()
if(M2_BOARD STREQUAL "model2c")
    set(M2_MODEL2C 1)
else()
    set(M2_MODEL2C 0)
endif()
# M2_DSB: the Digital Sound Board (Model 2C's Touring Car; The House of the Dead has none).
# M2_DSB2 as well: the Digital Sound Board 2, a 68000 (Top Skater).
set(M2_DSB2 0)
if(M2_ROMSET STREQUAL "stcc")
    set(M2_DSB 1)
elseif(M2_ROMSET STREQUAL "topskatr")
    set(M2_DSB 1)
    set(M2_DSB2 1)
else()
    set(M2_DSB 0)
endif()
# M2_M1SND: a Model 1 sound board beside the SCSP (Manx TT's engine sounds).
if(M2_ROMSET STREQUAL "manxttc")
    set(M2_M1SND 1)
else()
    set(M2_M1SND 0)
endif()
target_compile_definitions(runtime PUBLIC M2_ROMSET="${M2_ROMSET}" M2_BOARD="${M2_BOARD}" M2_MODEL2A=${M2_MODEL2A} M2_MODEL2B=${M2_MODEL2B}
    M2_MODEL2C=${M2_MODEL2C} M2_DSB=${M2_DSB} M2_DSB2=${M2_DSB2} M2_M1SND=${M2_M1SND})
# M2_FAST_PATHS: the ports' exact speed-ups: the Vita's rasterizer and
# 2D-layer caches (M2_VITA_RENDER_OPT) and the runtime fast paths that need
# no rewritten code (src/runtime/fast_paths.h), and the 3D drawn on several
# threads (M2_RASTER_THREADS in the environment sets how many). The same
# pixels and sound, checked frame by frame against the reference.
# On by default on every platform since 2026-11-04 (Insert-Coin #288 3d;
# before that only arm64 Linux, iOS and tvOS). -DM2_FAST_PATHS=OFF builds the
# reference, for lockstep runs and for checking a fast path against it.
set(M2_FAST_PATHS_DEFAULT ON)
option(M2_FAST_PATHS "Exact rasterizer caches, runtime fast paths and 3D threads (OFF: the reference)" ${M2_FAST_PATHS_DEFAULT})
if(M2_FAST_PATHS)
    target_compile_definitions(runtime PUBLIC M2_VITA_RENDER_OPT=1 M2_FAST_BUS=1 M2_FAST_TGP=1 M2_FAST_GEO=1)
    # The 3D drawn by several threads, each its own rows (src/runtime/raster.cpp).
    find_package(Threads REQUIRED)
    target_compile_definitions(runtime PUBLIC M2_RASTER_BANDS=1)
    target_link_libraries(runtime PUBLIC Threads::Threads)
endif()
# 7z ROM sets: the 7-Zip LZMA SDK's decoder (public-domain files only),
# fetched by scripts/setup.py into extern/lzma.
set(LZMA_DIR ${CMAKE_SOURCE_DIR}/extern/lzma/C)
if(EXISTS ${LZMA_DIR}/7zArcIn.c)
    add_library(lzma7z STATIC
        ${LZMA_DIR}/7zArcIn.c ${LZMA_DIR}/7zDec.c ${LZMA_DIR}/7zBuf.c ${LZMA_DIR}/7zCrc.c ${LZMA_DIR}/7zCrcOpt.c
        ${LZMA_DIR}/7zStream.c ${LZMA_DIR}/7zAlloc.c ${LZMA_DIR}/Alloc.c ${LZMA_DIR}/LzmaDec.c ${LZMA_DIR}/Lzma2Dec.c
        ${LZMA_DIR}/Bra.c ${LZMA_DIR}/Bra86.c ${LZMA_DIR}/BraIA64.c ${LZMA_DIR}/Bcj2.c ${LZMA_DIR}/Delta.c
        ${LZMA_DIR}/CpuArch.c ${LZMA_DIR}/Ppmd7.c ${LZMA_DIR}/Ppmd7Dec.c)
    target_include_directories(lzma7z SYSTEM PUBLIC ${LZMA_DIR})
    target_compile_definitions(lzma7z PRIVATE Z7_PPMD_SUPPORT) # 7z's PPMd method too, not only LZMA/LZMA2
    if(NOT MSVC)
        target_compile_options(lzma7z PRIVATE -w)
    endif()
else()
    message(STATUS "extern/lzma not found: 7z ROM sets unsupported (run scripts/setup.py)")
endif()
# The core interface header and the zip/7z readers, shared with the other
# core repos (submodule common/, Arcade-Foundry-Cores-Common); it uses
# lzma7z above when there is one.
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/common/CMakeLists.txt)
    message(FATAL_ERROR "common/ is empty: run git submodule update --init")
endif()
add_subdirectory(common)
target_link_libraries(runtime PUBLIC afc::afcommon afc::afcore)
target_link_libraries(runtime PUBLIC trace i960 dsb)

# YM3438 (the sound board's FM chip): ymfm, BSD-3, fetched by
# scripts/setup.py into extern/ymfm.
set(YMFM_DIR ${CMAKE_SOURCE_DIR}/extern/ymfm/src)
if(NOT EXISTS ${YMFM_DIR}/ymfm_opn.cpp)
    message(FATAL_ERROR "extern/ymfm not found: run scripts/setup.py")
endif()
add_library(ymfm STATIC ${YMFM_DIR}/ymfm_opn.cpp ${YMFM_DIR}/ymfm_adpcm.cpp ${YMFM_DIR}/ymfm_ssg.cpp)
target_include_directories(ymfm SYSTEM PUBLIC ${YMFM_DIR})
if(NOT MSVC)
    target_compile_options(ymfm PRIVATE -w)
endif()
target_link_libraries(runtime PUBLIC ymfm)

# Reference interpreter (MAME's i960 executor, transplanted, BSD-3): m2replay's
# oracle, and the game's fallback for unseeded code. It is built into the
# runtime; refcore is kept as a name for the tools that link it.
add_library(refcore INTERFACE)
target_link_libraries(refcore INTERFACE runtime)
if(NOT MSVC)
    # Upstream MAME code: keep its own warnings out of ours.
    set_source_files_properties(src/runtime/cpu.cpp src/refcore/i960_ref.cpp src/runtime/geo.cpp src/runtime/crypt5881/crypt5881.cpp PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-sign-compare;-Wno-unused-but-set-variable;-Wno-attributes")
endif()

include(cmake/model1.cmake)

add_executable(m2replay tools/m2replay/main.cpp)
target_link_libraries(m2replay PRIVATE refcore)

# The recompiler (ours; committed) and, when it has been run, its output.
add_executable(m2recomp tools/m2recomp/main.cpp)
target_link_libraries(m2recomp PRIVATE i960)

# The sound CPU (68000) decoder and static recompiler.
add_library(m68k STATIC src/m68k/decode.cpp src/m68k/reach.cpp)
target_include_directories(m68k PUBLIC src)
add_executable(m2sndrecomp tools/m2sndrecomp/main.cpp)
target_link_libraries(m2sndrecomp PRIVATE m68k)

# The windowed app: SDL3 (window, input, SDL_GPU: Vulkan, Direct3D 12, Metal),
# fetched by scripts/setup.py into extern/sdl3, Dear ImGui for the launcher,
# and the app's own code. Built whenever SDL is there, ROM set or not, so CI
# compiles all of it; the daytona executable itself also needs the generated
# game code (below).
# Off by default: the game cores (M2_CORE, M1_CORE) for the Arcade Foundry:
# Insert Coin app are the main product, and the app builds them without SDL.
# scripts/setup.py and scripts/recompile.py turn it on for the standalone
# desktop build.
option(M2_APP "Build the windowed app when SDL is there" OFF)
if(M2_APP AND EXISTS ${CMAKE_SOURCE_DIR}/extern/sdl3/CMakeLists.txt)
    set(M2_SDL_TARGET SDL3::SDL3-static)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    if(NOT TARGET ${M2_SDL_TARGET})
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        # SDL builds with its own flags, not ours: MSVC's /fp:strict (our FP
        # rule, for our code) rejects SDL's float constant tables (C2099).
        get_directory_property(M2_COMPILE_OPTIONS COMPILE_OPTIONS)
        set_directory_properties(PROPERTIES COMPILE_OPTIONS "")
        add_subdirectory(${CMAKE_SOURCE_DIR}/extern/sdl3 ${CMAKE_BINARY_DIR}/sdl3 EXCLUDE_FROM_ALL SYSTEM)
        set_directory_properties(PROPERTIES COMPILE_OPTIONS "${M2_COMPILE_OPTIONS}")
    endif()

    # Dear ImGui (launcher UI), with its SDL3 and SDL_GPU backends
    add_executable(test_gpu_startup tests/test_gpu_startup.cpp)
    target_include_directories(test_gpu_startup PRIVATE src ${CMAKE_SOURCE_DIR}/extern/sdl3/include)
    add_test(NAME gpu_startup COMMAND test_gpu_startup)
    set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/extern/imgui)
    add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp)
    target_include_directories(imgui SYSTEM PUBLIC ${IMGUI_DIR})
    target_link_libraries(imgui PUBLIC ${M2_SDL_TARGET})
    if(NOT MSVC)
        target_compile_options(imgui PRIVATE -w)
    endif()
    # The hardware renderer (SDL_GPU), shared by the app and m2gpushot.
    add_library(gpurender STATIC src/app/gpu/gpu_renderer.cpp)
    target_include_directories(gpurender PUBLIC src)
    target_link_libraries(gpurender PUBLIC runtime ${M2_SDL_TARGET})
    add_library(daytona_app OBJECT src/app/main.cpp src/app/launcher.cpp src/app/config.cpp src/app/controls.cpp src/app/ffb.cpp src/app/link_socket.cpp src/app/pacing.cpp)
    target_link_libraries(daytona_app PRIVATE runtime imgui gpurender ${M2_SDL_TARGET})
    if(WIN32)
        target_link_libraries(daytona_app PUBLIC ws2_32) # link play's sockets
    endif()
endif()

# Recompiled game code lives in build/gen/<set>/ (derived from the game: never
# committed). scripts/recompile.sh generates it and re-runs CMake.
set(GEN_DIR ${M2_GEN_ROOT}/${M2_ROMSET})
set(TGP_GEN ${M2_GEN_ROOT}/${M2_ROMSET}_tgp/tgp_gen.cpp)
if(M2_MODEL2B)
    # Model 2B: the recompiled SHARC program takes the TGP's place.
    set(TGP_GEN ${M2_GEN_ROOT}/${M2_ROMSET}_sharc/sharc_gen.cpp)
elseif(M2_MODEL2C)
    # Model 2C: the recompiled TGPx4 program.
    set(TGP_GEN ${M2_GEN_ROOT}/${M2_ROMSET}_tgpx4/tgpx4_gen.cpp)
endif()
set(SND_GEN ${M2_GEN_ROOT}/${M2_ROMSET}_snd/snd_gen.cpp)
if(M2_DSB2)
    # Top Skater: the Digital Sound Board 2's 68000 program, with the sound 68000's.
    set(DSB_GEN ${M2_GEN_ROOT}/${M2_ROMSET}_dsb2/dsb2_gen.cpp)
    if(EXISTS ${DSB_GEN})
        # m2dsb2check holds the board to MAME's (the m2trace plugin's M2TRACE_DSBLOG)
        add_executable(m2dsb2check tools/dsb2check/main.cpp ${DSB_GEN})
        target_link_libraries(m2dsb2check PRIVATE runtime)
    endif()
elseif(M2_M1SND)
    # Manx TT: the Model 1 sound board's 68000 program (a stub without its ROMs), in DSB_GEN's place.
    set(DSB_GEN ${M2_GEN_ROOT}/${M2_ROMSET}_m1snd/m1snd_gen.cpp)
elseif(M2_DSB)
    # Model 2C's Touring Car: the Digital Sound Board's Z80 program, with the sound 68000's.
    set(DSB_GEN ${M2_GEN_ROOT}/${M2_ROMSET}_dsb/dsb_gen.cpp)
    if(EXISTS ${DSB_GEN})
        # m2dsbcheck holds the board to MAME's (the m2trace plugin's M2TRACE_DSBLOG)
        add_executable(m2dsbcheck tools/dsbcheck/main.cpp ${DSB_GEN})
        target_link_libraries(m2dsbcheck PRIVATE model1)
    endif()
endif()
if(EXISTS ${GEN_DIR}/gen_table.cpp AND EXISTS ${TGP_GEN} AND EXISTS ${SND_GEN} AND (NOT DSB_GEN OR EXISTS "${DSB_GEN}"))

    file(GLOB GEN_SOURCES ${GEN_DIR}/*.cpp)
    # The coprocessor and sound code first: each is one large function in one
    # file, the longest compiles of a core, so they start before the i960
    # chunks instead of running alone at the end of the build (#46).
    set(M2_GEN_ALL ${TGP_GEN} ${SND_GEN} ${DSB_GEN} ${GEN_SOURCES})
    add_executable(m2native tools/m2native/main.cpp ${M2_GEN_ALL})
    target_link_libraries(m2native PRIVATE runtime)
    # The game on its own: native board runtime, no trace.
    add_executable(m2run tools/m2run/main.cpp src/app/link_socket.cpp ${M2_GEN_ALL})
    target_link_libraries(m2run PRIVATE runtime)
    if(WIN32)
        target_link_libraries(m2run PRIVATE ws2_32) # link play's sockets
    endif()

    # The game in a window (needs the app objects below).
    if(TARGET daytona_app)
        add_executable(daytona WIN32 $<TARGET_OBJECTS:daytona_app> ${M2_GEN_ALL})
        target_link_libraries(daytona PRIVATE runtime imgui gpurender ${M2_SDL_TARGET})
        if(M2_ROMSET STREQUAL "srallyc")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME segarally) # Sega Rally's build
        elseif(M2_ROMSET STREQUAL "manxttc")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME manxtt) # Manx TT's build
        elseif(M2_ROMSET STREQUAL "vcop2")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME vcop2) # Virtua Cop 2's build
        elseif(M2_ROMSET STREQUAL "vcop")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME vcop) # Virtua Cop's build
        elseif(M2_ROMSET STREQUAL "vf2")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME vf2) # Virtua Fighter 2's build
        elseif(M2_ROMSET STREQUAL "indy500")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME indy500) # INDY 500's build
        elseif(M2_ROMSET STREQUAL "fvipers")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME fvipers) # Fighting Vipers' build
        elseif(M2_ROMSET STREQUAL "sgt24h")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME sgt24h) # Super GT 24h's build
        elseif(M2_ROMSET STREQUAL "overrevb")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME overrev) # Over Rev's build
        elseif(M2_ROMSET STREQUAL "stcc")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME stcc) # Sega Touring Car Championship's build
        elseif(M2_ROMSET STREQUAL "von")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME von) # Virtual-On's build
        elseif(M2_ROMSET STREQUAL "hotdo")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME hotd) # The House of the Dead's build
        elseif(M2_ROMSET STREQUAL "gunblade")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME gunblade) # Gunblade NY's build
        elseif(M2_ROMSET STREQUAL "vstriker")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME vstriker) # Virtua Striker's build
        elseif(M2_ROMSET STREQUAL "zerogun")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME zerogun) # Zero Gunner's build
        elseif(M2_ROMSET STREQUAL "pltkids")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME pltkids) # Pilot Kids' build
        elseif(M2_ROMSET STREQUAL "motoraid")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME motoraid) # Motor Raid's build
        elseif(M2_ROMSET STREQUAL "dynamcop")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME dynamcop) # Dynamite Cop's build
        elseif(M2_ROMSET STREQUAL "skytargt")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME skytargt) # Sky Target's build
        elseif(M2_ROMSET STREQUAL "desert")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME desert) # Desert Tank's build
        elseif(M2_ROMSET STREQUAL "lastbrnx")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME lastbrnx) # Last Bronx's build
        elseif(M2_ROMSET STREQUAL "schamp")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME schamp) # Sonic Championship's build
        elseif(M2_ROMSET STREQUAL "dynabb97")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME dynabb97) # Dynamite Baseball 97's build
        elseif(M2_ROMSET STREQUAL "waverunr")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME waverunr) # Wave Runner's build
        elseif(M2_ROMSET STREQUAL "segawski")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME segawski) # Sega Water Ski's build
        elseif(M2_ROMSET STREQUAL "skisuprg")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME skisuprg) # Sega Ski Super G's build
        elseif(M2_ROMSET STREQUAL "bel")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME bel) # Behind Enemy Lines's build
        elseif(M2_ROMSET STREQUAL "topskatr")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME topskatr) # Top Skater's build
        elseif(M2_ROMSET STREQUAL "rchase2")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME rchase2) # Rail Chase 2's build
        elseif(M2_ROMSET STREQUAL "doab")
            set_target_properties(daytona PROPERTIES OUTPUT_NAME doab) # Dead or Alive's build
        endif()
        if(WIN32)
            target_link_libraries(daytona PRIVATE ws2_32) # link play's sockets
        endif()
        # Headless frames through the hardware renderer, to compare with m2run's.
        add_executable(m2gpushot tools/m2gpushot/main.cpp ${M2_GEN_ALL})
        target_link_libraries(m2gpushot PRIVATE runtime gpurender)
    else()
        message(STATUS "extern/sdl3 not found: skipping the windowed game (run scripts/setup.py)")
    endif()
    # The game core as a library with a C interface (common/include/afcore.h) for
    # other front ends: a shared library (a framework on Apple) that exports
    # only afcore_api_<romset>, so several games share a process.
    if(M2_CORE)
        add_library(m2core SHARED ${M2_GEN_ALL} src/core/m2core.cpp src/runtime/stereo.cpp src/app/link_socket.cpp)
        target_include_directories(m2core PUBLIC ${CMAKE_SOURCE_DIR}/common/include)
        target_compile_definitions(m2core PRIVATE AFCORE_BUILD M2_ROMSET_ID=${M2_ROMSET})
        target_link_libraries(m2core PRIVATE runtime)
        set_target_properties(m2core PROPERTIES OUTPUT_NAME afcore_${M2_ROMSET}
            CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
        if(APPLE)
            set_target_properties(m2core PROPERTIES FRAMEWORK TRUE
                MACOSX_FRAMEWORK_IDENTIFIER org.arcadefoundry.core.${M2_ROMSET}
                INSTALL_NAME_DIR @rpath BUILD_WITH_INSTALL_NAME_DIR ON # embedded in an app
                PUBLIC_HEADER ${CMAKE_SOURCE_DIR}/common/include/afcore.h)
            target_link_options(m2core PRIVATE "LINKER:-exported_symbol,_afcore_api_${M2_ROMSET}")
        elseif(WIN32)
            target_link_libraries(m2core PRIVATE ws2_32)
        else()
            file(WRITE ${CMAKE_BINARY_DIR}/m2core.map "{ global: afcore_api_${M2_ROMSET}; local: *; };\n")
            target_link_options(m2core PRIVATE "LINKER:--version-script=${CMAKE_BINARY_DIR}/m2core.map")
        endif()
        add_executable(m2coreshot tools/m2coreshot/main.c)
        target_compile_definitions(m2coreshot PRIVATE M2_ROMSET_ID=${M2_ROMSET})
        target_link_libraries(m2coreshot PRIVATE m2core)
        if(UNIX AND NOT APPLE)
            target_link_libraries(m2coreshot PRIVATE m) # sinf (--play)
        endif()
    endif()
    if(NOT MSVC)
        set_source_files_properties(${GEN_SOURCES} ${TGP_GEN} ${SND_GEN} ${DSB_GEN} PROPERTIES COMPILE_OPTIONS "-Wno-unused-label")
    endif()
endif()

# The TGP recompiler and, when it has been run, the lockstep check of its
# output against MAME's TGP log.
# The ROM set importer (zip or 7z) for the build.
add_executable(m2import tools/m2import/main.cpp)
target_link_libraries(m2import PRIVATE runtime)

add_executable(m2tgprecomp tools/m2tgprecomp/main.cpp)
target_include_directories(m2tgprecomp PRIVATE src)
# Model 2B: the SHARC recompiler and its check against MAME's SHARC log.
add_executable(m2sharcrecomp tools/m2sharcrecomp/main.cpp)
target_include_directories(m2sharcrecomp PRIVATE src)
# Model 2C: the TGPx4 recompiler and its check against MAME's TGPx4 log.
add_executable(m2tgpx4recomp tools/m2tgpx4recomp/main.cpp)
if(M2_MODEL2B AND EXISTS ${TGP_GEN})
    add_executable(m2sharccheck tools/m2sharccheck/main.cpp ${TGP_GEN})
    target_link_libraries(m2sharccheck PRIVATE runtime)
elseif(M2_MODEL2C AND EXISTS ${TGP_GEN})
    add_executable(m2tgpx4check tools/m2tgpx4check/main.cpp ${TGP_GEN})
    target_link_libraries(m2tgpx4check PRIVATE runtime)
elseif(EXISTS ${TGP_GEN})
    add_executable(m2tgpcheck tools/m2tgpcheck/main.cpp ${TGP_GEN})
    target_link_libraries(m2tgpcheck PRIVATE runtime)
    # The per-instruction hook exists only in this test build.
    target_compile_definitions(m2tgpcheck PRIVATE M2TGP_WITH_HOOK)
    if(NOT MSVC)
        set_source_files_properties(${TGP_GEN} PROPERTIES COMPILE_OPTIONS "-Wno-unused-label")
    endif()
endif()

# The lockstep check of the recompiled sound 68000 against MAME's log
# (M2TRACE_SNDLOG, patches/mame/0003).
if(EXISTS ${SND_GEN})
    add_executable(m2sndcheck tools/m2sndcheck/main.cpp ${SND_GEN})
    target_link_libraries(m2sndcheck PRIVATE runtime)
    if(NOT MSVC)
        set_source_files_properties(${SND_GEN} PROPERTIES COMPILE_OPTIONS "-Wno-unused-label")
    endif()
endif()

add_executable(i960dis tools/i960dis/main.cpp)
target_link_libraries(i960dis PRIVATE i960)

add_executable(tracediff tools/tracediff/main.cpp)
target_link_libraries(tracediff PRIVATE trace)

add_executable(tracedev tools/tracedev/main.cpp)
target_link_libraries(tracedev PRIVATE trace)

add_executable(traceinfo tools/traceinfo/main.cpp)
target_link_libraries(traceinfo PRIVATE trace)

enable_testing()

add_executable(test_scenery tests/test_scenery.cpp src/runtime/enhance.cpp)
target_include_directories(test_scenery PRIVATE src)
add_test(NAME scenery COMMAND test_scenery)

# Geometrizer clipping and display-list streaming (shared runtime code).
add_executable(test_geo_clip tests/test_geo_clip.cpp)
target_include_directories(test_geo_clip PRIVATE src)
add_test(NAME geo_clip COMMAND test_geo_clip)

add_executable(test_geo_stream tests/test_geo_stream.cpp)
target_include_directories(test_geo_stream PRIVATE src)
add_test(NAME geo_stream COMMAND test_geo_stream)

add_executable(test_decode tests/test_decode.cpp)
target_link_libraries(test_decode PRIVATE i960)
add_test(NAME decode COMMAND test_decode)

add_executable(test_m68k tests/test_m68k.cpp)
target_link_libraries(test_m68k PRIVATE m68k runtime)
add_test(NAME m68k COMMAND test_m68k)

add_executable(test_fp tests/test_fp.cpp)
target_link_libraries(test_fp PRIVATE i960)
find_package(Threads REQUIRED)
target_link_libraries(test_fp PRIVATE Threads::Threads)
add_test(NAME fp COMMAND test_fp)

add_executable(test_native_sample_mixer tests/test_native_sample_mixer.cpp src/runtime/native_sample_mixer.cpp)
target_include_directories(test_native_sample_mixer PRIVATE src/runtime)
add_test(NAME native_sample_mixer COMMAND test_native_sample_mixer)

add_executable(test_multipcm_volumes tests/test_multipcm_volumes.cpp src/runtime/multipcm.cpp)
target_include_directories(test_multipcm_volumes PRIVATE src)
add_test(NAME multipcm_volumes COMMAND test_multipcm_volumes)

add_executable(test_app_pacing tests/test_app_pacing.cpp src/app/pacing.cpp)
target_include_directories(test_app_pacing PRIVATE src)
add_test(NAME app_pacing COMMAND test_app_pacing)

add_executable(test_audio_ring tests/test_audio_ring.cpp)
target_include_directories(test_audio_ring PRIVATE src)
add_test(NAME audio_ring COMMAND test_audio_ring)

add_executable(test_test_hold tests/test_test_hold.cpp)
target_include_directories(test_test_hold PRIVATE src)
add_test(NAME test_hold COMMAND test_test_hold)

add_executable(test_native_sound_sequencer tests/test_native_sound_sequencer.cpp src/runtime/native_sound_sequencer.cpp)
target_include_directories(test_native_sound_sequencer PRIVATE src)
add_test(NAME native_sound_sequencer COMMAND test_native_sound_sequencer)

# Link play's communication board, two boards wired in memory (no ROM, no network).
add_executable(test_comm_board tests/test_comm_board.cpp src/runtime/comm_board.cpp)
target_include_directories(test_comm_board PRIVATE src)
add_test(NAME comm_board COMMAND test_comm_board)
add_executable(test_link_slack tests/test_link_slack.cpp src/runtime/comm_board.cpp)
target_include_directories(test_link_slack PRIVATE src)
target_link_libraries(test_link_slack PRIVATE Threads::Threads)
add_test(NAME link_slack COMMAND test_link_slack)

# Wheel and pedal bindings against SDL virtual joysticks.
if(TARGET SDL3::SDL3-static)
    add_executable(test_app_config tests/test_app_config.cpp src/app/config.cpp src/app/controls.cpp)
    target_include_directories(test_app_config PRIVATE src)
    target_link_libraries(test_app_config PRIVATE runtime SDL3::SDL3-static)
    add_test(NAME app_config COMMAND test_app_config)
    add_executable(test_app_controls tests/test_app_controls.cpp src/app/controls.cpp src/app/ffb.cpp)
    target_include_directories(test_app_controls PRIVATE src)
    target_link_libraries(test_app_controls PRIVATE runtime SDL3::SDL3-static)
    add_test(NAME app_controls COMMAND test_app_controls)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/extern/sdl3/include/SDL3/SDL_audio.h)
    add_executable(test_app_native_audio tests/test_app_native_audio.cpp)
    target_include_directories(test_app_native_audio PRIVATE extern/sdl3/include)
    target_link_libraries(test_app_native_audio PRIVATE Threads::Threads)
    add_test(NAME app_native_audio COMMAND test_app_native_audio)
endif()

# Measurement, not a test: where MAME's double FP disagrees with the model.
add_executable(fp_vs_mame tests/fp_vs_mame.cpp)
target_link_libraries(fp_vs_mame PRIVATE i960 Threads::Threads)

add_executable(test_reach tests/test_reach.cpp)
target_link_libraries(test_reach PRIVATE i960)
add_test(NAME reach COMMAND test_reach)

add_executable(test_trace tests/test_trace.cpp)
target_link_libraries(test_trace PRIVATE trace)
add_test(NAME trace COMMAND test_trace ${CMAKE_CURRENT_BINARY_DIR})

# The MAME plugin's Lua core against the C++ trace library (needs lupa).
# Python: the one shared venv (Cores-Common tools/afic-venv: AFIC_VENV,
# default ~/venvs/afic) when there is one and no Python3_EXECUTABLE is given
if(NOT Python3_EXECUTABLE)
  if(DEFINED ENV{AFIC_VENV})
    set(AFIC_VENV "$ENV{AFIC_VENV}")
  else()
    set(AFIC_VENV "$ENV{HOME}/venvs/afic")
  endif()
  if(EXISTS "${AFIC_VENV}/bin/python3")
    set(Python3_EXECUTABLE "${AFIC_VENV}/bin/python3" CACHE FILEPATH "Python (the shared afic venv)")
  endif()
endif()
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME lua_core
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/lua_core_test.py
                ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/lua_core)
    set_tests_properties(lua_core PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME lua_plugin_mock
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/lua_plugin_mock_test.py
                ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/lua_plugin_mock)
    set_tests_properties(lua_plugin_mock PROPERTIES SKIP_RETURN_CODE 77)
    # Second decode reference: Ghidra SLEIGH (third-party i960 module via
    # pypcode). Needs scripts/fetch_ghidra_i960.sh and pypcode (the shared venv).
    if(EXISTS ${CMAKE_SOURCE_DIR}/extern/ghidra_i960/data/languages/i960.slaspec)
        add_test(NAME ghidra_oracle
            COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/ghidra_oracle.py
                    ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/extern/ghidra_i960 --random 200000)
        set_tests_properties(ghidra_oracle PROPERTIES SKIP_RETURN_CODE 77)
    endif()
endif()

# Differential test against MAME's disassembler. Needs scripts/fetch_mame.sh.
set(MAME_DIR ${CMAKE_SOURCE_DIR}/extern/mame CACHE PATH "Pinned MAME checkout")
if(EXISTS ${MAME_DIR}/src/devices/cpu/i960/i960dis.cpp)
    add_executable(mame_oracle
        tests/mame_oracle.cpp
        ${MAME_DIR}/src/devices/cpu/i960/i960dis.cpp
    )
    target_include_directories(mame_oracle PRIVATE
        tests/mame_shim
        ${MAME_DIR}/src/devices
        ${MAME_DIR}/src/devices/cpu/i960
    )
    target_link_libraries(mame_oracle PRIVATE i960)
    find_package(Threads REQUIRED)
    target_link_libraries(mame_oracle PRIVATE Threads::Threads)
    # MAME's source, not ours: don't drown our warnings in its.
    set_source_files_properties(${MAME_DIR}/src/devices/cpu/i960/i960dis.cpp
        PROPERTIES COMPILE_OPTIONS "-w")
    add_test(NAME mame_oracle COMMAND mame_oracle)
else()
    message(STATUS "extern/mame not found: skipping mame_oracle (run scripts/fetch_mame.sh)")
endif()
