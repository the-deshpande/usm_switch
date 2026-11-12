# Nintendo Switch build: the statically recompiled game code + Horizon / libnx runtime and shims -> usm.nro
#   cmake -S switch -B build-switch -G Ninja && cmake --build build-switch
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DEVKITPRO}/cmake/Switch.cmake" CACHE PATH "")
endif()

project(usm_switch C CXX)
include("$ENV{DEVKITPRO}/cmake/Switch.cmake" REQUIRED)

set(SWITCH_APP_TITLE "Ultimate Spider-Man")
set(SWITCH_APP_AUTHOR "boomannotjeff / Switch Port")
set(SWITCH_APP_VERSION "1.0.0")

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)
file(GLOB RECOMP_GEN CONFIGURE_DEPENDS ${SRC}/recomp/gen/recomp_*.c)
if(NOT RECOMP_GEN)
    message(FATAL_ERROR "src/recomp/gen is empty: run recomp.py first")
endif()

file(GLOB SWITCH_SRC CONFIGURE_DEPENDS ${SRC}/switch/*.c)

add_executable(usm_switch.elf
    ${RECOMP_GEN}
    ${SRC}/recomp/fpu_port.c
    ${SWITCH_SRC}
)

target_include_directories(usm_switch.elf PRIVATE
    ${SRC}/recomp
    ${SRC}/recomp/gen
    ${SRC}/switch
)

# Runtime semantics matching the original x86:
# Wrapping arithmetic, unaligned / type-punning pointers, unsigned char
target_compile_definitions(usm_switch.elf PRIVATE RC_RELEASE __SWITCH__)
target_compile_options(usm_switch.elf PRIVATE
    -O2 -std=gnu11 -w
    -fno-strict-aliasing -fwrapv -funsigned-char
    -fno-tree-vectorize -fno-tree-slp-vectorize
)

target_link_libraries(usm_switch.elf PRIVATE
    -lnx
    -lEGL -lglapi -lGL
    -ldrm_nouveau
    -lpthread -lm
)

# Create Homebrew NRO
nx_generate_nacp(usm_switch.nacp
    NAME "${SWITCH_APP_TITLE}"
    AUTHOR "${SWITCH_APP_AUTHOR}"
    VERSION "${SWITCH_APP_VERSION}"
)

nx_create_nro(usm_switch
    TARGET usm_switch.elf
    NACP usm_switch.nacp
)
