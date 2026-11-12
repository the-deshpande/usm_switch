/* DirectInput 8 on Nintendo Switch (libnx).
   Emulates standard gamepad, mouse, and keyboard inputs for Ultimate Spider-Man. */
#include <switch.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "com.h"
#include "options_menu.h"
#include "usm_switch.h"

extern PadState pad;

/* Scan codes */
enum { K_ESC = 1, K_M = 50, K_UP = 200, K_LEFT = 203, K_RIGHT = 205, K_DOWN = 208 };
enum { DEV_KEYBOARD = 1, DEV_MOUSE = 2, DEV_PAD = 3 };
#define PAD_GUID 0x53574954u  /* "SWIT": Switch gamepad instance GUID */

static uint8_t g_keys[256], g_prev[256], g_mouse[2], g_mprev[2];
static uint32_t g_seq = 0;
static int32_t g_min[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static int32_t g_max[8] = {65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535};

static u64 g_buttons = 0;
static HidAnalogStickState g_lstick, g_rstick;

static void poll(void) {
    padUpdate(&pad);
    u64 kDown = padGetButtonsDown(&pad);
    g_buttons = padGetButtons(&pad);
    g_lstick = padGetStickPos(&pad, 0);
    g_rstick = padGetStickPos(&pad, 1);

    if (kDown & HidNpadButton_Minus) {
        options_menu_toggle();
    }

    if (options_menu_is_open()) {
        options_menu_update(kDown, g_buttons, g_lstick.x, g_lstick.y);
        g_buttons = 0;
        g_lstick.x = g_lstick.y = 0;
        g_rstick.x = g_rstick.y = 0;
        memset(g_keys, 0, sizeof g_keys);
        memset(g_mouse, 0, sizeof g_mouse);
        return;
    }

    memset(g_keys, 0, sizeof g_keys);
#define KEY(k, on) do { if (on) g_keys[k] = 0x80; } while (0)
    KEY(K_UP, g_buttons & (HidNpadButton_Up | HidNpadButton_StickLUp));
    KEY(K_DOWN, g_buttons & (HidNpadButton_Down | HidNpadButton_StickLDown));
    KEY(K_LEFT, g_buttons & (HidNpadButton_Left | HidNpadButton_StickLLeft));
    KEY(K_RIGHT, g_buttons & (HidNpadButton_Right | HidNpadButton_StickLRight));
    KEY(K_ESC, g_buttons & HidNpadButton_Plus);   /* Pause */
    KEY(K_M, g_buttons & HidNpadButton_Minus);    /* Map / Menu */
#undef KEY

    g_mouse[0] = (g_buttons & (HidNpadButton_L | HidNpadButton_ZL)) ? 0x80 : 0;
    g_mouse[1] = (g_buttons & (HidNpadButton_R | HidNpadButton_ZR)) ? 0x80 : 0;
}

static int dev_kind(Ctx* c) { return (int)LD32(ARG(0) + 8); }

static int32_t axis(int v, int i) {
    /* v: -32768..32767 -> range 0..65535 */
    if (v > -3500 && v < 3500) v = 0;
    double t = (v + 32768.0) / 65535.0;
    return g_min[i] + (int32_t)((g_max[i] - g_min[i]) * t + 0.5);
}

static void pad_state(uint32_t out, uint32_t cb) {
    poll();
    int32_t a[6] = {
        axis(g_lstick.x, 0),
        axis(-g_lstick.y, 1),
        axis(g_rstick.x, 2),
        axis(0, 3),
        axis(0, 4),
        axis(-g_rstick.y, 5)
    };
    for (int i = 0; i < 6 && 4u * i + 4 <= cb; i++) ST32(out + 4 * i, (uint32_t)a[i]);

    int up = (g_buttons & HidNpadButton_Up) != 0;
    int dn = (g_buttons & HidNpadButton_Down) != 0;
    int lf = (g_buttons & HidNpadButton_Left) != 0;
    int rt = (g_buttons & HidNpadButton_Right) != 0;
    uint32_t pov = up ? (rt ? 4500 : lf ? 31500 : 0) :
                   dn ? (rt ? 13500 : lf ? 22500 : 18000) :
                   rt ? 9000 : lf ? 27000 : 0xffffffffu;
    if (cb >= 48) {
        ST32(out + 32, pov);
        ST32(out + 36, 0xffffffffu);
        ST32(out + 40, 0xffffffffu);
        ST32(out + 44, 0xffffffffu);
    }

    /* Button mapping:
       0: Jump (B on Switch / Cross on PS)
       1: StickToWalls (A on Switch / Circle on PS)
       2: Punch (Y on Switch / Square on PS)
       3: Kick (X on Switch / Triangle on PS)
       4: Web Zip (L / ZL)
       5: Web Swing (R / ZR)
       6: Select / Map (Minus)
       7: Start / Pause (Plus)
       8: Camera Center (StickR click)
    */
    static const u64 map[] = {
        HidNpadButton_B,
        HidNpadButton_A,
        HidNpadButton_Y,
        HidNpadButton_X,
        HidNpadButton_L | HidNpadButton_ZL,
        HidNpadButton_R | HidNpadButton_ZR,
        HidNpadButton_Minus,
        HidNpadButton_Plus,
        HidNpadButton_StickR
    };
    for (unsigned i = 0; i < sizeof map / sizeof *map && 48 + i < cb; i++) {
        ST8(out + 48 + i, (g_buttons & map[i]) ? 0x80 : 0);
    }
}

static void dev_GetDeviceState(Ctx* c) {
    uint32_t cb = ARG(1), out = ARG(2);
    memset(H(out), 0, cb);
    if (dev_kind(c) == DEV_PAD) pad_state(out, cb);
    else if (dev_kind(c) == DEV_KEYBOARD) poll(), memcpy(H(out), g_keys, cb < 256 ? cb : 256);
    else if (dev_kind(c) == DEV_MOUSE && cb >= 14) poll(), ST8(out + 12, g_mouse[0]), ST8(out + 13, g_mouse[1]);
    c->eax = 0;
}

static void dev_GetDeviceData(Ctx* c) {
    uint32_t cb = ARG(1), out = ARG(2), pdw = ARG(3), flags = ARG(4);
    uint32_t nmax = pdw ? LD32(pdw) : 0, n = 0;
    if (dev_kind(c) == DEV_KEYBOARD) {
        poll();
        for (int k = 0; k < 256 && n < nmax; k++) {
            if (g_keys[k] != g_prev[k]) {
                if (out) {
                    uint32_t p = out + n * cb;
                    ST32(p, (uint32_t)k);
                    ST32(p + 4, g_keys[k]);
                    ST32(p + 8, (uint32_t)(armGetSystemTick() / 19200u));
                    ST32(p + 12, ++g_seq);
                }
                n++;
            }
        }
        if (!(flags & 1)) memcpy(g_prev, g_keys, sizeof g_prev);
    } else if (dev_kind(c) == DEV_MOUSE) {
        poll();
        for (int b = 0; b < 2 && n < nmax; b++) {
            if (g_mouse[b] != g_mprev[b]) {
                if (out) {
                    uint32_t p = out + n * cb;
                    ST32(p, 12u + b);
                    ST32(p + 4, g_mouse[b]);
                    ST32(p + 8, (uint32_t)(armGetSystemTick() / 19200u));
                    ST32(p + 12, ++g_seq);
                }
                n++;
            }
        }
        if (!(flags & 1)) memcpy(g_mprev, g_mouse, sizeof g_mprev);
    }
    if (pdw) ST32(pdw, n);
    c->eax = 0;
}

static void dev_SetProperty(Ctx* c) {
    uint32_t prop = ARG(1), p = ARG(2);
    if (prop == 4 && p) {
        uint32_t obj = LD32(p + 8), how = LD32(p + 12), i = how == 1 ? obj / 4 : how == 2 ? (obj >> 8) & 0xff : 0;
        if (i < 8) {
            g_min[i] = (int32_t)LD32(p + 16);
            g_max[i] = (int32_t)LD32(p + 20);
        }
    }
    c->eax = 0;
}

static void dev_EnumObjects(Ctx* c) {
    uint32_t cb = ARG(1), ref = ARG(2);
    static uint8_t* d;
    if (!d) d = guest_alloc(64);
    for (int i = 0; i < 6; i++) {
        memset(d, 0, 44);
        *(uint32_t*)d = 44;
        *(uint32_t*)(d + 16) = 0x01 | 0x02;
        *(uint32_t*)(d + 20) = i;
        guest_call(cb, 2, (uint32_t[]){G(d), ref});
    }
    c->eax = 0;
}

static void dev_GetCapabilities(Ctx* c) {
    uint32_t p = ARG(1);
    ST32(p + 8, 8);
    ST32(p + 12, 12);
    ST32(p + 16, 1);
    c->eax = 0;
}

static uint32_t pad_instance(void);

static void dev_GetDeviceInfo(Ctx* c) {
    uint32_t out = ARG(1), size = LD32(out) ? LD32(out) : 0x244;
    if (dev_kind(c) == DEV_PAD) {
        memcpy(H(out), H(pad_instance()), size < 0x244 ? size : 0x244);
        ST32(out, size);
    } else {
        memset(H(out) + 4, 0, (size < 0x244 ? size : 0x244) - 4);
        ST32(out + 4, dev_kind(c) == DEV_KEYBOARD ? 0x6f1d2b61u : 0x6f1d2b60u);
        ST32(out + 36, dev_kind(c) == DEV_KEYBOARD ? 0x13 : 0x12);
        strcpy((char*)H(out + 40), dev_kind(c) == DEV_KEYBOARD ? "Keyboard" : "Mouse");
    }
    c->eax = 0;
}

static void dev_GetProperty(Ctx* c) {
    uint32_t prop = ARG(1), p = ARG(2);
    if (prop == 4) {
        uint32_t obj = LD32(p + 8), how = LD32(p + 12), i = how == 1 ? obj / 4 : how == 2 ? (obj >> 8) & 0xff : 0;
        ST32(p + 16, (uint32_t)g_min[i & 7]);
        ST32(p + 20, (uint32_t)g_max[i & 7]);
    } else if (LD32(p) >= 20) {
        ST32(p + 16, 0);
    }
    c->eax = 0;
}

static void dev_CreateEffect(Ctx* c) { ST32(ARG(3), 0); c->eax = 0x80004001u; }
static void dev_Ok(Ctx* c) { c->eax = 0; }
static void dev_NotFound(Ctx* c) { c->eax = 0x80070002u; }
static void dev_NoImpl(Ctx* c) { c->eax = 0x80004001u; }

static const ShimDef dev_impl[] = {
    {"GetDeviceState", dev_GetDeviceState}, {"GetDeviceData", dev_GetDeviceData},
    {"SetProperty", dev_SetProperty}, {"EnumObjects", dev_EnumObjects},
    {"GetCapabilities", dev_GetCapabilities}, {"GetDeviceInfo", dev_GetDeviceInfo},
    {"GetProperty", dev_GetProperty}, {"CreateEffect", dev_CreateEffect},
    {"SetDataFormat", dev_Ok}, {"SetCooperativeLevel", dev_Ok}, {"Acquire", dev_Ok},
    {"Unacquire", dev_Ok}, {"Poll", dev_Ok}, {"SetEventNotification", dev_Ok},
    {"GetObjectInfo", dev_NotFound}, {"EnumEffects", dev_Ok},
    {"GetEffectInfo", dev_NotFound}, {"GetForceFeedbackState", dev_NoImpl},
    {"EnumCreatedEffectObjects", dev_Ok}, {"EnumEffectsInFile", dev_Ok},
    {"GetImageInfo", dev_NoImpl}, {NULL, NULL}
};

static uint32_t vt_di, vt_dev;

static uint32_t pad_instance(void) {
    static uint32_t inst;
    if (inst) return inst;
    inst = G(guest_alloc(0x244));
    ST32(inst, 0x244);
    ST32(inst + 4, PAD_GUID);
    ST32(inst + 20, PAD_GUID + 1);
    ST32(inst + 36, 0x0215);
    strcpy((char*)H(inst + 40), "Nintendo Switch Controller");
    strcpy((char*)H(inst + 300), "Nintendo Switch Controller");
    return inst;
}

static void di_EnumDevices(Ctx* c) {
    uint32_t type = ARG(1);
    if (type == 0 || type == 4) {
        uint32_t args[2] = {pad_instance(), ARG(3)};
        guest_call(ARG(2), 2, args);
    }
    c->eax = 0;
}

static void di_CreateDevice(Ctx* c) {
    uint32_t g = LD32(ARG(1));
    int kind = g == 0x6f1d2b61u ? DEV_KEYBOARD : g == 0x6f1d2b60u ? DEV_MOUSE : g == PAD_GUID ? DEV_PAD : 0;
    if (!kind) { ST32(ARG(2), 0); c->eax = 0x80040154u; return; }
    uint32_t d = com_new(vt_dev, 16);
    ST32(d + 8, (uint32_t)kind);
    ST32(ARG(2), d);
    vlog("DirectInput device %d created\n", kind);
    c->eax = 0;
}

static void di_GetDeviceStatus(Ctx* c) { c->eax = 0; }
static void di_FindDevice(Ctx* c) { c->eax = 0x80040154u; }

static const ShimDef di_impl[] = {
    {"CreateDevice", di_CreateDevice}, {"EnumDevices", di_EnumDevices},
    {"GetDeviceStatus", di_GetDeviceStatus}, {"FindDevice", di_FindDevice},
    {"EnumDevicesBySemantics", di_GetDeviceStatus}, {NULL, NULL}
};

static void DirectInput8Create_(Ctx* c) {
    if (!vt_di) {
        vt_di = com_vtable("IDirectInput8A", di_impl);
        vt_dev = com_vtable("IDirectInputDevice8A", dev_impl);
    }
    ST32(ARG(3), com_new(vt_di, 16));
    vlog("DirectInput8Create on Switch\n");
    c->eax = 0;
}

const ShimDef shims_dinput8[] = {{"DirectInput8Create", DirectInput8Create_}, {NULL, NULL}};
