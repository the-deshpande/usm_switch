/* Shim dispatch and registration for Ultimate Spider-Man on Switch. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include "switch.h"

#define MAX_SLOTS 8192

typedef struct {
    const char* name;
    shim_fn fn;
    int pops;
    uint8_t noted, quiet, kind;
} Slot;

enum { K_D3D = 1, K_DRAW = 2, K_SETSTREAM = 4, K_PURE = 8, K_BUFLOCK = 16, K_RELEASE = 32 };

static int quiet_name(const char* dll, const char* n) {
    if (!strncasecmp(dll, "winmm", 5)) return !strncmp(n, "time", 4);
    if (strncasecmp(dll, "msvcr", 5)) return 0;
    static const char* pre[] = {"is", "to", "str", "_str", "mem", "_CI", "_ftol", "rand", "sqrt", "floor", "ceil", "fabs",
                                "atoi", "atof", "_ismbb", "malloc", "free", "_aligned", "??", "_msize", "realloc", "calloc",
                                "qsort"};
    for (unsigned i = 0; i < sizeof pre / sizeof *pre; i++)
        if (!strncmp(n, pre[i], strlen(pre[i]))) return 1;
    return 0;
}

static Slot g_slot[MAX_SLOTS];
static unsigned g_nslots = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

extern const ShimDef shims_kernel32[], shims_msvcr71[], shims_user32[], shims_d3d9[], shims_dsound[], shims_dinput8[], shims_bink[], shims_misc[];
static const struct { const char* dll; const ShimDef* t; } g_tables[] = {
    {"kernel32.dll", shims_kernel32},
    {"msvcr71.dll", shims_msvcr71},
    {"user32.dll", shims_user32},
    {"d3d9.dll", shims_d3d9},
    {"dsound.dll", shims_dsound},
    {"dinput8.dll", shims_dinput8},
    {"binkw32.dll", shims_bink},
    {NULL, shims_misc},
};

uint32_t shim_new(const char* name, shim_fn fn, int pops) {
    pthread_mutex_lock(&g_lock);
    unsigned k = g_nslots < MAX_SLOTS ? g_nslots++ : MAX_SLOTS - 1;
    g_slot[k] = (Slot){name, fn, pops, 0, 0, 0};
    if (!strncmp(name, "IDirect3D", 9)) g_slot[k].kind = K_D3D;
    if (!strncmp(name, "IDirect3DDevice9::Draw", 22)) g_slot[k].kind |= K_DRAW;
    pthread_mutex_unlock(&g_lock);
    return SHIM_BASE + 4 * k;
}

static shim_fn find_table(const char* dll, const char* name, int* pops) {
    for (unsigned i = 0; i < sizeof g_tables / sizeof *g_tables; i++) {
        if (g_tables[i].dll && strcasecmp(g_tables[i].dll, dll)) continue;
        for (const ShimDef* d = g_tables[i].t; d && d->name; d++) {
            if (!strcmp(d->name, name)) return d->fn;
        }
    }
    return NULL;
}

uint32_t shim_addr(const char* dll, const char* name, int pops) {
    shim_fn fn = find_table(dll, name, &pops);
    pthread_mutex_lock(&g_lock);
    for (unsigned i = 0; i < g_nslots; i++) {
        if (!strcmp(g_slot[i].name, name)) {
            pthread_mutex_unlock(&g_lock);
            return SHIM_BASE + 4 * i;
        }
    }
    unsigned k = g_nslots < MAX_SLOTS ? g_nslots++ : MAX_SLOTS - 1;
    g_slot[k] = (Slot){strdup(name), fn, pops, 0, (uint8_t)quiet_name(dll, name), 0};
    pthread_mutex_unlock(&g_lock);
    return SHIM_BASE + 4 * k;
}

#define REC_N 16
static const char* g_rec[REC_N];
static unsigned g_nrec;

int shim_call(Ctx* c, uint32_t target) {
    if (target < SHIM_BASE) return 0;
    unsigned k = (target - SHIM_BASE) / 4;
    if (k >= g_nslots) return 0;
    Slot* s = &g_slot[k];
    g_rec[g_nrec++ % REC_N] = s->name;
    if (s->fn) {
        s->fn(c);
        c->esp += s->pops;
        return 1;
    }
    if (!s->noted) {
        s->noted = 1;
        vlog("unimplemented shim: %s (pops %d)\n", s->name, s->pops);
    }
    c->eax = 0;
    c->esp += s->pops;
    return 1;
}

void shim_log_recent(int n) {
    vlog("recent shims:\n");
    for (int i = 0; i < n && i < (int)g_nrec; i++) {
        int idx = (int)(g_nrec - 1 - i);
        if (idx >= 0) vlog("  %s\n", g_rec[idx % REC_N]);
    }
}
