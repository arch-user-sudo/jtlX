/*
 * Converts an ARGB hex color to a float array for wlroots.
 * Usage: COLOR(0xAARRGGBB)  e.g. COLOR(0xff0000ff) = opaque red
 */
#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }

/* ======================== APPEARANCE ======================== */

/* Border width in pixels around each window */
static const unsigned int borderpx         = 1;
/* Background color of the desktop (root window) */
static const float rootcolor[]             = COLOR(0x0F1011ff);
/* Border color of unfocused windows */
static const float bordercolor[]           = COLOR(0x333333ff);
/* Border color of the focused window */
static const float focuscolor[]            = COLOR(0x888888ff);


/* ======================== TAGS (WORKSPACES) ======================== */

/* Number of tags (workspaces). Must be 1-31. Default is 9 (Mod+1..9). */
#define TAGCOUNT (9)


/* ======================== LOGGING ======================== */

/* ======================== MONITORS ======================== */

/*
 * Each entry matches a monitor by name (substring match).
 * Fields: name, mfact, nmaster, scale, transform, x, y
 *   name       - monitor name substring (e.g. "DP-3", "eDP-1")
 *   mfact      - master area ratio (0.0-1.0), default 0.52
 *   nmaster    - number of clients in the master area
 *   scale      - fractional scale for HiDPI (1 = normal, 2 = 200%)
 *   transform  - output rotation, see WL_OUTPUT_TRANSFORM_* enum
 *   x, y       - position in the layout. (-1, -1) = auto-arrange
 * WARNING: negative values other than (-1, -1) cause problems with Xwayland.
 */
static const MonitorRule monrules[] = {
	{ "DP-1",       0.52f, 1,      1,        WL_OUTPUT_TRANSFORM_NORMAL,   0,     0  },
	{ "DVI-I-1",    0.5f,  1,      1,        WL_OUTPUT_TRANSFORM_NORMAL,   1000,  1102 },
};


/* ======================== WINDOW RULES ======================== */

/*
 * Each rule is checked against every new window; all matching rules apply.
 *   id            regex on the app-id (WM_CLASS for X11 windows),
 *                 case-insensitive, NULL = don't match by id
 *   title         optional regex on the window title, NULL = skip
 *   tags          workspace bitmask: 1 << n opens on workspace n+1
 *                 (e.g. 1 << 8 = workspace 9; 0 = current workspace)
 *   isfullscreen  1 = window opens fullscreen
 */
static const Rule rules[] = {
	/* app-id            title   workspace        fullscreen */
	{ NULL,              NULL,   0,               0 },
};


/* ======================== GAPS ======================== */

/* Outer gaps (between windows and screen edges) in pixels */
static const unsigned int gappoh = 10;
static const unsigned int gappov = 10;
/* Inner gaps (between tiled windows) in pixels */
static const unsigned int gappih = 5;
static const unsigned int gappiv = 5;


/* ======================== WINDOW ANIMATIONS ======================== */

/* Duration of the ease-out-cubic animation in milliseconds (0 = disabled) */
#define ANIM_DURATION_MS  300
/* Starting opacity for window entry (0.0 = invisible, 1.0 = opaque) */
#define ANIM_ALPHA_START  0.0f


/* ======================== KEYBOARD ======================== */

/* Key repeat rate (characters per second) and delay (ms before repeat starts) */
static const int repeat_rate = 50;
static const int repeat_delay = 250;


/* ======================== KEY BINDINGS ======================== */

/* Modifier key for most bindings. Use WLR_MODIFIER_LOGO for the Super/Windows key. */
#define MODKEY WLR_MODIFIER_LOGO

/* TAGKEYS macro: generates keybindings to switch to and move to each tag.
 *   Mod+Key = switch to that tag (view)
 *   Mod+Shift+Key = move focused window to that tag
 */
#define TAGKEYS(KEY,SHIFTKEY,TAG) \
	{ MODKEY,                    KEY,            view,            {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_SHIFT, SHIFTKEY,      tag,             {.ui = 1 << TAG} }

/* SHCMD: spawn a shell command.
 * Usage: SHCMD("firefox")  expands to { .v = (char*[]){ "/bin/sh", "-c", "firefox", NULL } }
 */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* ======================== LAUNCH COMMANDS ======================== */

/* Programs launched by keybindings. These are null-terminated argv arrays. */
static const char *termcmd[] = { "footclient", NULL };
/*static const char *menucmd[] = { "bash", "/home/lynch/BashScripts/fuzzel.sh", NULL };*/
static const char *filecmd[] = { "nemo", NULL };
static const char *startup[] = { "dwlservices", NULL };


/* Apps launched once at compositor start.
 * Reuses spawn() from the keybindings below. */
static const Arg startupapps[] = {
/*	{ .v = (const char *[]){ "dwlservices", NULL } },*/
	{ .v = (const char *[]){ "gentoo-pipewire-launcher", NULL } },
/*	{ .v = (const char *[]){ "foot", "--server", NULL } },
	{ .v = (const char *[]){ "wbg", "-s", "/home/lynch/Wallpapers/jtl.png", NULL } },*/
};


/* ======================== KEYBINDINGS TABLE ======================== */
/*
 * Each entry: { modifier, keysym, function, argument }
 * Available functions:
 *   spawn(Arg)        - launch a program (.v = argv array, or use SHCMD)
 *   killclient(Arg)   - send close to focused window
 *   togglefullscreen  - toggle fullscreen on focused window
 *   focusstack(Arg)   - focus next/previous window (+1 / -1)
 *   focusmon(Arg)     - focus adjacent monitor (direction)
 *   tagmon(Arg)       - move window to adjacent monitor
 *   view(Arg)         - switch to a tag (.ui = bitmask, 1 << n)
 *   setmfact(Arg)     - adjust master area ratio (.f = +/- 0.05)
 *   incnmaster(Arg)   - change number of master windows (.i = +/- 1)
 *   zoom(Arg)         - swap focused window with master
 *   setcursorshape    - set pointer cursor shape
 *   quit(Arg)         - exit dwl
 *   chvt(Arg)         - switch virtual terminal (.ui = VT number)
 */
static const Key keys[] = {
	/* modifier                     key                  function          arg */
	{ MODKEY,                        XKB_KEY_a,           spawn,            SHCMD("jtlabctl menu") },
	{ MODKEY,                        XKB_KEY_e,           spawn,            {.v = filecmd} },
	{ MODKEY,                        XKB_KEY_q,           spawn,            {.v = termcmd} },
	{ MODKEY,                        XKB_KEY_b,           spawn,            {.v = startup} },
	{ MODKEY,                        XKB_KEY_j,           focusstack,       {.i = +1} },
	{ MODKEY,                        XKB_KEY_k,           focusstack,       {.i = -1} },
	{ MODKEY,                        XKB_KEY_i,           incnmaster,       {.i = +1} },
	{ MODKEY,                        XKB_KEY_d,           incnmaster,       {.i = -1} },
	{ MODKEY,                        XKB_KEY_h,           setmfact,         {.f = -0.05f} },
	{ MODKEY,                        XKB_KEY_l,           setmfact,         {.f = +0.05f} },
	{ MODKEY,                        XKB_KEY_Return,      zoom,             {0} },
	{ MODKEY,                        XKB_KEY_Tab,         view,             {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT,     XKB_KEY_q,           killclient,       {0} },
	{ MODKEY,                        XKB_KEY_y,           togglefullscreen, {0} },
#ifdef FULLSCREEN_TEARING
	{ MODKEY,                        XKB_KEY_o,           enabletearing,    {0} },
	{ MODKEY,                        XKB_KEY_p,           disabletearing,   {0} },
#endif
	{ MODKEY,                        XKB_KEY_comma,       focusmon,         {.i = WLR_DIRECTION_UP} },
	{ MODKEY,                        XKB_KEY_period,      focusmon,         {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY|WLR_MODIFIER_SHIFT,     XKB_KEY_less,        tagmon,           {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_SHIFT,     XKB_KEY_greater,     tagmon,           {.i = WLR_DIRECTION_DOWN} },
	TAGKEYS(          XKB_KEY_1,          XKB_KEY_exclam,     0),
	TAGKEYS(          XKB_KEY_2,          XKB_KEY_at,         1),
	TAGKEYS(          XKB_KEY_3,          XKB_KEY_numbersign, 2),
	TAGKEYS(          XKB_KEY_4,          XKB_KEY_dollar,     3),
	TAGKEYS(          XKB_KEY_5,          XKB_KEY_percent,    4),
	TAGKEYS(          XKB_KEY_6,          XKB_KEY_asciicircum,5),
	TAGKEYS(          XKB_KEY_7,          XKB_KEY_ampersand,  6),
	TAGKEYS(          XKB_KEY_8,          XKB_KEY_asterisk,   7),
	TAGKEYS(          XKB_KEY_9,          XKB_KEY_parenleft,  8),
	{ MODKEY|WLR_MODIFIER_SHIFT,     XKB_KEY_p,           quit,             {0} },

	/* Ctrl-Alt-Backspace also quits (legacy X11 habit) */
	{ WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT, XKB_KEY_Terminate_Server, quit, {0} },
	/* Ctrl-Alt-F1..F12 switch virtual terminals (do not remove) */
#define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_##n, chvt, {.ui = (n)} }
	CHVT(1), CHVT(2), CHVT(3), CHVT(4), CHVT(5), CHVT(6),
	CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12),
};

/* Mouse bindings: none in pure tiling mode */
