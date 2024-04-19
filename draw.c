#define _POSIX_C_SOURCE 200112L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include "draw.h"
#include "shm.h"
#include <xkbcommon/xkbcommon.h>
#include <pixman.h>
#include <fcft/fcft.h>
#include "draw_help.h"
#include "config.h"

static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
	/* Sent by the compositor when it's no longer using this buffer */
	wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
	.release = wl_buffer_release,
};
uint32_t draw_examples(pixman_image_t *fg, pixman_image_t *bg,
		       struct client_state *state, uint32_t y,
		       struct example examples[], short examples_num)
{
	uint32_t x;
	for (int i = 0; i < examples_num; i++) {
		x = draw_text_with_bg(examples[i].src, DIMS.example_x,
				      DIMS.example_y + y, fg, bg, &body_color,
				      NULL, 10000, UY(body), 0, NULL, 0,
				      state->fonts.body);
		x = draw_text_with_bg(examples[i].dst,
				      DIMS.example_x_mid, DIMS.example_y + y,
				      fg, bg, &body_color, NULL, 10000,
				      UY(body), 0, NULL, 0, state->fonts.body);
		y += UY(body);
	}
	return y;
}
void draw_results(pixman_image_t *fg, pixman_image_t *bg,
		  struct client_state *state, struct result results[], int result_num)
{
	int fin = 0;
	int y = 0;
	for (int i = 0; i < result_num; i++) {
		fin = draw_text_with_bg(results[i].text, DIMS.result_x,
					DIMS.result_y + y, fg, bg, &h1_color,
					NULL, 10000, 0, 0, NULL, 0,
					state->fonts.h1);

		fin = draw_text_with_bg(results[i].pos, fin + UX(h1),
					DIMS.result_y + y + UX(h1) / 2, fg, bg,
					&body_color, NULL, 10000, UY(h1), 0,
					NULL, 0, state->fonts.body);
		for (int j = 0; j < results[i].translations_num; j++) {
			fin = draw_text_with_bg(results[i].translations[j].text,
						DIMS.translation_x,
						DIMS.translation_y + y, fg, bg,
						&h2_color, NULL, 10000, 0, 0,
						NULL, 0, state->fonts.h2);
			fin = draw_text_with_bg(
				results[i].translations[j].pos, fin + UX(h2),
				DIMS.translation_y + UX(h2) / 2 + y, fg, bg,
				&body_color, NULL, 10000, 0, 0, NULL, 0,
				state->fonts.body);
			y = draw_examples(fg, bg, state, y,
					  results[i].translations[j].examples, results[i].translations[j].examples_num);
                        y+=UY(h2);
		}
                y+=UY(h1);
	}
}
void draw_search(pixman_image_t *l1, 
		 struct client_state *state)
{

	pixman_image_t *l2 =
		pixman_image_create_bits(PIXMAN_a8r8g8b8, state->width,
					 UY(body), NULL, state->width * 4);
	draw_text_with_bg(state->query.text, 0, 0, l2, l1,
			  &body_color, &searchbar_bg_color, 10000, UY(body), 100, NULL, 0,
			  state->fonts.body);

        pixman_image_composite(PIXMAN_OP_OVER, l2, NULL, l1, 0, 0, 0, 0, UX(body), 0, state->width, UY(body));
 
}
struct wl_buffer *draw_frame(struct client_state *state)
{
	const int width = state->width, height = state->height;
	int stride = width * 4;
	int size = stride * height;

	int fd = allocate_shm_file(size);
	if (fd == -1) {
		return NULL;
	}

	uint32_t *data =
		mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(
		pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	pixman_image_t *final =
		pixman_image_create_bits(PIXMAN_a8r8g8b8, state->width,
					 state->height, data, state->width * 4);
	pixman_image_t *l1 =
		pixman_image_create_bits(PIXMAN_a8r8g8b8, state->width,
					 state->height, NULL, state->width * 4);
	pixman_image_t *rest_bg = pixman_image_create_solid_fill(&bg_color);
        pixman_image_composite(PIXMAN_OP_OVER, rest_bg, NULL, l1, 0, 0, 0, 0, 0, 0, state->width, state->height);


	draw_results(l1, l1, state, (*state->results), state->results_num);
	draw_search(l1, state);


	pixman_image_composite32(PIXMAN_OP_OVER, l1, NULL, final, 0, 0, 0, 0, 0,
				 0, state->width, state->height);
	pixman_image_unref(l1);
	pixman_image_unref(final);

	munmap(data, size);
	wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
	return buffer;
}
