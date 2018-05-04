#include <compositor.h>
#include <libweston-desktop.h>
#include <sequential.h>
#include <wayland-server.h>


#include "taiwins.h"
#include "shell.h"

struct workspace {
	/* a workspace have three layers,
	 * - the hiden layer that you won't be able to see, because it is covered by
	 shown float but we don't insert the third layer to
	 * the compositors since they are hiden for floating views. The postions
	 * of the two layers change when user interact with windows.
	 */
	struct weston_layer hiden_float_layout;
	struct weston_layer tiled_layout;
	struct weston_layer shown_float_layout;
	/* also, we need to remember the position of the layer */
	uint32_t float_layer_pos;
	uint32_t tiled_layer_pos;
	//the wayland-buffer
};

static struct desktop onedesktop;

struct launcher *
twshell_acquire_launcher(void)
{
	return &onedesktop.launcher;
}

void set_launcher(struct wl_client *client,
		  struct wl_resource *resource,
		  struct wl_resource *surface,
		  struct wl_resource *buffer)
{
	struct launcher *launcher = twshell_acquire_launcher();
	struct wl_shm_buffer *wl_buffer = wl_shm_buffer_get(buffer);
	launcher->decision_buffer = wl_buffer;
}


//the application has to sit on the ui layer. So it has to follow the
//taiwins_shell protocol

//if you call weston_layer_set_position, you will insert the layer into
//compositors layer_list, so we only do that when workspace is active.
static void
workspace_init(struct workspace *wp, struct weston_compositor *compositor)
{
	weston_layer_init(&wp->tiled_layout, compositor);
	weston_layer_init(&wp->shown_float_layout, compositor);
	weston_layer_init(&wp->hiden_float_layout, compositor);
	wp->float_layer_pos = WESTON_LAYER_POSITION_NORMAL + 1;
	wp->tiled_layer_pos = WESTON_LAYER_POSITION_NORMAL;
}


static void
switch_workspace(struct desktop *d, struct workspace *to)
{
	struct workspace *wp = d->actived_workspace[0];
	weston_layer_unset_position(&wp->tiled_layout);
	weston_layer_unset_position(&wp->shown_float_layout);

	d->actived_workspace[1] = wp;
	d->actived_workspace[0] = to;
//	weston_layer_set_position(&wp->hiden_float_layout, WESTON_LAYER_POSITION_NORMAL-1);
	weston_layer_set_position(&wp->tiled_layout, wp->tiled_layer_pos);
	weston_layer_set_position(&wp->shown_float_layout , wp->float_layer_pos);
	weston_compositor_schedule_repaint(d->compositor);
}

static void
switch_to_recent_workspace(struct desktop *d)
{
	struct workspace *wp = d->actived_workspace[1];
	switch_workspace(d, wp);
}

/**
 * @brief switch the tiling and floating layer, this happens when we focus on
 * the different layer. Later on I can write the renderer myself and blured out
 * window that is not focused (only in the application layer).
 *
 * other things need to notice here, if we are change from tiled layer to
 * floating layer, the floating layer should actually just have one view, the
 * rest should moved to hiden layer, there are some code here need to be done
 */
static bool
switch_layer(struct desktop *d)
{
	uint32_t tpos, fpos;
	struct workspace *wp = d->actived_workspace[0];

	wp->float_layer_pos = wp->tiled_layout.position;
	wp->tiled_layer_pos = wp->shown_float_layout.position;

	weston_layer_unset_position(&wp->shown_float_layout);
	weston_layer_unset_position(&wp->tiled_layout);
	weston_layer_set_position(&wp->shown_float_layout, wp->float_layer_pos);
	weston_layer_set_position(&wp->tiled_layout, wp->tiled_layer_pos);
	weston_compositor_schedule_repaint(d->compositor);
}

static void
switch_layer_refresh(struct desktop *d)
{
	switch_layer(d);
	weston_compositor_schedule_repaint(d->compositor);
}


/**
 * @brief the libweston-desktop implementation
 */
static void
twdesk_surface_added(struct weston_desktop_surface *surface,
	      void *user_data)
{
	struct weston_surface *wt_surface = weston_desktop_surface_get_surface(surface);
	struct workspace *wsp = onedesktop.actived_workspace[0];

	//decide ou se trouve le surface 1) tiled_layer, 2) float layer. If the
	//tiling layer, you will need to allocate the position to the
	//surface. If the floating layer, You can skip the allocator

	//And finally, switch the layer if you need to.

	//now you need to create the view

	//create
}

static void
twdesk_surface_removed(struct weston_desktop_surface *surface,
		void *user_data)
{
	//unlink_view
}


static struct weston_desktop_api desktop =  {
	.surface_added = twdesk_surface_added,
	.surface_removed = twdesk_surface_removed,
};
/*** libweston-desktop implementation ***/


static void
_free_workspace(void *data)
{
	struct weston_layer *workspace = (struct weston_layer *)data;
	while(workspace->view_list.link.next != &workspace->view_list.link) {
		struct wl_list *link = workspace->view_list.link.next;
		struct weston_view *view = container_of(link, struct weston_view, layer_link.link);
		struct weston_surface *surf = view->surface;
		weston_surface_destroy(surf);
		//close this application!!!! By close the surface?
		//we could get into a segment fault for this
	}
}

static void
unbind_desktop(struct wl_resource *r)
{

}

static void
bind_desktop(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource = wl_resource_create(client, &taiwins_launcher_interface,
							  TWDESKP_VERSION, id);
	wl_resource_set_implementation(resource, NULL, data, unbind_desktop);
}

bool
announce_desktop(struct weston_compositor *ec)
{
	//initialize the desktop
	vector_t *workspaces = &onedesktop.workspaces;
	vector_init(workspaces, sizeof(struct workspace), _free_workspace);
	//then afterwards, you don't spend time allocating workspace anymore
	vector_resize(workspaces, 9);
	for (int i = 0; i < workspaces->len; i++)
		workspace_init((struct workspace *)vector_at(workspaces, i), ec);
	//not sure if this is a good idea, since we are using vector
	onedesktop.actived_workspace[0] = (struct workspace *)vector_at(&onedesktop.workspaces, 0);
	onedesktop.actived_workspace[1] = (struct workspace *)vector_at(&onedesktop.workspaces, 0);
	switch_workspace(&onedesktop, onedesktop.actived_workspace[0]);
	//as we have
	wl_global_create(ec->wl_display, &taiwins_launcher_interface, TWDESKP_VERSION, &onedesktop, bind_desktop);
	return true;
}
