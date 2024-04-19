#define _POSIX_C_SOURCE 200112L
#include <fcft/fcft.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <xkbcommon/xkbcommon.h>
#include "draw.h"
#include "keyboard.h"
#include "draw_help.h"
#include "fetching.h"
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
				  uint32_t serial)
{
	struct client_state *state = data;
	xdg_surface_ack_configure(xdg_surface, serial);

	struct wl_buffer *buffer = draw_frame(state);
	wl_surface_attach(state->wl_surface, buffer, 0, 0);
	wl_surface_commit(state->wl_surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

// Respond to the ping to make sure we confirm our client is not deadlocked
static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
			     uint32_t serial)
{
	xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_ping,
};

static void wl_seat_capabilities(void *data, struct wl_seat *wl_seat,
				 uint32_t capabilities)
{
	struct client_state *state = data;
	bool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;

	if (have_keyboard && state->wl_keyboard == NULL) {
		state->wl_keyboard = wl_seat_get_keyboard(state->wl_seat);
		wl_keyboard_add_listener(state->wl_keyboard,
					 &wl_keyboard_listener, state);
	} else if (!have_keyboard && state->wl_keyboard != NULL) {
		wl_keyboard_release(state->wl_keyboard);
		state->wl_keyboard = NULL;
	}
}

static void wl_seat_name(void *data, struct wl_seat *wl_seat, const char *name)
{
	fprintf(stderr, "seat name: %s\n", name);
}

static const struct wl_seat_listener wl_seat_listener = {
	.capabilities = wl_seat_capabilities,
	.name = wl_seat_name,
};

static void xdg_toplevel_configure(void *data,
				   struct xdg_toplevel *xdg_toplevel,
				   int32_t width, int32_t height,
				   struct wl_array *states)
{
	struct client_state *state = data;
	if (width == 0 || height == 0) {
		/* Compositor is deferring to us */
		return;
	}
	state->width = width * state->buffer_scale;
	state->height = height * state->buffer_scale;
	state->dims.result_y = UY(body);
	state->dims.result_x = UX(body);
	state->dims.translation_x = UX(h1) * 2;
	state->dims.translation_y = UY(h1) + UY(body);
	state->dims.example_y = UY(h1) + UY(h2) + UY(body);
	state->dims.example_x = state->dims.translation_x;
	state->dims.example_x_mid = state->width / 2;
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	struct client_state *state = data;
	state->closed = true;
}
static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_configure,
	.close = xdg_toplevel_close,
};

// Taking care of registry and binding interfaces
static void registry_global(void *data, struct wl_registry *wl_registry,
			    uint32_t name, const char *interface,
			    uint32_t version)
{
	struct client_state *state = data;
	if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->wl_shm = wl_registry_bind(wl_registry, name,
						 &wl_shm_interface, 1);
	} else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->wl_compositor = wl_registry_bind(
			wl_registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		state->xdg_wm_base = wl_registry_bind(
			wl_registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(state->xdg_wm_base,
					 &xdg_wm_base_listener, state);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		state->wl_seat = wl_registry_bind(wl_registry, name,
						  &wl_seat_interface, 7);
		wl_seat_add_listener(state->wl_seat, &wl_seat_listener, state);
	}
}

static void registry_global_remove(void *data, struct wl_registry *wl_registry,
				   uint32_t name)
{
	/* This space deliberately left blank */
}
static const struct wl_registry_listener wl_registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static const struct wl_callback_listener wl_surface_frame_listener;

static void wl_surface_frame_done(void *data, struct wl_callback *cb,
				  uint32_t time)
{
	/* Destroy this callback */
	wl_callback_destroy(cb);

	/* Request another frame */
	struct client_state *state = data;
	cb = wl_surface_frame(state->wl_surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, state);

	/* Update scroll amount at 24 pixels per second */

	/* Submit a frame for this event */
	struct wl_buffer *buffer = draw_frame(state);
	wl_surface_attach(state->wl_surface, buffer, 0, 0);
	wl_surface_damage_buffer(state->wl_surface, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_commit(state->wl_surface);

	if (state->closed) {
		exit(EXIT_SUCCESS);
	}
}

static const struct wl_callback_listener wl_surface_frame_listener = {
	.done = wl_surface_frame_done,
};
void init_fonts(struct client_state *state)
{
	fcft_init(FCFT_LOG_COLORIZE_AUTO, 0, FCFT_LOG_CLASS_DEBUG);
	fcft_set_scaling_filter(FCFT_SCALING_FILTER_LANCZOS3);

	state->fonts.h1 = fcft_from_name(1, (const char *[]){ "Hack:size=30" },
					 "dpi=184:weight=bold");
	state->fonts.h2 = fcft_from_name(1, (const char *[]){ "Hack:size=20" },
					 "dpi=184:weight=bold");
	state->fonts.body = fcft_from_name(
		1, (const char *[]){ "Hack:size=14" }, "dpi=184");
	state->fonts.italic = fcft_from_name(
		1, (const char *[]){ "Hack:size=12" }, "dpi=184:slant=italic");
}
int main(int argc, char *argv[])
{
	// Initialize Low Level App State
	struct client_state state = { 0 };
	state.width = 640;
	state.height = 480;
	state.buffer_scale = 2;
	state.wl_display = wl_display_connect(NULL);
	state.wl_registry = wl_display_get_registry(state.wl_display);
	state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	wl_registry_add_listener(state.wl_registry, &wl_registry_listener,
				 &state);
	wl_display_roundtrip(state.wl_display);

	// Initialize an XDG surface and assign toplevel role
	state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
	state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base,
							state.wl_surface);
	xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener,
				 &state);
	state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
	xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener,
				  &state);
	xdg_toplevel_set_title(state.xdg_toplevel, "waydict");
	wl_surface_set_buffer_scale(state.wl_surface, state.buffer_scale);
	wl_surface_commit(state.wl_surface);
	struct wl_callback *cb = wl_surface_frame(state.wl_surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, &state);

	init_fonts(&state);
	state.curl = curl_easy_init();
	curl_easy_setopt(state.curl, CURLOPT_FOLLOWLOCATION, 1L);
	while (wl_display_dispatch(state.wl_display)) {
		/* This space deliberately left blank */
	}

	return 0;
}
