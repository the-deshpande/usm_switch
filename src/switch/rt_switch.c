/* Runtime for the translated code on Nintendo Switch (libnx / Horizon OS).
   Provides per-thread Ctx, lookup for f_XXXXXX, host-to-guest calls, shims dispatch, and logging. */
#define RC_GUEST_WRITER
#include <malloc.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "usm_switch.h"

uint8_t* rc_guest;
void (*rc_on_trap)(uint32_t at);

static FILE* g_log = NULL;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;

int log_on(void) {
    return 1;
}

void vlog(const char* fmt, ...) {
    pthread_mutex_lock(&g_log_lock);
    if (!g_log) {
        mkdir("sdmc:/switch", 0777);
        mkdir(USM_DIR, 0777);
        remove(USM_DIR "log_prev.txt");
        rename(USM_DIR "log.txt", USM_DIR "log_prev.txt");
        g_log = fopen(USM_DIR "log.txt", "w");
    }
    if (g_log) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(g_log, fmt, ap);
        va_end(ap);
        fflush(g_log);
    }
    pthread_mutex_unlock(&g_log_lock);
}

void rc_log(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    vlog("%s", buf);
}

void negative_guest(uint32_t a) {
    static int told;
    if (!told) {
        told = 1;
        vlog("warning: allocation below the image (guest address %08x)\n", a);
    }
}

uint32_t g_game_bytes = 0;
void* game_mem(void* p) {
    if (p) __atomic_add_fetch(&g_game_bytes, (uint32_t)malloc_usable_size(p), __ATOMIC_RELAXED);
    return p;
}

void* game_realloc(void* p, uint32_t n) {
    uint32_t old = p ? (uint32_t)malloc_usable_size(p) : 0;
    void* q = realloc(p, n);
    if (q || !n) __atomic_sub_fetch(&g_game_bytes, old, __ATOMIC_RELAXED);
    return game_mem(q);
}

void game_free(void* p) {
    if (p) {
        __atomic_sub_fetch(&g_game_bytes, (uint32_t)malloc_usable_size(p), __ATOMIC_RELAXED);
        free(p);
    }
}

void* guest_alloc(uint32_t n) {
    void* p = memalign(16, n);
    if (!p) {
        vcrash("out of memory (%u bytes)\n", n);
        abort();
    }
    memset(p, 0, n);
    GA(p);
    return p;
}

/* ---- Contexts ---- */
static pthread_key_t g_ctx_key;
static pthread_once_t g_ctx_once = PTHREAD_ONCE_INIT;
static void ctx_key(void) { pthread_key_create(&g_ctx_key, NULL); }

Ctx* ctx_new(uint32_t stack_size) {
    pthread_once(&g_ctx_once, ctx_key);
    Ctx* c = guest_alloc(sizeof(Ctx));
    uint8_t* stack = guest_alloc(stack_size);
    uint8_t* teb = guest_alloc(0x1000);
    uint8_t* peb = guest_alloc(0x1000);
    c->esp = G(stack + stack_size - 64);
    c->fsbase = G(teb);
    ST32(c->fsbase + 0x00, 0xffffffffu);            /* SEH chain: empty */
    ST32(c->fsbase + 0x04, G(stack + stack_size));  /* stack base */
    ST32(c->fsbase + 0x08, G(stack));               /* stack limit */
    ST32(c->fsbase + 0x18, c->fsbase);              /* self */
    ST32(c->fsbase + 0x30, G(peb));
    rc_fldcw(c, 0x27f);                             /* standard control word */
    rc_tls_init(c);
    pthread_setspecific(g_ctx_key, c);
    return c;
}

Ctx* rc_ctx(void) {
    pthread_once(&g_ctx_once, ctx_key);
    Ctx* c = pthread_getspecific(g_ctx_key);
    return c ? c : ctx_new(0x100000);
}

void vcrash(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    vlog("%s", buf);
    mkdir("sdmc:/switch", 0777);
    mkdir(USM_DIR, 0777);
    FILE* f = fopen(USM_DIR "crash.txt", "a");
    if (f) {
        fprintf(f, "%s", buf);
        fflush(f);
        fclose(f);
    }
}

/* ---- Traps ---- */
void rc_trap(Ctx* c, uint32_t at, const char* why) {
    vcrash("trap at %06x: %s\n", at, why);
    shim_log_recent(8);
    if (rc_on_trap) rc_on_trap(at);
    abort();
}

void rc_bad_return(Ctx* c, uint32_t at, uint32_t expected) {
    vlog("bad return from %06x: expected %06x, got %06x\n", at, expected, c->target);
}

/* ---- Function Lookup ---- */
static unsigned index_of(uint32_t a) {
    unsigned lo = 0, hi = rc_count;
    while (lo < hi) {
        unsigned m = (lo + hi) / 2;
        if (rc_addr[m] < a) lo = m + 1;
        else hi = m;
    }
    return (lo < rc_count && rc_addr[lo] == a) ? (lo + 1) : 0;
}

/* 0x597290: Options menu slider update callback (omitted from Ghidra CFG export) */
static void fn_597290(Ctx* restrict c) {
    uint32_t esp = c->esp;
    /* On entry from rc_call, c->esp points to return address */
    uint32_t arg0 = LD32(esp + 4);
    uint32_t arg2 = LD32(esp + 12);

    float pos = LDF32(arg0 + 0x88);
    int32_t count = (int32_t)LD32(arg2 + 0x98);
    int32_t val = (int32_t)floor((double)count * (double)pos + 0.5);

    vlog("fn_597290: slider widget %08x pos=%.3f count=%d -> val=%d (setting %08x)\n",
         arg0, pos, count, val, arg2);

    if (val >= 0 && val < count) {
        ST32(arg2 + 0x94, (uint32_t)val);
        c->eax = (uint32_t)val;
        c->ecx = arg2;
        uint32_t vtbl = LD32(arg2);
        uint32_t target = LD32(vtbl + 4);
        rc_tail(c, target);
        return;
    }
    c->eax = (uint32_t)val;
    c->esp += 4; /* pop return address */
}

/* 0x5972f0: Attaches slider callback to menu option object */
static void fn_5972f0(Ctx* restrict c) {
    uint32_t esp = c->esp;
    uint32_t arg0 = LD32(esp + 4);
    uint32_t this_obj = c->ecx;

    ST32(this_obj + 0x408, arg0);
    ST32(arg0 + 0x48, 0x597290u);
    ST32(arg0 + 0x4c, this_obj);

    uint32_t vtbl = LD32(this_obj);
    uint32_t target = LD32(vtbl + 4);

    c->ecx = this_obj;
    rc_call(c, target, 0x597309u);

    c->esp += 8; /* ret 4: pop ret_addr + 4-byte arg */
}

static uint32_t cache_idx[8192];

__attribute__((hot)) static rc_fn lookup(uint32_t a) {
    if (a == 0x597290u) return fn_597290;
    if (a == 0x5972f0u) return fn_5972f0;
    if (a >= SHIM_BASE) return NULL;
    unsigned h = (a >> 4 ^ a >> 17) & 8191;
    uint32_t k = __atomic_load_n(&cache_idx[h], __ATOMIC_RELAXED);
    if (k && rc_addr[k - 1] == a) return rc_impl[k - 1];
    k = index_of(a);
    if (!k) return NULL;
    __atomic_store_n(&cache_idx[h], k, __ATOMIC_RELAXED);
    return rc_impl[k - 1];
}

void rc_native(Ctx* c, uint32_t target) {
    if (target == 0x8226fa) {
        rc_jmprec* rec_ = rc_setjmp_rec(c, LD32(c->esp));
        rc_setjmp_save(c, rec_);
        c->eax = 0;
        return;
    }
    if (target == 0x597290u) {
        fn_597290(c);
        return;
    }
    if (target == 0x5972f0u) {
        fn_5972f0(c);
        return;
    }
    if (!shim_call(c, target)) rc_trap(c, target, "call to code that is neither translated nor a shim");
}

__attribute__((hot)) void rc_call(Ctx* c, uint32_t target, uint32_t ret) {
    rc_fn f = lookup(target);
    if (!f) {
        rc_native(c, target);
        return;
    }
    c->esp -= 4;
    ST32(c->esp, ret);
    f(c);
}

void rc_tail(Ctx* c, uint32_t target) {
    rc_fn f = lookup(target);
    if (f) {
        f(c);
        return;
    }
    uint32_t ra = LD32(c->esp);
    c->esp += 4;
    rc_native(c, target);
    c->target = ra;
}

void rc_jmp(Ctx* c, uint32_t target, uint32_t at) {
    (void)at;
    rc_tail(c, target);
}

#define SENTINEL 0xfffffff0u

uint32_t guest_call(uint32_t fn, int n, const uint32_t* args) {
    Ctx* c = rc_ctx();
    uint32_t saved[8];
    memcpy(saved, &c->eax, sizeof saved);
    for (int i = n - 1; i >= 0; i--) {
        c->esp -= 4;
        ST32(c->esp, args[i]);
    }
    rc_fn f = lookup(fn);
    if (f) {
        c->esp -= 4;
        ST32(c->esp, SENTINEL);
        f(c);
    } else if (!shim_call(c, fn)) {
        rc_trap(c, fn, "callback to code that is neither translated nor a shim");
    }
    uint32_t r = c->eax;
    memcpy(&c->eax, saved, sizeof saved);
    return r;
}

/* ---- Setjmp / Longjmp ---- */
#define NJMP 32

rc_jmprec* rc_setjmp_rec(Ctx* c, uint32_t jb) {
    if (!c->jmp) c->jmp = guest_alloc(sizeof(rc_jmprec) * NJMP);
    rc_jmprec* free_ = NULL;
    for (int i = 0; i < NJMP; i++) {
        if (c->jmp[i].jb == jb) return &c->jmp[i];
        if (!c->jmp[i].jb && !free_) free_ = &c->jmp[i];
    }
    if (!free_) free_ = &c->jmp[jb % NJMP];
    free_->jb = jb;
    return free_;
}

void rc_setjmp_save(Ctx* c, rc_jmprec* r) {
    memcpy(r->regs, &c->eax, 32);
    r->fs0 = LD32(c->fsbase);
}

int rc_longjmp(uint32_t jb, int v) {
    Ctx* c = rc_ctx();
    if (!c->jmp) return 0;
    for (int i = 0; i < NJMP; i++) {
        rc_jmprec* r = &c->jmp[i];
        if (r->jb != jb || r->regs[4] < c->esp) continue;
        memcpy(&c->eax, r->regs, 32);
        c->eax = v ? (uint32_t)v : 1;
        c->ftop = 0, c->fdepth = 0;
        ST32(c->fsbase, r->fs0);
        __builtin_longjmp(r->buf, 1);
    }
    return 0;
}

/* ---- Clean in-game quit hooks ---- */
void reg_flush(void);
void options_menu_cleanup(void);
void fo_5e99d0(Ctx* c);

static void quit_now(void) {
    vlog("quit_now: flushing registry and exiting cleanly to Horizon OS\n");
    reg_flush();
    options_menu_cleanup();
    svcExitProcess();
}

void f_5e99d0(Ctx* c) {
    /* 57b8b3 sets 0x95c178 on in-game quit; 5ad5bb is the return address from teardown delete */
    if (rc_guest && LD8(0x95c178) && LD32(c->esp) == 0x5ad5bb) {
        quit_now();
    }
    fo_5e99d0(c);
}

void f_4c2a80(Ctx* c) {
    (void)c;
    quit_now();
}

