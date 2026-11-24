/* In-game Options Menu for Ultimate Spider-Man on Nintendo Switch.
   Accessible by pressing Minus (-) in-game. Provides granular controls over graphics,
   controls, performance, QoL, system, and debug settings (modeled on SwitchWaker). */
#include "options_menu.h"
#include <glad/glad.h>
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu_font.h"
#include "usm_switch.h"
/* Generated every build by switch/build_info.cmake: USM_BUILD_SHA and
   USM_BUILD_BRANCH, when git was available. Absent in other builds. */
#if defined(__has_include)
#if __has_include("usm_build_info.h")
#include "usm_build_info.h"
#endif
#endif

static int g_open = 0;
static int g_tab = 0;
static int g_row = 0;

enum { TAB_GRAPHICS, TAB_CONTROLS, TAB_PERF, TAB_QOL, TAB_SYSTEM, TAB_DEBUG, NUM_TABS };
static const char* kTabNames[NUM_TABS] = {
    "Graphics", "Controls", "Performance", "QoL", "System", "Debug"
};

typedef struct {
    const char* name;
    int cur;
    int num_opts;
    const char* opts[8];
    const char* help;
} MenuRow;

static MenuRow s_graphics_rows[] = {
    {"View distance", 2, 6, {"80 m", "100 m", "120 m (Default)", "150 m", "200 m", "Max (Far City)"}, "City view distance before fog"},
    {"Far city fog", 1, 2, {"Off", "On"}, "Show distance fog over Manhattan"},
    {"Particle detail", 1, 3, {"Low", "Medium (Default)", "High"}, "Smoke, fire, and explosion effects"},
    {"Resolution mode", 0, 4, {"Native 720p (1280x720) [Crisp]", "1080p Docked (1920x1080)", "Performance 544p (960x544)", "Low 480p (640x480)"}, "Internal 3D rendering resolution - restart the game to apply"},
    {"Anti-Aliasing (FXAA)", 0, 4, {"Off", "Low (Light)", "Medium (Smooth)", "High (Ultra Smooth)"}, "Removes pixelated edges and jaggies from character and city"},
    {"Rescaling / Sharpening", 0, 4, {"Bilinear", "Bicubic Sharp", "FSR 1.0 (EASU+RCAS)", "FSR Ultra Sharp"}, "Adaptive contrast sharpening & image reconstruction"}
};

static MenuRow s_controls_rows[] = {
    /* Model indices: getters/save/load read these by number — never reorder.
       Invert X/Y were removed (Brandon's call, §3.4 fallback): camera invert lives
       in the game's own Options menu, which is the single source of truth.
       Rumble row removed: placebo, no vibration path (CreateEffect = E_NOTIMPL);
       keeping an honest "not supported" row instead is a one-line change. */
    {"Camera Stick Sensitivity", 2, 5, {"0.5x", "0.75x", "1.0x (Normal)", "1.25x", "1.5x"}, "Right-stick camera speed"},
    {"Swap A/B buttons", 0, 2, {"Off (B: Jump, A: Action)", "On (A: Jump, B: Action)"}, "Swap A and B button functions"},
    {"--- Controller Map ---", 0, 1, {"[ Switch Layout ]"}, "Read-only gamepad button mapping reference"},
    {"Button 1 (B)", 0, 1, {"Jump (B)"}, "Jump, release web swing - also confirms in menus"},
    {"Button 2 (A)", 0, 1, {"Wall Crawl / Web (A)"}, "Stick to walls, wall crawl, sprint - also cancels in menus"},
    {"Button 3 (Y)", 0, 1, {"Punch / Light (Y)"}, "Punch, light attack combos"},
    {"Button 4 (X)", 0, 1, {"Kick / Heavy (X)"}, "Kick, heavy attack combos"},
    {"Button 5 (L / ZL)", 0, 1, {"Web Zip (L / ZL)"}, "Quick web zip forward"},
    {"Button 6 (R / ZR)", 0, 1, {"Web Swing (R / ZR)"}, "Web swing, shoot web"},
    {"Button 7 (-)", 0, 1, {"(Unused)"}, "No action bound to Button 7"},
    {"Button 8 (Plus)", 0, 1, {"Pause Game (Plus)"}, "Pause menu and in-game options"},
    {"Button 9 (LS Click)", 0, 1, {"Open Map (LS Click)"}, "In-game city map overview (Left Stick click)"},
    {"Button 10 (RS Click)", 0, 1, {"Center Camera (RS Click)"}, "Reset camera behind Spider-Man (Right Stick click)"},
    {"Minus (-)", 0, 1, {"This overlay menu"}, "Open or close this overlay - L/R or ZL/ZR switch tabs"},
    {"Move", 0, 1, {"Left Stick / D-Pad"}, "Run around the city - Left Stick or D-Pad"},
    {"Look", 0, 1, {"Right Stick"}, "Rotate camera - Right Stick only. Camera invert lives in the game's Options menu"}
};

static MenuRow s_perf_rows[] = {
    {"Frame rate cap", 1, 3, {"20 FPS", "30 FPS (Stock)", "40 FPS (Boost)"}, "Target game update rate - 40 FPS cap also enables frame generation"},
    {"Frame Generation", 0, 2, {"Off", "On (+10 FPS Boost to 40)"}, "Synthesize 10 intermediate frames per sec to reach 40 FPS without blur (experimental)"},
    {"Display sync (VSync)", 1, 2, {"Off", "On (Double-buffered)"}, "Synchronize frame presentation"},
    {"FPS Counter", 0, 2, {"Hidden", "Visible"}, "On-screen frame rate overlay"},
    {"GPU Performance Profile", 0, 3, {"Stock (307 MHz)", "Boost (384 MHz)", "High (460 MHz)"}, "Official Nintendo GPU clock profile"},
    {"CPU Overclock", 0, 4, {"Stock (1020 MHz)", "Boost (1224 MHz)", "High (1581 MHz)", "Max (1785 MHz)"}, "CPU clock speed (clkrst)"},
    {"Pedestrian & Traffic", 2, 5, {"None", "Very Low", "Medium (Default)", "High", "Full (PC)"}, "City traffic and pedestrians density"}
};

static MenuRow s_qol_rows[] = {
    {"Skip intro movies", 0, 2, {"Off", "On"}, "Automatically bypass splash screens"},
    {"Fast map pause", 0, 2, {"Off", "On"}, "Speed up minimap transition"}
};

static MenuRow s_system_rows[] = {
    {"Save settings to SD", 0, 1, {"[ Press to Save ]"}, "Save current options to settings.ini"},
    {"Reset all settings", 0, 1, {"[ Press to Reset ]"}, "Delete settings.ini and registry.txt - guest settings reset on next boot"},
    {"Quit Game", 0, 1, {"[ Press to Exit ]"}, "Cleanly restore clocks and exit to Horizon OS"},
    /* SESSION (model 3..5): read-only. num_opts == 1, and the System action
       branch in options_menu_update only fires on 0..2. Values are built
       live in options_menu_render. */
    {"Build", 0, 1, {"-"}, "Commit this build was made from (read-only)"},
    {"Clocks", 0, 1, {"-"}, "Selected CPU/GPU profiles, set on the Performance tab (read-only)"},
    /* Plain file name until the NRO/NSP USM_DIR switch lands: the full
       save:/ path would be wrong for NRO users. */
    {"Settings file", 0, 1, {"settings.ini"}, "Holds these settings - registry.txt and resolution.txt sit beside it (read-only)"}
};

static MenuRow s_debug_rows[] = {
    {"Wireframe mode", 0, 2, {"Off", "On"}, "Render 3D scene in wireframe"},
    {"Write session log.txt", 0, 2, {"Off", "On"}, "Write detailed diagnostic logs to SD"},
    {"Test prompts", 0, 1, {"[ Show sample ]"}, "Show one sample prompt line with live Switch button names"},
    /* RUNTIME (model 3..4): read-only, values built live from s_fps. */
    {"FPS", 0, 1, {"-"}, "Presented frames per second, generated frames included (read-only)"},
    {"Frame time", 0, 1, {"-"}, "Average time between presented frames (read-only)"}
};

static MenuRow* s_tab_rows[NUM_TABS] = {
    s_graphics_rows, s_controls_rows, s_perf_rows, s_qol_rows, s_system_rows, s_debug_rows
};

/* Grouped views over the unchanged model arrays.
   A view entry >= 0 is an index into the tab's model array; < 0 is a section
   header, shown as a banner and never selectable. Getters keep reading the
   model by number, so grouping can never shift an index under them.
   Controls model (16 rows): [0] Sensitivity, [1] Swap, [2] Map header,
   [3] Button 1, [4] Button 2, [5] Button 3, [6] Button 4, [7] Button 5,
   [8] Button 6, [9] Button 7, [10] Button 8 (Plus), [11] Button 9 (LS),
   [12] Button 10 (RS), [13] Minus, [14] Move, [15] Look.
   The Map header row [2] is not in the view: the group banners replace it. */
#define HDR_MOVEMENT -1
#define HDR_COMBAT   -2
#define HDR_WEB      -3
#define HDR_CAMERA   -4
#define HDR_SYSTEM   -5
#define HDR_PICTURE  -6
#define HDR_CITY     -7
#define HDR_SPEED    -8
#define HDR_HARDWARE -9
#define HDR_GENERAL  -10
#define HDR_SESSION  -11
#define HDR_RUNTIME  -12

static const char* header_name(int h) {
    switch (h) {
        case HDR_MOVEMENT: return "MOVEMENT";
        case HDR_COMBAT:   return "COMBAT";
        case HDR_WEB:      return "WEB";
        case HDR_CAMERA:   return "CAMERA";
        case HDR_SYSTEM:   return "SYSTEM";
        case HDR_PICTURE:  return "PICTURE";
        case HDR_CITY:     return "CITY";
        case HDR_SPEED:    return "SPEED";
        case HDR_HARDWARE: return "HARDWARE";
        case HDR_GENERAL:  return "GENERAL";
        case HDR_SESSION:  return "SESSION";
        case HDR_RUNTIME:  return "RUNTIME";
        default: return "";
    }
}

static const int8_t s_graphics_view[] = {
    HDR_PICTURE, 3, 4, 5,
    HDR_CITY, 0, 1, 2,
};
static const int8_t s_controls_view[] = {
    HDR_MOVEMENT, 14,
    HDR_COMBAT, 5, 6,
    HDR_WEB, 4, 3, 7, 8, 9,
    HDR_CAMERA, 15, 12, 0,
    HDR_SYSTEM, 10, 11, 13, 1,
};
static const int8_t s_perf_view[] = {
    HDR_SPEED, 0, 1, 2, 3,
    HDR_HARDWARE, 4, 5, 6,
};
static const int8_t s_qol_view[] = {
    HDR_GENERAL, 0, 1,
};
static const int8_t s_system_view[] = {
    HDR_GENERAL, 0, 1, 2,
    HDR_SESSION, 3, 4, 5,
};
static const int8_t s_debug_view[] = {
    HDR_GENERAL, 0, 1, 2,
    HDR_RUNTIME, 3, 4,
};

static const int8_t* s_tab_views[NUM_TABS] = {
    s_graphics_view, s_controls_view, s_perf_view, s_qol_view, s_system_view, s_debug_view
};

static int s_tab_view_counts[NUM_TABS] = {
    (int)(sizeof(s_graphics_view)/sizeof(s_graphics_view[0])),
    (int)(sizeof(s_controls_view)/sizeof(s_controls_view[0])),
    (int)(sizeof(s_perf_view)/sizeof(s_perf_view[0])),
    (int)(sizeof(s_qol_view)/sizeof(s_qol_view[0])),
    (int)(sizeof(s_system_view)/sizeof(s_system_view[0])),
    (int)(sizeof(s_debug_view)/sizeof(s_debug_view[0]))
};

/* First selectable (non-header) view position of a tab. */
static int view_first_selectable(int tab) {
    const int8_t* v = s_tab_views[tab];
    for (int i = 0; i < s_tab_view_counts[tab]; i++) {
        if (v[i] >= 0) return i;
    }
    return 0;
}

/* Move g_row one selectable entry in dir (+1 down, -1 up). Taps wrap; a held
   repeat (wrap = 0) stops at the first/last selectable entry instead, so
   holding Down through the long Controls list can't fly past the end. */
static void view_move(int dir, int wrap) {
    const int8_t* v = s_tab_views[g_tab];
    int count = s_tab_view_counts[g_tab];
    int pos = g_row;
    for (int i = 0; i < count; i++) {
        pos += dir;
        if (pos < 0 || pos >= count) {
            if (!wrap) return;
            pos = (pos + count) % count;
        }
        if (v[pos] >= 0) { g_row = pos; return; }
    }
}

/* Overlay Confirm = the Jump button, Back = the Action button — the game's own
   menu convention. With Swap OFF that is Confirm = B, Back = A; with Swap ON,
   Confirm = A, Back = B. */
static u64 confirm_btn(void) { return options_get_swap_ab() ? HidNpadButton_A : HidNpadButton_B; }
static u64 back_btn(void)    { return options_get_swap_ab() ? HidNpadButton_B : HidNpadButton_A; }
static char confirm_letter(void) { return options_get_swap_ab() ? 'A' : 'B'; }
static char back_letter(void)    { return options_get_swap_ab() ? 'B' : 'A'; }

/* Live Switch label for a game button, Swap-aware for Buttons 1/2 (input.c map).
   Buttons 3..10 are fixed. The static opts[0] strings stay as fallbacks.
   {(B)} is a button-icon token: drawn as the Kenney icon when the icon
   texture is available, as the text "(B)" otherwise (see draw_text_shear).
   Plus and the stick clicks have no icon slot yet, so they stay words. */
static void map_label(int button /* 1..10 */, char* out, size_t n) {
    int swap = options_get_swap_ab();
    switch (button) {
        case 1:  snprintf(out, n, "Jump {(%c)}", swap ? 'A' : 'B'); break;
        case 2:  snprintf(out, n, "Wall Crawl / Web {(%c)}", swap ? 'B' : 'A'); break;
        case 3:  snprintf(out, n, "Punch / Light {(Y)}"); break;
        case 4:  snprintf(out, n, "Kick / Heavy {(X)}"); break;
        case 5:  snprintf(out, n, "Web Zip {(L)} / ZL"); break;
        case 6:  snprintf(out, n, "Web Swing {(R)} / ZR"); break;
        case 7:  snprintf(out, n, "(Unused)"); break;
        case 8:  snprintf(out, n, "Pause Game (Plus)"); break;
        case 9:  snprintf(out, n, "Open Map (LS Click)"); break;
        case 10: snprintf(out, n, "Center Camera (RS Click)"); break;
        default: snprintf(out, n, ""); break;
    }
}

/* Live Switch name for a game button, Swap-aware for Buttons 1/2 -- the row
   names the Controller Map draws, so they flip with the values (map_label). */
static void map_name(int button /* 1..10 */, char* out, size_t n) {
    int swap = options_get_swap_ab();
    switch (button) {
        case 1:  snprintf(out, n, "Button 1 {(%c)}", swap ? 'A' : 'B'); break;
        case 2:  snprintf(out, n, "Button 2 {(%c)}", swap ? 'B' : 'A'); break;
        case 3:  snprintf(out, n, "Button 3 {(Y)}"); break;
        case 4:  snprintf(out, n, "Button 4 {(X)}"); break;
        case 5:  snprintf(out, n, "Button 5 {(L)} / ZL"); break;
        case 6:  snprintf(out, n, "Button 6 {(R)} / ZR"); break;
        case 7:  snprintf(out, n, "Button 7 (-)"); break;
        case 8:  snprintf(out, n, "Button 8 (Plus)"); break;
        case 9:  snprintf(out, n, "Button 9 (LS Click)"); break;
        case 10: snprintf(out, n, "Button 10 (RS Click)"); break;
        default: snprintf(out, n, ""); break;
    }
}

/* One-shot "Test prompts" sample line, shown via the status message. */
static char s_test_line[160];

static uint64_t now_ns(void) { return armTicksToNs(armGetSystemTick()); }

/* Status line: shown in place of the help text until s_status_until. */
static uint64_t s_status_until = 0;
static const char* s_status_msg = NULL;
static void set_status(const char* msg, uint32_t ms) {
    s_status_msg = msg;
    s_status_until = now_ns() + (uint64_t)ms * 1000000ull;
}

/* GPU/CPU rows apply on release (see options_menu_update). */
static void apply_pending_clocks(void);

/* Remembered row per tab (view position), -1 = first selectable. */
static int g_row_per_tab[NUM_TABS] = {-1, -1, -1, -1, -1, -1};

int options_menu_is_open(void) { return g_open; }
void options_menu_toggle(void) {
    g_open = !g_open;
    if (!g_open) apply_pending_clocks();
    /* g_row is kept across close/open: reopening lands on the last row. */
    vlog("options_menu: toggle called, now g_open = %d\n", g_open);
}

float options_get_density(void) {
    static const float d[] = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};
    int idx = s_perf_rows[6].cur;
    if (idx < 0 || idx >= 5) idx = 2;
    return d[idx];
}

static bool s_apm_inited = false;
static bool s_clkrst_inited = false;
static ClkrstSession s_clk_cpu;

void options_apply_gpu_profile(int idx) {
    static const u32 configs[] = {0x00020003, 0x00020004, 0x92220008};
    if (idx < 0 || idx >= 3) idx = 0;
    if (!s_apm_inited) {
        if (R_SUCCEEDED(apmInitialize())) s_apm_inited = true;
    }
    if (s_apm_inited) {
        Result rc = apmSetPerformanceConfiguration(ApmPerformanceMode_Normal, configs[idx]);
        vlog("options: GPU profile %d (0x%08x): rc 0x%x\n", idx, configs[idx], (unsigned)rc);
    }
}

void options_apply_cpu_clock(int idx) {
    static const u32 freqs[] = {1020000000u, 1224000000u, 1581000000u, 1785000000u};
    if (idx < 0 || idx >= 4) idx = 0;
    if (!s_clkrst_inited) {
        if (hosversionAtLeast(8, 0, 0) && R_SUCCEEDED(clkrstInitialize())) {
            if (R_SUCCEEDED(clkrstOpenSession(&s_clk_cpu, PcvModuleId_CpuBus, 3))) {
                s_clkrst_inited = true;
            }
        }
    }
    if (s_clkrst_inited) {
        Result rc = clkrstSetClockRate(&s_clk_cpu, freqs[idx]);
        u32 actual = 0;
        clkrstGetClockRate(&s_clk_cpu, &actual);
        vlog("options: CPU clock set to %u MHz (rc 0x%x, actual %.1f MHz)\n",
             (unsigned)(freqs[idx] / 1000000u), (unsigned)rc, actual / 1e6);
    }
}

void options_menu_cleanup(void) {
    if (s_clkrst_inited) {
        clkrstSetClockRate(&s_clk_cpu, 1020000000u);
        clkrstCloseSession(&s_clk_cpu);
        clkrstExit();
        s_clkrst_inited = false;
        vlog("options: CPU clock restored to stock (1020 MHz)\n");
    }
    if (s_apm_inited) {
        apmSetPerformanceConfiguration(ApmPerformanceMode_Normal, 0x00020003);
        apmExit();
        s_apm_inited = false;
        vlog("options: GPU profile restored to stock (307 MHz)\n");
    }
}

float options_get_view_distance(void) {
    static const float v[] = {80.0f, 100.0f, 120.0f, 150.0f, 200.0f, 1000.0f};
    int idx = s_graphics_rows[0].cur;
    if (idx < 0 || idx >= 6) idx = 2;
    return v[idx];
}

int options_get_fps_cap(void) {
    static const int c[] = {20, 30, 40};
    int idx = s_perf_rows[0].cur;
    if (idx < 0 || idx >= 3) idx = 1;
    return c[idx];
}

int options_get_frame_gen(void) {
    if (s_perf_rows[1].cur == 1 || s_perf_rows[0].cur == 2) {
        return 1;
    }
    return 0;
}

int options_get_vsync(void) {
    int idx = s_perf_rows[2].cur;
    return (idx == 1);
}

int options_show_fps(void) {
    int idx = s_perf_rows[3].cur;
    return (idx == 1);
}

int options_get_fxaa(void) {
    return s_graphics_rows[4].cur;
}

int options_get_sharpen(void) {
    return s_graphics_rows[5].cur;
}

int options_get_wireframe(void) {
    int idx = s_debug_rows[0].cur;
    return (idx == 1);
}

/* Camera invert is owned by the game's own Options menu (Brandon's call,
   §3.4 fallback) — there is no overlay invert setting any more, so the
   overlay invert getters and the rx/ry flip in input.c are removed with it. */

float options_get_sensitivity(void) {
    static const float s[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f};
    int idx = s_controls_rows[0].cur;
    if (idx < 0 || idx >= 5) idx = 2;
    return s[idx];
}

int options_get_swap_ab(void) {
    int idx = s_controls_rows[1].cur;
    return (idx == 1);
}

static void reset_defaults(void) {
    s_graphics_rows[0].cur = 2; /* 120 m (Default) */
    s_graphics_rows[1].cur = 1; /* On */
    s_graphics_rows[2].cur = 1; /* Medium (Default) */
    s_graphics_rows[3].cur = 0; /* Native 720p */
    s_graphics_rows[4].cur = 0; /* FXAA Off */
    s_graphics_rows[5].cur = 0; /* Bilinear */

    s_controls_rows[0].cur = 2; /* 1.0x (Normal) */
    s_controls_rows[1].cur = 0; /* Off (B: Jump, A: Action) */

    s_perf_rows[0].cur = 1; /* 30 FPS (Stock) */
    s_perf_rows[1].cur = 0; /* Frame Gen Off */
    s_perf_rows[2].cur = 1; /* On (Double-buffered) */
    s_perf_rows[3].cur = 0; /* Hidden */
    s_perf_rows[4].cur = 0; /* Stock (307 MHz) */
    s_perf_rows[5].cur = 0; /* Stock (1020 MHz) */
    s_perf_rows[6].cur = 2; /* Medium (Default) */

    s_qol_rows[0].cur = 0; /* Off */
    s_qol_rows[1].cur = 0; /* Off */

    s_system_rows[0].cur = 0;
    s_system_rows[1].cur = 0;
    s_system_rows[2].cur = 0;

    s_debug_rows[0].cur = 0; /* Off */
    s_debug_rows[1].cur = 0; /* Off */
    s_debug_rows[2].cur = 0; /* Test prompts is one-shot, no stored state */
}

static void save_settings(void) {
    /* Save resolution choice to resolution.txt */
    static const char* kResNames[] = {"1280x720", "1920x1080", "960x544", "640x480"};
    int r_idx = s_graphics_rows[3].cur;
    if (r_idx >= 0 && r_idx < 4) {
        FILE* rf = fopen(USM_DIR "resolution.txt", "w");
        if (rf) {
            fprintf(rf, "%s\n", kResNames[r_idx]);
            fclose(rf);
        }
    }
    FILE* f = fopen(USM_DIR "settings.ini", "w");
    if (!f) return;
    fprintf(f, "[graphics]\nview_dist=%d\nfog=%d\nparticles=%d\nres=%d\nfxaa=%d\nsharpen=%d\n\n",
            s_graphics_rows[0].cur, s_graphics_rows[1].cur, s_graphics_rows[2].cur, s_graphics_rows[3].cur,
            s_graphics_rows[4].cur, s_graphics_rows[5].cur);
    fprintf(f, "[controls]\nsens=%d\nswap_ab=%d\n\n",
            s_controls_rows[0].cur, s_controls_rows[1].cur);
    fprintf(f, "[performance]\nfps_cap=%d\nframe_gen=%d\nvsync=%d\nshow_fps=%d\ngpu_profile=%d\ncpu_clock=%d\ndensity=%d\n\n",
            s_perf_rows[0].cur, s_perf_rows[1].cur, s_perf_rows[2].cur, s_perf_rows[3].cur, s_perf_rows[4].cur, s_perf_rows[5].cur, s_perf_rows[6].cur);
    fprintf(f, "[qol]\nskip_intro=%d\nfast_map=%d\n\n",
            s_qol_rows[0].cur, s_qol_rows[1].cur);
    fprintf(f, "[debug]\nwireframe=%d\nlog=%d\n",
            s_debug_rows[0].cur, s_debug_rows[1].cur);
    fclose(f);
    fsdevCommitDevice("save");
    vlog("options: settings saved to " USM_DIR "settings.ini\n");
}

static void load_settings(void) {
    FILE* f = fopen(USM_DIR "settings.ini", "r");
    if (!f) f = fopen("romfs:/settings.ini", "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        int v;
        if (sscanf(line, "view_dist=%d", &v) == 1) {
            if (v >= 0 && v < s_graphics_rows[0].num_opts) s_graphics_rows[0].cur = v;
        } else if (sscanf(line, "fog=%d", &v) == 1) {
            if (v >= 0 && v < s_graphics_rows[1].num_opts) s_graphics_rows[1].cur = v;
        } else if (sscanf(line, "particles=%d", &v) == 1) {
            if (v >= 0 && v < s_graphics_rows[2].num_opts) s_graphics_rows[2].cur = v;
        } else if (sscanf(line, "res=%d", &v) == 1) {
            if (v >= 0 && v < s_graphics_rows[3].num_opts) s_graphics_rows[3].cur = v;
        } else if (sscanf(line, "fxaa=%d", &v) == 1) {
            if (v >= 0 && v < s_graphics_rows[4].num_opts) s_graphics_rows[4].cur = v;
        } else if (sscanf(line, "sharpen=%d", &v) == 1) {
            if (v >= 0 && v < s_graphics_rows[5].num_opts) s_graphics_rows[5].cur = v;
        } else if (sscanf(line, "sens=%d", &v) == 1) {
            if (v >= 0 && v < s_controls_rows[0].num_opts) s_controls_rows[0].cur = v;
        } else if (sscanf(line, "swap_ab=%d", &v) == 1) {
            if (v >= 0 && v < s_controls_rows[1].num_opts) s_controls_rows[1].cur = v;
        } else if (sscanf(line, "fps_cap=%d", &v) == 1) {
            if (v >= 0 && v < s_perf_rows[0].num_opts) s_perf_rows[0].cur = v;
        } else if (sscanf(line, "frame_gen=%d", &v) == 1) {
            if (v >= 0 && v < s_perf_rows[1].num_opts) s_perf_rows[1].cur = v;
        } else if (sscanf(line, "vsync=%d", &v) == 1) {
            if (v >= 0 && v < s_perf_rows[2].num_opts) s_perf_rows[2].cur = v;
        } else if (sscanf(line, "show_fps=%d", &v) == 1) {
            if (v >= 0 && v < s_perf_rows[3].num_opts) s_perf_rows[3].cur = v;
        } else if (sscanf(line, "gpu_profile=%d", &v) == 1) {
            if (v >= 0 && v < s_perf_rows[4].num_opts) s_perf_rows[4].cur = v;
        } else if (sscanf(line, "cpu_clock=%d", &v) == 1) {
            if (v >= 0 && v < s_perf_rows[5].num_opts) s_perf_rows[5].cur = v;
        } else if (sscanf(line, "density=%d", &v) == 1) {
            if (v >= 0 && v < s_perf_rows[6].num_opts) s_perf_rows[6].cur = v;
        } else if (sscanf(line, "skip_intro=%d", &v) == 1) {
            if (v >= 0 && v < s_qol_rows[0].num_opts) s_qol_rows[0].cur = v;
        } else if (sscanf(line, "fast_map=%d", &v) == 1) {
            if (v >= 0 && v < s_qol_rows[1].num_opts) s_qol_rows[1].cur = v;
        } else if (sscanf(line, "wireframe=%d", &v) == 1) {
            if (v >= 0 && v < s_debug_rows[0].num_opts) s_debug_rows[0].cur = v;
        } else if (sscanf(line, "log=%d", &v) == 1) {
            if (v >= 0 && v < s_debug_rows[1].num_opts) s_debug_rows[1].cur = v;
        }
    }
    fclose(f);
}

static void init_gui_gl(void);

void options_menu_init(void) {
    reset_defaults();
    load_settings();
    g_row = view_first_selectable(g_tab);
    options_apply_gpu_profile(s_perf_rows[4].cur);
    options_apply_cpu_clock(s_perf_rows[5].cur);
    init_gui_gl();
}

/* GPU/CPU rows: each step used to call apm/clkrst immediately, so a held
   Left/Right would re-clock at ~11 Hz and every change can hitch a frame.
   Now the displayed value cycles while held and the clock is set once,
   when Left/Right are released (or the menu closes, or the tab changes). */
static int s_pending_gpu = 0, s_pending_cpu = 0;
static void apply_pending_clocks(void) {
    if (s_pending_gpu) { s_pending_gpu = 0; options_apply_gpu_profile(s_perf_rows[4].cur); }
    if (s_pending_cpu) { s_pending_cpu = 0; options_apply_cpu_clock(s_perf_rows[5].cur); }
}

/* Two-press actions (Reset, Quit): the first Confirm arms, a second within
   2 s fires. Any navigation disarms. */
static int s_armed_row = -1;
static uint64_t s_armed_until = 0;
static char s_arm_msg[64];

static void change_tab(int dir) {
    apply_pending_clocks();
    g_row_per_tab[g_tab] = g_row;
    g_tab = (g_tab + dir + NUM_TABS) % NUM_TABS;
    g_row = g_row_per_tab[g_tab] >= 0 ? g_row_per_tab[g_tab] : view_first_selectable(g_tab);
    s_armed_row = -1;
}

static void change_value(MenuRow* r, int midx, int dir) {
    if (r->num_opts <= 1) return;
    r->cur = (r->cur + dir + r->num_opts) % r->num_opts;
    if (g_tab == TAB_PERF && midx == 4) s_pending_gpu = 1;
    if (g_tab == TAB_PERF && midx == 5) s_pending_cpu = 1;
}

#define BTN_UP    (HidNpadButton_Up | HidNpadButton_StickLUp)
#define BTN_DOWN  (HidNpadButton_Down | HidNpadButton_StickLDown)
#define BTN_LEFT  (HidNpadButton_Left | HidNpadButton_StickLLeft)
#define BTN_RIGHT (HidNpadButton_Right | HidNpadButton_StickLRight)
#define REPEAT_DELAY_NS 350000000ull
#define REPEAT_RATE_NS   90000000ull

/* Tap = one step now; hold = repeat after REPEAT_DELAY, then every
   REPEAT_RATE. Returns the step to take this poll (0 = none) and whether
   it came from a repeat (*rep = 1) rather than a tap. */
static int repeat_axis(uint64_t kDown, uint64_t kHeld, u64 neg, u64 pos,
                       int* dir_state, uint64_t* next, uint64_t now, int* rep) {
    *rep = 0;
    int step = (kDown & neg) ? -1 : (kDown & pos) ? +1 : 0;
    if (step) { *dir_state = step; *next = now + REPEAT_DELAY_NS; return step; }
    int held = (kHeld & neg) ? -1 : (kHeld & pos) ? +1 : 0;
    if (!held) { *dir_state = 0; return 0; }
    if (held != *dir_state) { *dir_state = held; *next = now + REPEAT_DELAY_NS; return 0; }
    if (now >= *next) { *next = now + REPEAT_RATE_NS; *rep = 1; return held; }
    return 0;
}

void options_menu_update(uint64_t kDown, uint64_t kHeld, int32_t lx, int32_t ly) {
    (void)lx;
    (void)ly;
    static uint64_t s_vnext = 0, s_hnext = 0;
    static int s_vdir = 0, s_hdir = 0;
    if (!g_open) {
        s_vdir = s_hdir = 0;
        return;
    }

    if (kDown & back_btn()) {
        g_open = 0;
        apply_pending_clocks();
        s_armed_row = -1;
        vlog("options_menu: closed by back button\n");
        return;
    }

    /* Repeat state never crosses a tab switch: holding Down while pressing
       R must not keep scrolling the new tab. */
    if (kDown & (HidNpadButton_L | HidNpadButton_ZL)) { change_tab(-1); s_vdir = s_hdir = 0; }
    if (kDown & (HidNpadButton_R | HidNpadButton_ZR)) { change_tab(+1); s_vdir = s_hdir = 0; }

    uint64_t now = now_ns();
    int rep = 0;
    int vstep = repeat_axis(kDown, kHeld, BTN_UP, BTN_DOWN, &s_vdir, &s_vnext, now, &rep);
    if (vstep) { view_move(vstep, !rep); s_armed_row = -1; }

    /* g_row is a view position; midx is the model index it points at.
       All row actions compare the model index, never the view position. */
    int midx = s_tab_views[g_tab][g_row];
    MenuRow* r = &s_tab_rows[g_tab][midx];

    int hstep = repeat_axis(kDown, kHeld, BTN_LEFT, BTN_RIGHT, &s_hdir, &s_hnext, now, &rep);
    if (hstep) change_value(r, midx, hstep);
    if (!(kHeld & (BTN_LEFT | BTN_RIGHT))) apply_pending_clocks();

    /* Actions fire on Confirm only. D-pad Right / stick-right used to fire
       them too, so a tilt (or stick drift) on Quit Game exited at once. */
    if (!(kDown & confirm_btn())) return;
    int row_key = g_tab * 64 + midx;
    if (g_tab == TAB_SYSTEM && midx == 0) {
        save_settings();
        set_status("Settings saved to SD (settings.ini & resolution.txt)", 4000);
    } else if (g_tab == TAB_SYSTEM && (midx == 1 || midx == 2)) {
        if (s_armed_row != row_key || now > s_armed_until) {
            s_armed_row = row_key;
            s_armed_until = now + 2000000000ull;
            snprintf(s_arm_msg, sizeof s_arm_msg, "Press {%c} again to %s",
                     confirm_letter(), midx == 1 ? "reset all settings" : "quit the game");
            set_status(s_arm_msg, 2000);
            return;
        }
        s_armed_row = -1;
        if (midx == 1) {
            remove(USM_DIR "settings.ini");
            remove(USM_DIR "registry.txt");
            remove(USM_DIR "resolution.txt");
            /* Deletes on a journaled save:/ are only durable once committed. */
            fsdevCommitDevice("save");
            reset_defaults();
            s_pending_gpu = s_pending_cpu = 0;
            options_apply_gpu_profile(s_perf_rows[4].cur);
            options_apply_cpu_clock(s_perf_rows[5].cur);
            set_status("All settings reset - restart the game for guest settings to take effect", 4000);
        } else {
            vlog("options_menu: Quit Game confirmed by user -> exiting to Horizon OS\n");
            options_menu_cleanup();
            svcExitProcess();
        }
    } else if (g_tab == TAB_DEBUG && midx == 2) {
        /* Test prompts: one-shot sample line with live Switch button names. */
        snprintf(s_test_line, sizeof s_test_line,
                 "PRESS {%c} TO JUMP - {Y} TO PUNCH - {X} TO KICK", confirm_letter());
        set_status(s_test_line, 10000);
    } else {
        change_value(r, midx, +1);
        apply_pending_clocks();  /* a Confirm step is a single press: apply now */
    }
}

/* Modern Shader-based 2D GUI Renderer */
typedef struct {
    float x, y;
    float u, v;
    float r, g, b, a;
} MenuVert;

static GLuint s_prog = 0;
static GLuint s_vbo = 0;
static GLuint s_vao = 0;
static GLuint s_font_tex = 0;
static GLint s_u_res = -1;
static GLint s_u_tex = -1;
static GLint s_u_mode = -1;

static MenuVert s_verts[4096];
static int s_vert_count = 0;
static float s_char_u0[95], s_char_v0[95], s_char_u1[95], s_char_v1[95];

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, sizeof buf, NULL, buf);
        vlog("options_menu: shader compile failed: %s\n%s\n", buf, src);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static void init_gui_gl(void) {
    if (s_prog) return;

    /* Create 256x256 RGBA atlas for Aileron font */
    static uint32_t atlas[256 * 256];
    memset(atlas, 0, sizeof atlas);
    int cx = 0, cy = 0;
    for (int i = 0; i < 95; i++) {
        int w = menu_font_w[i];
        /* 1 px transparent gutter right of and below every glyph: with
           GL_LINEAR at non-integer scales, edge texels blend with their
           neighbour, and packed edge to edge that neighbour is the next
           glyph (faint slivers beside letters). */
        if (cx + w + 1 > 256) { cx = 0; cy += MENU_FONT_H + 1; }
        s_char_u0[i] = (float)cx / 256.0f;
        s_char_v0[i] = (float)cy / 256.0f;
        s_char_u1[i] = (float)(cx + w) / 256.0f;
        s_char_v1[i] = (float)(cy + MENU_FONT_H) / 256.0f;

        const unsigned char* p = menu_font_px + menu_font_off[i];
        for (int row = 0; row < MENU_FONT_H; row++) {
            for (int col = 0; col < w; col++) {
                uint8_t a = p[row * w + col];
                atlas[(cy + row) * 256 + (cx + col)] = ((uint32_t)a << 24) | 0x00FFFFFFu;
            }
        }
        cx += w + 1;
    }

    glGenTextures(1, &s_font_tex);
    glBindTexture(GL_TEXTURE_2D, s_font_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    static const char* vs =
        "#version 430 core\n"
        "layout(location = 0) in vec2 a_pos;\n"
        "layout(location = 1) in vec2 a_uv;\n"
        "layout(location = 2) in vec4 a_col;\n"
        "uniform vec2 u_res;\n"
        "out vec2 v_uv;\n"
        "out vec4 v_col;\n"
        "void main() {\n"
        "    v_uv = a_uv;\n"
        "    v_col = a_col;\n"
        "    gl_Position = vec4(a_pos.x / u_res.x * 2.0 - 1.0, 1.0 - a_pos.y / u_res.y * 2.0, 0.0, 1.0);\n"
        "}\n";

    static const char* fs =
        "#version 430 core\n"
        "precision mediump float;\n"
        "uniform sampler2D u_tex;\n"
        "uniform int u_mode;\n"
        "in vec2 v_uv;\n"
        "in vec4 v_col;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    if (u_mode == 1) {\n"
        "        fragColor = texture(u_tex, v_uv) * v_col;\n"
        "    } else {\n"
        "        fragColor = v_col;\n"
        "    }\n"
        "}\n";

    GLuint v = compile_shader(GL_VERTEX_SHADER, vs);
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, fs);
    s_prog = glCreateProgram();
    glAttachShader(s_prog, v);
    glAttachShader(s_prog, f);
    glLinkProgram(s_prog);
    glDeleteShader(v);
    glDeleteShader(f);

    s_u_res = glGetUniformLocation(s_prog, "u_res");
    s_u_tex = glGetUniformLocation(s_prog, "u_tex");
    s_u_mode = glGetUniformLocation(s_prog, "u_mode");

    glGenVertexArrays(1, &s_vao);
    glBindVertexArray(s_vao);
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof s_verts, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(MenuVert), (void*)offsetof(MenuVert, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(MenuVert), (void*)offsetof(MenuVert, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(MenuVert), (void*)offsetof(MenuVert, r));

    glBindVertexArray(0);
}

static void flush_verts(int mode) {
    if (s_vert_count == 0) return;
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glUniform1i(s_u_mode, mode);
    glBufferSubData(GL_ARRAY_BUFFER, 0, s_vert_count * sizeof(MenuVert), s_verts);
    glDrawArrays(GL_TRIANGLES, 0, s_vert_count);
    s_vert_count = 0;
}

static void add_quad(float x0, float y0, float x1, float y1,
                     float u0, float v0, float u1, float v1,
                     float r, float g, float b, float a) {
    if (s_vert_count + 6 > 4096) return;
    MenuVert* v = &s_verts[s_vert_count];
    v[0] = (MenuVert){x0, y0, u0, v0, r, g, b, a};
    v[1] = (MenuVert){x1, y0, u1, v0, r, g, b, a};
    v[2] = (MenuVert){x0, y1, u0, v1, r, g, b, a};
    v[3] = (MenuVert){x1, y0, u1, v0, r, g, b, a};
    v[4] = (MenuVert){x1, y1, u1, v1, r, g, b, a};
    v[5] = (MenuVert){x0, y1, u0, v1, r, g, b, a};
    s_vert_count += 6;
}

static void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    add_quad(x, y, x + w, y + h, 0, 0, 1, 1, r, g, b, a);
}

/* Fake italic: the top verts shift +shear in x, the bottom verts stay, so the
   glyph leans right. Advance widths are unchanged, so text_width() still
   matches. shear 0 is upright text. */
/* Button icons (A1): the game's own prompt atlas (glyphs.h, Kenney Input
   Prompts, CC0), already uploaded by gl.c, so the overlay adds no texture.
   Slots: 0 B, 1 A, 2 Y, 3 X, 4 L, 5 R. Icon quads are queued apart from the
   glyph quads and drawn in one batch at the end of the frame, with the icon
   texture bound. 0 = no texture: tokens fall back to text. */
#define ICON_SLOTS 8
static GLuint s_icon_tex = 0;
static MenuVert s_icon_verts[64 * 6];
static int s_icon_count = 0;

static int icon_slot(char c) {
    switch (c) {
        case 'B': return 0; case 'A': return 1; case 'Y': return 2;
        case 'X': return 3; case 'L': return 4; case 'R': return 5;
        default: return -1;
    }
}

/* Parses an icon token at str ("{B}" or "{(B)}"). Returns its length and the
   slot, and the text to draw instead when there is no icon texture. */
static int parse_token(const char* str, int* slot, char* fallback) {
    if (str[0] != '{') return 0;
    int paren = str[1] == '(';
    const char* p = str + 1 + paren;
    int sl = icon_slot(p[0]);
    if (sl < 0) return 0;
    if (paren && p[1] != ')') return 0;
    const char* end = p + 1 + paren;
    if (*end != '}') return 0;
    *slot = sl;
    if (paren) { fallback[0] = '('; fallback[1] = p[0]; fallback[2] = ')'; fallback[3] = 0; }
    else       { fallback[0] = p[0]; fallback[1] = 0; }
    return (int)(end - str) + 1;
}

static float icon_size(float scale) { return (float)MENU_FONT_H * scale * 0.9f; }

static void queue_icon(float x, float y, float size, int slot, float a) {
    if (s_icon_count + 6 > (int)(sizeof s_icon_verts / sizeof s_icon_verts[0])) return;
    float u0 = (float)slot / ICON_SLOTS, u1 = (float)(slot + 1) / ICON_SLOTS;
    MenuVert* v = &s_icon_verts[s_icon_count];
    v[0] = (MenuVert){x, y, u0, 0, 1, 1, 1, a};
    v[1] = (MenuVert){x + size, y, u1, 0, 1, 1, 1, a};
    v[2] = (MenuVert){x, y + size, u0, 1, 1, 1, 1, a};
    v[3] = (MenuVert){x + size, y, u1, 0, 1, 1, 1, a};
    v[4] = (MenuVert){x + size, y + size, u1, 1, 1, 1, 1, a};
    v[5] = (MenuVert){x, y + size, u0, 1, 1, 1, 1, a};
    s_icon_count += 6;
}

/* Fake italic: the top verts shift +shear in x, the bottom verts stay, so the
   glyph leans right. Advance widths are unchanged, so text_width() still
   matches. shear 0 is upright text. Icon tokens become icons (upright). */
static void draw_text_shear(float x, float y, float scale, const char* str,
                            float r, float g, float b, float a, float shear) {
    if (!str) return;
    float cur_x = x;
    float gh = (float)MENU_FONT_H * scale;
    while (*str) {
        int slot; char fb[4];
        int tl = parse_token(str, &slot, fb);
        if (tl) {
            str += tl;
            if (s_icon_tex) {
                float sz = icon_size(scale);
                if (a > 0.5f && !(r == 0.0f && g == 0.0f && b == 0.0f))  /* not in outline passes */
                    queue_icon(cur_x, y + (gh - sz) * 0.5f, sz, slot, a);
                cur_x += sz + 1.0f * scale;
            } else {
                draw_text_shear(cur_x, y, scale, fb, r, g, b, a, shear);
                for (const char* f = fb; *f; f++) cur_x += (float)menu_font_w[*f - 32] * scale + 1.0f * scale;
            }
            continue;
        }
        unsigned char c = (unsigned char)*str++;
        if (c < 32 || c > 126) c = 32;
        int idx = c - 32;
        float gw = (float)menu_font_w[idx] * scale;
        if (s_vert_count + 6 <= 4096) {
            MenuVert* v = &s_verts[s_vert_count];
            float u0 = s_char_u0[idx], v0 = s_char_v0[idx], u1 = s_char_u1[idx], v1 = s_char_v1[idx];
            float x0 = cur_x, x1 = cur_x + gw, y0 = y, y1 = y + gh;
            v[0] = (MenuVert){x0 + shear, y0, u0, v0, r, g, b, a};
            v[1] = (MenuVert){x1 + shear, y0, u1, v0, r, g, b, a};
            v[2] = (MenuVert){x0, y1, u0, v1, r, g, b, a};
            v[3] = (MenuVert){x1 + shear, y0, u1, v0, r, g, b, a};
            v[4] = (MenuVert){x1, y1, u1, v1, r, g, b, a};
            v[5] = (MenuVert){x0, y1, u0, v1, r, g, b, a};
            s_vert_count += 6;
        }
        cur_x += gw + 1.0f * scale;
    }
}

static void draw_text(float x, float y, float scale, const char* str, float r, float g, float b, float a) {
    draw_text_shear(x, y, scale, str, r, g, b, a, 0.0f);
}

/* Visible width of a string at a scale, matching draw_text's advance (glyph
   width + 1 px per char, no trailing gap). */
static float text_width(const char* str, float scale) {
    if (!str) return 0.0f;
    float w = 0.0f;
    while (*str) {
        int slot; char fb[4];
        int tl = parse_token(str, &slot, fb);
        if (tl) {
            str += tl;
            if (s_icon_tex) w += icon_size(scale) + 1.0f * scale;
            else for (const char* f = fb; *f; f++) w += (float)menu_font_w[*f - 32] * scale + 1.0f * scale;
            continue;
        }
        unsigned char c = (unsigned char)*str++;
        if (c < 32 || c > 126) c = 32;
        w += (float)menu_font_w[c - 32] * scale + 1.0f * scale;
    }
    if (w > 0.0f) w -= 1.0f * scale;
    return w;
}

/* The game's italic: ~6 degrees, i.e. +3 px over a 28 px cell at scale 1.0
   (sampled from the game's menus). */
#define ITALIC_SHEAR 3.0f

/* Headers and tab titles: italic plus a 1 px black outline (four offset
   passes under the colour pass). Rows stay single-pass: a 5-pass outline on
   every visible row would blow the 4096-vert budget. */
static void draw_text_title(float x, float y, float scale, const char* str,
                            float r, float g, float b, float a) {
    float shear = ITALIC_SHEAR * scale;
    draw_text_shear(x - 1.0f, y, scale, str, 0.0f, 0.0f, 0.0f, a, shear);
    draw_text_shear(x + 1.0f, y, scale, str, 0.0f, 0.0f, 0.0f, a, shear);
    draw_text_shear(x, y - 1.0f, scale, str, 0.0f, 0.0f, 0.0f, a, shear);
    draw_text_shear(x, y + 1.0f, scale, str, 0.0f, 0.0f, 0.0f, a, shear);
    draw_text_shear(x, y, scale, str, r, g, b, a, shear);
}

static void cleanup_gui_gl_state(void) {
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl_bind_main_vao();
}

/* ---- Overlay look (720p geometry; colours sampled from the game's menus) ---- */
#define COL_MAROON   0.388f, 0.082f, 0.082f  /* #631515: banners, tab bar, highlight, footer bar */
#define COL_YELLOW   0.84f, 0.76f, 0.26f     /* #D6C242: active row, headers, active tab */
#define COL_ORANGE   0.776f, 0.467f, 0.243f  /* #C6773E: active value */
#define COL_ORANGE_D 0.702f, 0.416f, 0.216f  /* #B36A37: inactive value */
#define COL_PANEL    0.06f, 0.09f, 0.06f     /* opaque: the game frame must not bleed through */

/* Row pitch is the one layout constant: everything below derives from it.
   48 px gives the 1.1x italic active row room to breathe; 42 px also works
   (11 rows visible instead of 10). */
#define ROW_PITCH      48.0f
#define ROW_GAP         6.0f                       /* between highlight rects */
#define ROW_RECT_H     (ROW_PITCH - ROW_GAP)
#define ROW_ZONE_TOP  111.0f
#define ROW_ZONE_BOT  591.0f                       /* 9 px above the help box */
#define MAX_VIS       ((int)((ROW_ZONE_BOT - ROW_ZONE_TOP) / ROW_PITCH))
#define ROW_SCALE      0.95f
#define ACTIVE_SCALE   1.1f
#define HEADER_SCALE   1.25f
#define TAB_SCALE      1.25f
#define HELP_Y        600.0f
#define HELP_H         56.0f
#define FOOTER_H       28.0f
#define VALUE_RIGHT    90.0f                       /* value right edge = screen_w - this */

/* y that vertically centres a text cell of scale s in a box at top, height h */
static float centre_y(float top, float h, float s) { return top + (h - (float)MENU_FONT_H * s) * 0.5f; }

static float s_fps = 30.0f;  /* presented frames per second, for the counter and the Debug rows */

void options_menu_render(uint32_t screen_w, uint32_t screen_h) {
    static uint64_t last_time = 0;
    static int frames = 0;
    uint64_t now = now_ns() / 1000ull;
    frames++;
    if (!last_time) last_time = now;
    if (now - last_time >= 500000ull) {
        s_fps = (float)frames * 1000000.0f / (float)(now - last_time);
        frames = 0;
        last_time = now;
    }

    if (!g_open && !options_show_fps()) return;

    init_gui_gl();
    s_icon_tex = gl_icon_texture();
    s_icon_count = 0;

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(MenuVert), (void*)offsetof(MenuVert, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(MenuVert), (void*)offsetof(MenuVert, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(MenuVert), (void*)offsetof(MenuVert, r));

    glViewport(0, 0, screen_w, screen_h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    glUseProgram(s_prog);
    glUniform2f(s_u_res, (float)screen_w, (float)screen_h);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_font_tex);
    glUniform1i(s_u_tex, 0);

    /* FPS Counter overlay if enabled */
    if (options_show_fps() && !g_open) {
        char fps_str[32];
        snprintf(fps_str, sizeof fps_str, "%.1f FPS", s_fps);
        draw_rect(screen_w - 140.0f, 16.0f, 124.0f, 32.0f, 0.05f, 0.05f, 0.05f, 0.75f);
        flush_verts(0);
        draw_text(screen_w - 132.0f, 20.0f, 0.75f, fps_str, 0.3f, 1.0f, 0.4f, 1.0f);
        flush_verts(1);
        cleanup_gui_gl_state();
        return;
    }

    if (!g_open) {
        cleanup_gui_gl_state();
        return;
    }

    const float sw = (float)screen_w, sh = (float)screen_h;
    const float row_w = sw - 150.0f;
    const float panel_bot = sh - 20.0f;

    /* Scroll window over the view (g_row is a view position). */
    int count = s_tab_view_counts[g_tab];
    int max_vis = MAX_VIS;
    int scroll_top = (g_row >= max_vis) ? (g_row - max_vis + 1) : 0;
    int end_vis = scroll_top + max_vis;
    if (end_vis > count) end_vis = count;
    /* Insurance only (no live case under this scroll rule): never let the
       window end on a section header with its rows cut off below. */
    while (end_vis < count && end_vis > scroll_top + 1 && s_tab_views[g_tab][end_vis - 1] < 0 && scroll_top < g_row) {
        scroll_top++;
        end_vis = scroll_top + max_vis;
        if (end_vis > count) end_vis = count;
    }

    /* Tab layout: each title centred in its slot, shrunk if a slot is tight. */
    float tab_spacing = (sw - 160.0f) / NUM_TABS;
    float tab_scale[NUM_TABS], tab_tx[NUM_TABS];
    for (int t = 0; t < NUM_TABS; t++) {
        float s = TAB_SCALE;
        float w = text_width(kTabNames[t], s);
        if (w > tab_spacing - 12.0f) { s *= (tab_spacing - 12.0f) / w; w = text_width(kTabNames[t], s); }
        tab_scale[t] = s;
        tab_tx[t] = 80.0f + t * tab_spacing + (tab_spacing - w) * 0.5f;
    }

    /* ---- Solid shapes pass (mode 0). Every rect that is not a section
       banner is queued here, before any text: the vertex buffer is shared
       and a mode-0 flush would draw pending glyph quads as solid bars. ---- */
    draw_rect(60.0f, 40.0f, sw - 120.0f, panel_bot - 40.0f, COL_PANEL, 1.0f);
    draw_rect(60.0f, 40.0f, sw - 120.0f, 54.0f, COL_MAROON, 1.0f);                     /* tab bar */
    draw_rect(tab_tx[g_tab], 86.0f, text_width(kTabNames[g_tab], tab_scale[g_tab]), 3.0f, COL_YELLOW, 1.0f);
    float hl_top = ROW_ZONE_TOP + (g_row - scroll_top) * ROW_PITCH;
    draw_rect(75.0f, hl_top, row_w, ROW_RECT_H, COL_MAROON, 1.0f);                     /* active row */
    if (count > max_vis) {                                                             /* scrollbar */
        float track_h = max_vis * ROW_PITCH - ROW_GAP;
        float thumb_h = track_h * max_vis / count;
        float thumb_y = ROW_ZONE_TOP + scroll_top * (track_h - thumb_h) / (count - max_vis);
        draw_rect(1208.0f, ROW_ZONE_TOP, 4.0f, track_h, 0.25f, 0.25f, 0.25f, 1.0f);
        draw_rect(1208.0f, thumb_y, 4.0f, thumb_h, COL_YELLOW, 1.0f);
    }
    draw_rect(75.0f, HELP_Y, row_w, HELP_H, 0.06f, 0.10f, 0.07f, 1.0f);              /* help box */
    draw_rect(60.0f, panel_bot - FOOTER_H, sw - 120.0f, FOOTER_H, COL_MAROON, 1.0f);   /* footer bar */
    flush_verts(0);

    /* ---- Text pass (mode 1) ---- */
    for (int t = 0; t < NUM_TABS; t++) {
        float ty = centre_y(40.0f, 46.0f, tab_scale[t]);
        if (t == g_tab) draw_text_title(tab_tx[t], ty, tab_scale[t], kTabNames[t], COL_YELLOW, 1.0f);
        else            draw_text_title(tab_tx[t], ty, tab_scale[t], kTabNames[t], 1.0f, 1.0f, 1.0f, 0.9f);
    }

    MenuRow* rows = s_tab_rows[g_tab];
    const int8_t* view = s_tab_views[g_tab];
    for (int r = scroll_top; r < end_vis; r++) {
        float top = ROW_ZONE_TOP + (r - scroll_top) * ROW_PITCH;
        int entry = view[r];
        if (entry < 0) {
            /* Section header: maroon banner, yellow italic title. Flush
               pending text (mode 1) BEFORE the banner rect -- see above. */
            flush_verts(1);
            draw_rect(75.0f, top, row_w, ROW_RECT_H, COL_MAROON, 1.0f);
            flush_verts(0);
            draw_text_title(90.0f, centre_y(top, ROW_RECT_H, HEADER_SCALE), HEADER_SCALE,
                            header_name(entry), COL_YELLOW, 1.0f);
            flush_verts(1);
            continue;
        }
        int active = (r == g_row);
        /* The game's selected row is yellow, larger and italic. */
        float scale = active ? ACTIVE_SCALE : ROW_SCALE;
        float shear = active ? ITALIC_SHEAR * scale : 0.0f;
        float ty = centre_y(top, ROW_RECT_H, scale);

        /* Controller Map row names are built live (map_name) so they flip
           under Swap exactly like their values (map_label) do. */
        char name_buf[32];
        const char* name_str = rows[entry].name;
        if (g_tab == TAB_CONTROLS && entry >= 3 && entry <= 12) {
            map_name(entry - 2, name_buf, sizeof name_buf);
            name_str = name_buf;
        }
        if (active) draw_text_shear(90.0f, ty, scale, name_str, COL_YELLOW, 1.0f, shear);
        else        draw_text_shear(90.0f, ty, scale, name_str, 0.82f, 0.85f, 0.90f, 1.0f, 0.0f);

        /* Values built live: System actions and SESSION, the Controller Map,
           the Debug RUNTIME rows; everything else from the model. */
        char val_buf[80];
        const char* val_str = NULL;
        if (g_tab == TAB_SYSTEM && entry <= 2) {
            const char* verb = (entry == 0) ? "Save" : (entry == 1) ? "Reset" : "Exit";
            snprintf(val_buf, sizeof val_buf, "[ Press {%c} to %s ]", confirm_letter(), verb);
            val_str = val_buf;
        } else if (g_tab == TAB_SYSTEM && entry == 3) {
#ifdef USM_BUILD_SHA
            snprintf(val_buf, sizeof val_buf, "%s (%s)", USM_BUILD_SHA, USM_BUILD_BRANCH);
#else
            snprintf(val_buf, sizeof val_buf, "%s %s", __DATE__, __TIME__);
#endif
            val_str = val_buf;
        } else if (g_tab == TAB_SYSTEM && entry == 4) {
            /* Selected profiles (settings, not a hardware readback). */
            int ci = s_perf_rows[5].cur, gi = s_perf_rows[4].cur;
            if (ci < 0 || ci >= s_perf_rows[5].num_opts) ci = 0;
            if (gi < 0 || gi >= s_perf_rows[4].num_opts) gi = 0;
            snprintf(val_buf, sizeof val_buf, "%s / %s", s_perf_rows[5].opts[ci], s_perf_rows[4].opts[gi]);
            val_str = val_buf;
        } else if (g_tab == TAB_DEBUG && entry == 3) {
            snprintf(val_buf, sizeof val_buf, "%.1f", s_fps);
            val_str = val_buf;
        } else if (g_tab == TAB_DEBUG && entry == 4) {
            snprintf(val_buf, sizeof val_buf, "%.1f ms", s_fps > 0.1f ? 1000.0f / s_fps : 0.0f);
            val_str = val_buf;
        } else if (g_tab == TAB_CONTROLS && entry >= 3 && entry <= 12) {
            map_label(entry - 2, val_buf, sizeof val_buf);
            val_str = val_buf;
        } else {
            int cur_val = rows[entry].cur;
            if (cur_val >= 0 && cur_val < rows[entry].num_opts) {
                if (rows[entry].num_opts > 1) snprintf(val_buf, sizeof val_buf, "<  %s  >", rows[entry].opts[cur_val]);
                else                          snprintf(val_buf, sizeof val_buf, "%s", rows[entry].opts[cur_val]);
                val_str = val_buf;
            }
        }
        if (val_str) {
            /* Right-aligned at the panel's right inset, like the game. */
            float vx = sw - VALUE_RIGHT - text_width(val_str, scale);
            if (active) draw_text_shear(vx, ty, scale, val_str, COL_ORANGE, 1.0f, shear);
            else        draw_text_shear(vx, ty, scale, val_str, COL_ORANGE_D, 1.0f, 0.0f);
        }
    }

    /* Help line: the status message (save result, two-press prompt, Test
       prompts sample) replaces the help text while it is showing. */
    float help_y = centre_y(HELP_Y, HELP_H, 0.85f);
    if (s_status_msg && now_ns() < s_status_until) {
        draw_text(90.0f, help_y, 0.85f, s_status_msg, 0.3f, 1.0f, 0.4f, 1.0f);
    } else {
        int help_entry = view[g_row];
        if (help_entry >= 0 && rows[help_entry].help)
            draw_text(90.0f, help_y, 0.85f, rows[help_entry].help, 0.85f, 0.90f, 0.95f, 1.0f);
    }

    /* Footer on the maroon bar, built live from the Confirm/Back buttons;
       shrinks rather than run off the panel. */
    {
        char footer_buf[160];
        snprintf(footer_buf, sizeof footer_buf,
                 "{L}/{R}: Switch Tabs  |  D-Pad/Stick: Navigate  |  Left/Right, {%c}: Change Value  |  {%c}: Close Menu",
                 confirm_letter(), back_letter());
        float fs = 0.8f, avail = sw - 180.0f, fw = text_width(footer_buf, fs);
        if (fw > avail) fs *= avail / fw;
        draw_text(90.0f, centre_y(panel_bot - FOOTER_H, FOOTER_H, fs), fs, footer_buf, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    flush_verts(1);

    /* Icons last, in one batch, with the icon atlas bound. */
    if (s_icon_count && s_icon_tex) {
        memcpy(s_verts, s_icon_verts, (size_t)s_icon_count * sizeof(MenuVert));
        s_vert_count = s_icon_count;
        glBindTexture(GL_TEXTURE_2D, s_icon_tex);
        flush_verts(1);
        glBindTexture(GL_TEXTURE_2D, s_font_tex);
    }
    s_icon_count = 0;

    cleanup_gui_gl_state();
}
