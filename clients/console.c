#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <wayland-client.h>
#include <wayland-taiwins-shell-client-protocol.h>
#include <os/exec.h>

#include "../config.h"
#include "client.h"
#include "ui.h"
#include "nk_backends.h"


struct desktop_console {
	struct taiwins_console *interface;
	struct wl_globals globals;
	struct app_surface surface;
	struct shm_pool pool;
	struct wl_buffer *decision_buffer;

	struct wl_callback *exec_cb;
	uint32_t exec_id;

	off_t cursor;
	char chars[256];
	bool quit;
	//rendering data
	//for nuklear
	struct nk_wl_backend *bkend;
	//a good hack is that this text_edit is stateless, we don't need to
	//store anything once submitted
	struct nk_text_edit text_edit;
};

static const char *tmp_tab_chars[5] = {
	"aaaaaa",
	"bbbbbb",
	"cccccc",
	"dddddd",
	"eeeeee",
};


/**
 * @brief get the next
 */
static const char *
auto_complete(struct desktop_console *console)
{
	//we have some shadowed context here
	static int i = 0;
	return tmp_tab_chars[i++ % 5];
}

static void
submit_console(struct app_surface *surf)
{
	struct desktop_console *console =
		container_of(surf, struct desktop_console, surface);
	taiwins_console_submit(console->interface, console->decision_buffer, console->exec_id);

	app_surface_release(&console->surface);
}


/**
 * @brief a more serious console implementation
 */
static void
draw_console(struct nk_context *ctx, float width, float height,
	      struct app_surface *surf)
{
	//TODO change the state machine
	enum EDITSTATE {NORMAL, COMPLETING, SUBMITTING};
	static enum EDITSTATE edit_state = NORMAL;
	static char previous_tab[256] = {0};

	struct desktop_console *console =
		container_of(surf, struct desktop_console, surface);

	nk_layout_row_static(ctx, 30, 80, 2);
	nk_button_label(ctx, "button");
	nk_label(ctx, "another", NK_TEXT_LEFT);

	nk_layout_row_static(ctx, height - 30, width, 1);
	nk_edit_buffer(ctx, NK_EDIT_FIELD, &console->text_edit, nk_filter_default);
	//we could go into two different state, first is compeletion, then it is submission
	if (nk_wl_get_keyinput(ctx) == XKB_KEY_NoSymbol) //key up
		return;
	if (nk_wl_get_keyinput(ctx) == XKB_KEY_Tab)
		edit_state = COMPLETING;
	else if (nk_wl_get_keyinput(ctx) == XKB_KEY_Return)
		edit_state = SUBMITTING;
	else
		edit_state = NORMAL;

	switch (edit_state) {
	case COMPLETING:
		nk_textedit_delete(&console->text_edit, console->text_edit.cursor - strlen(previous_tab),
				   strlen(previous_tab));
		strcpy(previous_tab, auto_complete(console));
		nk_textedit_text(&console->text_edit, previous_tab, strlen(previous_tab));
		break;
	case SUBMITTING:
		memset(previous_tab, 0, sizeof(previous_tab));
		edit_state = NORMAL;
		nk_wl_add_idle(ctx, submit_console);
		break;
	case NORMAL:
		memset(previous_tab, 0, sizeof(previous_tab));
		break;
	}
	//TODO, have a close option, thus close without submiting
}

//fuck, I wish that I have c++
static void
update_app_config(void *data,
		  struct taiwins_console *taiwins_console,
		  const char *app_name,
		  uint32_t floating,
		  wl_fixed_t scale)
{
//we don't nothing here now
}



static void
start_console(void *data,
	       struct taiwins_console *taiwins_console,
	       wl_fixed_t width,
	       wl_fixed_t height,
	       wl_fixed_t scale)
{
	struct desktop_console *console = (struct desktop_console *)data;
	struct app_surface *surface = &console->surface;
	struct wl_surface *wl_surface = NULL;
	struct tw_ui *ui;
	int w = wl_fixed_to_int(width);
	int h = wl_fixed_to_int(height);

	wl_surface = wl_compositor_create_surface(console->globals.compositor);
	ui = taiwins_console_launch(taiwins_console, wl_surface);

	app_surface_init(surface, wl_surface, (struct wl_proxy *)ui);
	surface->wl_globals = &console->globals;
	nk_egl_impl_app_surface(surface, console->bkend, draw_console,
				w, h, 0, 0);
	app_surface_frame(surface, false);
}

static void
exec_application(void *data, struct taiwins_console *console, uint32_t id)
{
	struct desktop_console *desktop_console = data;
	if (!strlen(desktop_console->chars))
		return;
	char *const forks[] = {desktop_console->chars, NULL};

	if (id != desktop_console->exec_id) {
		fprintf(stderr, "exec order not consistant, something wrong.");
	} else {
		fprintf(stderr, "creating weston terminal");
		//parsing the input and command buffer. Then do it
		fork_exec(1, forks);
	}
	nk_textedit_init_fixed(&desktop_console->text_edit, desktop_console->chars, 256);

	desktop_console->exec_id++;
}

struct taiwins_console_listener console_impl = {
	.application_configure = update_app_config,
	.start = start_console,
	.exec = exec_application,
};

/** constructor-destructor **/
static void
init_console(struct desktop_console *console)
{
	memset(console->chars, 0, sizeof(console->chars));
	console->quit = false;
	shm_pool_init(&console->pool, console->globals.shm,
		      TAIWINS_CONSOLE_CONF_NUM_DECISIONS * sizeof(struct taiwins_decision_key),
		      console->globals.buffer_format);
	console->decision_buffer = shm_pool_alloc_buffer(&console->pool,
							  sizeof(struct taiwins_decision_key),
							  TAIWINS_CONSOLE_CONF_NUM_DECISIONS);

	console->bkend = nk_egl_create_backend(console->globals.display, NULL);
	nk_textedit_init_fixed(&console->text_edit, console->chars, 256);
}


static void
release_console(struct desktop_console *console)
{
	nk_textedit_free(&console->text_edit);
	nk_egl_destroy_backend(console->bkend);
	shm_pool_release(&console->pool);

	taiwins_console_destroy(console->interface);
	wl_globals_release(&console->globals);

	console->quit = true;
}


static
void announce_globals(void *data,
		      struct wl_registry *wl_registry,
		      uint32_t name,
		      const char *interface,
		      uint32_t version)
{
	struct desktop_console *console = (struct desktop_console *)data;

	if (strcmp(interface, taiwins_console_interface.name) == 0) {
		fprintf(stderr, "console registé\n");
		console->interface = (struct taiwins_console *)
			wl_registry_bind(wl_registry, name, &taiwins_console_interface, version);
		taiwins_console_add_listener(console->interface, &console_impl, console);
	} else
		wl_globals_announce(&console->globals, wl_registry, name, interface, version);
}


static void
announce_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name)
{
}

static struct wl_registry_listener registry_listener = {
	.global = announce_globals,
	.global_remove = announce_global_remove
};




int
main(int argc, char *argv[])
{
	struct desktop_console tw_console;
	struct wl_display *display = wl_display_connect(NULL);
	if (!display) {
		fprintf(stderr, "could not connect to display\n");
		return -1;
	}
	wl_globals_init(&tw_console.globals, display);

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, &tw_console);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);
	init_console(&tw_console);
	tw_console.globals.theme = taiwins_dark_theme;

	//okay, now we should create the buffers
	//event loop
	while(wl_display_dispatch(display) != -1 && !tw_console.quit);
	release_console(&tw_console);
	wl_registry_destroy(registry);
	wl_display_disconnect(display);
	return 0;
}