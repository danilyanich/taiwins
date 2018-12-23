#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <unistd.h>
#include <compositor.h>
#include <libweston-desktop.h>
#include <sequential.h>
#include <wayland-server.h>
#include <wayland-taiwins-shell-server-protocol.h>

#include "taiwins.h"
#include "shell.h"


/**
 * this struct handles the request and sends the event from
 * taiwins_console.
 */
struct tw_console {
	// client  info
	char path[256];
	struct wl_client *client;
	struct wl_resource *resource;
	pid_t pid; uid_t uid; gid_t gid;

	struct weston_compositor *compositor;
	struct twshell *shell;
	struct wl_shm_buffer *decision_buffer;
	struct weston_surface *surface;
	struct wl_listener close_console_listener;
};

static struct tw_console CONSOLE;

static void
console_surface_destroy_cb(struct wl_listener *listener, void *data)
{
	struct tw_console *console = container_of(listener, struct tw_console, close_console_listener);
	console->surface = NULL;
}


static void
close_console(struct wl_client *client,
		       struct wl_resource *resource,
		       struct wl_resource *wl_buffer,
		       uint32_t exec_id)
{
	fprintf(stderr, "the console client is %p\n", client);
	struct tw_console *lch = (struct tw_console *)wl_resource_get_user_data(resource);
	lch->decision_buffer = wl_shm_buffer_get(wl_buffer);
	taiwins_console_send_exec(resource, exec_id);
}


static void
set_console(struct wl_client *client,
	     struct wl_resource *resource,
	     uint32_t ui_elem,
	     struct wl_resource *wl_surface)
{
	struct tw_console *lch = wl_resource_get_user_data(resource);
	twshell_create_ui_elem(lch->shell, client, ui_elem, wl_surface, NULL, 100, 100, TW_UI_TYPE_WIDGET);
	lch->surface = tw_surface_from_resource(wl_surface);
	wl_resource_add_destroy_listener(wl_surface, &lch->close_console_listener);
}


static struct taiwins_console_interface console_impl = {
	.launch = set_console,
	.submit = close_console
};


static void
unbind_console(struct wl_resource *r)
{
	fprintf(stderr, "console closed.\n");
	struct tw_console *console = wl_resource_get_user_data(r);
	console->client = NULL;
	console->resource = NULL;
	console->pid = console->uid = console->gid = -1;
	//I don't think I need to release the wl_resource
}


static void
bind_console(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	/* int pid, uid, gid; */
	/* wl_client_get_credentials(client, &pid, &uid, &gid); */
	struct tw_console *console = data;
	struct wl_resource *wl_resource = wl_resource_create(client, &taiwins_console_interface,
							  TWDESKP_VERSION, id);

	uid_t uid; gid_t gid; pid_t pid;
	wl_client_get_credentials(client, &pid, &uid, &gid);
	if (console->client &&
	    (uid != console->uid || pid != console->pid || gid != console->gid)) {
		wl_resource_post_error(wl_resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "client %d is not un atherized console", id);
		wl_resource_destroy(wl_resource);
		return;
	}
	console->client = client;
	console->resource = wl_resource;
	wl_resource_set_implementation(wl_resource, &console_impl, console, unbind_console);
}

static void
should_start_console(struct weston_keyboard *keyboard,
		      const struct timespec *time, uint32_t key,
		      void *data)
{
	struct tw_console *lch = data;
	if (!lch->client) //we do not have a console
		return;
	if ((lch->surface) &&
	    wl_list_length(&lch->surface->views)) //or the console is active
		return;

	taiwins_console_send_start(lch->resource,
				    wl_fixed_from_int(200),
				    wl_fixed_from_int(100),
				    wl_fixed_from_int(1));
}

static void
launch_console_client(void *data)
{
	struct tw_console *console = data;
	console->client = tw_launch_client(console->compositor, console->path);
	wl_client_get_credentials(console->client, &console->pid, &console->uid, &console->gid);
}

struct tw_console *announce_console(struct weston_compositor *compositor,
				       struct twshell *shell, const char *path)
{
	CONSOLE.surface = NULL;
	CONSOLE.resource = NULL;
	CONSOLE.compositor = compositor;
	CONSOLE.shell = shell;
	wl_list_init(&CONSOLE.close_console_listener.link);
	CONSOLE.close_console_listener.notify = console_surface_destroy_cb;

	wl_global_create(compositor->wl_display, &taiwins_console_interface, TWDESKP_VERSION, &CONSOLE, bind_console);
	weston_compositor_add_key_binding(compositor, KEY_P, MODIFIER_CTRL, should_start_console, &CONSOLE);

	if (path) {
		assert(strlen(path) +1 <= sizeof(CONSOLE.path));
		strcpy(CONSOLE.path, path);
		struct wl_event_loop *loop = wl_display_get_event_loop(compositor->wl_display);
		wl_event_loop_add_idle(loop, launch_console_client, &CONSOLE);
	}

	return &CONSOLE;
}