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
    {"Resolution mode", 0, 4, {"Native 720p (1280x720) [Crisp]", "1080p Docked (1920x1080)", "Performance 544p (960x544)", "Low 480p (640x480)"}, "Internal 3D rendering resolution"},
    {"Anti-Aliasing (FXAA)", 0, 4, {"Off", "Low (Light)", "Medium (Smooth)", "High (Ultra Smooth)"}, "Removes pixelated edges and jaggies from character and city"},
    {"Rescaling / Sharpening", 0, 4, {"Bilinear", "Bicubic Sharp", "FSR 1.0 (EASU+RCAS)", "FSR Ultra Sharp"}, "Adaptive contrast sharpening & image reconstruction"}
};

static MenuRow s_controls_rows[] = {
    {"Camera Invert X", 0, 2, {"Normal", "Inverted"}, "Horizontal camera rotation"},
    {"Camera Invert Y", 0, 2, {"Normal", "Inverted"}, "Vertical camera pitch"},
    {"Stick Sensitivity", 2, 5, {"0.5x", "0.75x", "1.0x (Normal)", "1.25x", "1.5x"}, "Look stick sensitivity"},
    {"Vibration / Rumble", 1, 2, {"Off", "On"}, "HD Rumble feedback on hits & web swing"},
    {"Swap A/B buttons", 0, 2, {"Off (B: Jump, A: Action)", "On (A: Jump, B: Action)"}, "Swap A and B button functions"},
    {"--- Controller Map ---", 0, 1, {"[ Xbox Layout ]"}, "Read-only gamepad button mapping reference"},
    {"Button 1 (A/B)", 0, 1, {"Jump (Xbox A)"}, "Standard jump, release web swing, menu confirm"},
    {"Button 2 (B/A)", 0, 1, {"Wall Crawl / Web (Xbox B)"}, "Stick to walls, wall crawl, sprint, menu cancel"},
    {"Button 3 (Y)", 0, 1, {"Punch / Light (Xbox X)"}, "Punch, light attack combos"},
    {"Button 4 (X)", 0, 1, {"Kick / Heavy (Xbox Y)"}, "Kick, heavy attack combos"},
    {"Button 5 (L / ZL)", 0, 1, {"Web Zip (LB / LT)"}, "Quick web zip forward"},
    {"Button 6 (R / ZR)", 0, 1, {"Web Swing (RB / RT)"}, "Web swing, shoot web"},
    {"Button 8 (Plus)", 0, 1, {"Pause Game (Start)"}, "Pause menu and in-game options"},
    {"Button 9 (LS Click)", 0, 1, {"Open Map (Back)"}, "In-game city map overview (Left Stick click)"},
    {"Button 10 (RS Click)", 0, 1, {"Center Camera (R3)"}, "Reset camera behind Spider-Man (Right Stick click)"},
    {"Minus (-)", 0, 1, {"SwitchWaker Overlay"}, "Toggle this settings overlay menu (exclusive)"}
};

static MenuRow s_perf_rows[] = {
    {"Framerate Target", 0, 3, {"30 FPS (Native)", "60 FPS (Frame Gen)", "Uncapped"}, "Target display presentation rate"},
    {"Frame Generation", 0, 3, {"Off", "60 FPS AI Interpolation", "Dynamic Adaptive Gen"}, "Synthesize intermediate frames between game ticks without CPU overhead"},
    {"Display sync (VSync)", 1, 2, {"Off", "On (Double-buffered)"}, "Synchronize frame presentation to display refresh"},
    {"FPS Counter", 0, 3, {"Hidden", "Visible (Simple)", "Visible (Detailed FG)"}, "On-screen frame rate overlay showing Real + Generated FPS"},
    {"GPU Performance Profile", 0, 3, {"Stock (307 MHz)", "Boost (384 MHz)", "High (460 MHz)"}, "Official Nintendo GPU clock profile"},
    {"CPU Overclock", 0, 4, {"Stock (1020 MHz)", "Boost (1224 MHz)", "High (1581 MHz)", "Max (1785 MHz)"}, "CPU clock speed (clkrst)"},
    {"Pedestrian & Traffic", 2, 5, {"None", "Very Low", "Medium (Default)", "High", "Full (PC)"}, "City traffic and pedestrians density"}
};

static MenuRow s_qol_rows[] = {
    {"Skip intro movies", 0, 2, {"Off", "On"}, "Automatically bypass splash screens"},
    {"Fast map pause", 0, 2, {"Off", "On"}, "Speed up minimap transition"}
};

static MenuRow s_system_rows[] = {
    {"Save settings to SD", 0, 1, {"[ Press A to Save ]"}, "Save current options to settings.ini"},
    {"Reset to player defaults", 0, 1, {"[ Press A to Reset ]"}, "Restore original factory defaults"},
    {"Quit Game", 0, 1, {"[ Press A to Exit to Home ]"}, "Cleanly restore clocks and exit to Horizon OS"}
};

static MenuRow s_debug_rows[] = {
    {"Wireframe mode", 0, 2, {"Off", "On"}, "Render 3D scene in wireframe"},
    {"Write session log.txt", 0, 2, {"Off", "On"}, "Write detailed diagnostic logs to SD"},
    {"Available Switch RAM", 0, 1, {"~3100 MB free"}, "Total Horizon application memory available"}
};

static MenuRow* s_tab_rows[NUM_TABS] = {
    s_graphics_rows, s_controls_rows, s_perf_rows, s_qol_rows, s_system_rows, s_debug_rows
};

static int s_tab_counts[NUM_TABS] = {
    (int)(sizeof(s_graphics_rows)/sizeof(s_graphics_rows[0])),
    (int)(sizeof(s_controls_rows)/sizeof(s_controls_rows[0])),
    (int)(sizeof(s_perf_rows)/sizeof(s_perf_rows[0])),
    (int)(sizeof(s_qol_rows)/sizeof(s_qol_rows[0])),
    (int)(sizeof(s_system_rows)/sizeof(s_system_rows[0])),
    (int)(sizeof(s_debug_rows)/sizeof(s_debug_rows[0]))
};

static uint32_t s_status_timer = 0;
static const char* s_status_msg = NULL;

int options_menu_is_open(void) { return g_open; }
void options_menu_toggle(void) { g_open = !g_open; }

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
    static const int c[] = {30, 60, 0};
    int idx = s_perf_rows[0].cur;
    if (idx < 0 || idx >= 3) idx = 0;
    return c[idx];
}

int options_get_frame_gen(void) {
    int fg = s_perf_rows[1].cur;
    if (fg == 0 && s_perf_rows[0].cur == 1) {
        return 1;
    }
    return fg;
}

int options_get_vsync(void) {
    int idx = s_perf_rows[2].cur;
    return (idx == 1);
}

int options_show_fps(void) {
    int idx = s_perf_rows[3].cur;
    return (idx > 0);
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

int options_get_cam_invert_x(void) {
    int idx = s_controls_rows[0].cur;
    return (idx == 1);
}

int options_get_cam_invert_y(void) {
    int idx = s_controls_rows[1].cur;
    return (idx == 1);
}

float options_get_sensitivity(void) {
    static const float s[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f};
    int idx = s_controls_rows[2].cur;
    if (idx < 0 || idx >= 5) idx = 2;
    return s[idx];
}

int options_get_swap_ab(void) {
    int idx = s_controls_rows[4].cur;
    return (idx == 1);
}

/* Frame statistics tracking */
static uint64_t s_last_fps_calc = 0;
static float s_calc_total_fps = 30.0f;
static float s_calc_real_fps = 30.0f;
static float s_calc_gen_fps = 0.0f;
static int s_total_frames = 0;
static int s_real_frames = 0;
static int s_gen_frames = 0;

void options_record_real_frame(void) {
    s_total_frames++;
    s_real_frames++;
}

void options_record_gen_frame(void) {
    s_total_frames++;
    s_gen_frames++;
}

float options_get_real_fps(void) { return s_calc_real_fps; }
float options_get_gen_fps(void) { return s_calc_gen_fps; }
float options_get_total_fps(void) { return s_calc_total_fps; }

static void reset_defaults(void) {
    s_graphics_rows[0].cur = 2; /* 120 m (Default) */
    s_graphics_rows[1].cur = 1; /* On */
    s_graphics_rows[2].cur = 1; /* Medium (Default) */
    s_graphics_rows[3].cur = 0; /* Native 720p */
    s_graphics_rows[4].cur = 0; /* FXAA Off */
    s_graphics_rows[5].cur = 0; /* Bilinear */

    s_controls_rows[0].cur = 0; /* Normal */
    s_controls_rows[1].cur = 0; /* Normal */
    s_controls_rows[2].cur = 2; /* 1.0x (Normal) */
    s_controls_rows[3].cur = 1; /* On */
    s_controls_rows[4].cur = 0; /* Off (B: Jump, A: Action) */

    s_perf_rows[0].cur = 0; /* 30 FPS */
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
    s_debug_rows[2].cur = 0;
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
    fprintf(f, "[controls]\ninvert_x=%d\ninvert_y=%d\nsens=%d\nrumble=%d\nswap_ab=%d\n\n",
            s_controls_rows[0].cur, s_controls_rows[1].cur, s_controls_rows[2].cur, s_controls_rows[3].cur, s_controls_rows[4].cur);
    fprintf(f, "[performance]\nfps_cap=%d\nframe_gen=%d\nvsync=%d\nshow_fps=%d\ngpu_profile=%d\ncpu_clock=%d\ndensity=%d\n\n",
            s_perf_rows[0].cur, s_perf_rows[1].cur, s_perf_rows[2].cur, s_perf_rows[3].cur, s_perf_rows[4].cur, s_perf_rows[5].cur, s_perf_rows[6].cur);
    fprintf(f, "[qol]\nskip_intro=%d\nfast_map=%d\n\n",
            s_qol_rows[0].cur, s_qol_rows[1].cur);
    fprintf(f, "[debug]\nwireframe=%d\nlog=%d\n",
            s_debug_rows[0].cur, s_debug_rows[1].cur);
    fclose(f);
    vlog("options: settings saved to " USM_DIR "settings.ini\n");
}

static void load_settings(void) {
    FILE* f = fopen(USM_DIR "settings.ini", "r");
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
        } else if (sscanf(line, "invert_x=%d", &v) == 1) {
            if (v >= 0 && v < s_controls_rows[0].num_opts) s_controls_rows[0].cur = v;
        } else if (sscanf(line, "invert_y=%d", &v) == 1) {
            if (v >= 0 && v < s_controls_rows[1].num_opts) s_controls_rows[1].cur = v;
        } else if (sscanf(line, "sens=%d", &v) == 1) {
            if (v >= 0 && v < s_controls_rows[2].num_opts) s_controls_rows[2].cur = v;
        } else if (sscanf(line, "rumble=%d", &v) == 1) {
            if (v >= 0 && v < s_controls_rows[3].num_opts) s_controls_rows[3].cur = v;
        } else if (sscanf(line, "swap_ab=%d", &v) == 1) {
            if (v >= 0 && v < s_controls_rows[4].num_opts) s_controls_rows[4].cur = v;
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
    options_apply_gpu_profile(s_perf_rows[4].cur);
    options_apply_cpu_clock(s_perf_rows[5].cur);
    init_gui_gl();
}

void options_menu_update(uint64_t kDown, uint64_t kHeld, int32_t lx, int32_t ly) {
    (void)kHeld;
    (void)lx;
    (void)ly;
    if (!g_open) return;

    if (kDown & HidNpadButton_B) {
        g_open = 0;
        return;
    }

    if (kDown & (HidNpadButton_L | HidNpadButton_ZL)) {
        g_tab = (g_tab + NUM_TABS - 1) % NUM_TABS;
        g_row = 0;
    }
    if (kDown & (HidNpadButton_R | HidNpadButton_ZR)) {
        g_tab = (g_tab + 1) % NUM_TABS;
        g_row = 0;
    }

    int count = s_tab_counts[g_tab];
    if (kDown & (HidNpadButton_Up | HidNpadButton_StickLUp)) {
        g_row = (g_row + count - 1) % count;
    }
    if (kDown & (HidNpadButton_Down | HidNpadButton_StickLDown)) {
        g_row = (g_row + 1) % count;
    }

    MenuRow* r = &s_tab_rows[g_tab][g_row];
    if (kDown & (HidNpadButton_Left | HidNpadButton_StickLLeft)) {
        if (r->num_opts > 1) {
            r->cur = (r->cur + r->num_opts - 1) % r->num_opts;
            if (g_tab == TAB_PERF && g_row == 4) options_apply_gpu_profile(r->cur);
            if (g_tab == TAB_PERF && g_row == 5) options_apply_cpu_clock(r->cur);
        }
    }
    if (kDown & (HidNpadButton_Right | HidNpadButton_StickLRight | HidNpadButton_A)) {
        if (g_tab == TAB_SYSTEM && g_row == 0) {
            save_settings();
            s_status_msg = "Settings saved to SD (settings.ini & resolution.txt)";
            s_status_timer = 120;
        } else if (g_tab == TAB_SYSTEM && g_row == 1) {
            remove(USM_DIR "settings.ini");
            reset_defaults();
            options_apply_gpu_profile(s_perf_rows[4].cur);
            options_apply_cpu_clock(s_perf_rows[5].cur);
            s_status_msg = "Player defaults restored";
            s_status_timer = 120;
        } else if (g_tab == TAB_SYSTEM && g_row == 2) {
            vlog("options_menu: Quit Game confirmed by user -> exiting to Horizon OS\n");
            options_menu_cleanup();
            svcExitProcess();
        } else if (r->num_opts > 1) {
            r->cur = (r->cur + 1) % r->num_opts;
            if (g_tab == TAB_PERF && g_row == 4) options_apply_gpu_profile(r->cur);
            if (g_tab == TAB_PERF && g_row == 5) options_apply_cpu_clock(r->cur);
        }
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
        if (cx + w >= 256) { cx = 0; cy += MENU_FONT_H; }
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
        cx += w;
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

static void draw_text(float x, float y, float scale, const char* str, float r, float g, float b, float a) {
    if (!str) return;
    float cur_x = x;
    while (*str) {
        unsigned char c = (unsigned char)*str++;
        if (c < 32 || c > 126) c = 32;
        int idx = c - 32;
        int w = menu_font_w[idx];
        float gw = (float)w * scale;
        float gh = (float)MENU_FONT_H * scale;

        add_quad(cur_x, y, cur_x + gw, y + gh,
                 s_char_u0[idx], s_char_v0[idx], s_char_u1[idx], s_char_v1[idx],
                 r, g, b, a);
        cur_x += gw + 1.0f * scale;
    }
}

static void cleanup_gui_gl_state(void) {
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl_bind_main_vao();
}

void options_menu_render(uint32_t screen_w, uint32_t screen_h) {
    uint64_t now = armTicksToNs(armGetSystemTick()) / 1000ull;
    if (now - s_last_fps_calc >= 500000ull) {
        float dt = (float)(now - s_last_fps_calc) / 1000000.0f;
        if (dt > 0.0f) {
            s_calc_total_fps = (float)s_total_frames / dt;
            s_calc_real_fps = (float)s_real_frames / dt;
            s_calc_gen_fps = (float)s_gen_frames / dt;
        }
        s_total_frames = s_real_frames = s_gen_frames = 0;
        s_last_fps_calc = now;
    }

    if (!g_open && !options_show_fps()) return;

    init_gui_gl();

    glBindVertexArray(s_vao);
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
        char fps_str[48];
        int show_mode = s_perf_rows[3].cur; /* 1 = simple, 2 = detailed */
        if (options_get_frame_gen() > 0 && show_mode == 2) {
            snprintf(fps_str, sizeof fps_str, "%.1f FPS (FG: %.0fR+%.0fG)", s_calc_total_fps, s_calc_real_fps, s_calc_gen_fps);
            draw_rect(screen_w - 240.0f, 16.0f, 224.0f, 32.0f, 0.05f, 0.05f, 0.05f, 0.75f);
            flush_verts(0);
            draw_text(screen_w - 232.0f, 20.0f, 0.75f, fps_str, 0.3f, 1.0f, 0.4f, 1.0f);
        } else {
            snprintf(fps_str, sizeof fps_str, "%.1f FPS", s_calc_total_fps);
            draw_rect(screen_w - 140.0f, 16.0f, 124.0f, 32.0f, 0.05f, 0.05f, 0.05f, 0.75f);
            flush_verts(0);
            draw_text(screen_w - 132.0f, 20.0f, 0.75f, fps_str, 0.3f, 1.0f, 0.4f, 1.0f);
        }
        flush_verts(1);
        cleanup_gui_gl_state();
        return;
    }

    if (!g_open) {
        cleanup_gui_gl_state();
        return;
    }

    /* Solid shapes pass (mode = 0) */
    /* Dark semi-transparent backdrop */
    draw_rect(60.0f, 40.0f, screen_w - 120.0f, screen_h - 80.0f, 0.05f, 0.07f, 0.12f, 0.94f);

    /* Tab header bar */
    draw_rect(60.0f, 40.0f, screen_w - 120.0f, 54.0f, 0.09f, 0.13f, 0.22f, 0.98f);

    /* Active tab highlight */
    float tab_spacing = (screen_w - 160.0f) / NUM_TABS;
    float tab_x = 80.0f + g_tab * tab_spacing;
    draw_rect(tab_x - 6.0f, 44.0f, tab_spacing - 8.0f, 46.0f, 0.18f, 0.38f, 0.72f, 0.95f);

    /* Active row highlight with vertical scrolling */
    int count = s_tab_counts[g_tab];
    int max_vis = 11;
    int scroll_top = (g_row >= max_vis) ? (g_row - max_vis + 1) : 0;
    int end_vis = scroll_top + max_vis;
    if (end_vis > count) end_vis = count;

    float row_y = 120.0f + (g_row - scroll_top) * 42.0f;
    draw_rect(75.0f, row_y - 4.0f, screen_w - 150.0f, 38.0f, 0.16f, 0.32f, 0.60f, 0.65f);

    /* Bottom help box */
    draw_rect(75.0f, screen_h - 110.0f, screen_w - 150.0f, 55.0f, 0.07f, 0.10f, 0.16f, 0.88f);
    flush_verts(0);

    /* Text pass (mode = 1) */
    /* Draw tab titles */
    for (int t = 0; t < NUM_TABS; t++) {
        float tx = 80.0f + t * tab_spacing;
        if (t == g_tab) {
            draw_text(tx, 56.0f, 1.0f, kTabNames[t], 1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            draw_text(tx, 56.0f, 1.0f, kTabNames[t], 0.6f, 0.65f, 0.75f, 0.8f);
        }
    }

    /* Draw visible rows in the current tab */
    MenuRow* rows = s_tab_rows[g_tab];
    for (int r = scroll_top; r < end_vis; r++) {
        float ry = 124.0f + (r - scroll_top) * 42.0f;
        int active = (r == g_row);
        float cr = active ? 1.0f : 0.82f;
        float cg = active ? 1.0f : 0.85f;
        float cb = active ? 1.0f : 0.90f;

        draw_text(90.0f, ry, 0.95f, rows[r].name, cr, cg, cb, 1.0f);

        /* Value / choice */
        int cur_val = rows[r].cur;
        if (cur_val >= 0 && cur_val < rows[r].num_opts) {
            char val_buf[64];
            if (rows[r].num_opts > 1) {
                snprintf(val_buf, sizeof val_buf, "<  %s  >", rows[r].opts[cur_val]);
            } else {
                snprintf(val_buf, sizeof val_buf, "%s", rows[r].opts[cur_val]);
            }
            float vr = active ? 0.35f : 0.70f;
            float vg = active ? 0.85f : 0.75f;
            float vb = active ? 1.00f : 0.85f;
            draw_text(screen_w - 480.0f, ry, 0.95f, val_buf, vr, vg, vb, 1.0f);
        }
    }

    /* Scroll indicators if needed */
    if (scroll_top > 0) {
        draw_text(screen_w / 2 - 20.0f, 105.0f, 0.8f, "^ More ^", 0.5f, 0.8f, 1.0f, 0.7f);
    }
    if (end_vis < count) {
        draw_text(screen_w / 2 - 20.0f, 120.0f + max_vis * 42.0f, 0.8f, "v More v", 0.5f, 0.8f, 1.0f, 0.7f);
    }

    /* Help text */
    if (g_row >= 0 && g_row < count && rows[g_row].help) {
        draw_text(90.0f, screen_h - 96.0f, 0.85f, rows[g_row].help, 0.85f, 0.90f, 0.95f, 0.95f);
    }

    /* Status message */
    if (s_status_timer > 0 && s_status_msg) {
        s_status_timer--;
        draw_text(screen_w - 460.0f, screen_h - 70.0f, 0.85f, s_status_msg, 0.3f, 1.0f, 0.4f, 1.0f);
    }

    /* Controller guide footer */
    draw_text(90.0f, screen_h - 30.0f, 0.75f,
              "L/R, ZL/ZR: Switch Tabs  |  D-Pad/Stick: Navigate  |  Left/Right, A: Change Value  |  B: Close Menu",
              0.55f, 0.60f, 0.70f, 0.9f);

    flush_verts(1);

    cleanup_gui_gl_state();
}
