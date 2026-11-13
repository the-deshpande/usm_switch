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
    {"Resolution mode", 0, 2, {"720p Display (960x544 base)", "1080p Enhanced (WIP)"}, "Internal rendering target resolution"}
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
    {"Frame rate cap", 1, 3, {"20 FPS", "30 FPS (Stock)", "60 FPS (Fast)"}, "Target game update rate"},
    {"Display sync (VSync)", 1, 2, {"Off", "On (Double-buffered)"}, "Synchronize frame presentation"},
    {"FPS Counter", 0, 2, {"Hidden", "Visible"}, "On-screen frame rate overlay"},
    {"Pedestrian & Traffic", 2, 5, {"None", "Very Low", "Medium (Default)", "High", "Full (PC)"}, "City traffic and pedestrians density"}
};

static MenuRow s_qol_rows[] = {
    {"Skip intro movies", 0, 2, {"Off", "On"}, "Automatically bypass splash screens"},
    {"Fast map pause", 0, 2, {"Off", "On"}, "Speed up minimap transition"}
};

static MenuRow s_system_rows[] = {
    {"Save settings to SD", 0, 1, {"[ Press A to Save ]"}, "Save current options to settings.ini"},
    {"Reset to player defaults", 0, 1, {"[ Press A to Reset ]"}, "Restore original factory defaults"}
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
    int idx = s_perf_rows[3].cur;
    if (idx < 0 || idx >= 5) idx = 2;
    return d[idx];
}

float options_get_view_distance(void) {
    static const float v[] = {80.0f, 100.0f, 120.0f, 150.0f, 200.0f, 1000.0f};
    int idx = s_graphics_rows[0].cur;
    if (idx < 0 || idx >= 6) idx = 2;
    return v[idx];
}

int options_get_fps_cap(void) {
    static const int c[] = {20, 30, 60};
    int idx = s_perf_rows[0].cur;
    if (idx < 0 || idx >= 3) idx = 1;
    return c[idx];
}

int options_get_vsync(void) {
    int idx = s_perf_rows[1].cur;
    return (idx == 1);
}

int options_show_fps(void) {
    int idx = s_perf_rows[2].cur;
    return (idx == 1);
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

static void reset_defaults(void) {
    s_graphics_rows[0].cur = 2; /* 120 m (Default) */
    s_graphics_rows[1].cur = 1; /* On */
    s_graphics_rows[2].cur = 1; /* Medium (Default) */
    s_graphics_rows[3].cur = 0; /* Native 720p */

    s_controls_rows[0].cur = 0; /* Normal */
    s_controls_rows[1].cur = 0; /* Normal */
    s_controls_rows[2].cur = 2; /* 1.0x (Normal) */
    s_controls_rows[3].cur = 1; /* On */
    s_controls_rows[4].cur = 0; /* Off (B: Jump, A: Action) */

    s_perf_rows[0].cur = 1; /* 30 FPS (Stock) */
    s_perf_rows[1].cur = 1; /* On (Double-buffered) */
    s_perf_rows[2].cur = 0; /* Hidden */
    s_perf_rows[3].cur = 2; /* Medium (Default) */

    s_qol_rows[0].cur = 0; /* Off */
    s_qol_rows[1].cur = 0; /* Off */

    s_system_rows[0].cur = 0;
    s_system_rows[1].cur = 0;

    s_debug_rows[0].cur = 0; /* Off */
    s_debug_rows[1].cur = 0; /* Off */
    s_debug_rows[2].cur = 0;
}

static void save_settings(void) {
    FILE* f = fopen(USM_DIR "settings.ini", "w");
    if (!f) return;
    fprintf(f, "[graphics]\nview_dist=%d\nfog=%d\nparticles=%d\nres=%d\n\n",
            s_graphics_rows[0].cur, s_graphics_rows[1].cur, s_graphics_rows[2].cur, s_graphics_rows[3].cur);
    fprintf(f, "[controls]\ninvert_x=%d\ninvert_y=%d\nsens=%d\nrumble=%d\nswap_ab=%d\n\n",
            s_controls_rows[0].cur, s_controls_rows[1].cur, s_controls_rows[2].cur, s_controls_rows[3].cur, s_controls_rows[4].cur);
    fprintf(f, "[performance]\nfps_cap=%d\nvsync=%d\nshow_fps=%d\ndensity=%d\n\n",
            s_perf_rows[0].cur, s_perf_rows[1].cur, s_perf_rows[2].cur, s_perf_rows[3].cur);
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
        } else if (sscanf(line, "vsync=%d", &v) == 1) {
            if (v >= 0 && v < s_perf_rows[1].num_opts) s_perf_rows[1].cur = v;
        } else if (sscanf(line, "show_fps=%d", &v) == 1) {
            if (v >= 0 && v < s_perf_rows[2].num_opts) s_perf_rows[2].cur = v;
        } else if (sscanf(line, "density=%d", &v) == 1) {
            if (v >= 0 && v < s_perf_rows[3].num_opts) s_perf_rows[3].cur = v;
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
        }
    }
    if (kDown & (HidNpadButton_Right | HidNpadButton_StickLRight | HidNpadButton_A)) {
        if (g_tab == TAB_SYSTEM && g_row == 0) {
            save_settings();
            s_status_msg = "Settings saved to SD (settings.ini)";
            s_status_timer = 120;
        } else if (g_tab == TAB_SYSTEM && g_row == 1) {
            remove(USM_DIR "settings.ini");
            reset_defaults();
            s_status_msg = "Player defaults restored";
            s_status_timer = 120;
        } else if (r->num_opts > 1) {
            r->cur = (r->cur + 1) % r->num_opts;
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
    glBindAttribLocation(s_prog, 0, "a_pos");
    glBindAttribLocation(s_prog, 1, "a_uv");
    glBindAttribLocation(s_prog, 2, "a_col");
    glLinkProgram(s_prog);
    GLint linked = 0;
    glGetProgramiv(s_prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        char buf[512];
        glGetProgramInfoLog(s_prog, sizeof buf, NULL, buf);
        vlog("options_menu: program link failed: %s\n", buf);
    }
    glDeleteShader(v);
    glDeleteShader(f);

    s_u_res = glGetUniformLocation(s_prog, "u_res");
    s_u_tex = glGetUniformLocation(s_prog, "u_tex");
    s_u_mode = glGetUniformLocation(s_prog, "u_mode");

    glGenVertexArrays(1, &s_vao);
    glBindVertexArray(s_vao);
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(MenuVert), (void*)offsetof(MenuVert, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(MenuVert), (void*)offsetof(MenuVert, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(MenuVert), (void*)offsetof(MenuVert, r));

    /* Restore main VAO */
    gl_bind_main_vao();
}

static void flush_verts(int mode) {
    if (s_vert_count == 0) return;
    glUniform1i(s_u_mode, mode);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, s_vert_count * sizeof(MenuVert), s_verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, s_vert_count);
    s_vert_count = 0;
}

static void push_quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                      float r, float g, float b, float a) {
    if (s_vert_count + 6 > 4096) return;
    MenuVert* v = &s_verts[s_vert_count];
    v[0] = (MenuVert){x, y, u0, v0, r, g, b, a};
    v[1] = (MenuVert){x + w, y, u1, v0, r, g, b, a};
    v[2] = (MenuVert){x + w, y + h, u1, v1, r, g, b, a};

    v[3] = (MenuVert){x, y, u0, v0, r, g, b, a};
    v[4] = (MenuVert){x + w, y + h, u1, v1, r, g, b, a};
    v[5] = (MenuVert){x, y + h, u0, v1, r, g, b, a};
    s_vert_count += 6;
}

static void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    push_quad(x, y, w, h, 0, 0, 0, 0, r, g, b, a);
}

static float draw_text(float x, float y, float scale, const char* str, float r, float g, float b, float a) {
    float cur_x = x;
    for (const char* p = str; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 32 || c > 126) continue;
        int idx = c - 32;
        int cw = menu_font_w[idx];
        float gw = cw * scale;
        float gh = MENU_FONT_H * scale;
        push_quad(cur_x, y, gw, gh, s_char_u0[idx], s_char_v0[idx], s_char_u1[idx], s_char_v1[idx], r, g, b, a);
        cur_x += gw + 2.0f * scale;
    }
    return cur_x - x;
}

static void restore_gl_state(GLint prev_prog, GLint prev_vbo, GLint prev_active_tex, GLint prev_tex,
                             const GLint* prev_vp,
                             GLboolean prev_blend, GLint prev_blend_src_rgb, GLint prev_blend_dst_rgb,
                             GLint prev_blend_src_a, GLint prev_blend_dst_a,
                             GLint prev_blend_eq_rgb, GLint prev_blend_eq_a,
                             GLboolean prev_depth, GLboolean prev_cull, GLboolean prev_scissor) {
    if (prev_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFuncSeparate(prev_blend_src_rgb, prev_blend_dst_rgb, prev_blend_src_a, prev_blend_dst_a);
    glBlendEquationSeparate(prev_blend_eq_rgb, prev_blend_eq_a);
    if (prev_depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (prev_cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (prev_scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
    glUseProgram(prev_prog);
    glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prev_tex);
    glActiveTexture(prev_active_tex);
    gl_bind_main_vao();
}

void options_menu_render(uint32_t screen_w, uint32_t screen_h) {
    /* If FPS counter enabled, calculate and draw it even when menu is closed */
    static uint64_t last_time = 0;
    static float fps = 30.0f;
    static int frames = 0;
    uint64_t now = armTicksToNs(armGetSystemTick()) / 1000ull;
    frames++;
    if (now - last_time >= 500000ull) {
        fps = (float)frames * 1000000.0f / (float)(now - last_time);
        frames = 0;
        last_time = now;
    }

    if (!g_open && !options_show_fps()) return;

    init_gui_gl();

    /* Save comprehensive GL state */
    GLint prev_prog = 0, prev_vbo = 0, prev_active_tex = GL_TEXTURE0, prev_tex = 0;
    GLint prev_vp[4] = {0, 0, (GLint)screen_w, (GLint)screen_h};
    GLint prev_blend_src_rgb = GL_ONE, prev_blend_dst_rgb = GL_ZERO;
    GLint prev_blend_src_a = GL_ONE, prev_blend_dst_a = GL_ZERO;
    GLint prev_blend_eq_rgb = GL_FUNC_ADD, prev_blend_eq_a = GL_FUNC_ADD;

    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_prog);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_vbo);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active_tex);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
    glGetIntegerv(GL_VIEWPORT, prev_vp);
    glGetIntegerv(GL_BLEND_SRC_RGB, &prev_blend_src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &prev_blend_dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &prev_blend_src_a);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &prev_blend_dst_a);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &prev_blend_eq_rgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &prev_blend_eq_a);
    GLboolean prev_blend = glIsEnabled(GL_BLEND);
    GLboolean prev_depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean prev_cull = glIsEnabled(GL_CULL_FACE);
    GLboolean prev_scissor = glIsEnabled(GL_SCISSOR_TEST);

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
        char fps_str[32];
        snprintf(fps_str, sizeof fps_str, "%.1f FPS", fps);
        draw_rect(screen_w - 140.0f, 16.0f, 124.0f, 32.0f, 0.05f, 0.05f, 0.05f, 0.75f);
        flush_verts(0);
        draw_text(screen_w - 132.0f, 20.0f, 0.75f, fps_str, 0.3f, 1.0f, 0.4f, 1.0f);
        flush_verts(1);
        restore_gl_state(prev_prog, prev_vbo, prev_active_tex, prev_tex, prev_vp,
                         prev_blend, prev_blend_src_rgb, prev_blend_dst_rgb,
                         prev_blend_src_a, prev_blend_dst_a,
                         prev_blend_eq_rgb, prev_blend_eq_a,
                         prev_depth, prev_cull, prev_scissor);
        return;
    }

    if (!g_open) {
        restore_gl_state(prev_prog, prev_vbo, prev_active_tex, prev_tex, prev_vp,
                         prev_blend, prev_blend_src_rgb, prev_blend_dst_rgb,
                         prev_blend_src_a, prev_blend_dst_a,
                         prev_blend_eq_rgb, prev_blend_eq_a,
                         prev_depth, prev_cull, prev_scissor);
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
    /* Draw Tab titles */
    for (int t = 0; t < NUM_TABS; t++) {
        float tx = 80.0f + t * tab_spacing + 10.0f;
        if (t == g_tab) {
            draw_text(tx, 54.0f, 0.85f, kTabNames[t], 1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            draw_text(tx, 54.0f, 0.85f, kTabNames[t], 0.60f, 0.68f, 0.80f, 0.80f);
        }
    }

    /* Draw Options for current tab */
    for (int i = scroll_top; i < end_vis; i++) {
        MenuRow* r = &s_tab_rows[g_tab][i];
        float ry = 124.0f + (i - scroll_top) * 42.0f;
        if (i == g_row) {
            draw_text(100.0f, ry, 0.85f, r->name, 1.0f, 1.0f, 0.85f, 1.0f);
            char val[64];
            if (r->num_opts > 1) {
                snprintf(val, sizeof val, "< %s >", r->opts[r->cur]);
            } else {
                snprintf(val, sizeof val, "[ %s ]", r->opts[r->cur]);
            }
            draw_text(screen_w - 480.0f, ry, 0.85f, val, 0.40f, 0.90f, 1.0f, 1.0f);
        } else {
            draw_text(100.0f, ry, 0.85f, r->name, 0.85f, 0.85f, 0.85f, 0.95f);
            draw_text(screen_w - 465.0f, ry, 0.85f, r->opts[r->cur], 0.70f, 0.75f, 0.80f, 0.90f);
        }
    }

    /* Scroll indicators */
    if (scroll_top > 0) {
        draw_text(screen_w - 150.0f, 124.0f, 0.75f, "^ More", 0.40f, 0.85f, 1.0f, 0.90f);
    }
    if (end_vis < count) {
        draw_text(screen_w - 150.0f, 124.0f + (max_vis - 1) * 42.0f, 0.75f, "v More", 0.40f, 0.85f, 1.0f, 0.90f);
    }

    /* Bottom help text / status message */
    MenuRow* sel = &s_tab_rows[g_tab][g_row];
    if (s_status_timer > 0) {
        s_status_timer--;
        draw_text(95.0f, screen_h - 104.0f, 0.75f, s_status_msg ? s_status_msg : "", 0.30f, 1.0f, 0.40f, 1.0f);
    } else {
        draw_text(95.0f, screen_h - 104.0f, 0.75f, sel->help, 0.90f, 0.90f, 0.60f, 1.0f);
    }

    /* Quick controls guide */
    draw_text(95.0f, screen_h - 78.0f, 0.70f,
              "[L/R] Switch Tab   [D-Pad] Navigate   [A / Left-Right] Adjust   [B] Close",
              0.55f, 0.65f, 0.75f, 0.90f);

    flush_verts(1);

    restore_gl_state(prev_prog, prev_vbo, prev_active_tex, prev_tex, prev_vp,
                     prev_blend, prev_blend_src_rgb, prev_blend_dst_rgb,
                     prev_blend_src_a, prev_blend_dst_a,
                     prev_blend_eq_rgb, prev_blend_eq_a,
                     prev_depth, prev_cull, prev_scissor);
}
