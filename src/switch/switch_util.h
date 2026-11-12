/* Helpers shared by the Switch shims. */
#pragma once
#include <stdint.h>

#define GAME_CWD "C:\\USM"

char* gstrdup(const char* s);
int gformat(char* out, uint32_t n, const char* fmt, uint32_t ap);
void map_path(const char* game, char* out, unsigned n);
