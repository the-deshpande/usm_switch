/* usm_switch: Ultimate Spider-Man on Nintendo Switch.
   Loads sdmc:/switch/usm/USM.exe into guest memory as data, binds imports to shims,
   initializes Horizon OS services and starts the translated C entry point. */
#define RC_GUEST_WRITER
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "switch.h"

#define IMAGE_SIZE 0x5ea060u

static uint8_t* g_image;

void fatal(const char* msg) {
    vlog("FATAL: %s\n", msg);
    consoleInit(NULL);
    printf("\n\n  Ultimate Spider-Man (Nintendo Switch)\n\n");
    printf("  %s\n\n", msg);
    printf("  Press + to exit...\n");
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    exit(1);
}

static int pops_of(const char* dll, const char* name) {
    for (unsigned i = 0; i < g_nimports; i++) {
        if (!strcasecmp(g_imports[i].dll, dll) && !strcmp(g_imports[i].name, name))
            return g_imports[i].pops;
    }
    return 0;
}

static uint32_t load_exe(void) {
    vlog("loading " USM_DIR "USM.exe\n");
    FILE* f = fopen(USM_DIR "USM.exe", "rb");
    if (!f) {
        vlog("cannot open " USM_DIR "USM.exe: copy it there from your PC install\n");
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* file = malloc(sz);
    if (!file || fread(file, 1, sz, f) != (size_t)sz) {
        if (file) free(file);
        fclose(f);
        return 0;
    }
    fclose(f);

    uint32_t pe = *(uint32_t*)(file + 0x3c);
    uint32_t base = *(uint32_t*)(file + pe + 24 + 28);
    rc_guest = g_image - base;
    uint8_t* image = g_image;
    memset(image, 0, IMAGE_SIZE);

    uint16_t nsec = *(uint16_t*)(file + pe + 6);
    uint16_t optsz = *(uint16_t*)(file + pe + 20);
    uint8_t* sec = file + pe + 24 + optsz;
    for (int i = 0; i < nsec; i++, sec += 40) {
        uint32_t vsz = *(uint32_t*)(sec + 8);
        uint32_t va = *(uint32_t*)(sec + 12);
        uint32_t raw = *(uint32_t*)(sec + 16);
        uint32_t off = *(uint32_t*)(sec + 20);
        memcpy(image + va, file + off, raw < vsz ? raw : vsz);
    }
    vlog("PE sections loaded, binding imports\n");

    unsigned bound = 0;
    uint32_t imp = *(uint32_t*)(file + pe + 24 + 104);
    for (uint8_t* d = image + imp; *(uint32_t*)(d + 12); d += 20) {
        const char* dll = (const char*)image + *(uint32_t*)(d + 12);
        uint32_t* names = (uint32_t*)(image + (*(uint32_t*)d ? *(uint32_t*)d : *(uint32_t*)(d + 16)));
        uint32_t* slots = (uint32_t*)(image + *(uint32_t*)(d + 16));
        for (; *names; names++, slots++, bound++) {
            char ord[16];
            const char* name = (const char*)image + *names + 2;
            if (*names & 0x80000000u) {
                snprintf(ord, sizeof ord, "#%u", *names & 0xffff);
                name = ord;
            }
            uint32_t var = crt_data_import(name);
            *slots = var ? var : shim_addr(dll, name, pops_of(dll, name));
        }
    }

    uint32_t entry = base + *(uint32_t*)(file + pe + 40);
    free(file);

    /* Gamepad default button table fix */
    static const struct { uint32_t row, code; } pad[] = {
        {0x88d888, 0x1a}, {0x88d8d0, 0x19}, {0x88d918, 0x0c}, {0x88d960, 0x0b},
        {0x88d9a8, 0x06}, {0x88d9f0, 0x05}, {0x88da38, 0x1d}
    };
    for (unsigned i = 0; i < sizeof pad / sizeof *pad; i++) {
        *(uint32_t*)(image + pad[i].row - base + 0x40) = (3u << 16) | pad[i].code;
    }

    vlog("USM.exe loaded at %p (guest %06x), %u imports bound\n", image, base, bound);
    return entry;
}

PadState pad;

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    romfsInit();
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    g_image = memalign(0x10000, IMAGE_SIZE);
    vlog("usm_switch started (image at %p)\n", g_image);
    if (!g_image) {
        fatal("Out of memory for guest image allocation.");
        return 1;
    }

    uint32_t entry = load_exe();
    if (!entry) {
        fatal("Cannot load " USM_DIR "USM.exe\nMake sure USM.exe and the Data folder are inside sdmc:/switch/usm/");
    }

    Ctx* c = ctx_new(0x100000);
    vlog("starting translated entry point at %06x\n", entry);
    guest_call(entry, 0, NULL);
    vlog("game loop exited cleanly\n");

    romfsExit();
    return 0;
}
