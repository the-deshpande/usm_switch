# Nintendo Switch build: the statically recompiled game code + Horizon / libnx runtime and shims -> usm.nro
#   cmake -S switch -B build-switch -G Ninja && cmake --build build-switch
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DEVKITPRO}/cmake/Switch.cmake" CACHE PATH "")
endif()

project(usm_switch C CXX)
include("$ENV{DEVKITPRO}/cmake/Switch.cmake" REQUIRED)

set(SWITCH_APP_TITLE "Ultimate Spider-Man")
set(SWITCH_APP_AUTHOR "the_deshpande")
set(SWITCH_APP_VERSION "1.0.0")

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)
file(GLOB RECOMP_GEN CONFIGURE_DEPENDS ${SRC}/recomp/gen/recomp_*.c)
if(NOT RECOMP_GEN)
    message(FATAL_ERROR "src/recomp/gen is empty: run recomp.py first")
endif()

# FFmpeg Bink decoders
include(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/ffmpeg/ffmpeg.cmake)

file(GLOB SWITCH_SRC CONFIGURE_DEPENDS ${SRC}/switch/*.c)

add_executable(usm_switch.elf
    ${RECOMP_GEN}
    ${SRC}/recomp/fpu_port.c
    ${SWITCH_SRC}
)

# Build stamp for the overlay's System > Build row, regenerated every build.
set(USM_BUILD_INFO ${CMAKE_CURRENT_BINARY_DIR}/usm_build_info.h)
add_custom_target(usm_build_info ALL
    COMMAND ${CMAKE_COMMAND} -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/.. -DOUT=${USM_BUILD_INFO}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/build_info.cmake
    BYPRODUCTS ${USM_BUILD_INFO}
    COMMENT "Stamping build commit"
)
add_dependencies(usm_switch.elf usm_build_info)

target_include_directories(usm_switch.elf PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SRC}/recomp
    ${SRC}/recomp/gen
    ${SRC}/switch
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/ffmpeg
    /opt/devkitpro/portlibs/switch/include
)

target_link_directories(usm_switch.elf PRIVATE
    /opt/devkitpro/portlibs/switch/lib
)

# Runtime semantics matching the original x86:
# Wrapping arithmetic, unaligned / type-punning pointers, unsigned char
target_compile_definitions(usm_switch.elf PRIVATE RC_RELEASE __SWITCH__ USM_OPENGL)
target_compile_options(usm_switch.elf PRIVATE
    -O1 -std=gnu11 -w
    -fno-strict-aliasing -fwrapv -funsigned-char
    -fno-tree-vectorize -fno-tree-slp-vectorize
)

target_link_libraries(usm_switch.elf PRIVATE
    ffbink
    -lnx
    -lEGL -lglapi -lglad
    -ldrm_nouveau
    -lstdc++ -lpthread -lm
)

# Linker specs: a copy of devkitPro's libnx switch.specs (see
# usm_switch.specs.in, generated at configure time so the script path is
# absolute) that uses the repo's linker script (usm_switch.ld) instead of
# the platform's switch.ld, for the TEXTREL reason documented there.
# The generated specs file REPLACES the platform one on the link line:
# the toolchain puts -specs=<platform>/switch.specs in
# CMAKE_EXE_LINKER_FLAGS, and that string is swapped for the generated
# file below, so exactly one -specs (and therefore exactly one -T) is in
# effect. Two -T options do NOT work -- the platform specs already pass
# -T switch.ld, and with both present ld breaks the link (it reports
# .tls.align overlapping .text), because the specs' *link: lines
# concatenate (they start with '+'). *startfile: is copied verbatim, so
# startup is unchanged.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/usm_switch.specs.in
               ${CMAKE_CURRENT_BINARY_DIR}/usm_switch.specs @ONLY)
string(REGEX REPLACE "-specs=[^ ]*switch\\.specs"
       "-specs=${CMAKE_CURRENT_BINARY_DIR}/usm_switch.specs"
       CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
if(NOT "${CMAKE_EXE_LINKER_FLAGS}" MATCHES "usm_switch\\.specs")
    message(WARNING "usm_switch: could not swap in usm_switch.specs; the link will use the platform switch.ld and will fail on TEXTREL")
endif()

# Create Homebrew NRO
nx_generate_nacp(usm_switch.nacp
    NAME "${SWITCH_APP_TITLE}"
    AUTHOR "${SWITCH_APP_AUTHOR}"
    VERSION "${SWITCH_APP_VERSION}"
)

nx_create_nro(usm_switch
    TARGET usm_switch.elf
    NACP usm_switch.nacp
    ICON ${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg
)

add_custom_target(usm_switch_nso ALL
    COMMAND elf2nso $<TARGET_FILE:usm_switch.elf> main
    COMMAND cp usm_switch.nacp control.nacp
    DEPENDS usm_switch.elf usm_switch.nacp
)
