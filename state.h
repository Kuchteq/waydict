#include <stdbool.h>
#include <wayland-client.h>
#include <curl/curl.h>


struct ui_dims {
        uint32_t search_y;
        uint32_t bar_y;
        uint32_t result_y;
        uint32_t result_x;
        uint32_t translation_x;
        uint32_t translation_y;
        uint32_t example_y;
        uint32_t example_x;
        uint32_t example_x_mid;
};
struct example {
	char *src;
	char *dst;
};
struct translation {
	char *text;
	char *pos;
        short examples_num;
	struct example examples[5];
};

struct result {
	char *text;
	char *pos;
        short translations_num;
	struct translation translations[40];
};

struct client_state {
	/* Globals */
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_shm *wl_shm;
	struct wl_compositor *wl_compositor;
	struct xdg_wm_base *xdg_wm_base;
	struct wl_seat *wl_seat;
	/* Objects */
	struct wl_surface *wl_surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	struct wl_keyboard *wl_keyboard;
	struct wl_pointer *wl_pointer;
	struct wl_touch *wl_touch;
	/* State */
	float offset;
	int width, height;
	bool closed;
	struct xkb_state *xkb_state;
	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
        int buffer_scale;
        struct {
               struct fcft_font *h1;
               struct fcft_font *h2;
               struct fcft_font *body;
               struct fcft_font *italic;

        } fonts;
        struct ui_dims dims;
        struct {
                char text[100];
                unsigned char idx;
        } query;
        CURL *curl;
        struct result (*results)[];
        int results_num;
};
