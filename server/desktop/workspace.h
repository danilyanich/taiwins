#ifndef TW_WORKSPACE_H
#define TW_WORKSPACE_H

#include <stdbool.h>
#include <compositor.h>
#include "unistd.h"
#include "layout.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct workspace {
	struct wl_list floating_layout_link;
	struct wl_list tiling_layout_link;
	//workspace does not distinguish the outputs.
	//so when we `switch_workspace, all the output has to update.
	//The layouting algorithm may have to worry about output
	struct weston_layer hidden_layer;
	struct weston_layer tiling_layer;
	struct weston_layer floating_layer;
	//Recent views::
	//we need a recent_views struct for user to switch among views. FIRST a
	//link list would be ideal but weston view struct does not have a link
	//for it. The SECOND best choice is a link-list that wraps the view in
	//it, but this requires extensive memory allocation. The NEXT best thing
	//is a stack. Since the recent views and stack share the same logic. We
	//will need a unique stack which can eliminate the duplicated elements.

	//the only tiling layer here will create the problem when we want to do
	//the stacking layout, for example. Only show two views.
};

extern size_t workspace_size;

void workspace_init(struct workspace *wp, struct weston_compositor *compositor);
void workspace_release(struct workspace *);
void workspace_switch(struct workspace *to, struct workspace *from,
		      struct weston_keyboard *keyboard);

void arrange_view_for_workspace(struct workspace *ws, struct weston_view *v,
				const enum disposer_command command,
				const struct disposer_op *arg);

bool is_view_on_workspace(const struct weston_view *v, const struct workspace *ws);
bool is_workspace_empty(const struct workspace *ws);


bool workspace_focus_view(struct workspace *ws, struct weston_view *v);
void workspace_add_view(struct workspace *w, struct weston_view *view);
bool workspace_move_view(struct workspace *w, struct weston_view *v,
				  const struct weston_position *pos);
void workspace_add_output(struct workspace *wp, struct weston_output *output);
void workspace_remove_output(struct workspace *w, struct weston_output *output);


#ifdef  __cplusplus
}
#endif



#endif
