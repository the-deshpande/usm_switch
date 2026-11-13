#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void gl_bind_main_vao(void);

void options_menu_init(void);
void options_menu_cleanup(void);
int options_menu_is_open(void);
void options_menu_toggle(void);
void options_menu_update(uint64_t kDown, uint64_t kHeld, int32_t lx, int32_t ly);
void options_menu_render(uint32_t screen_w, uint32_t screen_h);
void input_poll(void);

/* Settings queries */
float options_get_density(void);
float options_get_view_distance(void);
int options_get_fps_cap(void);
int options_get_vsync(void);
int options_show_fps(void);
int options_get_wireframe(void);
int options_get_swap_ab(void);
int options_get_cam_invert_x(void);
int options_get_cam_invert_y(void);
float options_get_sensitivity(void);

/* Graphics post-processing and frame generation */
int options_get_frame_gen(void);
int options_get_fxaa(void);
int options_get_sharpen(void);

#ifdef __cplusplus
}
#endif
