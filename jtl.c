/* JTL */

#include <fcntl.h>
#include <getopt.h>
#include <libinput.h>

#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>

#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>

#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/region.h>
#include <xkbcommon/xkbcommon.h>

#ifdef FOREIGN_TOPLEVEL
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#endif

#ifdef WORKSPACES
#include <wlr/types/wlr_ext_workspace_v1.h>
#endif

#ifdef XWAYLAND
#include <wlr/xwayland.h>
#endif

#include "xdg-shell-protocol.h"

#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define CLAMP(X, L, H)          (MIN(MAX((X), (L)), (H)))
#define CLEANMASK(mask)         ((mask) & ~WLR_MODIFIER_CAPS)
#define VISIBLEON(C, M)         ((M) && (C)->mon == (M) && ((C)->tags & (M)->tagset[(M)->seltags]))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define END(A)                  ((A) + LENGTH(A))
#define TAGMASK                 ((1u << TAGCOUNT) - 1)
#define LISTEN(E, L, H)         wl_signal_add((E), ((L)->notify = (H), (L)))
#define LISTEN_STATIC(E, H)     do { struct wl_listener *_l = ecalloc(1, sizeof(*_l)); _l->notify = (H); wl_signal_add((E), _l); } while (0)

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
int fd_set_nonblock(int fd);

enum { CurNormal, CurPressed };
enum { XDGShell, LayerShell, X11 };
enum { LyrBg, LyrBottom, LyrTile, LyrFloat, LyrTop, LyrOverlay, LyrFS, NUM_LAYERS };

typedef union {
	int i;
	uint32_t ui;
	float f;
	const void *v;
} Arg;

typedef struct Monitor Monitor;

typedef struct {
	uint64_t start_time;
	float x, y, w, h;
	float alpha;
	float from_x, from_y, from_w, from_h, from_alpha;
	float to_x, to_y, to_w, to_h, to_alpha;
	int active;
} ClientAnim;

typedef struct {
	struct wlr_xdg_popup *xdg_popup;
	struct wl_listener commit;
	struct wl_listener destroy;
} Popup;

typedef struct {

	unsigned int type;
	int pending_fullscreen;

	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border[4];
	struct wlr_scene_tree *scene_surface;
	struct wlr_box geom;
	unsigned int bw;
	uint32_t tags;
	int isfloating;
	int isfullscreen;
	ClientAnim anim;
	struct wlr_box prev;
	struct wlr_box bounds;
	union {
		struct wlr_xdg_surface *xdg;
		struct wlr_xwayland_surface *xwayland;
	} surface;
	struct wl_list link;
	struct wl_list flink;
	struct wl_listener commit;
	struct wl_listener map;
	struct wl_listener maximize;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener fullscreen;
	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wl_listener set_decoration_mode;
	struct wl_listener destroy_decoration;
#ifdef XWAYLAND
	struct wl_listener activate;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener configure;
#endif
	int float_w, float_h;
	uint32_t resize;
	int suppress_arrange;
#ifdef FOREIGN_TOPLEVEL
	struct wlr_foreign_toplevel_handle_v1 *toplevel_handle;
	struct wl_listener toplevel_handle_activate;
	struct wl_listener toplevel_handle_fullscreen;
	struct wl_listener toplevel_handle_close;
	struct wl_listener toplevel_handle_destroy;
	struct wl_listener toplevel_set_title;
	struct wl_listener toplevel_set_app_id;
#endif

} Client;

typedef struct {
	uint32_t mod;
	xkb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

	typedef struct {
		struct wlr_keyboard_group *wlr_group;
		int keysym_count;
		struct wl_event_source *key_repeat_source;
		uint32_t mods;
		xkb_keysym_t keysym_buf[32];
		struct wl_listener modifiers;
		struct wl_listener key;
		struct wl_listener destroy;
	} KeyboardGroup;

typedef struct {

	int mapped;
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wlr_layer_surface_v1 *layer_surface;
	struct wl_list link;
	struct wlr_scene_tree *popups;
	struct wl_listener destroy;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
} LayerSurface;

struct Monitor {

	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_box m;
	struct wlr_box w;
	unsigned int seltags;
	uint32_t tagset[2];
	float mfact;
	int nmaster;
	struct wl_list layers[4];
	struct wl_list link;
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;
#ifdef WORKSPACES
	struct wlr_ext_workspace_group_handle_v1 *ext_group;
#endif
};

typedef struct {
	const char *name;
	float mfact;
	int nmaster;
	float scale;
	enum wl_output_transform rr;
	int x, y;
} MonitorRule;

typedef struct {
	const char *id;           /* regex on app-id (WM_CLASS for X11), NULL = skip */
	const char *title;        /* regex on window title, NULL = skip */
	uint32_t tags;            /* workspace bitmask, 0 = current workspace */
	int isfullscreen;         /* 1 = open fullscreen */
} Rule;

typedef struct {
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
} PointerConstraint;

static double tween_ease(double t);
static void tween_start(Client *c, float tx, float ty, float tw, float th, float ta);
static void tween_cancel(Client *c);
static int tween_run(void);
static void opacity_buffer(struct wlr_scene_buffer *buffer, int sx, int sy, void *user_data);
static void scale_buffer(struct wlr_scene_buffer *buffer, int sx, int sy, void *user_data);
static int animateclient(Client *c);
static void startclientanim(Client *c);

static void rendermon(struct wl_listener *listener, void *data);

static void motionnotify(uint32_t time, struct wlr_input_device *device, double dx,
		double dy, double dx_unaccel, double dy_unaccel);
static void motionabsolute(struct wl_listener *listener, void *data);
static void motionrelative(struct wl_listener *listener, void *data);
static Monitor *xytomon(double x, double y);
static void xytonode(double x, double y, struct wlr_surface **psurface,
		Client **pc, LayerSurface **pl, double *nx, double *ny);
static void pointerfocus(Client *c, struct wlr_surface *surface,
		double sx, double sy, uint32_t time);

static void arrange(Monitor *m);
static void tile(Monitor *m);
static void arrangelayer(Monitor *m, struct wl_list *list,
		struct wlr_box *usable_area, int exclusive);
static void arrangelayers(Monitor *m);
static void commitnotify(struct wl_listener *listener, void *data);
static void resize(Client *c, struct wlr_box geo);
static void applybounds(Client *c, struct wlr_box *bbox);
static void setfloat(Client *c);
static void centerfloat(Client *c);
static void movefloat(Client *c, Monitor *m);

static void killpopups(Client *c);
static void focusclient(Client *c, int lift);
static Client *focustop(Monitor *m);

static void applyrules(Client *c);
static void axisnotify(struct wl_listener *listener, void *data);
static void buttonpress(struct wl_listener *listener, void *data);
static void chvt(const Arg *arg);
static void cleanup(void);
static void cleanupmon(struct wl_listener *listener, void *data);
static void cleanuplisteners(void);
static void closemon(Monitor *m);
static void commitlayersurfacenotify(struct wl_listener *listener, void *data);
static void commitpopup(struct wl_listener *listener, void *data);
static void createdecoration(struct wl_listener *listener, void *data);
static void createkeyboard(struct wlr_keyboard *keyboard);
static KeyboardGroup *createkeyboardgroup(void);
static void createlayersurface(struct wl_listener *listener, void *data);
static void createmon(struct wl_listener *listener, void *data);
static void createnotify(struct wl_listener *listener, void *data);
static void createpointer(struct wlr_pointer *pointer);
static void createpointerconstraint(struct wl_listener *listener, void *data);
static void createpopup(struct wl_listener *listener, void *data);
static void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint);
static void cursorframe(struct wl_listener *listener, void *data);
static void cursorwarptohint(void);
static void destroypopup(struct wl_listener *listener, void *data);
static void destroydragicon(struct wl_listener *listener, void *data);
static void destroylayersurfacenotify(struct wl_listener *listener, void *data);
static void destroynotify(struct wl_listener *listener, void *data);
static void destroypointerconstraint(struct wl_listener *listener, void *data);
static void destroykeyboardgroup(struct wl_listener *listener, void *data);
static void destroydecoration(struct wl_listener *listener, void *data);
static Monitor *dirtomon(enum wlr_direction dir);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static void fullscreennotify(struct wl_listener *listener, void *data);
static void handlesig(int signo);
static void incnmaster(const Arg *arg);
static void inputdevice(struct wl_listener *listener, void *data);
static int keybinding(uint32_t mods, xkb_keysym_t sym);
static void keypress(struct wl_listener *listener, void *data);
static void keypressmod(struct wl_listener *listener, void *data);
static int keyrepeat(void *data);
static void killclient(const Arg *arg);
static void mapnotify(struct wl_listener *listener, void *data);
static void maximizenotify(struct wl_listener *listener, void *data);
static void outputmgrapply(struct wl_listener *listener, void *data);
static void outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int test);
static void outputmgrtest(struct wl_listener *listener, void *data);
static void quit(const Arg *arg);
static void requestdecorationmode(struct wl_listener *listener, void *data);
static void requeststartdrag(struct wl_listener *listener, void *data);
static void requestmonstate(struct wl_listener *listener, void *data);
static int deactivateold(struct wlr_surface *old, Client *old_c,
		LayerSurface *old_l, int old_client_type, Client *c);
static void run(void);
static void setcursor(struct wl_listener *listener, void *data);
static void setcursorshape(struct wl_listener *listener, void *data);
static void setfullscreen(Client *c, int fullscreen);
static void setmfact(const Arg *arg);
static void setmon(Client *c, Monitor *m, uint32_t newtags);
static void setpsel(struct wl_listener *listener, void *data);
static void setsel(struct wl_listener *listener, void *data);
static void setup(void);
static void setupinput(void);
#ifdef XWAYLAND
static void setupx(void);
#endif
static void spawn(const Arg *arg);
static void startdrag(struct wl_listener *listener, void *data);
static void tagmon(const Arg *arg);
static void togglefullscreen(const Arg *arg);
#ifdef FULLSCREEN_TEARING
static void enabletearing(const Arg *arg);
static void disabletearing(const Arg *arg);
static int moncantear(Monitor *m);
#endif
static void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
static void unmapnotify(struct wl_listener *listener, void *data);
static void updatemons(struct wl_listener *listener, void *data);
static void tag(const Arg *arg);
static void view(const Arg *arg);
static void zoom(const Arg *arg);

#ifdef FOREIGN_TOPLEVEL
static void setup_toplevel_handle(Client *c, Client *p);
static void toplevel_handle_activate_cb(struct wl_listener *listener, void *data);
static void toplevel_handle_fullscreen_cb(struct wl_listener *listener, void *data);
static void toplevel_handle_close_cb(struct wl_listener *listener, void *data);
static void toplevel_handle_destroy_cb(struct wl_listener *listener, void *data);
static void toplevel_set_title_cb(struct wl_listener *listener, void *data);
static void toplevel_set_app_id_cb(struct wl_listener *listener, void *data);
#endif

static int client_is_x11(Client *c);
static struct wlr_surface *client_surface(Client *c);
static int toplevel_from_wlr_surface(struct wlr_surface *s, Client **pc, LayerSurface **pl);
static void client_activate_surface(struct wlr_surface *s, int activated);
static void client_set_bounds(Client *c, int32_t width, int32_t height);
static void client_get_geometry(Client *c, struct wlr_box *geom);
static Client *client_get_parent(Client *c);
static int client_has_children(Client *c);
static int client_is_unmanaged(Client *c);
static void client_notify_enter(struct wlr_surface *s, struct wlr_keyboard *kb);
static void client_send_close(Client *c);
static void client_set_border_color(Client *c, const float color[static 4]);
static void client_set_fullscreen(Client *c, int fullscreen);
static void client_set_scale(struct wlr_surface *s, int32_t scale);
static uint32_t client_set_size(Client *c, uint32_t width, uint32_t height);
static void client_set_tiled(Client *c, uint32_t edges);
static void client_set_suspended(Client *c, int suspended);
static int client_wants_focus(Client *c);
static int client_wants_fullscreen(Client *c);

static struct wlr_cursor *cursor;
static struct wlr_seat *seat;
static Monitor *selmon;
static struct wlr_scene_tree *layers[NUM_LAYERS];
static struct wl_list clients;
static struct wl_list fstack;
static struct wlr_output_layout *output_layout;
static void *exclusive_focus;
static unsigned int cursor_mode;
static struct wlr_scene_tree *drag_icon;
static KeyboardGroup *kb_group;
static int n_animated = 0;
#ifdef FULLSCREEN_TEARING
static int tearing_enabled = 0;
#endif

static struct wl_display *dpy;
static struct wl_event_loop *event_loop;
static struct wlr_backend *backend;
static struct wlr_scene *scene;
static struct wlr_renderer *drw;
static struct wlr_allocator *alloc;
static struct wlr_compositor *compositor;
static struct wlr_session *session;
static struct wlr_scene_rect *root_bg;

static struct wlr_pointer_constraint_v1 *active_constraint;
static struct wlr_xcursor_manager *cursor_mgr;
static struct wl_list mons;

#ifdef FOREIGN_TOPLEVEL
static struct wlr_foreign_toplevel_manager_v1 *toplevel_manager;
#endif

#ifdef WORKSPACES
#define EXT_WORKSPACE_ENABLE_CAPS \
	EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE | \
	EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_DEACTIVATE

struct JtlWorkspace {
	struct wl_list link;
	uint32_t tag; /* 1..TAGCOUNT */
	Monitor *m;
	struct wlr_ext_workspace_handle_v1 *ext_workspace;
};

static struct wlr_ext_workspace_manager_v1 *ext_workspace_mgr;
static struct wl_list ext_workspaces; /* list of JtlWorkspace */

static void workspaces_send(Monitor *m);
static void workspaces_add(Monitor *m, uint32_t tag);
static void workspaces_remove_for_monitor(Monitor *m);
static void workspaces_handle_commit(struct wl_listener *listener, void *data);
#endif

static struct wlr_xdg_shell *xdg_shell;
static struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
static struct wlr_layer_shell_v1 *layer_shell;
static struct wlr_output_manager_v1 *output_mgr;
static struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
static struct wlr_pointer_constraints_v1 *pointer_constraints;
static struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;

static const int layermap[] = { LyrBg, LyrBottom, LyrTop, LyrOverlay };

static struct wl_listener cursor_axis = {.notify = axisnotify};
static struct wl_listener cursor_button = {.notify = buttonpress};
static struct wl_listener cursor_frame = {.notify = cursorframe};
static struct wl_listener cursor_motion = {.notify = motionrelative};
static struct wl_listener cursor_motion_absolute = {.notify = motionabsolute};

static struct wl_listener layout_change = {.notify = updatemons};
static struct wl_listener new_input_device = {.notify = inputdevice};
static struct wl_listener new_pointer_constraint = {.notify = createpointerconstraint};
static struct wl_listener new_output = {.notify = createmon};
static struct wl_listener new_xdg_toplevel = {.notify = createnotify};
static struct wl_listener new_xdg_popup = {.notify = createpopup};
static struct wl_listener new_xdg_decoration = {.notify = createdecoration};
static struct wl_listener new_layer_surface = {.notify = createlayersurface};
static struct wl_listener output_mgr_apply = {.notify = outputmgrapply};
static struct wl_listener output_mgr_test = {.notify = outputmgrtest};
static struct wl_listener request_cursor = {.notify = setcursor};
static struct wl_listener request_set_psel = {.notify = setpsel};
static struct wl_listener request_set_sel = {.notify = setsel};
static struct wl_listener request_set_cursor_shape = {.notify = setcursorshape};
static struct wl_listener request_start_drag = {.notify = requeststartdrag};
static struct wl_listener start_drag = {.notify = startdrag};
#ifdef XWAYLAND
static void activatex11(struct wl_listener *listener, void *data);
static void associatex11(struct wl_listener *listener, void *data);
static void configurex11(struct wl_listener *listener, void *data);
static void createnotifyx11(struct wl_listener *listener, void *data);
static void dissociatex11(struct wl_listener *listener, void *data);

static void xwaylandready(struct wl_listener *listener, void *data);
static struct wl_listener new_xwayland_surface = {.notify = createnotifyx11};
static struct wl_listener xwayland_ready = {.notify = xwaylandready};
static struct wlr_xwayland *xwayland;
#endif

#include "config.h"

void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		die("calloc:");
	return p;
}

static void
requestdecorationmode(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, set_decoration_mode);
	if (c->surface.xdg->initialized)
		wlr_xdg_toplevel_decoration_v1_set_mode(c->decoration,
				WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void
axisnotify(struct wl_listener *listener, void *data)
{

	struct wlr_pointer_axis_event *event = data;
	wlr_seat_pointer_notify_axis(seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source, event->relative_direction);
}

static void
buttonpress(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_button_event *event = data;
	Client *c;

	switch (event->state) {
	case WL_POINTER_BUTTON_STATE_PRESSED:
		cursor_mode = CurPressed;
		selmon = xytomon(cursor->x, cursor->y);
		xytonode(cursor->x, cursor->y, NULL, &c, NULL, NULL, NULL);
		if (c && (!client_is_unmanaged(c) || client_wants_focus(c)))
			focusclient(c, 1);

		break;
	case WL_POINTER_BUTTON_STATE_RELEASED:
		cursor_mode = CurNormal;
		break;
	}

	wlr_seat_pointer_notify_button(seat,
			event->time_msec, event->button, event->state);
}

static void
chvt(const Arg *arg)
{
	wlr_session_change_vt(session, arg->ui);
}

static void
cleanuplisteners(void)
{
	struct wl_listener *listeners[] = {
		&cursor_axis, &cursor_button, &cursor_frame, &cursor_motion,
		&cursor_motion_absolute, &layout_change,
		&new_input_device, &new_pointer_constraint, &new_output,
		&new_xdg_toplevel, &new_xdg_decoration, &new_xdg_popup,
		&new_layer_surface, &output_mgr_apply, &output_mgr_test,
		&request_cursor, &request_set_psel, &request_set_sel,
		&request_set_cursor_shape, &request_start_drag, &start_drag,
#ifdef XWAYLAND
		&new_xwayland_surface, &xwayland_ready,
#endif
	};
	for (size_t i = 0; i < LENGTH(listeners); i++)
		wl_list_remove(&listeners[i]->link);
}

static void
destroykeyboardgroup(struct wl_listener *listener, void *data)
{
	KeyboardGroup *group = wl_container_of(listener, group, destroy);
	wl_event_source_remove(group->key_repeat_source);
	wl_list_remove(&group->key.link);
	wl_list_remove(&group->modifiers.link);
	wl_list_remove(&group->destroy.link);
	wlr_keyboard_group_destroy(group->wlr_group);
	free(group);
}

static void
cleanup(void)
{
	cleanuplisteners();
#ifdef XWAYLAND
	wlr_xwayland_destroy(xwayland);
	xwayland = NULL;
#endif
	wl_display_destroy_clients(dpy);
	wlr_scene_node_destroy(&scene->tree.node);
	wlr_xcursor_manager_destroy(cursor_mgr);
	wlr_cursor_destroy(cursor);

	destroykeyboardgroup(&kb_group->destroy, NULL);
	wlr_allocator_destroy(alloc);
	wlr_renderer_destroy(drw);
	wlr_backend_destroy(backend);

	wl_display_destroy(dpy);
}

static void
closemon(Monitor *m)
{

	Client *c;
	int i = 0, nmons = wl_list_length(&mons);
	if (!nmons) {
		selmon = NULL;
	} else if (m == selmon) {
		do
			selmon = wl_container_of(mons.next, selmon, link);
		while (!selmon->wlr_output->enabled && i++ < nmons);

		if (!selmon->wlr_output->enabled)
			selmon = NULL;
	}

	wl_list_for_each(c, &clients, link) {
		if (c->mon == m)
			setmon(c, selmon, c->tags);
	}
	focusclient(focustop(selmon), 1);
}

static void
cleanupmon(struct wl_listener *listener, void *data)
{
	Monitor *m = wl_container_of(listener, m, destroy);
	LayerSurface *l, *tmp;
	size_t i;
	for (i = 0; i < LENGTH(m->layers); i++) {
		wl_list_for_each_safe(l, tmp, &m->layers[i], link)
			wlr_layer_surface_v1_destroy(l->layer_surface);
	}

	wl_list_remove(&m->destroy.link);
	wl_list_remove(&m->frame.link);
	wl_list_remove(&m->link);
	wl_list_remove(&m->request_state.link);
	m->wlr_output->data = NULL;
	wlr_output_layout_remove(output_layout, m->wlr_output);
	wlr_scene_output_destroy(m->scene_output);

	closemon(m);
#ifdef WORKSPACES
	workspaces_remove_for_monitor(m);
	if (m->ext_group)
		wlr_ext_workspace_group_handle_v1_destroy(m->ext_group);
#endif
	free(m);
}

static void
commitlayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *l = wl_container_of(listener, l, surface_commit);
	struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;
	struct wlr_scene_tree *scene_layer = layers[layermap[layer_surface->current.layer]];
	struct wlr_layer_surface_v1_state old_state;

	if (l->layer_surface->initial_commit) {
		client_set_scale(layer_surface->surface, (int32_t)l->mon->wlr_output->scale);
		old_state = l->layer_surface->current;
		l->layer_surface->current = l->layer_surface->pending;
		arrangelayers(l->mon);
		l->layer_surface->current = old_state;
		return;
	}

	if (layer_surface->current.committed == 0 && l->mapped == layer_surface->surface->mapped)
		return;
	l->mapped = layer_surface->surface->mapped;

	if (scene_layer != l->scene->node.parent) {
		wlr_scene_node_reparent(&l->scene->node, scene_layer);
		wl_list_remove(&l->link);
		wl_list_insert(&l->mon->layers[layer_surface->current.layer], &l->link);
		wlr_scene_node_reparent(&l->popups->node, (layer_surface->current.layer
				< ZWLR_LAYER_SHELL_V1_LAYER_TOP ? layers[LyrTop] : scene_layer));
	}

	arrangelayers(l->mon);
}

static void
destroypopup(struct wl_listener *listener, void *data)
{
	Popup *popup;
	popup = wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->commit.link);
	wl_list_remove(&popup->destroy.link);
	free(popup);
}

static void
commitpopup(struct wl_listener *listener, void *data)
{
	Popup *p;
	struct wlr_xdg_popup *popup;
	LayerSurface *l = NULL;
	Client *c = NULL;
	struct wlr_box box;
	int type = -1;

	p = wl_container_of(listener, p, commit);
	popup = p->xdg_popup;

	if (!popup->base->initial_commit)
		return;

	type = toplevel_from_wlr_surface(popup->base->surface, &c, &l);
	if (!popup->parent || type < 0)
		return;
	popup->base->surface->data = wlr_scene_xdg_surface_create(
			popup->parent->data, popup->base);
	if ((l && !l->mon) || (c && !c->mon)) {
		wlr_xdg_popup_destroy(popup);
		return;
	}
	box = type == LayerShell ? l->mon->m : c->mon->w;
	box.x -= (type == LayerShell ? l->scene->node.x : c->geom.x);
	box.y -= (type == LayerShell ? l->scene->node.y : c->geom.y);
	wlr_xdg_popup_unconstrain_from_box(popup, &box);
}

static void
createdecoration(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;
	Client *c = deco->toplevel->base->data;
	c->decoration = deco;

	LISTEN(&deco->events.request_mode, &c->set_decoration_mode, requestdecorationmode);
	LISTEN(&deco->events.destroy, &c->destroy_decoration, destroydecoration);

	requestdecorationmode(&c->set_decoration_mode, deco);
}

static void
createkeyboard(struct wlr_keyboard *keyboard)
{

	wlr_keyboard_set_keymap(keyboard, kb_group->wlr_group->keyboard.keymap);
	wlr_keyboard_group_add_keyboard(kb_group->wlr_group, keyboard);
}

static KeyboardGroup *
createkeyboardgroup(void)
{
	KeyboardGroup *group = ecalloc(1, sizeof(*group));
	struct xkb_context *context;
	struct xkb_keymap *keymap;

	group->wlr_group = wlr_keyboard_group_create();
	group->wlr_group->data = group;
	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!(keymap = xkb_keymap_new_from_names(context,
				&(struct xkb_rule_names){0}, XKB_KEYMAP_COMPILE_NO_FLAGS)))
		die("failed to compile keymap");

	wlr_keyboard_set_keymap(&group->wlr_group->keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	wlr_keyboard_set_repeat_info(&group->wlr_group->keyboard, repeat_rate, repeat_delay);
	LISTEN(&group->wlr_group->keyboard.events.key, &group->key, keypress);
	LISTEN(&group->wlr_group->keyboard.events.modifiers, &group->modifiers, keypressmod);

	group->key_repeat_source = wl_event_loop_add_timer(event_loop, keyrepeat, group);
	wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);
	return group;
}

static void
createlayersurface(struct wl_listener *listener, void *data)
{
	struct wlr_layer_surface_v1 *layer_surface = data;
	LayerSurface *l;
	struct wlr_surface *surface = layer_surface->surface;
	struct wlr_scene_tree *scene_layer = layers[layermap[layer_surface->pending.layer]];

	if (!layer_surface->output
			&& !(layer_surface->output = selmon ? selmon->wlr_output : NULL)) {
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	l = layer_surface->data = ecalloc(1, sizeof(*l));
	LISTEN(&surface->events.commit, &l->surface_commit, commitlayersurfacenotify);
	LISTEN(&surface->events.unmap, &l->unmap, unmaplayersurfacenotify);
	LISTEN(&layer_surface->events.destroy, &l->destroy, destroylayersurfacenotify);

	l->layer_surface = layer_surface;
	l->mon = layer_surface->output->data;
	l->scene_layer = wlr_scene_layer_surface_v1_create(scene_layer, layer_surface);
	l->scene = l->scene_layer->tree;
	l->popups = surface->data = wlr_scene_tree_create(layer_surface->current.layer
			< ZWLR_LAYER_SHELL_V1_LAYER_TOP ? layers[LyrTop] : scene_layer);
	l->scene->node.data = l->popups->node.data = l;

	wl_list_insert(&l->mon->layers[layer_surface->pending.layer], &l->link);
	wlr_surface_send_enter(surface, layer_surface->output);
}

static void
createmon(struct wl_listener *listener, void *data)
{

	struct wlr_output *wlr_output = data;
	const MonitorRule *r;
	size_t i;
	struct wlr_output_state state;
	Monitor *m;

	if (!wlr_output_init_render(wlr_output, alloc, drw))
		return;

	m = wlr_output->data = ecalloc(1, sizeof(*m));
	m->wlr_output = wlr_output;

	for (i = 0; i < LENGTH(m->layers); i++)
		wl_list_init(&m->layers[i]);

	wlr_output_state_init(&state);

	m->tagset[0] = m->tagset[1] = 1;
	m->m.x = m->m.y = -1;
	m->mfact = 0.55f;
	m->nmaster = 1;
	for (r = monrules; r < END(monrules); r++) {
		if (!r->name || strstr(wlr_output->name, r->name)) {
			m->m.x = r->x;
			m->m.y = r->y;
			m->mfact = r->mfact;
			m->nmaster = r->nmaster;
			wlr_output_state_set_scale(&state, r->scale);
			wlr_output_state_set_transform(&state, r->rr);
			break;
		}
	}
	wlr_output_state_set_mode(&state, wlr_output_preferred_mode(wlr_output));
	LISTEN(&wlr_output->events.frame, &m->frame, rendermon);
	LISTEN(&wlr_output->events.destroy, &m->destroy, cleanupmon);
	LISTEN(&wlr_output->events.request_state, &m->request_state, requestmonstate);

	wlr_output_state_set_enabled(&state, 1);
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	wl_list_insert(&mons, &m->link);
	m->scene_output = wlr_scene_output_create(scene, wlr_output);
#ifdef WORKSPACES
	if (ext_workspace_mgr) {
		uint32_t t;
		m->ext_group = wlr_ext_workspace_group_handle_v1_create(
				ext_workspace_mgr, 0);
		wlr_ext_workspace_group_handle_v1_output_enter(m->ext_group,
				wlr_output);
		for (t = 1; t <= TAGCOUNT; t++)
			workspaces_add(m, t);
		workspaces_send(m);
	}
#endif
	if (m->m.x == -1 && m->m.y == -1)
		wlr_output_layout_add_auto(output_layout, wlr_output);
	else
		wlr_output_layout_add(output_layout, wlr_output, m->m.x, m->m.y);
}

static void
createnotify(struct wl_listener *listener, void *data)
{

	struct wlr_xdg_toplevel *toplevel = data;
	Client *c = NULL;
	c = toplevel->base->data = ecalloc(1, sizeof(*c));
	c->surface.xdg = toplevel->base;
	c->bw = borderpx;

	LISTEN(&toplevel->base->surface->events.commit, &c->commit, commitnotify);
	LISTEN(&toplevel->base->surface->events.map, &c->map, mapnotify);
	LISTEN(&toplevel->base->surface->events.unmap, &c->unmap, unmapnotify);
	LISTEN(&toplevel->events.destroy, &c->destroy, destroynotify);
	LISTEN(&toplevel->events.request_fullscreen, &c->fullscreen, fullscreennotify);
	LISTEN(&toplevel->events.request_maximize, &c->maximize, maximizenotify);
}

static void
createpointer(struct wlr_pointer *pointer)
{
	struct libinput_device *device;
	if (wlr_input_device_is_libinput(&pointer->base)
			&& (device = wlr_libinput_get_device_handle(&pointer->base))) {

		if (libinput_device_config_tap_get_finger_count(device)) {
			libinput_device_config_tap_set_enabled(device, tap_to_click);
			libinput_device_config_tap_set_drag_enabled(device, tap_and_drag);
			libinput_device_config_tap_set_drag_lock_enabled(device, drag_lock);
			libinput_device_config_tap_set_button_map(device, button_map);
		}

		if (libinput_device_config_scroll_has_natural_scroll(device))
			libinput_device_config_scroll_set_natural_scroll_enabled(device, natural_scrolling);

		if (libinput_device_config_dwt_is_available(device))
			libinput_device_config_dwt_set_enabled(device, disable_while_typing);

		if (libinput_device_config_left_handed_is_available(device))
			libinput_device_config_left_handed_set(device, left_handed);

		if (libinput_device_config_middle_emulation_is_available(device))
			libinput_device_config_middle_emulation_set_enabled(device, middle_button_emulation);

		if (libinput_device_config_scroll_get_methods(device) != LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
			libinput_device_config_scroll_set_method(device, scroll_method);

		if (libinput_device_config_click_get_methods(device) != LIBINPUT_CONFIG_CLICK_METHOD_NONE)
			libinput_device_config_click_set_method(device, click_method);

		if (libinput_device_config_send_events_get_modes(device))
			libinput_device_config_send_events_set_mode(device, send_events_mode);

		if (libinput_device_config_accel_is_available(device)) {
			libinput_device_config_accel_set_profile(device,
					LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
			libinput_device_config_accel_set_speed(device, accel_speed);
		}
	}

	wlr_cursor_attach_input_device(cursor, &pointer->base);
}

static void
createpointerconstraint(struct wl_listener *listener, void *data)
{
	PointerConstraint *pointer_constraint = ecalloc(1, sizeof(*pointer_constraint));
	pointer_constraint->constraint = data;
	LISTEN(&pointer_constraint->constraint->events.destroy,
			&pointer_constraint->destroy, destroypointerconstraint);
}

static void
createpopup(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_popup *xdg_popup;
	Popup *popup;

	xdg_popup = data;
	popup = ecalloc(1, sizeof(*popup));
	popup->xdg_popup = xdg_popup;
	popup->commit.notify = commitpopup;
	wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);
	popup->destroy.notify = destroypopup;
	wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
}

static void
destroydecoration(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, destroy_decoration);

	wl_list_remove(&c->destroy_decoration.link);
	wl_list_remove(&c->set_decoration_mode.link);
}

static void
destroydragicon(struct wl_listener *listener, void *data)
{

	focusclient(focustop(selmon), 1);
	motionnotify(0, NULL, 0, 0, 0, 0);
	wl_list_remove(&listener->link);
	free(listener);
}

static void
destroylayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *l = wl_container_of(listener, l, destroy);

	wl_list_remove(&l->link);
	wl_list_remove(&l->destroy.link);
	wl_list_remove(&l->unmap.link);
	wl_list_remove(&l->surface_commit.link);
	wlr_scene_node_destroy(&l->scene->node);
	wlr_scene_node_destroy(&l->popups->node);
	free(l);
}

static void
destroynotify(struct wl_listener *listener, void *data)
{

	Client *c = wl_container_of(listener, c, destroy);
	tween_cancel(c);

#ifdef FOREIGN_TOPLEVEL
	if (c->toplevel_handle)
		wlr_foreign_toplevel_handle_v1_destroy(c->toplevel_handle);
#endif

	wl_list_remove(&c->destroy.link);
	wl_list_remove(&c->fullscreen.link);
#ifdef XWAYLAND
	if (c->type != XDGShell) {
		wl_list_remove(&c->activate.link);
		wl_list_remove(&c->associate.link);
		wl_list_remove(&c->configure.link);
		wl_list_remove(&c->dissociate.link);

	} else
#endif
	{
		wl_list_remove(&c->commit.link);
		wl_list_remove(&c->map.link);
		wl_list_remove(&c->unmap.link);
		wl_list_remove(&c->maximize.link);
	}
	free(c);
}

static void
cursorwarptohint(void)
{
	Client *c = NULL;
	double sx = active_constraint->current.cursor_hint.x;
	double sy = active_constraint->current.cursor_hint.y;

	toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
	if (c && active_constraint->current.cursor_hint.enabled) {
		wlr_cursor_warp(cursor, NULL, sx + c->geom.x + c->bw, sy + c->geom.y + c->bw);
		wlr_seat_pointer_warp(active_constraint->seat, sx, sy);
	}
}

static void
destroypointerconstraint(struct wl_listener *listener, void *data)
{
	PointerConstraint *pointer_constraint = wl_container_of(listener, pointer_constraint, destroy);

	if (active_constraint == pointer_constraint->constraint) {
		cursorwarptohint();
		active_constraint = NULL;
	}

	wl_list_remove(&pointer_constraint->destroy.link);
	free(pointer_constraint);
}

static Monitor *
dirtomon(enum wlr_direction dir)
{
	struct wlr_output *next;
	if (!wlr_output_layout_get(output_layout, selmon->wlr_output))
		return selmon;
	if ((next = wlr_output_layout_adjacent_output(output_layout,
			dir, selmon->wlr_output, selmon->m.x, selmon->m.y)))
		return next->data;
	if ((next = wlr_output_layout_farthest_output(output_layout,
			dir ^ (WLR_DIRECTION_LEFT|WLR_DIRECTION_RIGHT),
			selmon->wlr_output, selmon->m.x, selmon->m.y)))
		return next->data;
	return selmon;
}

static void
focusmon(const Arg *arg)
{
	int i = 0, nmons = wl_list_length(&mons);
	if (nmons) {
		do
			selmon = dirtomon(arg->i);
		while (!selmon->wlr_output->enabled && i++ < nmons);
	}
	focusclient(focustop(selmon), 1);
#ifdef WORKSPACES
	workspaces_send(selmon);
#endif
}

static void
focusstack(const Arg *arg)
{

	Client *c, *sel = focustop(selmon);
	if (!sel || (sel->isfullscreen && !client_has_children(sel)))
		return;
	if (arg->i > 0) {
		wl_list_for_each(c, &sel->link, link) {
			if (&c->link == &clients)
				continue;
			if (VISIBLEON(c, selmon))
				break;
		}
	} else {
		wl_list_for_each_reverse(c, &sel->link, link) {
			if (&c->link == &clients)
				continue;
			if (VISIBLEON(c, selmon))
				break;
		}
	}

	focusclient(c, 1);
}

static void
fullscreennotify(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, fullscreen);
	c->pending_fullscreen = client_wants_fullscreen(c);
	if (client_surface(c)->mapped)
		setfullscreen(c, c->pending_fullscreen);
}

static void
quit(const Arg *arg)
{
	wl_display_terminate(dpy);
}

static void
handlesig(int signo)
{
	if (signo == SIGCHLD)
		while (waitpid(-1, NULL, WNOHANG) > 0);
	else if (signo == SIGINT || signo == SIGTERM)
		quit(NULL);
}

static void
incnmaster(const Arg *arg)
{
	if (!arg || !selmon)
		return;
	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
	motionnotify(0, NULL, 0, 0, 0, 0);
}

static void
inputdevice(struct wl_listener *listener, void *data)
{

	struct wlr_input_device *device = data;
	uint32_t caps;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		createkeyboard(wlr_keyboard_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_POINTER:
		createpointer(wlr_pointer_from_input_device(device));
		break;
	default:

		break;
	}

	caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&kb_group->wlr_group->devices))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(seat, caps);
}

static void
killclient(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (sel)
		client_send_close(sel);
}

static void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon)
		return;
	f = arg->f < 1.0f ? arg->f + selmon->mfact : arg->f - 1.0f;
	if (f < 0.1 || f > 0.9)
		return;
	selmon->mfact = f;
	arrange(selmon);
	motionnotify(0, NULL, 0, 0, 0, 0);
}

static void
maximizenotify(struct wl_listener *listener, void *data)
{

	Client *c = wl_container_of(listener, c, maximize);
	if (c->surface.xdg->initialized
			&& wl_resource_get_version(c->surface.xdg->toplevel->resource)
					< XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)
		wlr_xdg_surface_schedule_configure(c->surface.xdg);
}

static void
outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int test)
{

	struct wlr_output_configuration_head_v1 *config_head;
	int ok = 1;

	wl_list_for_each(config_head, &config->heads, link) {
		struct wlr_output *wlr_output = config_head->state.output;
		Monitor *m = wlr_output->data;
		struct wlr_output_state state;

		wlr_output_state_init(&state);
		wlr_output_state_set_enabled(&state, config_head->state.enabled);
		if (!config_head->state.enabled)
			goto apply_or_test;

		if (config_head->state.mode)
			wlr_output_state_set_mode(&state, config_head->state.mode);
		else
			wlr_output_state_set_custom_mode(&state,
					config_head->state.custom_mode.width,
					config_head->state.custom_mode.height,
					config_head->state.custom_mode.refresh);

		wlr_output_state_set_transform(&state, config_head->state.transform);
		wlr_output_state_set_scale(&state, config_head->state.scale);
		wlr_output_state_set_adaptive_sync_enabled(&state,
				config_head->state.adaptive_sync_enabled);

apply_or_test:
		ok &= test ? wlr_output_test_state(wlr_output, &state)
				: wlr_output_commit_state(wlr_output, &state);
		if (!test && wlr_output->enabled && (m->m.x != config_head->state.x || m->m.y != config_head->state.y))
			wlr_output_layout_add(output_layout, wlr_output,
					config_head->state.x, config_head->state.y);

		wlr_output_state_finish(&state);
	}

	if (ok)
		wlr_output_configuration_v1_send_succeeded(config);
	else
		wlr_output_configuration_v1_send_failed(config);
	wlr_output_configuration_v1_destroy(config);
	updatemons(NULL, NULL);
}

static void
outputmgrapply(struct wl_listener *listener, void *data)
{
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 0);
}

static void
outputmgrtest(struct wl_listener *listener, void *data)
{
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 1);
}

static void
requeststartdrag(struct wl_listener *listener, void *data)
{
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(seat, event->origin,
			event->serial))
		wlr_seat_start_pointer_drag(seat, event->drag, event->serial);
	else
		wlr_data_source_destroy(event->drag->source);
}

static void
requestmonstate(struct wl_listener *listener, void *data)
{
	struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(event->output, event->state);
	updatemons(NULL, NULL);
}

void
die(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

int
fd_set_nonblock(int fd) {
	int flags = fcntl(fd, F_GETFL);
	if (flags < 0) {
		perror("fcntl(F_GETFL):");
		return -1;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		perror("fcntl(F_SETFL):");
		return -1;
	}

	return 0;
}

static void
run(void)
{

	const char *socket = wl_display_add_socket_auto(dpy);
	if (!socket)
		die("startup: display_add_socket_auto");
	setenv("WAYLAND_DISPLAY", socket, 1);
	if (!wlr_backend_start(backend))
		die("startup: backend_start");

	for (size_t i = 0; i < LENGTH(startupapps); i++)
		spawn(&startupapps[i]);

	if (fd_set_nonblock(STDOUT_FILENO) < 0)
		close(STDOUT_FILENO);
	selmon = xytomon(cursor->x, cursor->y);
	wlr_cursor_warp_closest(cursor, NULL, cursor->x, cursor->y);
	wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
	wl_display_run(dpy);
}

static void
setcursor(struct wl_listener *listener, void *data)
{

	struct wlr_seat_pointer_request_set_cursor_event *event = data;

	if (event->seat_client == seat->pointer_state.focused_client)
		wlr_cursor_set_surface(cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
}

static void
setcursorshape(struct wl_listener *listener, void *data)
{
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;

	if (event->seat_client == seat->pointer_state.focused_client)
		wlr_cursor_set_xcursor(cursor, cursor_mgr,
				wlr_cursor_shape_v1_name(event->shape));
}

static void
setpsel(struct wl_listener *listener, void *data)
{

	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat, event->source, event->serial);
}

static void
setsel(struct wl_listener *listener, void *data)
{

	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat, event->source, event->serial);
}

static void
setup(void)
{
	int drm_fd, i, sig[] = {SIGCHLD, SIGINT, SIGTERM, SIGPIPE};
	struct sigaction sa = {.sa_flags = SA_RESTART, .sa_handler = handlesig};
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < (int)LENGTH(sig); i++)
		sigaction(sig[i], &sa, NULL);
	dpy = wl_display_create();
	event_loop = wl_display_get_event_loop(dpy);
	if (!(backend = wlr_backend_autocreate(event_loop, &session)))
		die("couldn't create backend");
	scene = wlr_scene_create();
	root_bg = wlr_scene_rect_create(&scene->tree, 0, 0, rootcolor);
	for (i = 0; i < NUM_LAYERS; i++)
		layers[i] = wlr_scene_tree_create(&scene->tree);
	drag_icon = wlr_scene_tree_create(&scene->tree);
	if (!(drw = wlr_renderer_autocreate(backend)))
		die("couldn't create renderer");
	wlr_renderer_init_wl_shm(drw, dpy);

	if (wlr_renderer_get_texture_formats(drw, WLR_BUFFER_CAP_DMABUF)) {
		wlr_scene_set_linux_dmabuf_v1(scene,
				wlr_linux_dmabuf_v1_create_with_renderer(dpy, 5, drw));
	}

	if ((drm_fd = wlr_renderer_get_drm_fd(drw)) >= 0 && drw->features.timeline
			&& backend->features.timeline)
		wlr_linux_drm_syncobj_manager_v1_create(dpy, 1, drm_fd);
	if (!(alloc = wlr_allocator_autocreate(backend, drw)))
		die("couldn't create allocator");
	compositor = wlr_compositor_create(dpy, 6, drw);
	wlr_subcompositor_create(dpy);
	wlr_data_device_manager_create(dpy);
	wlr_screencopy_manager_v1_create(dpy);
	wlr_data_control_manager_v1_create(dpy);
	wlr_ext_data_control_manager_v1_create(dpy, 1);
	wlr_primary_selection_v1_device_manager_create(dpy);
	wlr_viewporter_create(dpy);
	wlr_single_pixel_buffer_manager_v1_create(dpy);
	wlr_presentation_create(dpy, backend, 2);

#ifdef FOREIGN_TOPLEVEL
	toplevel_manager = wlr_foreign_toplevel_manager_v1_create(dpy);
#endif

#ifdef WORKSPACES
	ext_workspace_mgr = wlr_ext_workspace_manager_v1_create(dpy, 1);
	wl_list_init(&ext_workspaces);
	LISTEN_STATIC(&ext_workspace_mgr->events.commit, workspaces_handle_commit);
#endif

	wlr_scene_set_gamma_control_manager_v1(scene, wlr_gamma_control_manager_v1_create(dpy));
	output_layout = wlr_output_layout_create(dpy);
	wl_signal_add(&output_layout->events.change, &layout_change);

	wlr_xdg_output_manager_v1_create(dpy, output_layout);
	wl_list_init(&mons);
	wl_signal_add(&backend->events.new_output, &new_output);
	wl_list_init(&clients);
	wl_list_init(&fstack);

	xdg_shell = wlr_xdg_shell_create(dpy, 6);
	wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);
	wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

	layer_shell = wlr_layer_shell_v1_create(dpy, 3);
	wl_signal_add(&layer_shell->events.new_surface, &new_layer_surface);
	wlr_server_decoration_manager_set_default_mode(
			wlr_server_decoration_manager_create(dpy),
			WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(dpy, 2);
	wl_signal_add(&xdg_decoration_mgr->events.new_toplevel_decoration,
			&new_xdg_decoration);
	unsetenv("DISPLAY");

}

static void
setupinput(void)
{	pointer_constraints = wlr_pointer_constraints_v1_create(dpy);
	wl_signal_add(&pointer_constraints->events.new_constraint, &new_pointer_constraint);

	relative_pointer_mgr = wlr_relative_pointer_manager_v1_create(dpy);
	cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(cursor, output_layout);
	cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	setenv("XCURSOR_SIZE", "24", 1);
	wl_signal_add(&cursor->events.motion, &cursor_motion);
	wl_signal_add(&cursor->events.motion_absolute, &cursor_motion_absolute);
	wl_signal_add(&cursor->events.button, &cursor_button);
	wl_signal_add(&cursor->events.axis, &cursor_axis);
	wl_signal_add(&cursor->events.frame, &cursor_frame);

	cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(dpy, 1);
	wl_signal_add(&cursor_shape_mgr->events.request_set_shape, &request_set_cursor_shape);
	wl_signal_add(&backend->events.new_input, &new_input_device);

	seat = wlr_seat_create(dpy, "seat0");
	wl_signal_add(&seat->events.request_set_cursor, &request_cursor);
	wl_signal_add(&seat->events.request_set_selection, &request_set_sel);
	wl_signal_add(&seat->events.request_set_primary_selection, &request_set_psel);
	wl_signal_add(&seat->events.request_start_drag, &request_start_drag);
	wl_signal_add(&seat->events.start_drag, &start_drag);

	kb_group = createkeyboardgroup();
	wl_list_init(&kb_group->destroy.link);

	output_mgr = wlr_output_manager_v1_create(dpy);
	wl_signal_add(&output_mgr->events.apply, &output_mgr_apply);
	wl_signal_add(&output_mgr->events.test, &output_mgr_test);
}

#ifdef XWAYLAND
static void
setupx(void)
{
	if ((xwayland = wlr_xwayland_create(dpy, compositor, 1))) {
		wl_signal_add(&xwayland->events.ready, &xwayland_ready);
		wl_signal_add(&xwayland->events.new_surface, &new_xwayland_surface);
		setenv("DISPLAY", xwayland->display_name, 1);
	} else {
		fprintf(stderr, "failed to setup XWayland X server, continuing without it\n");
	}
}
#endif

static void
spawn(const Arg *arg)
{
	if (fork() == 0) {
		dup2(STDERR_FILENO, STDOUT_FILENO);
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("dwl: execvp %s failed:", ((char **)arg->v)[0]);
	}
}

static void
startdrag(struct wl_listener *listener, void *data)
{
	struct wlr_drag *drag = data;
	if (!drag->icon)
		return;

	drag->icon->data = &wlr_scene_drag_icon_create(drag_icon, drag->icon)->node;
	LISTEN_STATIC(&drag->icon->events.destroy, destroydragicon);
}

static void
tagmon(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (sel)
		setmon(sel, dirtomon(arg->i), 0);
}

static void
togglefullscreen(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (sel)
		setfullscreen(sel, !sel->isfullscreen);
}

#ifdef FULLSCREEN_TEARING
static void
enabletearing(const Arg *arg)
{
	tearing_enabled = 1;
}

static void
disabletearing(const Arg *arg)
{
	tearing_enabled = 0;
}

static int
moncantear(Monitor *m)
{
	Client *c = focustop(m);
	return tearing_enabled && c && c->isfullscreen;
}
#endif

#ifdef WORKSPACES
static void
workspaces_toggle_tag(uint32_t tagbit)
{
	if (!selmon || !(tagbit & TAGMASK))
		return;
	selmon->seltags ^= 1;
	selmon->tagset[selmon->seltags] =
		(selmon->tagset[selmon->seltags ^ 1] ^ tagbit) & TAGMASK;
	arrange(selmon);
	focusclient(focustop(selmon), 1);
}

static void
workspaces_status(Monitor *m)
{
	struct JtlWorkspace *ws;
	if (!ext_workspace_mgr || !m)
		return;
	wl_list_for_each(ws, &ext_workspaces, link) {
		uint32_t tagbit = 1u << (ws->tag - 1);
		Client *c;
		int has_win = 0;
		wl_list_for_each(c, &clients, link)
			if (c->mon == m && (c->tags & tagbit & TAGMASK)) {
				has_win = 1;
				break;
			}
		wlr_ext_workspace_handle_v1_set_hidden(ws->ext_workspace, !has_win);
		wlr_ext_workspace_handle_v1_set_urgent(ws->ext_workspace, false);
		wlr_ext_workspace_handle_v1_set_active(ws->ext_workspace,
				(m->tagset[m->seltags] & tagbit) != 0);
	}
}

static void
workspaces_send(Monitor *m)
{
	Monitor *mm;
	if (!m) {
		wl_list_for_each(mm, &mons, link)
			workspaces_status(mm);
		return;
	}
	workspaces_status(m);
}

static void
workspaces_handle_commit(struct wl_listener *listener, void *data)
{
	struct wlr_ext_workspace_v1_commit_event *event = data;
	struct wlr_ext_workspace_v1_request *request;
	struct JtlWorkspace *ws;
	(void)listener;

	wl_list_for_each(request, event->requests, link) {
		switch (request->type) {
		case WLR_EXT_WORKSPACE_V1_REQUEST_ACTIVATE:
			wl_list_for_each(ws, &ext_workspaces, link)
				if (ws->ext_workspace == request->activate.workspace) {
					view(&(Arg){ .ui = 1u << (ws->tag - 1) });
					break;
				}
			break;
		case WLR_EXT_WORKSPACE_V1_REQUEST_DEACTIVATE:
			wl_list_for_each(ws, &ext_workspaces, link)
				if (ws->ext_workspace == request->deactivate.workspace) {
					workspaces_toggle_tag(1u << (ws->tag - 1));
					break;
				}
			break;
		default:
			break;
		}
	}
}

static void
workspaces_add(Monitor *m, uint32_t tag)
{
	struct JtlWorkspace *ws;
	char name[8];
	snprintf(name, sizeof(name), "%u", tag);

	ws = ecalloc(1, sizeof(*ws));
	ws->tag = tag;
	ws->m = m;
	ws->ext_workspace = wlr_ext_workspace_handle_v1_create(
			ext_workspace_mgr, name, EXT_WORKSPACE_ENABLE_CAPS);
	ws->ext_workspace->data = ws;

	wlr_ext_workspace_handle_v1_set_group(ws->ext_workspace, m->ext_group);
	wlr_ext_workspace_handle_v1_set_name(ws->ext_workspace, name);
	wl_list_insert(ext_workspaces.prev, &ws->link);
}

static void
workspaces_remove_for_monitor(Monitor *m)
{
	struct JtlWorkspace *ws, *tmp;
	wl_list_for_each_safe(ws, tmp, &ext_workspaces, link)
		if (ws->m == m) {
			wlr_ext_workspace_handle_v1_destroy(ws->ext_workspace);
			wl_list_remove(&ws->link);
			free(ws);
		}
}
#endif

static void
view(const Arg *arg)
{
	if (!selmon || (arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1;
	if (arg->ui & TAGMASK)
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
	arrange(selmon);
	focusclient(focustop(selmon), 1);
#ifdef WORKSPACES
	workspaces_send(selmon);
#endif
}

static void
tag(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (!sel || (arg->ui & TAGMASK) == 0)
		return;

	sel->tags = arg->ui & TAGMASK;
	{
		Client *w;
		wl_list_for_each(w, &clients, link) {
			if (w != sel && w->isfullscreen && w->mon == sel->mon
					&& (w->tags & sel->tags))
				setfullscreen(w, 0);
		}
	}
	focusclient(focustop(selmon), 1);
	arrange(selmon);
#ifdef WORKSPACES
	workspaces_send(selmon);
#endif
}

static void
zoom(const Arg *arg)
{
	Client *c, *sel = focustop(selmon);

	if (!sel || !selmon)
		return;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, selmon)) {
			if (c != sel)
				break;
			sel = NULL;
		}
	}
	if (&c->link == &clients)
		return;
	if (!sel)
		sel = c;
	wl_list_remove(&sel->link);
	wl_list_insert(&clients, &sel->link);

	focusclient(sel, 1);
	arrange(selmon);
}

#ifdef XWAYLAND
static void
activatex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, activate);
	if (!client_is_unmanaged(c))
		wlr_xwayland_surface_activate(c->surface.xwayland, 1);
}

static void
associatex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, associate);
	LISTEN(&client_surface(c)->events.map, &c->map, mapnotify);
	LISTEN(&client_surface(c)->events.unmap, &c->unmap, unmapnotify);
}

static void
configurex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, configure);
	struct wlr_xwayland_surface_configure_event *event = data;

	if (!client_surface(c) || !client_surface(c)->mapped) {
		wlr_xwayland_surface_configure(c->surface.xwayland,
				event->x, event->y, event->width, event->height);
		return;
	}

	if (client_is_unmanaged(c)) {
		wlr_scene_node_set_position(&c->scene->node, event->x, event->y);
		wlr_xwayland_surface_configure(c->surface.xwayland,
				event->x, event->y, event->width, event->height);
		return;
	}

	if (c->isfloating) {
		resize(c, (struct wlr_box){.x = event->x - c->bw,
				.y = event->y - c->bw, .width = event->width + c->bw * 2,
				.height = event->height + c->bw * 2});
	} else {
		arrange(c->mon);
	}
}

static void
createnotifyx11(struct wl_listener *listener, void *data)
{
	struct wlr_xwayland_surface *xsurface = data;
	Client *c;

	c = xsurface->data = ecalloc(1, sizeof(*c));
	c->surface.xwayland = xsurface;
	c->type = X11;
	c->bw = client_is_unmanaged(c) ? 0 : borderpx;

	LISTEN(&xsurface->events.associate, &c->associate, associatex11);
	LISTEN(&xsurface->events.destroy, &c->destroy, destroynotify);
	LISTEN(&xsurface->events.dissociate, &c->dissociate, dissociatex11);
	LISTEN(&xsurface->events.request_activate, &c->activate, activatex11);
	LISTEN(&xsurface->events.request_configure, &c->configure, configurex11);
	LISTEN(&xsurface->events.request_fullscreen, &c->fullscreen, fullscreennotify);

}

static void
dissociatex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, dissociate);
	wl_list_remove(&c->map.link);
	wl_list_remove(&c->unmap.link);
}

static void
xwaylandready(struct wl_listener *listener, void *data)
{
	struct wlr_xcursor *xcursor;

	wlr_xwayland_set_seat(xwayland, seat);

	if ((xcursor = wlr_xcursor_manager_get_xcursor(cursor_mgr, "default", 1)))
		wlr_xwayland_set_cursor(xwayland, wlr_xcursor_image_get_buffer(xcursor->images[0]),
				xcursor->images[0]->hotspot_x, xcursor->images[0]->hotspot_y);
}
#endif

static int
client_is_x11(Client *c)
{
#ifdef XWAYLAND
	return c->type == X11;
#endif
	return 0;
}

static struct wlr_surface *
client_surface(Client *c)
{
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->surface;
#endif
	return c->surface.xdg->surface;
}

static int
toplevel_from_wlr_surface(struct wlr_surface *s, Client **pc, LayerSurface **pl)
{
	struct wlr_xdg_surface *xdg_surface, *tmp_xdg_surface;
	struct wlr_surface *root_surface;
	struct wlr_layer_surface_v1 *layer_surface;
	Client *c = NULL;
	LayerSurface *l = NULL;
	int type = -1;
#ifdef XWAYLAND
	struct wlr_xwayland_surface *xsurface;
#endif

	if (!s)
		return -1;
	root_surface = wlr_surface_get_root_surface(s);

#ifdef XWAYLAND
	if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(root_surface))) {
		c = xsurface->data;
		type = c->type;
		goto end;
	}
#endif

	if ((layer_surface = wlr_layer_surface_v1_try_from_wlr_surface(root_surface))) {
		l = layer_surface->data;
		type = LayerShell;
		goto end;
	}

	xdg_surface = wlr_xdg_surface_try_from_wlr_surface(root_surface);
	while (xdg_surface) {
		tmp_xdg_surface = NULL;
		switch (xdg_surface->role) {
		case WLR_XDG_SURFACE_ROLE_POPUP:
			if (!xdg_surface->popup || !xdg_surface->popup->parent)
				return -1;

			tmp_xdg_surface = wlr_xdg_surface_try_from_wlr_surface(xdg_surface->popup->parent);

			if (!tmp_xdg_surface)
				return toplevel_from_wlr_surface(xdg_surface->popup->parent, pc, pl);

			xdg_surface = tmp_xdg_surface;
			break;
		case WLR_XDG_SURFACE_ROLE_TOPLEVEL:
			c = xdg_surface->data;
			type = c->type;
			goto end;
		case WLR_XDG_SURFACE_ROLE_NONE:
			return -1;
		}
	}

end:
	if (pl)
		*pl = l;
	if (pc)
		*pc = c;
	return type;
}

static void
client_activate_surface(struct wlr_surface *s, int activated)
{
	struct wlr_xdg_toplevel *toplevel;
#ifdef XWAYLAND
	struct wlr_xwayland_surface *xsurface;
	if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(s))) {
		wlr_xwayland_surface_activate(xsurface, activated);
		return;
	}
#endif
	if ((toplevel = wlr_xdg_toplevel_try_from_wlr_surface(s)))
		wlr_xdg_toplevel_set_activated(toplevel, activated);
}

static void
client_set_bounds(Client *c, int32_t width, int32_t height)
{
#ifdef XWAYLAND
	if (client_is_x11(c))
		return;
#endif
	if (wl_resource_get_version(c->surface.xdg->toplevel->resource) >=
			XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION && width >= 0 && height >= 0
			&& (c->bounds.width != width || c->bounds.height != height)) {
		c->bounds.width = width;
		c->bounds.height = height;
		wlr_xdg_toplevel_set_bounds(c->surface.xdg->toplevel, width, height);
	}
}

static uint32_t
client_set_size(Client *c, uint32_t width, uint32_t height)
{
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wlr_xwayland_surface_configure(c->surface.xwayland,
				c->geom.x + c->bw, c->geom.y + c->bw, width, height);
		return 0;
	}
#endif
	if ((int32_t)width == c->surface.xdg->toplevel->current.width
			&& (int32_t)height == c->surface.xdg->toplevel->current.height)
		return 0;
	return wlr_xdg_toplevel_set_size(c->surface.xdg->toplevel, (int32_t)width, (int32_t)height);
}

static void
client_set_scale(struct wlr_surface *s, int32_t scale)
{
	wlr_surface_set_preferred_buffer_scale(s, scale);
}

static void
client_get_geometry(Client *c, struct wlr_box *geom)
{
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		geom->x = c->surface.xwayland->x;
		geom->y = c->surface.xwayland->y;
		geom->width = c->surface.xwayland->width;
		geom->height = c->surface.xwayland->height;
		return;
	}
#endif
	*geom = c->surface.xdg->geometry;
}

static void
client_set_suspended(Client *c, int suspended)
{
#ifdef XWAYLAND
	if (client_is_x11(c))
		return;
#endif
	wlr_xdg_toplevel_set_suspended(c->surface.xdg->toplevel, suspended);
}

static void
client_notify_enter(struct wlr_surface *s, struct wlr_keyboard *kb)
{
	if (kb)
		wlr_seat_keyboard_notify_enter(seat, s, kb->keycodes,
				kb->num_keycodes, &kb->modifiers);
	else
		wlr_seat_keyboard_notify_enter(seat, s, NULL, 0, NULL);
}

static int
client_is_unmanaged(Client *c)
{
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->override_redirect;
#endif
	return 0;
}

static int
client_wants_focus(Client *c)
{
#ifdef XWAYLAND
	return client_is_unmanaged(c)
		&& wlr_xwayland_surface_override_redirect_wants_focus(c->surface.xwayland)
		&& wlr_xwayland_surface_icccm_input_model(c->surface.xwayland) != WLR_ICCCM_INPUT_MODEL_NONE;
#endif
	return 0;
}

static int
client_has_children(Client *c)
{
#ifdef XWAYLAND
	if (client_is_x11(c))
		return !wl_list_empty(&c->surface.xwayland->children);
#endif
	return wl_list_length(&c->surface.xdg->link) > 1;
}

static int
client_wants_fullscreen(Client *c)
{
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->fullscreen;
#endif
	return c->surface.xdg->toplevel->requested.fullscreen;
}

static void
client_send_close(Client *c)
{
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wlr_xwayland_surface_close(c->surface.xwayland);
		return;
	}
#endif
	wlr_xdg_toplevel_send_close(c->surface.xdg->toplevel);
}

static void
client_set_fullscreen(Client *c, int fullscreen)
{
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wlr_xwayland_surface_set_fullscreen(c->surface.xwayland, fullscreen);
		return;
	}
#endif
	wlr_xdg_toplevel_set_fullscreen(c->surface.xdg->toplevel, fullscreen);
}

static void
client_set_border_color(Client *c, const float color[static 4])
{
	int i;
	if (client_is_unmanaged(c))
		return;
	for (i = 0; i < 4; i++)
		wlr_scene_rect_set_color(c->border[i], color);
}

static void
client_set_tiled(Client *c, uint32_t edges)
{
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wlr_xwayland_surface_set_maximized(c->surface.xwayland,
				edges != WLR_EDGE_NONE, edges != WLR_EDGE_NONE);
		return;
	}
#endif
	if (wl_resource_get_version(c->surface.xdg->toplevel->resource)
			>= XDG_TOPLEVEL_STATE_TILED_RIGHT_SINCE_VERSION) {
		wlr_xdg_toplevel_set_tiled(c->surface.xdg->toplevel, edges);
	} else {
		wlr_xdg_toplevel_set_maximized(c->surface.xdg->toplevel, edges != WLR_EDGE_NONE);
	}
}

static Client *
client_get_parent(Client *c)
{
	Client *p = NULL;
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		if (c->surface.xwayland->parent)
			toplevel_from_wlr_surface(c->surface.xwayland->parent->surface, &p, NULL);
		return p;
	}
#endif
	if (c->surface.xdg->toplevel->parent)
		toplevel_from_wlr_surface(c->surface.xdg->toplevel->parent->base->surface, &p, NULL);
	return p;
}

static Client *
focustop(Monitor *m)
{
	Client *c;
	wl_list_for_each(c, &fstack, flink) {
		if (VISIBLEON(c, m))
			return c;
	}
	return NULL;
}

static int
deactivateold(struct wlr_surface *old, Client *old_c,
		LayerSurface *old_l, int old_client_type, Client *c)
{
	int dummy_lx, dummy_ly;

	if (!old || (c && client_surface(c) == old))
		return 0;

	if (old_client_type == LayerShell && wlr_scene_node_coords(
				&old_l->scene->node, &dummy_lx, &dummy_ly)
			&& old_l->layer_surface->current.layer
			>= ZWLR_LAYER_SHELL_V1_LAYER_TOP)
		return 1;

	if (old_c)
		client_set_border_color(old_c, bordercolor);

#ifdef FOREIGN_TOPLEVEL
	if (old_c && old_c->toplevel_handle)
		wlr_foreign_toplevel_handle_v1_set_activated(old_c->toplevel_handle, 0);
#endif

	client_activate_surface(old, 0);
	return 0;
}

static void
killpopups(Client *c)
{
#ifdef XWAYLAND
	Client *tmp;
#endif
	if (c && !client_is_x11(c)) {
		struct wlr_xdg_popup *p, *ptmp;
		wl_list_for_each_safe(p, ptmp, &c->surface.xdg->popups, link)
			wlr_xdg_popup_destroy(p);
	}

#ifdef XWAYLAND
	wl_list_for_each_safe(c, tmp, &clients, link) {
		if (client_is_x11(c) && client_is_unmanaged(c))
			wlr_xwayland_surface_close(c->surface.xwayland);
	}
#endif
}

static void
applybounds(Client *c, struct wlr_box *bbox)
{

	c->geom.width = MAX(1 + 2 * (int)c->bw, c->geom.width);
	c->geom.height = MAX(1 + 2 * (int)c->bw, c->geom.height);

	if (c->geom.x >= bbox->x + bbox->width)
		c->geom.x = bbox->x + bbox->width - c->geom.width;
	if (c->geom.y >= bbox->y + bbox->height)
		c->geom.y = bbox->y + bbox->height - c->geom.height;
	if (c->geom.x + c->geom.width <= bbox->x)
		c->geom.x = bbox->x;
	if (c->geom.y + c->geom.height <= bbox->y)
		c->geom.y = bbox->y;
}

static int
rule_regex_match(const char *pattern, const char *str)
{
	regex_t re;
	int matched;

	if (!str || regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0)
		return 0;
	matched = regexec(&re, str, 0, NULL, 0) == 0;
	regfree(&re);
	return matched;
}

static void
applyrules(Client *c)
{
	const Rule *r;
	const char *app_id, *title;

	if (!c->mon)
		c->mon = selmon;
	c->tags = c->mon->tagset[c->mon->seltags];

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		app_id = c->surface.xwayland->class;
		title = c->surface.xwayland->title;
	} else
#endif
	{
		app_id = c->surface.xdg->toplevel->app_id;
		title = c->surface.xdg->toplevel->title;
	}

	for (r = rules; r < END(rules); r++) {
		if (!r->id && !r->title)
			continue;
		if (r->id && !rule_regex_match(r->id, app_id))
			continue;
		if (r->title && !rule_regex_match(r->title, title))
			continue;
		c->tags |= r->tags & TAGMASK;
		if (r->isfullscreen)
			c->pending_fullscreen = 1;
	}
}

static void
resize(Client *c, struct wlr_box geo)
{
	struct wlr_box old;

	if (!c->mon || !client_surface(c)->mapped)
		return;

	old = c->geom;
	c->geom = geo;
	applybounds(c, &c->mon->w);
	client_set_bounds(c, c->geom.width - 2 * c->bw, c->geom.height - 2 * c->bw);

	if (old.x != c->geom.x || old.y != c->geom.y
			|| old.width != c->geom.width || old.height != c->geom.height) {
		tween_start(c, c->geom.x, c->geom.y, c->geom.width, c->geom.height, 1.0);
		wlr_output_schedule_frame(c->mon->wlr_output);
	}

	c->resize = client_set_size(c, c->geom.width - 2 * c->bw, c->geom.height - 2 * c->bw);
}

static void
tile(Monitor *m)
{
	int mw, my, ty, h, r, oe, ie;
	int i, n = 0;
	Client *c;
	int OUTER_GAP_H = gappoh;
	int OUTER_GAP_V = gappov;
	int INNER_GAP_H = gappih;
	int INNER_GAP_V = gappiv;

	wl_list_for_each(c, &clients, link)
		if (VISIBLEON(c, m) && !c->isfullscreen && !c->isfloating)
			n++;
	if (n == 0)
		return;
	if (n == 1) {
		oe = 0;
	} else {
		oe = 1;
	}
	ie = 1;

	if (n > m->nmaster)
		mw = m->nmaster ? (int)((m->w.width - 2 * OUTER_GAP_H * oe
				- INNER_GAP_H * ie) * m->mfact + INNER_GAP_H * ie + 0.5f) : 0;
	else
		mw = m->w.width - 2 * OUTER_GAP_H * oe + INNER_GAP_H * ie;

	i = 0;
	my = ty = OUTER_GAP_V * oe;

	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isfullscreen || c->isfloating)
			continue;

		/* Toggle border width based on single window count */
		c->bw = (n == 1) ? 0 : borderpx;

		if (i < m->nmaster) {
			r = MIN(n, m->nmaster) - i;
			h = (m->w.height - my - OUTER_GAP_V * oe - INNER_GAP_V * ie * (r - 1)) / r;
			{
				struct wlr_box target = {.x = m->w.x + OUTER_GAP_H * oe,
					.y = m->w.y + my,
					.width = mw - INNER_GAP_H * ie, .height = h};
				if (c->geom.x != target.x || c->geom.y != target.y
						|| c->geom.width != target.width
						|| c->geom.height != target.height)
					resize(c, target);
			}
			my += c->geom.height + INNER_GAP_V * ie;
		} else {
			r = n - i;
			h = (m->w.height - ty - OUTER_GAP_V * oe - INNER_GAP_V * ie * (r - 1)) / r;
			{
				struct wlr_box target = {.x = m->w.x + mw + OUTER_GAP_H * oe,
					.y = m->w.y + ty,
					.width = m->w.width - mw - 2 * OUTER_GAP_H * oe,
					.height = h};
				if (c->geom.x != target.x || c->geom.y != target.y
						|| c->geom.width != target.width
						|| c->geom.height != target.height)
					resize(c, target);
			}
			ty += c->geom.height + INNER_GAP_V * ie;
		}
		i++;
	}
}

static void
arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area, int exclusive)
{
	LayerSurface *l;
	struct wlr_box full_area = m->m;

	wl_list_for_each(l, list, link) {
		struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;

		if (!layer_surface->initialized)
			continue;

		if (exclusive != (layer_surface->current.exclusive_zone > 0))
			continue;

		wlr_scene_layer_surface_v1_configure(l->scene_layer, &full_area, usable_area);
		wlr_scene_node_set_position(&l->popups->node, l->scene->node.x, l->scene->node.y);
	}
}

static void
arrangelayers(Monitor *m)
{
	int i;
	struct wlr_box usable_area = m->m;
	LayerSurface *l;
	static const uint32_t layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
	};
	if (!m->wlr_output->enabled)
		return;
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 1);

	if (!wlr_box_equal(&usable_area, &m->w)) {
		m->w = usable_area;
		arrange(m);
		motionnotify(0, NULL, 0, 0, 0, 0);
	}
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 0);
	for (i = 0; i < (int)LENGTH(layers_above_shell); i++) {
		wl_list_for_each_reverse(l, &m->layers[layers_above_shell[i]], link) {
			if (!l->layer_surface->current.keyboard_interactive || !l->mapped)
				continue;

			focusclient(NULL, 0);
			exclusive_focus = l;
			client_notify_enter(l->layer_surface->surface, wlr_seat_get_keyboard(seat));
			return;
		}
	}
}

static void
arrange(Monitor *m)
{
	Client *c;

	if (!m->wlr_output->enabled)
		return;

	wl_list_for_each(c, &clients, link) {
		if (c->mon == m) {
			int visible = VISIBLEON(c, m);
			wlr_scene_node_set_enabled(&c->scene->node, visible);
			client_set_suspended(c, !visible);
			if (c->scene->node.parent != layers[LyrFS]
					&& c->scene->node.parent != layers[LyrFloat]
					&& !c->isfloating)
				wlr_scene_node_reparent(&c->scene->node, layers[LyrTile]);
		}
	}

	tile(m);
}

static void
commitnotify(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, commit);

	if (c->surface.xdg->initial_commit) {

		applyrules(c);
		if (c->mon) {
			client_set_scale(client_surface(c), (int32_t)c->mon->wlr_output->scale);
		}
		wlr_xdg_toplevel_set_wm_capabilities(c->surface.xdg->toplevel,
				WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
		if (c->decoration)
			requestdecorationmode(&c->set_decoration_mode, c->decoration);
		return;
	}

	if (c->isfloating) {

		struct wlr_box old = c->geom;
		client_get_geometry(c, &c->geom);
		c->geom.x = old.x;
		c->geom.y = old.y;
		c->geom.width += 2 * c->bw;
		c->geom.height += 2 * c->bw;
		if (c->mon) {
			applybounds(c, &c->mon->w);
			if (old.x != c->geom.x || old.y != c->geom.y
					|| old.width != c->geom.width
					|| old.height != c->geom.height) {
				tween_start(c, c->geom.x, c->geom.y,
						c->geom.width, c->geom.height, 1.0);
				wlr_output_schedule_frame(c->mon->wlr_output);
			}
		}
		client_set_bounds(c, c->geom.width - 2 * c->bw,
				c->geom.height - 2 * c->bw);
	} else {
		resize(c, c->geom);
	}
	if (c->resize && c->resize <= c->surface.xdg->current.configure_serial)
		c->resize = 0;
}

static void
setfloat(Client *c)
{
	c->isfloating = 0;
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		xcb_size_hints_t *h = c->surface.xwayland->size_hints;
		if (h && (h->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)
				&& (h->flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)
				&& h->min_width > 0 && h->min_height > 0
				&& h->max_width == h->min_width
				&& h->max_height == h->min_height)
			c->isfloating = 1;
	} else
#endif
	{
		struct wlr_xdg_toplevel *t = c->surface.xdg->toplevel;
		if (t->current.max_width > 0 && t->current.max_height > 0
				&& t->current.max_width == t->current.min_width
				&& t->current.max_height == t->current.min_height)
			c->isfloating = 1;
	}
	c->float_w = c->geom.width;
	c->float_h = c->geom.height;
}

static void
centerfloat(Client *c)
{
	c->geom.width = c->float_w;
	c->geom.height = c->float_h;
	c->geom.x = c->mon->w.x + (c->mon->w.width - c->float_w) / 2;
	c->geom.y = c->mon->w.y + (c->mon->w.height - c->float_h) / 2;
	applybounds(c, &c->mon->w);
	client_set_size(c, c->geom.width - 2 * c->bw,
			c->geom.height - 2 * c->bw);
	wlr_scene_node_reparent(&c->scene->node, layers[LyrFloat]);
}

static void
movefloat(Client *c, Monitor *m)
{
	if (!c->isfloating)
		return;
	c->geom.x = m->w.x + (m->w.width - c->geom.width) / 2;
	c->geom.y = m->w.y + (m->w.height - c->geom.height) / 2;
	applybounds(c, &m->w);
	client_set_size(c, c->geom.width - 2 * c->bw,
			c->geom.height - 2 * c->bw);
	tween_start(c, c->geom.x, c->geom.y, c->geom.width, c->geom.height, 1.0);
	wlr_output_schedule_frame(m->wlr_output);
}

static void
setmon(Client *c, Monitor *m, uint32_t newtags)
{
	Monitor *oldmon = c->mon;

	if (oldmon == m)
		return;

	c->mon = m;

#ifdef FOREIGN_TOPLEVEL
	if (c->toplevel_handle) {
		if (oldmon)
			wlr_foreign_toplevel_handle_v1_output_leave(c->toplevel_handle,
					oldmon->wlr_output);
		if (m)
			wlr_foreign_toplevel_handle_v1_output_enter(c->toplevel_handle,
					m->wlr_output);
	}
#endif

	if (oldmon) {
		arrange(oldmon);
		motionnotify(0, NULL, 0, 0, 0, 0);
	}
	if (m) {
		c->tags = newtags ? newtags : m->tagset[m->seltags];
		movefloat(c, m);

		{
			int saved_suppress = c->suppress_arrange;
			c->suppress_arrange = 1;
			if (c->isfullscreen) {
				c->prev.x += m->m.x - (oldmon ? oldmon->m.x : 0);
				c->prev.y += m->m.y - (oldmon ? oldmon->m.y : 0);
				resize(c, m->m);
			} else {
				setfullscreen(c, c->isfullscreen);
			}
			c->suppress_arrange = saved_suppress;
			if (!c->suppress_arrange)
				arrange(m);
		}
	}
	focusclient(focustop(selmon), 1);
#ifdef WORKSPACES
	workspaces_send(NULL);
#endif
}

static void
setfullscreen(Client *c, int fullscreen)
{
	if (!c || !c->mon || !client_surface(c)->mapped)
		return;
	if (fullscreen && c->isfloating)
		return;
	if (c->isfullscreen == fullscreen)
		return;

	c->isfullscreen = fullscreen;
	c->bw = fullscreen ? 0 : borderpx;
	client_set_fullscreen(c, fullscreen);

#ifdef FOREIGN_TOPLEVEL
	if (c->toplevel_handle)
		wlr_foreign_toplevel_handle_v1_set_fullscreen(c->toplevel_handle,
				fullscreen);
#endif

	if (c->isfullscreen)
		wlr_scene_node_reparent(&c->scene->node, layers[LyrFS]);
	else
		wlr_scene_node_reparent(&c->scene->node, layers[LyrTile]);
	if (fullscreen) {
		c->prev = c->geom;
		resize(c, c->mon->m);

	} else {
		resize(c, c->prev);
	}
	if (!c->suppress_arrange) {
		arrange(c->mon);
		motionnotify(0, NULL, 0, 0, 0, 0);
	}
}

static void
startclientanim(Client *c)
{
	int slide_dir;
	float start_x, start_y;

	if (!c->mon)
		return;
	if (c->isfloating)
		centerfloat(c);
	arrange(c->mon);
	c->anim.w = c->geom.width;
	c->anim.h = c->geom.height;
	if (c->isfloating) {
		slide_dir = 3;
	} else if (c->geom.width >= c->mon->w.width - 2 * (int)c->bw) {
		slide_dir = 0;
	} else {
		int cx = c->geom.x + c->geom.width / 2;
		int cy = c->geom.y + c->geom.height / 2;
		int dists[] = {
			cx - c->mon->w.x,
			(c->mon->w.x + c->mon->w.width) - cx,
			cy - c->mon->w.y,
			(c->mon->w.y + c->mon->w.height) - cy,
		};
		slide_dir = 0;
		for (int j = 1; j < 4; j++)
			if (dists[j] < dists[slide_dir])
				slide_dir = j;
	}

	switch (slide_dir) {
	case 0: start_x = c->mon->m.x - c->geom.width;  start_y = c->geom.y; break;
	case 1: start_x = c->mon->m.x + c->mon->m.width; start_y = c->geom.y; break;
	case 2: start_x = c->geom.x; start_y = c->mon->m.y - c->geom.height; break;
	case 3:
	default: start_x = c->geom.x; start_y = c->mon->m.y + c->mon->m.height; break;
	}
	wlr_scene_node_set_position(&c->scene->node,
			(int)(start_x + 0.5), (int)(start_y + 0.5));
	c->anim.x = start_x;
	c->anim.y = start_y;
	c->anim.alpha = ANIM_ALPHA_START;
	tween_start(c, c->geom.x, c->geom.y, c->geom.width, c->geom.height, 1.0f);
	wlr_output_schedule_frame(c->mon->wlr_output);
}

#ifdef FOREIGN_TOPLEVEL
static void
setup_toplevel_handle(Client *c, Client *p)
{
	const char *app_id = NULL;
	const char *title = NULL;

	c->toplevel_handle = wlr_foreign_toplevel_handle_v1_create(toplevel_manager);

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		app_id = c->surface.xwayland->class;
		title = c->surface.xwayland->title;
	} else
#endif
	{
		app_id = c->surface.xdg->toplevel->app_id;
		title = c->surface.xdg->toplevel->title;
	}
	wlr_foreign_toplevel_handle_v1_set_app_id(c->toplevel_handle,
			app_id ? app_id : "");
	wlr_foreign_toplevel_handle_v1_set_title(c->toplevel_handle,
			title ? title : "");

	if (c->mon)
		wlr_foreign_toplevel_handle_v1_output_enter(c->toplevel_handle,
				c->mon->wlr_output);
	if (p && p->toplevel_handle)
		wlr_foreign_toplevel_handle_v1_set_parent(c->toplevel_handle,
				p->toplevel_handle);

	LISTEN(&c->toplevel_handle->events.request_activate,
			&c->toplevel_handle_activate, toplevel_handle_activate_cb);
	LISTEN(&c->toplevel_handle->events.request_fullscreen,
			&c->toplevel_handle_fullscreen, toplevel_handle_fullscreen_cb);
	LISTEN(&c->toplevel_handle->events.request_close,
			&c->toplevel_handle_close, toplevel_handle_close_cb);
	LISTEN(&c->toplevel_handle->events.destroy,
			&c->toplevel_handle_destroy, toplevel_handle_destroy_cb);

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		LISTEN(&c->surface.xwayland->events.set_title,
				&c->toplevel_set_title, toplevel_set_title_cb);
		LISTEN(&c->surface.xwayland->events.set_class,
				&c->toplevel_set_app_id, toplevel_set_app_id_cb);
	} else
#endif
	{
		LISTEN(&c->surface.xdg->toplevel->events.set_title,
				&c->toplevel_set_title, toplevel_set_title_cb);
		LISTEN(&c->surface.xdg->toplevel->events.set_app_id,
				&c->toplevel_set_app_id, toplevel_set_app_id_cb);
	}
}
#endif

static void
mapnotify(struct wl_listener *listener, void *data)
{

	Client *p = NULL;
	Client *c = wl_container_of(listener, c, map);
	Monitor *m;
	int i;
	c->scene = client_surface(c)->data = wlr_scene_tree_create(layers[LyrTile]);

	wlr_scene_node_set_enabled(&c->scene->node, client_is_unmanaged(c));
	c->scene_surface = c->type == XDGShell
			? wlr_scene_xdg_surface_create(c->scene, c->surface.xdg)
			: wlr_scene_subsurface_tree_create(c->scene, client_surface(c));
	c->scene->node.data = c->scene_surface->node.data = c;

	client_get_geometry(c, &c->geom);

	if (client_is_unmanaged(c)) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrFloat]);
		wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);
		client_set_size(c, c->geom.width, c->geom.height);
		if (client_wants_focus(c)) {
			focusclient(c, 1);
			exclusive_focus = c;
		}
		goto unsetFullscreen;
	}

	for (i = 0; i < 4; i++) {
		c->border[i] = wlr_scene_rect_create(c->scene, 0, 0,
				bordercolor);
		c->border[i]->node.data = c;
	}
	c->geom.width += 2 * c->bw;
	c->geom.height += 2 * c->bw;
	c->anim.x = c->geom.x;
	c->anim.y = c->geom.y;
	c->anim.w = c->geom.width;
	c->anim.h = c->geom.height;
	c->anim.alpha = 1.0f;
	setfloat(c);
	if (!c->isfloating)
		client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
	wl_list_insert(&clients, &c->link);
	wl_list_insert(&fstack, &c->flink);
	c->suppress_arrange = 1;
	applyrules(c);
	if ((p = client_get_parent(c))) {
		c->mon = p->mon;
		c->tags = p->tags;
	}
	c->suppress_arrange = 0;

#ifdef FOREIGN_TOPLEVEL
	if (toplevel_manager && !client_is_unmanaged(c))
		setup_toplevel_handle(c, p);
#endif

	unsetFullscreen:
	m = c->mon ? c->mon : xytomon(c->geom.x, c->geom.y);
	{
		Client *w;
		wl_list_for_each(w, &clients, link) {
			if (w != c && w != p && w->isfullscreen && m == w->mon && (w->tags & c->tags))
				setfullscreen(w, 0);
		}
	}

	if (c->pending_fullscreen) {
		c->pending_fullscreen = 0;
		c->suppress_arrange = 1;
		setfullscreen(c, 1);
		c->suppress_arrange = 0;
	}

	startclientanim(c);

	if (c->mon && VISIBLEON(c, selmon)) {
		focusclient(c, 1);
	}

#ifdef WORKSPACES
	workspaces_send(c->mon);
#endif
}

static void
unmapnotify(struct wl_listener *listener, void *data)
{

	Client *c = wl_container_of(listener, c, unmap);

	if (client_is_unmanaged(c)) {
		if (c == exclusive_focus) {
			exclusive_focus = NULL;
			focusclient(focustop(selmon), 1);
		}
	} else {
		wl_list_remove(&c->link);
		setmon(c, NULL, 0);
		wl_list_remove(&c->flink);
	}

#ifdef FOREIGN_TOPLEVEL
	if (c->toplevel_handle)
		wlr_foreign_toplevel_handle_v1_destroy(c->toplevel_handle);
#endif

	wlr_scene_node_destroy(&c->scene->node);
	motionnotify(0, NULL, 0, 0, 0, 0);
#ifdef WORKSPACES
	workspaces_send(c->mon);
#endif
}

static void
unmaplayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *l = wl_container_of(listener, l, unmap);

	l->mapped = 0;
	wlr_scene_node_set_enabled(&l->scene->node, 0);
	if (l == exclusive_focus)
		exclusive_focus = NULL;
	if (l->layer_surface->output && (l->mon = l->layer_surface->output->data))
		arrangelayers(l->mon);
	if (l->layer_surface->surface == seat->keyboard_state.focused_surface)
		focusclient(focustop(selmon), 1);
	motionnotify(0, NULL, 0, 0, 0, 0);
}

static void
focusclient(Client *c, int lift)
{
	struct wlr_surface *old = seat->keyboard_state.focused_surface;
	int old_client_type;
	Client *old_c = NULL;
	LayerSurface *old_l = NULL;
	if (c && lift)
		wlr_scene_node_raise_to_top(&c->scene->node);

	if (c && client_surface(c) == old)
		return;

	if ((old_client_type = toplevel_from_wlr_surface(old, &old_c, &old_l)) == XDGShell)
		killpopups(old_c);

	if (active_constraint && (!c || active_constraint->surface != client_surface(c))) {
		wlr_pointer_constraint_v1_send_deactivated(active_constraint);
		active_constraint = NULL;
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
	}
	if (c && !client_is_unmanaged(c)) {
		wl_list_remove(&c->flink);
		wl_list_insert(&fstack, &c->flink);
		selmon = c->mon;
		if (!exclusive_focus && !seat->drag)
			client_set_border_color(c, focuscolor);

#ifdef FOREIGN_TOPLEVEL
		if (c->toplevel_handle)
			wlr_foreign_toplevel_handle_v1_set_activated(c->toplevel_handle, 1);
#endif

	}

	if (deactivateold(old, old_c, old_l, old_client_type, c))
		return;

	motionnotify(0, NULL, 0, 0, 0, 0);

	if (!c) {
		wlr_seat_keyboard_notify_clear_focus(seat);
		return;
	}
	client_notify_enter(client_surface(c), wlr_seat_get_keyboard(seat));
	client_activate_surface(client_surface(c), 1);
}

static double
tween_ease(double t)
{
	double u = 1.0 - t;
	return 1.0 - u * u * u;
}

static void
tween_start(Client *c, float tx, float ty, float tw, float th, float ta)
{
	struct timespec now;

	if (ANIM_DURATION_MS <= 0) {
		c->anim.x = tx;
		c->anim.y = ty;
		c->anim.w = tw;
		c->anim.h = th;
		c->anim.alpha = ta;
		return;
	}

	if (!c->anim.active)
		n_animated++;

	c->anim.from_x = c->anim.x;
	c->anim.from_y = c->anim.y;
	c->anim.from_w = c->anim.w;
	c->anim.from_h = c->anim.h;
	c->anim.from_alpha = c->anim.alpha;
	c->anim.to_x = tx;
	c->anim.to_y = ty;
	c->anim.to_w = tw;
	c->anim.to_h = th;
	c->anim.to_alpha = ta;

	clock_gettime(CLOCK_MONOTONIC, &now);
	c->anim.start_time = (uint64_t)now.tv_sec * 1000000000ULL
			+ (uint64_t)now.tv_nsec;
	c->anim.active = 1;
}

static void
tween_cancel(Client *c)
{
	if (!c->anim.active)
		return;
	c->anim.x = c->anim.to_x;
	c->anim.y = c->anim.to_y;
	c->anim.w = c->anim.to_w;
	c->anim.h = c->anim.to_h;
	c->anim.alpha = c->anim.to_alpha;
	c->anim.active = 0;
	if (n_animated > 0)
		n_animated--;
}

struct anim_scale_data {
	struct wlr_box dest;
	struct wlr_surface *surface;
};

static void
opacity_buffer(struct wlr_scene_buffer *buffer, int sx, int sy, void *user_data)
{
	float opacity = *(float *)user_data;
	if (opacity >= 1.0f)
		return;
	wlr_scene_buffer_set_opacity(buffer, opacity);
}

static void
scale_buffer(struct wlr_scene_buffer *buffer, int sx, int sy, void *user_data)
{
	struct anim_scale_data *data = user_data;
	struct wlr_scene_surface *scene_surface =
			wlr_scene_surface_try_from_buffer(buffer);

	if (!scene_surface || scene_surface->surface != data->surface)
		return;

	wlr_scene_buffer_set_dest_size(buffer, data->dest.width, data->dest.height);
}

static int
animateclient(Client *c)
{
	int aw, ah, bw, cw, ch;
	int animating = 0;
	float opacity = CLAMP(c->anim.alpha, 0.0f, 1.0f);
	if (c->anim.x != c->geom.x || c->anim.y != c->geom.y
			|| c->anim.w != c->geom.width || c->anim.h != c->geom.height)
		animating = 1;

	wlr_scene_node_set_position(&c->scene->node,
			(int)(c->anim.x + 0.5), (int)(c->anim.y + 0.5));
	wlr_scene_node_set_position(&c->scene_surface->node,
			(int)c->bw, (int)c->bw);

	bw = (int)c->bw;
	aw = MAX((int)(c->anim.w + 0.5), 1);
	ah = MAX((int)(c->anim.h + 0.5), 1);
	cw = MAX(aw - 2 * bw, 1);
	ch = MAX(ah - 2 * bw, 1);

	wlr_scene_rect_set_size(c->border[0], aw, bw);
	wlr_scene_rect_set_size(c->border[1], aw, bw);
	wlr_scene_rect_set_size(c->border[2], bw, ch);
	wlr_scene_rect_set_size(c->border[3], bw, ch);
	wlr_scene_node_set_position(&c->border[1]->node, 0, ah - bw);
	wlr_scene_node_set_position(&c->border[2]->node, 0, bw);
	wlr_scene_node_set_position(&c->border[3]->node, aw - bw, bw);

	{
		struct wlr_box clip = {
			.x = c->type == XDGShell
					? c->surface.xdg->geometry.x : 0,
			.y = c->type == XDGShell
					? c->surface.xdg->geometry.y : 0,
			.width = cw,
			.height = ch,
		};
		wlr_scene_subsurface_tree_set_clip(
				&c->scene_surface->node, &clip);
	}
	wlr_scene_node_for_each_buffer(
			&c->scene->node,
			opacity_buffer, &opacity);

	if (animating) {
		struct anim_scale_data scale = {
			.dest = { .width = cw, .height = ch },
			.surface = client_surface(c),
		};
		wlr_scene_node_for_each_buffer(
				&c->scene_surface->node,
				scale_buffer, &scale);
	}

	if (!animating)
		return 0;

	return 1;
}

static int
tween_run(void)
{
	Client *c;
	uint64_t now_ns, dur_ns;
	double progress, e;
	int active = 0;
	struct timespec now;

	if (!n_animated)
		return 0;

	clock_gettime(CLOCK_MONOTONIC, &now);
	now_ns = (uint64_t)now.tv_sec * 1000000000ULL
			+ (uint64_t)now.tv_nsec;
	dur_ns = (uint64_t)ANIM_DURATION_MS * 1000000ULL;

	wl_list_for_each(c, &clients, link) {
		if (!c->anim.active)
			continue;

		progress = dur_ns > 0
				? (double)(now_ns - c->anim.start_time) / dur_ns : 1.0;

		if (progress >= 1.0) {
			c->anim.x = c->anim.to_x;
			c->anim.y = c->anim.to_y;
			c->anim.w = c->anim.to_w;
			c->anim.h = c->anim.to_h;
			c->anim.alpha = c->anim.to_alpha;
			c->anim.active = 0;
			if (n_animated > 0)
				n_animated--;
			continue;
		}

		e = tween_ease(progress);
		c->anim.x = c->anim.from_x + (c->anim.to_x - c->anim.from_x) * (float)e;
		c->anim.y = c->anim.from_y + (c->anim.to_y - c->anim.from_y) * (float)e;
		c->anim.w = c->anim.from_w + (c->anim.to_w - c->anim.from_w) * (float)e;
		c->anim.h = c->anim.from_h + (c->anim.to_h - c->anim.from_h) * (float)e;
		c->anim.alpha = c->anim.from_alpha + (c->anim.to_alpha - c->anim.from_alpha) * (float)e;
		active = 1;
	}
	return active;
}

static Monitor *
xytomon(double x, double y)
{
	struct wlr_output *o = wlr_output_layout_output_at(output_layout, x, y);
	return o ? o->data : NULL;
}

static void
xytonode(double x, double y, struct wlr_surface **psurface,
		Client **pc, LayerSurface **pl, double *nx, double *ny)
{
	struct wlr_scene_node *node, *pnode;
	struct wlr_surface *surface = NULL;
	Client *c = NULL;
	LayerSurface *l = NULL;
	int layer;

	for (layer = NUM_LAYERS - 1; !surface && layer >= 0; layer--) {
		if (!(node = wlr_scene_node_at(&layers[layer]->node, x, y, nx, ny)))
			continue;

		if (node->type == WLR_SCENE_NODE_BUFFER) {
			struct wlr_scene_surface *surf = wlr_scene_surface_try_from_buffer(
					wlr_scene_buffer_from_node(node));
			if (surf)
				surface = surf->surface;
		}

		for (pnode = node; pnode && !c; pnode = &pnode->parent->node) {
			c = pnode->data;
			if (c && c->type != XDGShell && c->type != LayerShell
#ifdef XWAYLAND
					&& c->type != X11
#endif
					)
				c = NULL;
			if (!pnode->parent)
				break;
		}
		if (c && c->type == LayerShell) {
			c = NULL;
			l = pnode->data;
		}
	}
	if (psurface) *psurface = surface;
	if (pc) *pc = c;
	if (pl) *pl = l;
}

static void
cursorconstrain(struct wlr_pointer_constraint_v1 *constraint)
{
	Client *c = NULL;
	toplevel_from_wlr_surface(constraint->surface, &c, NULL);

	if (!c || constraint->surface != seat->pointer_state.focused_surface)
		return;

	if (active_constraint == constraint)
		return;
	if (active_constraint)
		wlr_pointer_constraint_v1_send_deactivated(active_constraint);
	active_constraint = constraint;
	wlr_pointer_constraint_v1_send_activated(constraint);
}

static void
pointerfocus(Client *c, struct wlr_surface *surface, double sx, double sy,
		uint32_t time)
{
	struct timespec now;

	if (surface != seat->pointer_state.focused_surface &&
			time && c && !client_is_unmanaged(c))
		focusclient(c, 0);
	if (!surface) {
		wlr_seat_pointer_notify_clear_focus(seat);
		return;
	}

	if (!time) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		time = now.tv_sec * 1000 + now.tv_nsec / 1000000;
	}
	wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
	wlr_seat_pointer_notify_motion(seat, time, sx, sy);
}

static void
motionnotify(uint32_t time, struct wlr_input_device *device, double dx, double dy,
		double dx_unaccel, double dy_unaccel)
{
	double sx = 0, sy = 0, sx_confined, sy_confined;
	Client *c = NULL, *w = NULL;
	LayerSurface *l = NULL;
	struct wlr_surface *surface = NULL;
	struct wlr_pointer_constraint_v1 *constraint;
	xytonode(cursor->x, cursor->y, &surface, &c, NULL, &sx, &sy);

	if (cursor_mode == CurPressed && !seat->drag
			&& surface != seat->pointer_state.focused_surface
			&& toplevel_from_wlr_surface(seat->pointer_state.focused_surface, &w, &l) >= 0) {
		c = w;
		surface = seat->pointer_state.focused_surface;
		sx = cursor->x - (l ? l->scene->node.x : w->geom.x);
		sy = cursor->y - (l ? l->scene->node.y : w->geom.y);
	}
	if (time) {
		wlr_relative_pointer_manager_v1_send_relative_motion(
				relative_pointer_mgr, seat, (uint64_t)time * 1000,
				dx, dy, dx_unaccel, dy_unaccel);

		wl_list_for_each(constraint, &pointer_constraints->constraints, link)
			cursorconstrain(constraint);

		if (active_constraint) {
			toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
			if (c && active_constraint->surface == seat->pointer_state.focused_surface) {
				sx = cursor->x - c->geom.x - c->bw;
				sy = cursor->y - c->geom.y - c->bw;
				if (wlr_region_confine(&active_constraint->region, sx, sy,
						sx + dx, sy + dy, &sx_confined, &sy_confined)) {
					dx = sx_confined - sx;
					dy = sy_confined - sy;
				}
				if (active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
					return;
			}
		}

		wlr_cursor_move(cursor, device, dx, dy);
		selmon = xytomon(cursor->x, cursor->y);
	}
	wlr_scene_node_set_position(&drag_icon->node, (int)(cursor->x + 0.5), (int)(cursor->y + 0.5));
	if (!surface && !seat->drag)
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");

	pointerfocus(c, surface, sx, sy, time);
}

static void
motionrelative(struct wl_listener *listener, void *data)
{

	struct wlr_pointer_motion_event *event = data;

	motionnotify(event->time_msec, &event->pointer->base, event->delta_x, event->delta_y,
			event->unaccel_dx, event->unaccel_dy);
}

static void
motionabsolute(struct wl_listener *listener, void *data)
{

	struct wlr_pointer_motion_absolute_event *event = data;
	double lx, ly, dx, dy;
	if (!event->time_msec)
		wlr_cursor_warp_absolute(cursor, &event->pointer->base, event->x, event->y);
	wlr_cursor_absolute_to_layout_coords(cursor, &event->pointer->base, event->x, event->y, &lx, &ly);
	dx = lx - cursor->x;
	dy = ly - cursor->y;
	motionnotify(event->time_msec, &event->pointer->base, dx, dy, dx, dy);
}

static void
cursorframe(struct wl_listener *listener, void *data)
{
	wlr_seat_pointer_notify_frame(seat);
}

static void
rendermon(struct wl_listener *listener, void *data)
{
	Monitor *m = wl_container_of(listener, m, frame);
	Client *c;
	struct wlr_scene_output *scene_output = m->scene_output;
	struct wlr_output_state pending = {0};
	struct timespec now;
	int needs_another_frame = 0;

	m->wlr_output->frame_pending = false;

	if (!wlr_scene_output_needs_frame(scene_output))
		goto skip;

	needs_another_frame |= tween_run();
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || !client_surface(c)->mapped)
			continue;
		needs_another_frame |= animateclient(c);
	}

	wlr_output_state_init(&pending);
	if (!wlr_scene_output_build_state(scene_output, &pending, NULL)) {
		wlr_output_state_finish(&pending);
		goto skip;
	}

#ifdef FULLSCREEN_TEARING
	if (moncantear(m)) {
		pending.tearing_page_flip = 1;
		if (!wlr_output_test_state(m->wlr_output, &pending))
			pending.tearing_page_flip = 0;
	}
#endif

	wlr_output_commit_state(m->wlr_output, &pending);
	wlr_output_state_finish(&pending);

skip:
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);

	if (needs_another_frame)
		wlr_output_schedule_frame(m->wlr_output);
}

static void
updatemons(struct wl_listener *listener, void *data)
{
	struct wlr_box sgeom;
	struct wlr_output_configuration_v1 *config
			= wlr_output_configuration_v1_create();
	Client *c;
	struct wlr_output_configuration_head_v1 *config_head;
	Monitor *m;
	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled) {
			if (!wlr_output_layout_get(output_layout, m->wlr_output))
				wlr_output_layout_add_auto(output_layout, m->wlr_output);
		} else {
			config_head = wlr_output_configuration_head_v1_create(config, m->wlr_output);
			config_head->state.enabled = 0;
			wlr_output_layout_remove(output_layout, m->wlr_output);
			m->m = m->w = (struct wlr_box){0};
		}
	}
	wlr_output_layout_get_box(output_layout, NULL, &sgeom);

	wlr_scene_node_set_position(&root_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(root_bg, sgeom.width, sgeom.height);

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled)
			continue;
		config_head = wlr_output_configuration_head_v1_create(config, m->wlr_output);
		wlr_output_layout_get_box(output_layout, m->wlr_output, &m->m);
		m->w = m->m;
		wlr_scene_output_set_position(m->scene_output, m->m.x, m->m.y);
		arrangelayers(m);

		arrange(m);

		if ((c = focustop(m)) && c->isfullscreen)
			resize(c, m->m);

		config_head->state.x = m->m.x;
		config_head->state.y = m->m.y;

		if (!selmon || !selmon->wlr_output->enabled) {
			selmon = m;
		}
	}

	if (selmon && selmon->wlr_output->enabled) {
		wl_list_for_each(c, &clients, link) {
			if (!c->mon && client_surface(c)->mapped)
				setmon(c, selmon, c->tags);
		}
		focusclient(focustop(selmon), 1);
	}
	wlr_cursor_move(cursor, NULL, 0, 0);

	wlr_output_manager_v1_set_configuration(output_mgr, config);
}

static int
keybinding(uint32_t mods, xkb_keysym_t sym)
{

	const Key *k;
	for (k = keys; k < END(keys); k++) {
		if (CLEANMASK(mods) == CLEANMASK(k->mod)
				&& xkb_keysym_to_lower(sym) == xkb_keysym_to_lower(k->keysym)
				&& k->func) {
			k->func(&k->arg);
			return 1;
		}
	}
	return 0;
}

static void
keypress(struct wl_listener *listener, void *data)
{
	int i;

	KeyboardGroup *group = wl_container_of(listener, group, key);
	struct wlr_keyboard_key_event *event = data;
	uint32_t keycode = event->keycode + 8;

	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			group->wlr_group->keyboard.xkb_state, keycode, &syms);

	int handled = 0;
	uint32_t mods = wlr_keyboard_get_modifiers(&group->wlr_group->keyboard);
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (i = 0; i < nsyms; i++)
			handled = keybinding(mods, syms[i]) || handled;
	}

	if (handled && group->wlr_group->keyboard.repeat_info.delay > 0) {
		group->mods = mods;
		group->keysym_count = nsyms < 32 ? nsyms : 32;
		memcpy(group->keysym_buf, syms, group->keysym_count * sizeof(syms[0]));
		wl_event_source_timer_update(group->key_repeat_source,
				group->wlr_group->keyboard.repeat_info.delay);
	} else {
		wl_event_source_timer_update(group->key_repeat_source, 0);
	}

	if (handled)
		return;

	wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);

	wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
}

static void
keypressmod(struct wl_listener *listener, void *data)
{

	KeyboardGroup *group = wl_container_of(listener, group, modifiers);

	wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);

	wlr_seat_keyboard_notify_modifiers(seat,
			&group->wlr_group->keyboard.modifiers);
}

static int
keyrepeat(void *data)
{
	KeyboardGroup *group = data;
	int i;
	if (!group->keysym_count || group->wlr_group->keyboard.repeat_info.rate <= 0)
		return 0;

	wl_event_source_timer_update(group->key_repeat_source,
			1000 / group->wlr_group->keyboard.repeat_info.rate);

	for (i = 0; i < group->keysym_count; i++)
		keybinding(group->mods, group->keysym_buf[i]);

	return 0;
}

#ifdef FOREIGN_TOPLEVEL
static void
toplevel_handle_activate_cb(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, toplevel_handle_activate);
	if (!c->mon)
		return;
	if (c->tags != c->mon->tagset[c->mon->seltags]) {
		c->mon->seltags ^= 1;
		c->mon->tagset[c->mon->seltags] = c->tags;
		arrange(c->mon);
#ifdef WORKSPACES
		workspaces_send(c->mon);
#endif
	}
	focusclient(c, 1);
}

static void
toplevel_handle_fullscreen_cb(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, toplevel_handle_fullscreen);
	if (c->mon)
		setfullscreen(c, !c->isfullscreen);
}

static void
toplevel_handle_close_cb(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, toplevel_handle_close);
	client_send_close(c);
}

static void
toplevel_handle_destroy_cb(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, toplevel_handle_destroy);
	wl_list_remove(&c->toplevel_handle_activate.link);
	wl_list_remove(&c->toplevel_handle_fullscreen.link);
	wl_list_remove(&c->toplevel_handle_close.link);
	wl_list_remove(&c->toplevel_handle_destroy.link);
	wl_list_remove(&c->toplevel_set_title.link);
	wl_list_remove(&c->toplevel_set_app_id.link);
	c->toplevel_handle = NULL;
}

static void
toplevel_set_title_cb(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, toplevel_set_title);
	const char *title = NULL;
	if (!c->toplevel_handle)
		return;
#ifdef XWAYLAND
	if (client_is_x11(c))
		title = c->surface.xwayland->title;
	else
#endif
		title = c->surface.xdg->toplevel->title;
	wlr_foreign_toplevel_handle_v1_set_title(c->toplevel_handle,
			title ? title : "");
}

static void
toplevel_set_app_id_cb(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, toplevel_set_app_id);
	const char *app_id = NULL;
	if (!c->toplevel_handle)
		return;
#ifdef XWAYLAND
	if (client_is_x11(c))
		app_id = c->surface.xwayland->class;
	else
#endif
		app_id = c->surface.xdg->toplevel->app_id;
	wlr_foreign_toplevel_handle_v1_set_app_id(c->toplevel_handle,
			app_id ? app_id : "");
}
#endif

int
main(int argc, char *argv[])
{
	int c;

	while ((c = getopt(argc, argv, "hv")) != -1) {
		if (c == 'v')
			die("jtl " VERSION);
		else
			goto usage;
	}
	if (optind < argc)
		goto usage;
	if (!getenv("XDG_RUNTIME_DIR"))
		die("XDG_RUNTIME_DIR must be set");
	setup();
	setupinput();
#ifdef XWAYLAND
	setupx();
#endif
	run();
	cleanup();
	return EXIT_SUCCESS;

usage:
	die("Usage: %s [-v]", argv[0]);
}
