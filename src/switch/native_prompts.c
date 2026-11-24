/* Switch button prompts for Ultimate Spider-Man.
   Ports the four prompt natives from src/vita/native.c (boomannotjeff/usm_vita) to the
   Switch build, and nothing else from that file: the rest of Vita native.c is Vita
   render-pipeline / perf work that does not apply here.
   tools/recomp/native.txt lists these four addresses; recomp.py emits their translation
   as fo_XXXX and a weak f_XXXX that just calls fo_XXXX, so these strong definitions
   take their place for every caller. The Switch build globs src/switch/*.c, so this
   file is picked up with no CMake change.

   Why this exists: without it the game runs the original PC prompt code. The tutorial
   token loop (callers 0x595664, 0x59567b, 0x595692) takes the first of an action's three
   bindings that has a name, keyboard first. On Switch most actions' 1st slot is unbound
   (the guest Controls table shows 1ST NONE for Punch, Kick, Stick to Walls, ...), so
   the loop gets an empty name and stops there: PRESS "" TO PUNCH. Jump is the
   exception because its 1st slot is SPACE. Returning no name for keyboard/mouse from
   those callers makes the loop fall through to the pad slot, as on Vita.

   Slot order (the game's, from f_820890 / f_7791a0; the atlas in src/switch/glyphs.h
   follows it): slot 0 = game Button 1, 1 = Button 2, 2 = Button 3, 3 = Button 4,
   4 = Button 5, 5 = Button 6. The Switch atlas (Kenney Input Prompts, see
   tools/make_glyphs.py) draws those slots as B, A, Y, X, L, R. Swap A/B is honoured
   here, at prompt-display time, by swapping which icon character Buttons 1/2 return:
   Button 1 (Jump) is B with Swap off and A with Swap on, Button 2 the other way round.
   The atlas slots themselves never move.

   Buttons with no face-button icon are words, as on Vita (SELECT / START / REAR TOUCH):
   Button 8 (Pause) = PLUS, Button 9 (Map) = LS CLICK, Button 10 (Center Camera) =
   RS CLICK. Button 7 is unmapped on Switch (input.c) and falls through to the
   original, which names it "Button 7". Stick directions and the D-pad keep the Vita
   words (LEFT STICK RIGHT, D-PAD UP, ...).

   Scope note: the Venom Escape QTE ("... LEFT CLICK ... RIGHT CLICK ...") is NOT
   rewritten here. Its text names mouse buttons directly and neither its text-table key
   nor a ~token caller for it was identified in this repo, so no native here can reach
   it the way f_81c580 reaches RESUME. On Vita the QTE's mouse buttons are the L/R
   triggers only (src/vita/input.c: the game also binds left click to the zip,
   BlackButton / Button 5, and right click to ThrowWeb / Button 6, so L/R are the
   bumpers' own actions); on Switch those are L/ZL and R/ZR. Punch/Kick (Y/X) are
   NOT bound to the mouse clicks anywhere, so mapping the QTE to Y/X would be wrong.
   Whether alternating L/R escapes it on Switch is a hardware-test question: if the
   QTE polls the BlackButton/ThrowWeb actions it already works; if it polls the mouse
   device directly, the Switch input layer (which forces the mouse buttons to 0)
   would need the Vita mouse mapping restored instead. See the PR description. */
#include <string.h>

#include "options_menu.h"
#include "usm_switch.h"

void fo_820890(Ctx* c);
void fo_7791a0(Ctx* c);
void fo_779570(Ctx* c);
void fo_81c580(Ctx* c);

static inline uint32_t parg(Ctx* c, int i) { return LD32(c->esp + 4 * i); }
static inline void nret(Ctx* c, uint32_t pop) { c->target = LD32(c->esp); c->esp += 4 + pop; }

/* 820890: the display name of a binding (device, code), used by prompts and the Controls menu. On PC a pad shows
   "Button 5", "LX+"; the Switch ones are named after its controls (input.c: buttons 1..10 = Jump, Stick to
   Walls, Punch, Kick, Web Zip, Web Swing, (unused), Pause, Map, Center Camera; left stick on X/Y, right
   stick on Z/Rz, D-pad as the PoV). Keyboard and mouse keep the original. */
void f_820890(Ctx* c) {  /* thiscall (input manager, device, code), returns a string; ret 8 */
    /* Face buttons and L/R are icon characters (f_7791a0), the rest words. Icon characters by game button,
       before the Swap A/B exchange below: 0x15 -> slot 0 (B), 0x16 -> slot 1 (A), 0x17 -> slot 2 (Y),
       0x18 -> slot 3 (X), 0x19 -> slot 4 (L), 0x1a -> slot 5 (R). */
    static const char* const icon_for_button[] = {"\x80", "\x81", "\x8d", "\x8f", "\x90", "\x9d"};
    static const char* const ax[] = {"LEFT STICK RIGHT", "LEFT STICK LEFT", "LEFT STICK DOWN", "LEFT STICK UP",
                                     "RIGHT STICK RIGHT", "RIGHT STICK LEFT", 0, 0, 0, 0, "RIGHT STICK DOWN", "RIGHT STICK UP"};
    static const char* const pov[] = {"D-PAD RIGHT", "D-PAD LEFT", "D-PAD DOWN", "D-PAD UP"};
    uint32_t dev = parg(c, 1), code = parg(c, 2), from = LD32(c->esp);
    const char* s = NULL;
    /* The tutorial token loop (0x5953a3: "~square" -> the name of Punch's binding) takes the first of an action's
       three bindings that has a name: keyboard ones come first, and the Switch's emulated keyboard has no key
       names for the unbound 1st slots (an empty string, "PRESS "" TO PUNCH"). No name for keyboard/mouse
       there, so it falls through to the pad slot. */
    if (dev <= 2 && (from == 0x595664 || from == 0x59567b || from == 0x595692)) {
        c->eax = 0;
        nret(c, 8);
        return;
    }
    if (dev >= 3 && dev <= 12) {
        if (code == 0x15) s = icon_for_button[options_get_swap_ab() ? 1 : 0];  /* Jump: B, or A when swapped */
        else if (code == 0x16) s = icon_for_button[options_get_swap_ab() ? 0 : 1];  /* Stick to Walls: A, or B */
        else if (code >= 0x17 && code <= 0x1a) s = icon_for_button[code - 0x15];
        /* Vita's table only covers 0x15..0x1d (its 0x1d is rear touch, CameraCenter). Switch differs here:
           0x1c Pause, 0x1d Map, 0x1e Center Camera are the Switch's own pad defaults (shims_misc.c). */
        else if (code == 0x1c) s = "PLUS";
        else if (code == 0x1d) s = "LS CLICK";
        else if (code == 0x1e) s = "RS CLICK";
        else if (code >= 1 && code <= 12) s = ax[code - 1];
        else if (code >= 0x11 && code <= 0x14) s = pov[code - 0x11];
    }
    if (!s) {
        fo_820890(c);
        return;
    }
    uint32_t buf = 0x987a20;  /* the original's own 80-byte result buffer for pad buttons */
    memset((void*)RC_P(buf), 0, 80);
    strcpy((char*)RC_P(buf), s);
    c->eax = buf;
    nret(c, 8);
}

/* Button icons in text, like the PS2 version's prompts. 6 characters no English text uses stand for the icons
   (slot k of the glyphs.h atlas: B, A, Y, X, L, R). */
static const uint8_t icon_char[6] = {0x80, 0x81, 0x8d, 0x8f, 0x90, 0x9d};
static int is_icon_char(uint8_t ch) { return memchr(icon_char, ch, sizeof icon_char) != NULL; }

/* 7791a0: makes a font from its descriptor text. Font: +0x24 texture, +0x28 glyphs (7 ints: x, y, glyph w, glyph h,
   offset x, offset y, advance), +0x30 cell height, +0x38 first char, +0x3c count, +0x4c texcoords {u0 v0 u1 v1}.
   Each icon character becomes a square glyph the size of a capital, centred on the capitals, with texcoords at
   u 1000+k, v -1000 (no real texture goes there; gl.c icon_quad swaps in the icon atlas). The tables grow if the font stops before 0x9d. */
void f_7791a0(Ctx* c) {  /* cdecl (name, descriptor text), returns the font */
    fo_7791a0(c);
    uint32_t f = c->eax;
    if (!f) return;
    uint32_t first = LD32(f + 0x38), num = LD32(f + 0x3c), gl = LD32(f + 0x28), uv = LD32(f + 0x4c);
    if (!gl || !uv || first > 'A' || first + num <= 'A') return;
    if (first + num <= 0x9d) {
        uint32_t n = 0x9e - first, g2 = G(guest_alloc(n * 0x1c)), u2 = G(guest_alloc(n * 16));
        memcpy(H(g2), H(gl), num * 0x1c), memcpy(H(u2), H(uv), num * 16);
        for (uint32_t i = num; i < n; i++)  /* new chars draw as a space, as the renderer did with them before */
            memcpy(H(g2 + i * 0x1c), H(gl + (' ' - first) * 0x1c), 0x1c), memcpy(H(u2 + i * 16), H(uv + (' ' - first) * 16), 16);
        ST32(f + 0x28, gl = g2), ST32(f + 0x4c, uv = u2), ST32(f + 0x3c, num = n);
    }
    uint32_t a = gl + ('A' - first) * 0x1c;
    int32_t ay = (int32_t)LD32(a + 20), ah = (int32_t)LD32(a + 12), s = ah * 5 / 4;
    for (int k = 0; k < 6; k++) {
        uint32_t g = gl + (icon_char[k] - first) * 0x1c, t = uv + (icon_char[k] - first) * 16;
        ST32(g + 8, (uint32_t)s), ST32(g + 12, (uint32_t)s), ST32(g + 16, 0), ST32(g + 20, (uint32_t)(ay + ah / 2 - s / 2));
        ST32(g + 24, (uint32_t)(s - s / 16));  /* the icon's disc has a transparent edge: this spaces it like a letter */
        STF32(t, 1000.f + k), STF32(t + 4, -1000.f), STF32(t + 8, 1001.f + k), STF32(t + 12, -999.f);
    }
}

/* 779570: lays out a text string for drawing. Prompt texts quote their button token (PRESS "~square" TO PUNCH); with an
   icon the quotes go, as on the PS2. The layout keeps pointers into the string until the frame is drawn, so the
   unquoted copy lives in a small cache keyed by the original text (the same strings come back every frame). */
void f_779570(Ctx* c) {  /* cdecl (font, out, x, y, sx, sy, color, text, count) */
    uint32_t sp = c->esp, text = parg(c, 8);
    const uint8_t* s = (const uint8_t*)H(text);
    int has = 0;
    for (const uint8_t* p = s; p && *p; p++)
        if (*p == '"' && (is_icon_char(p[1]) || (p > s && is_icon_char(p[-1])))) { has = 1; break; }
    if (!has || strlen((const char*)s) >= 256) { fo_779570(c); return; }
    static struct { char key[256]; uint32_t copy; } cache[32];
    static unsigned next;
    int k = -1;
    for (int i = 0; i < 32; i++)
        if (cache[i].copy && !strcmp(cache[i].key, (const char*)s)) { k = i; break; }
    if (k < 0) {
        k = (int)(next++ % 32);
        strcpy(cache[k].key, (const char*)s);
        if (!cache[k].copy) cache[k].copy = G(guest_alloc(256));
        char* o = (char*)H(cache[k].copy);
        for (const uint8_t* p = s; *p; p++)
            if (!(*p == '"' && (is_icon_char(p[1]) || (p > s && is_icon_char(p[-1]))))) *o++ = (char)*p;
        *o = 0;
    }
    ST32(sp + 4 * 8, cache[k].copy);
    fo_779570(c);
    ST32(sp + 4 * 8, text);  /* the caller's argument slot as it was */
}

/* 81c580: the PC text table lookup (table, key) -> string. RESUME is hardcoded as PRESS "SPACE" TO RESUME (Data/
   usm_lte.usm); on the Switch, Plus pauses and resumes. */
void f_81c580(Ctx* c) {  /* cdecl */
    static uint32_t resume;
    const char* key = (const char*)RC_P(parg(c, 2));
    if (strcmp(key, "RESUME") == 0) {
        if (!resume) resume = G(guest_alloc(32)), strcpy((char*)RC_P(resume), "PRESS \"PLUS\" TO RESUME");
        c->eax = resume;
        nret(c, 0);
        return;
    }
    fo_81c580(c);
}
