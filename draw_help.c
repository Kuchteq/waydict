// Stolen from dwlb.c
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
#include <xkbcommon/xkbcommon.h>
#include <pixman.h>
#include <fcft/fcft.h>
#include "utf8.h"
#include "draw_help.h"
uint32_t draw_text_with_bg(char *text, uint32_t x, uint32_t y,
			   pixman_image_t *foreground,
			   pixman_image_t *background, pixman_color_t *fg_color,
			   pixman_color_t *bg_color, uint32_t max_x,
			   uint32_t buf_height, uint32_t padding, Color *colors,
			   uint32_t colors_l, struct fcft_font *font)
{
	if (!text || !*text || !max_x)
		return x;

	uint32_t ix = x, nx;

	if ((nx = x + padding) + padding >= max_x)
		return x;
	x = nx;

	bool draw_fg = foreground && fg_color;
	bool draw_bg = background && bg_color;

	pixman_image_t *fg_fill;
	pixman_color_t *cur_bg_color;
	if (draw_fg)
		fg_fill = pixman_image_create_solid_fill(fg_color);
	if (draw_bg)
		cur_bg_color = bg_color;

	uint32_t color_ind = 0, codepoint, utf_state = UTF8_ACCEPT, last_cp = 0;
	for (char *p = text; *p; p++) {
		/* Check for new colors */
		if (utf_state == UTF8_ACCEPT && colors &&
		    (draw_fg || draw_bg)) {
			while (color_ind < colors_l &&
			       p == colors[color_ind].start) {
				if (colors[color_ind].bg) {
					if (draw_bg)
						cur_bg_color =
							&colors[color_ind].color;
				} else if (draw_fg) {
					pixman_image_unref(fg_fill);
					fg_fill = pixman_image_create_solid_fill(
						&colors[color_ind].color);
				}
				color_ind++;
			}
		}

		/* Returns nonzero if more bytes are needed */
		if (utf8decode(&utf_state, &codepoint, *p))
			continue;

		/* Turn off subpixel rendering, which complicates things when
		 * mixed with alpha channels */
		const struct fcft_glyph *glyph = fcft_rasterize_char_utf32(
			font, codepoint, FCFT_SUBPIXEL_NONE);
		if (!glyph)
			continue;

		/* Adjust x position based on kerning with previous glyph */
		long kern = 0;
		if (last_cp)
			fcft_kerning(font, last_cp, codepoint, &kern, NULL);
		if ((nx = x + kern + glyph->advance.x) + padding > max_x)
			break;
		last_cp = codepoint;
		x += kern;

		if (draw_fg) {
			/* Detect and handle pre-rendered glyphs (e.g. emoji) */
			if (pixman_image_get_format(glyph->pix) ==
			    PIXMAN_a8r8g8b8) {
				/* Only the alpha channel of the mask is used, so we can
				 * use fgfill here to blend prerendered glyphs with the
				 * same opacity */
				pixman_image_composite32(
					PIXMAN_OP_OVER, glyph->pix, fg_fill,
					foreground, 0, 0, 0, 0, x + glyph->x,
					y + font->ascent - glyph->y, glyph->width,
					glyph->height);
			} else {
				/* Applying the foreground color here would mess up
				 * component alphas for subpixel-rendered text, so we
				 * apply it when blending. */
				pixman_image_composite32(
					PIXMAN_OP_OVER, fg_fill, glyph->pix,
					foreground, 0, 0, 0, 0, x + glyph->x,
					y + font->ascent - glyph->y, glyph->width,
					glyph->height);
			}
		}
		/* increment pen position */
		x = nx;
	}

	if (draw_fg)
		pixman_image_unref(fg_fill);
	if (!last_cp)
		return ix;

	nx = x + padding;
	if (draw_bg) {
		pixman_image_fill_boxes(
			PIXMAN_OP_OVER, background, bg_color, 1,
			&(pixman_box32_t){
				.x1 = ix, .x2 = nx, .y1 = y, .y2 = y + buf_height });
	}

	return nx;
}


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
	  uint32_t colors_l, struct fcft_font *font)
{
	if (!text || !*text || !max_x)
		return x;

	uint32_t ix = x, nx;

	if ((nx = x + padding) + padding >= max_x)
		return x;
	x = nx;

	bool draw_fg = foreground && fg_color;
	
	pixman_image_t *fg_fill;
	pixman_color_t *cur_bg_color;
	if (draw_fg)
		fg_fill = pixman_image_create_solid_fill(fg_color);

	uint32_t color_ind = 0, codepoint, utf_state = UTF8_ACCEPT, last_cp = 0;
	for (char *p = text; *p; p++) {

		/* Returns nonzero if more bytes are needed */
		if (utf8decode(&utf_state, &codepoint, *p))
			continue;

		/* Turn off subpixel rendering, which complicates things when
		 * mixed with alpha channels */
		const struct fcft_glyph *glyph = fcft_rasterize_char_utf32(font, codepoint, FCFT_SUBPIXEL_NONE);
		if (!glyph)
			continue;

		/* Adjust x position based on kerning with previous glyph */
		long kern = 0;
		if (last_cp)
			fcft_kerning(font, last_cp, codepoint, &kern, NULL);
		if ((nx = x + kern + glyph->advance.x) + padding > max_x)
			break;
		last_cp = codepoint;
		x += kern;

		if (draw_fg) {
			/* Detect and handle pre-rendered glyphs (e.g. emoji) */
			if (pixman_image_get_format(glyph->pix) == PIXMAN_a8r8g8b8) {
				/* Only the alpha channel of the mask is used, so we can
				 * use fgfill here to blend prerendered glyphs with the
				 * same opacity */
				pixman_image_composite32(
					PIXMAN_OP_OVER, glyph->pix, fg_fill, foreground, 0, 0, 0, 0,
					x + glyph->x, y - glyph->y, glyph->width, glyph->height);
			} else {
				/* Applying the foreground color here would mess up
				 * component alphas for subpixel-rendered text, so we
				 * apply it when blending. */
				pixman_image_composite32(
					PIXMAN_OP_OVER, fg_fill, glyph->pix, foreground, 0, 0, 0, 0,
					x + glyph->x, y - glyph->y, glyph->width, glyph->height);
			}
		}
		
		/* increment pen position */
		x = nx;
	}
	
	if (draw_fg)
		pixman_image_unref(fg_fill);
	if (!last_cp)
		return ix;
	
	nx = x + padding;
	return nx;
}

