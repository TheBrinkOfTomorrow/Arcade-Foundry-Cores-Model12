# Sega Model 1 (Virtua Racing, then Star Wars Arcade, Virtua Fighter and Wing War):
# the V60, the board's memory map and the trace harness. Included from
# CMakeLists.txt; the Model 2 targets are untouched.

# v60: MAME's V60 executor (transplanted, BSD-3, src/v60/mame) as v60::Core,
# with the reference interpreter. The interpreter is the trace harness's
# oracle and the game's fallback for code the recompiler did not reach
# (Board::FallbackEnv; M2_STRICT=1 turns it off): m1run and m1core link it.
add_library(v60_core INTERFACE)
target_include_directories(v60_core INTERFACE src)
add_library(v60_interp STATIC src/v60/interp.cpp)
target_link_libraries(v60_interp PUBLIC v60_core)

add_library(model1 STATIC
    src/model1/rom_set.cpp
    src/model1/memory.cpp
    src/model1/trace.cpp
    src/model1/replay_bus.cpp
    src/model1/tgp.cpp
    src/model1/video.cpp
    src/model1/io_board.cpp
    src/model1/io_board2.cpp
    src/model1/comm_board.cpp
)
target_link_libraries(model1 PUBLIC v60_core runtime dsb)
# Model 1's exact speed-ups, on every platform: only the changed tiles
# redrawn, the HUD overlay in one pass (the same pixels). OFF builds the
# reference.
option(M1_FAST_PATHS "Model 1 exact video speed-ups" ON)
if(M1_FAST_PATHS)
    target_compile_definitions(model1 PUBLIC M1_FAST_PATHS=1)
endif()

if(NOT MSVC)
    # Upstream MAME code: keep its own warnings out of ours.
    set_source_files_properties(src/v60/interp.cpp PROPERTIES COMPILE_OPTIONS
        "-Wno-unused-parameter;-Wno-sign-compare;-Wno-unused-but-set-variable;-Wno-unused-variable;-Wno-implicit-fallthrough;-Wno-unused-function")
endif()

add_executable(m1replay tools/m1replay/main.cpp)
target_link_libraries(m1replay PRIVATE model1 v60_interp)

# The recompiler (runs MAME's handlers at recompile time, src/v60/mame).
add_executable(m1recomp tools/m1recomp/main.cpp tools/m1recomp/sym.cpp)
target_link_libraries(m1recomp PRIVATE model1)
if(NOT MSVC)
    set_source_files_properties(tools/m1recomp/sym.cpp tools/m1recomp/main.cpp PROPERTIES COMPILE_OPTIONS
        "-Wno-unused-parameter;-Wno-sign-compare;-Wno-unused-but-set-variable;-Wno-unused-variable;-Wno-implicit-fallthrough;-Wno-unused-function")
endif()

# The recompiled game code for M1_ROMSET (vr), generated under the build
# directory by m1recomp into gen/<set>_v60/ (never committed): m1native, the lockstep harness.
set(M1_ROMSET vr CACHE STRING "Model 1 ROM set to recompile")
# Re-run cmake after m1recomp: the chunk files are globbed.
file(GLOB M1_GEN ${M2_GEN_ROOT}/${M1_ROMSET}_v60/*.cpp)
# The TGP program (m2tgprecomp --model1 on m1rom's tgp_copro): the same for
# every Model 1 game with the 315-5573 ROM.
set(M1_TGP_GEN ${M2_GEN_ROOT}/m1_tgp.cpp)
if(EXISTS ${M1_TGP_GEN})
    list(APPEND M1_GEN ${M1_TGP_GEN})
    set(M1_NATIVE_TGP ON)
endif()
set(M1_SND_GEN ${M2_GEN_ROOT}/${M1_ROMSET}_snd/snd_gen.cpp)
# Star Wars Arcade's Digital Sound Board: its Z80 program (dsbrecomp on
# m1rom's dsb_cpu). Sets without the board link a stand-in that is never run.
set(M1_DSB_GEN ${M2_GEN_ROOT}/${M1_ROMSET}_dsb/dsb_gen.cpp)
if(NOT EXISTS ${M1_DSB_GEN})
    set(M1_DSB_GEN ${CMAKE_SOURCE_DIR}/src/model1/dsb_none.cpp)
endif()
# Wing War's I/O board (model1io2): its Z80 BIOS (dsbrecomp --io2 on m1rom's
# io2_bios). Sets with the first board link a stand-in that is never run.
set(M1_IO2_GEN ${M2_GEN_ROOT}/${M1_ROMSET}_io/io_gen.cpp)
if(NOT EXISTS ${M1_IO2_GEN})
    set(M1_IO2_GEN ${CMAKE_SOURCE_DIR}/src/model1/io2_none.cpp)
endif()
if(M1_GEN)
    list(APPEND M1_GEN ${M1_DSB_GEN} ${M1_IO2_GEN})
    if(EXISTS ${M1_SND_GEN})
        list(APPEND M1_GEN ${M1_SND_GEN})
        set(M1_NATIVE_SOUND ON)
    endif()
    add_executable(m1native tools/m1native/main.cpp ${M1_GEN})
    # The interpreter is linked only for --check (the game itself never links it)
    target_link_libraries(m1native PRIVATE model1 v60_interp)
    if(M1_NATIVE_TGP)
        target_compile_definitions(m1native PRIVATE M1_NATIVE_TGP=1)
    endif()
    if(M1_NATIVE_SOUND)
        target_compile_definitions(m1native PRIVATE M1_NATIVE_SOUND=1)
    endif()
    if(NOT MSVC)
        set_source_files_properties(${M1_GEN} PROPERTIES COMPILE_OPTIONS
            "-Wno-unused-parameter;-Wno-sign-compare;-Wno-unused-but-set-variable;-Wno-unused-variable;-Wno-implicit-fallthrough;-Wno-unused-function;-Wno-unused-label")
    endif()
endif()

add_executable(dsbrecomp tools/dsbrecomp/main.cpp)

add_executable(m1traceinfo tools/m1traceinfo/main.cpp)
target_link_libraries(m1traceinfo PRIVATE model1)

add_executable(m1rom tools/m1rom/main.cpp)
target_link_libraries(m1rom PRIVATE model1)

# The sound board's 68000 program (m2sndrecomp on m1rom's m1audio, with
# seeds/<set>_snd.txt) into gen/<set>_snd/snd_gen.cpp: m1sndcheck holds it to
# MAME's sound CPU log (patch 0003's M2TRACE_SNDLOG, which the Model 1
# board's 68000 also writes); m1native --sound plays it.
if(NOT M1_DSB_GEN MATCHES "dsb_none")
    # dsbcheck holds the Digital Sound Board to MAME's (scripts/dsb_tap.lua)
    add_executable(dsbcheck tools/dsbcheck/main.cpp ${M1_DSB_GEN})
    target_link_libraries(dsbcheck PRIVATE model1)
endif()
if(NOT M1_IO2_GEN MATCHES "io2_none")
    # m1io2check holds the board to MAME's (its dual-port RAM's log, M1IO_LOG)
    add_executable(m1io2check tools/m1io2check/main.cpp ${M1_IO2_GEN})
    target_link_libraries(m1io2check PRIVATE model1)
endif()
if(EXISTS ${M1_SND_GEN})
    add_executable(m1sndcheck tools/m2sndcheck/main.cpp ${M1_SND_GEN})
    target_link_libraries(m1sndcheck PRIVATE runtime)
endif()

# M1_CORE: the game core library (common/include/afcore.h, src/core/m1core.cpp) for
# M1_ROMSET from the generated code: afcore_<set> (a framework on Apple),
# exporting only afcore_api_<set>, like the Model 2 games' m2core.
if(M1_GEN AND M1_NATIVE_TGP AND M1_NATIVE_SOUND)
    # The board free-running, headless (frames, audio, MAME's frame hashes)
    add_executable(m1run tools/m1run/main.cpp src/model1/board.cpp src/app/link_socket.cpp ${M1_GEN})
    target_link_libraries(m1run PRIVATE model1 v60_interp)
endif()

# M1_TITLES_<set>: cmake/games.cmake (games.toml)
# Stereo: metres per game unit, measured from a car's width and a plane's
# wingspan (the others default to 1).
set(M1_STEREO_SCALE_vr 0.8f)
set(M1_STEREO_SCALE_wingwar 0.9f)
if(M1_CORE)
    if(NOT M1_GEN OR NOT M1_NATIVE_TGP OR NOT M1_NATIVE_SOUND)
        message(FATAL_ERROR "M1_CORE: generate the code first (scripts/recompile.py --set ${M1_ROMSET} --core)")
    endif()
    add_library(m1core SHARED src/core/m1core.cpp src/model1/board.cpp src/model1/stereo.cpp src/app/link_socket.cpp ${M1_GEN})
    target_include_directories(m1core PRIVATE ${CMAKE_SOURCE_DIR}/common/include)
    target_compile_definitions(m1core PRIVATE AFCORE_BUILD M1_SET_NAME="${M1_ROMSET}" M2_ROMSET_ID=${M1_ROMSET}
        M1_GAME_TITLE="${M1_TITLES_${M1_ROMSET}}")
    target_link_libraries(m1core PRIVATE model1 v60_interp)
    if(DEFINED M1_STEREO_SCALE_${M1_ROMSET})
        target_compile_definitions(m1core PRIVATE M1_STEREO_SCALE=${M1_STEREO_SCALE_${M1_ROMSET}})
    endif()
    set_target_properties(m1core PROPERTIES OUTPUT_NAME afcore_${M1_ROMSET}
        CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
    if(APPLE)
        set_target_properties(m1core PROPERTIES FRAMEWORK TRUE
            MACOSX_FRAMEWORK_IDENTIFIER org.arcadefoundry.core.${M1_ROMSET}
            INSTALL_NAME_DIR @rpath BUILD_WITH_INSTALL_NAME_DIR ON # embedded in an app
            PUBLIC_HEADER ${CMAKE_SOURCE_DIR}/common/include/afcore.h)
        target_link_options(m1core PRIVATE "LINKER:-exported_symbol,_afcore_api_${M1_ROMSET}")
    elseif(NOT WIN32)
        file(WRITE ${CMAKE_BINARY_DIR}/m1core.map "{ global: afcore_api_${M1_ROMSET}; local: *; };\n")
        target_link_options(m1core PRIVATE "LINKER:--version-script=${CMAKE_BINARY_DIR}/m1core.map")
    endif()
    # A headless run of the core through its C interface (frames, audio)
    add_executable(m1coreshot tools/m2coreshot/main.c)
    target_compile_definitions(m1coreshot PRIVATE M2_ROMSET_ID=${M1_ROMSET})
    target_include_directories(m1coreshot PRIVATE ${CMAKE_SOURCE_DIR}/common/include)
    target_link_libraries(m1coreshot PRIVATE m1core)
endif()
