#define _POSIX_C_SOURCE 200112L
#include <stdint.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <pixman.h>
#include <fcft/fcft.h>

#define UX(which) state->fonts.which->max_advance.x
#define UY(which) state->fonts.which->max_advance.y
#define DIMS state->dims

typedef struct {
	pixman_color_t color;
	bool bg;
	char *start;
} Color;

uint32_t
draw_text_with_bg(char *text,
	  uint32_t x,
	  uint32_t y,
	  pixman_image_t *foreground,
	  pixman_image_t *background,
	  pixman_color_t *fg_color,
	  pixman_color_t *bg_color,
	  uint32_t max_x,
	  uint32_t buf_height,
	  uint32_t padding,
	  Color *colors,
	  uint32_t colors_l, struct fcft_font *font);

uint32_t
draw_text(char *text,
	  uint32_t x,
	  uint32_t y,
	  pixman_image_t *foreground,
	  pixman_color_t *fg_color,
	  uint32_t max_x,
	  uint32_t buf_height,
	  uint32_t padding,
	  Color *colors,
	  uint32_t colors_l, struct fcft_font *font);
