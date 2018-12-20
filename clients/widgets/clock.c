#include "../../3rdparties/iconheader/IconsFontAwesome5_c.h"
#include "../widget.h"

/*
 * This is a simple illustration of how to work with a row. We may really does
 * not know how much space the widget occupies inadvance.
 *
 * the widget should start with nk_layout_row_push to occupy its space, we limit
 * one widget right now, as it is what most time is. But we may need to have
 * more versatile approach
 */
static int
clock_widget_anchor(struct shell_widget *widget, struct shell_widget_label *label)
{
	//it seems to work, we still need a library to figure out the text size
	/* nk_layout_row_push(ctx, 100); */
	static const char * daysoftheweek[] =
		{"sun", "mon", "tus", "wed", "thu", "fri", "sat"};
	time_t epochs = time(NULL);
	struct tm *tim = localtime(&epochs);

	return snprintf(label->label, 255, "%s %02d:%02d:%02d",
		       daysoftheweek[tim->tm_wday], tim->tm_hour, tim->tm_min, tim->tm_sec);
}

static void
clock_widget_sample(struct nk_context *ctx, float width, float height, struct app_surface *app)
{
	/* enum nk_buttons btn; */
	/* uint32_t sx, sy; */
	//TODO, change the draw function to app->draw_widget(app);
	enum {EASY, HARD};
	static int op = EASY;
	static struct nk_text_edit text_edit;
	static bool init_text_edit = false;
	static char text_buffer[256];
	if (!init_text_edit) {
		init_text_edit = true;
		nk_textedit_init_fixed(&text_edit, text_buffer, 256);
	}

	nk_layout_row_static(ctx, 30, 80, 2);
	nk_button_label(ctx, "button");
	nk_label(ctx, "another", NK_TEXT_LEFT);
	nk_layout_row_dynamic(ctx, 30, 2);
	if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
	if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;

	nk_layout_row_dynamic(ctx, 25, 1);
	nk_edit_buffer(ctx, NK_EDIT_FIELD, &text_edit, nk_filter_default);
}

struct shell_widget clock_widget = {
	.ancre_cb = clock_widget_anchor,
	.draw_cb = clock_widget_sample,
	.w = 200,
	.h = 150,
	.widget.s = 1,
	.interval = {
		.it_value = {
			.tv_sec = 1,
			.tv_nsec = 0,
		},
		.it_interval = {
			.tv_sec = 1,
			.tv_nsec = 0,
		},
	},
	.file_path = NULL,
};
