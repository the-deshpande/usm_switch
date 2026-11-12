/* Ultimate Spider-Man on Nintendo Switch (libnx / Horizon OS).
   Maps 32-bit guest x86 memory, threads, and Windows/Direct3D shims onto Horizon. */
#pragma once
#include <stdint.h>
#include <string.h>
#include <switch.h>

#include "rt.h"

#define USM_DIR "sdmc:/switch/usm/"

static inline uint32_t G(const void* p) { return p ? (uint32_t)((uintptr_t)p - (uintptr_t)rc_guest) : 0; }
static inline void* H(uint32_t a) { return a ? (void*)RC_P(a) : NULL; }
void negative_guest(uint32_t a);
static inline uint32_t GA(const void* p) { uint32_t a = G(p); if (a >= 0x80000000u) negative_guest(a); return a; }

#define SHIM_BASE 0xfff00000u
#define ARG(i) LD32(c->esp + 4 * (i))
typedef void (*shim_fn)(Ctx* c);

typedef struct { const char* dll; const char* name; int pops; } ImportInfo;
extern const ImportInfo g_imports[];
extern const unsigned g_nimports;

typedef struct { const char* name; shim_fn fn; } ShimDef;
uint32_t shim_addr(const char* dll, const char* name, int pops);
uint32_t shim_new(const char* name, shim_fn fn, int pops);
int shim_call(Ctx* c, uint32_t target);
void shim_log_recent(int n);

uint32_t guest_call(uint32_t fn, int n, const uint32_t* args);
uint32_t crt_data_import(const char* name);
void* crt_file(uint32_t f);
void apc_run(Ctx* c);
void vlog(const char* fmt, ...);
void vcrash(const char* fmt, ...);
int log_on(void);
void* guest_alloc(uint32_t n);

extern uint32_t g_game_bytes;
void* game_mem(void* p);
void* game_realloc(void* p, uint32_t n);
void game_free(void* p);
Ctx* ctx_new(uint32_t stack_size);

char* gstrdup(const char* s);
void fatal(const char* msg);
