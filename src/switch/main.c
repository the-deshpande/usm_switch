/* usm_switch: Ultimate Spider-Man on Nintendo Switch.
   Loads sdmc:/switch/usm/USM.exe into guest memory as data, binds imports to shims,
   initializes Horizon OS services and starts the translated C entry point. */
#define RC_GUEST_WRITER
#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <switch.h>

#include "usm_switch.h"
#include "options_menu.h"

#define IMAGE_SIZE 0x5ea060u

static uint8_t* g_image;
PadState pad;
int main(int argc, char* argv[]);

/* libnx default exception handling */

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
    vlog("loading USM.exe\n");
    FILE* f = fopen("romfs:/USM.exe", "rb");
    if (f) {
        vlog("found USM.exe in romfs:/\n");
    } else {
        f = fopen(USM_DIR "USM.exe", "rb");
        if (f) vlog("found USM.exe in " USM_DIR "\n");
    }
    if (!f) {
        /* NRO install (README Method B): USM.exe sits next to the NRO in
           sdmc:/switch/usm/. The NSP build loads from romfs:/ or save:/
           above; as an NRO, save:/ mounts the NSP title's save data, which
           never holds USM.exe, so fall back to the SD folder. */
        f = fopen("sdmc:/switch/usm/USM.exe", "rb");
        if (f) vlog("found USM.exe in sdmc:/switch/usm/\n");
    }
    if (!f) {
        vlog("cannot open USM.exe in romfs:/, " USM_DIR " or sdmc:/switch/usm/\n");
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

    /* Gamepad default button table fix. Rows are 0x48 bytes, pad slot at +0x40 (device << 16 | code).
       The 7th row (0x88da38) is CameraCenter: on Vita it was 0x1d (rear touch), but on Switch
       Center Camera is Button 10 / 0x1e (Right Stick click) and 0x1d is BackButton / Map
       (shims_misc.c). This table was copied from Vita with the stale 0x1d; Revert to Default on
       the Controls page (guest 0x587900, saved by 0x587c50) copies this table over the live
       bindings, so the stale value rebound Center Camera to Left Stick click until the next
       boot's registry upgrade fixed it again. */
    static const struct { uint32_t row, code; } pad[] = {
        {0x88d888, 0x1a}, {0x88d8d0, 0x19}, {0x88d918, 0x0c}, {0x88d960, 0x0b},
        {0x88d9a8, 0x06}, {0x88d9f0, 0x05}, {0x88da38, 0x1e}
    };
    for (unsigned i = 0; i < sizeof pad / sizeof *pad; i++) {
        *(uint32_t*)(image + pad[i].row - base + 0x40) = (3u << 16) | pad[i].code;
    }

    vlog("USM.exe loaded at %p (guest %06x), %u imports bound\n", image, base, bound);
    return entry;
}

typedef struct {
    uint32_t entry;
    int result;
} GameThreadArgs;

static void* game_thread(void* raw) {
    GameThreadArgs* args = (GameThreadArgs*)raw;
    vlog("game_thread started with 16MB host stack; allocating 4MB guest stack\n");
    Ctx* c = ctx_new(4 * 1024 * 1024);
    vlog("starting translated entry point at %06x\n", args->entry);
    guest_call(args->entry, 0, NULL);
    vlog("game loop exited cleanly\n");
    args->result = 0;
    return NULL;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    romfsInit();
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    /* Mount user save data in emuNAND / NAND (save:/) */
    AccountUid uid = {0};
    if (R_SUCCEEDED(accountInitialize(AccountServiceType_Application))) {
        if (R_FAILED(accountGetPreselectedUser(&uid))) {
            accountGetLastOpenedUser(&uid);
        }
        accountExit();
    }
    u64 app_id = 0x01c483a5b2b53000ULL;
    Result rc = fsdevMountSaveData("save", app_id, uid);
    if (R_FAILED(rc)) {
        rc = fsdevMountDeviceSaveData("save", app_id);
    }
    if (R_FAILED(rc)) {
        FsSaveDataAttribute attr;
        FsSaveDataCreationInfo info;
        FsSaveDataMetaInfo meta;
        memset(&attr, 0, sizeof(attr));
        memset(&info, 0, sizeof(info));
        memset(&meta, 0, sizeof(meta));

        attr.application_id = app_id;
        attr.uid = uid;
        attr.save_data_type = FsSaveDataType_Account;
        attr.save_data_rank = FsSaveDataRank_Primary;

        info.save_data_size = 16 * 1024 * 1024;
        info.journal_size = 16 * 1024 * 1024;
        info.owner_id = app_id;
        info.save_data_space_id = FsSaveDataSpaceId_User;

        fsCreateSaveDataFileSystem(&attr, &info, &meta);
        rc = fsdevMountSaveData("save", app_id, uid);
    }

    if (R_SUCCEEDED(rc)) {
        mkdir("save:/Activision", 0777);
        mkdir("save:/Activision/Ultimate Spider-Man", 0777);
        mkdir("save:/Activision/Ultimate Spider-Man/Save", 0777);
        mkdir("save:/Activision/Ultimate Spider-Man/Screenshot", 0777);
        mkdir("save:/shadercache", 0777);
    } else {
        /* Running as Homebrew NRO without save partition: use sdmc save directory */
        mkdir("sdmc:/switch/usm", 0777);
        mkdir("sdmc:/switch/usm/save", 0777);
        mkdir("sdmc:/switch/usm/save/Activision", 0777);
        mkdir("sdmc:/switch/usm/save/Activision/Ultimate Spider-Man", 0777);
        mkdir("sdmc:/switch/usm/save/Activision/Ultimate Spider-Man/Save", 0777);
        mkdir("sdmc:/switch/usm/save/Activision/Ultimate Spider-Man/Screenshot", 0777);
        mkdir("sdmc:/switch/usm/save/shadercache", 0777);
    }

    g_image = memalign(0x10000, IMAGE_SIZE);
    vlog("usm_switch memory allocated (image at %p, main at %p)\n", g_image, (void*)&main);
    if (!g_image) {
        fatal("Out of memory for guest image allocation.");
        return 1;
    }

    uint32_t entry = load_exe();
    if (!entry) {
        fatal("Cannot load USM.exe from romfs:/, " USM_DIR "or sdmc:/switch/usm/USM.exe");
    }

    GameThreadArgs args = { entry, 0 };
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 16 * 1024 * 1024);
    pthread_t thread;
    int err = pthread_create(&thread, &attr, game_thread, &args);
    pthread_attr_destroy(&attr);

    if (err != 0) {
        vlog("pthread_create failed (%d), falling back to main thread\n", err);
        Ctx* c = ctx_new(4 * 1024 * 1024);
        guest_call(entry, 0, NULL);
    } else {
        pthread_join(thread, NULL);
    }

    vlog("=== usm_switch ended cleanly ===\n");
    void reg_flush(void);
    reg_flush();
    fsdevCommitDevice("save");
    fsdevUnmountDevice("save");
    options_menu_cleanup();
    romfsExit();
    svcExitProcess();
    return 0;
}
