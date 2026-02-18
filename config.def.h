/* See LICENSE file for copyright and license details. */

/* Color scheme - Gruvbox Material */
#define COLOR_BG_PRIMARY    "#1d2021"    /* main background */
#define COLOR_BG_SECONDARY  "#282828"    /* secondary background */
#define COLOR_BG_ACCENT     "#3c3836"    /* accent background */
#define COLOR_FG_PRIMARY    "#d4be98"    /* main text */
#define COLOR_FG_SECONDARY  "#a89984"    /* secondary text */
#define COLOR_ACCENT_BLUE   "#7daea3"    /* blue accent */
#define COLOR_ACCENT_GREEN  "#a9b665"    /* green accent */
#define COLOR_ACCENT_ORANGE "#e78a4e"    /* orange accent */
#define COLOR_ACCENT_RED    "#ea6962"    /* red accent */
#define COLOR_ACCENT_PURPLE "#d3869b"    /* purple accent */
#define COLOR_ACCENT_YELLOW "#d8a657"    /* yellow accent */
#define COLOR_BORDER_NORMAL COLOR_BG_ACCENT
#define COLOR_BORDER_FOCUS  COLOR_ACCENT_ORANGE

/* autostart */
static const char *autostartscript = "~/.config/dwm/autostart.sh";

/* preview mode */
static const int previewmode_default = 0; /* 0 for scroll mode, 1 for grid mode */

/* appearance */
static const unsigned int gappx       = 4;        /* gaps between windows */
static const int cornerradius   = 8;   /* round corner radius */
static const unsigned int scrollstartgap  = 4;  /* gap before first client in scroll layout */
static const unsigned int snap        = 32;       /* snap pixel */
static const unsigned int borderpx    = 3;       /* focused border thickness */
static const unsigned int systraypinning = 0;     /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayspacing = 2;     /* systray spacing */
static const int systraypinningfailfirst = 1;     /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray          = 1;        /* 0 means no systray */
static const float systrayiconheight  = 0.6;      /* systray icon height as fraction of bar height (0.0 - 1.0) */
static const int showbar              = 1;        /* 0 means no bar */
static const int topbar               = 1;        /* 0 means bottom bar */
static const int enableoffscreen      = 1;        /* enable XComposite offscreen redirection for containers */
static const char supericon[]         = "   ";
static const char logotext[]          = "Arch-linux";
static const char *fonts[]            = {
	"Hack Nerd Font:size=13",
	"WenQuanYi Zen Hei:size=9"
};

static const char *interface_name[] = {
    "lo",
    "enp3s0",
    "wlp2s0"
};

/* virtual monitor split: NULL disables; "vertical" splits left/right; "horizontal" splits top/bottom */
static const char *colors[][3]      = {
	/*                fg                     bg                    border   */
	[SchemeNorm]    = {COLOR_FG_PRIMARY,     COLOR_BG_PRIMARY,     COLOR_BORDER_NORMAL},
	[SchemeSel]     = {COLOR_ACCENT_PURPLE,  COLOR_BG_SECONDARY,   COLOR_BORDER_FOCUS},
	[SchemeFG]      = {COLOR_FG_PRIMARY,     COLOR_FG_PRIMARY,     COLOR_FG_PRIMARY},
	[SchemeBlue]    = {COLOR_ACCENT_BLUE,    COLOR_ACCENT_BLUE,    COLOR_BORDER_NORMAL},
	[SchemeGreen]   = {COLOR_ACCENT_GREEN,   COLOR_ACCENT_GREEN,   COLOR_BORDER_NORMAL},
	[SchemeOrange]  = {COLOR_ACCENT_ORANGE,  COLOR_ACCENT_ORANGE,  COLOR_BORDER_NORMAL},
	[SchemeRed]     = {COLOR_ACCENT_RED,     COLOR_ACCENT_RED,     COLOR_BORDER_NORMAL},
	[SchemeYellow]  = {COLOR_ACCENT_YELLOW,  COLOR_ACCENT_YELLOW,  COLOR_BORDER_NORMAL},
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* class      instance    title       tagindex      isfloating   monitor */
	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
	{ "Firefox",  NULL,       NULL,       8,            0,           -1 },
};

/* corner radius rules - per-window corner radius configuration */
static const CornerRule cornerrules[] = {
	/* class      instance    title       override_redirect    radius */
	{ NULL,       NULL,       "rofi",     1,                   24 },
};

/* layout(s) */
static const int resizehints    = 1;    /* 1 means respect size hints in tiled resizals */
static const int refreshrate    = 120;  /* refresh rate (per second) for client move/resize */
static const float mfactdefault = 0.7; /* factor of master area size [0.05..0.95] */
static const float autofloatthreshold = 0.7; /* auto-float threshold for window height as fraction of monitor height */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ " ",        scroll },        /* scroll layout - default */
	{ "󰆾 ",        NULL },          /* no layout function means floating behavior */
};

/* key definitions */
#define MODKEY Mod1Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.i = TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.i = TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static const char *roficmd[]      = {"rofi", "-mousable", "-show", NULL};
static const char *termcmd[]      = {"kitty", NULL};

static const Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_p,      spawn,          {.v = roficmd } },
	{ MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	{ MODKEY,                       XK_Tab,    viewlast,       {0} },
	{ MODKEY|ShiftMask,             XK_c,      killclient,     {0} },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
	{ MODKEY,                       XK_j,      focusstep,      {.i = +1} },
	{ MODKEY,                       XK_k,      focusstep,      {.i = -1} },
	{ MODKEY,                       XK_r,      previewscroll,  {0} },
	{ MODKEY|ShiftMask,             XK_j,      focusstepvisible, {.i = +1} },
	{ MODKEY|ShiftMask,             XK_k,      focusstepvisible, {.i = -1} },
	{ MODKEY,                       XK_Return, ensureselectedvisible, {0} },
	{ MODKEY,                       XK_space,  scrolltogglesize, {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating,   {0} },
	{ MODKEY|ShiftMask,             XK_f,      togglefullscreen, {0} },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY|ShiftMask,             XK_q,      quit,           {0} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkNullWinTitle, ClkWinClass, ClkSuperIcon, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkSuperIcon,         0,              Button1,        togglesupericon,{0} },
	{ ClkWinClass,          0,              Button1,        spawn,          {.v = roficmd } },
	{ ClkWinClass,          0,              Button3,        spawn,          {.v = termcmd } },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button4,        scrollmoveothers,{.i = -100} },
	{ ClkClientWin,         MODKEY,         Button5,        scrollmoveothers,{.i = +100} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkWinTitle,          0,              Button1,        focusonclick,   {0} },
	{ ClkWinTitle,          0,              Button4,        scrollmove,     {.i = -100} },
	{ ClkWinTitle,          0,              Button5,        scrollmove,     {.i = +100} },
	{ ClkStatusText,        0,              Button1,        handleStatus1,  {0} },
	{ ClkStatusText,        0,              Button2,        handleStatus2,  {0} },
	{ ClkStatusText,        0,              Button3,        handleStatus3,  {0} },
	{ ClkStatusText,        0,              Button4,        handleStatus4,  {0} },
	{ ClkStatusText,        0,              Button5,        handleStatus5,  {0} },
};

/* status bar commands */
static const char *script_menu[]  = { "script-menu.sh", NULL};
static const char *sys_monitor[]  = { "mate-system-monitor", NULL};
static const char *tog_volume[]   = { "pactl", "set-sink-mute", "@DEFAULT_SINK@", "toggle", NULL};
static const char *inc_volume[]   = { "pactl", "set-sink-volume", "@DEFAULT_SINK@", "+5%", NULL};
static const char *dec_volume[]   = { "pactl", "set-sink-volume", "@DEFAULT_SINK@", "-5%", NULL};
static const char *inc_volume_1[] = { "pactl", "set-sink-volume", "@DEFAULT_SINK@", "+1%", NULL};
static const char *dec_volume_1[] = { "pactl", "set-sink-volume", "@DEFAULT_SINK@", "-1%", NULL};
static const char *inc_light[]    = { "brightnessctl", "set", "+5%", NULL};
static const char *dec_light[]    = { "brightnessctl", "set", "5%-", NULL};
static const char *inc_light_1[]  = { "brightnessctl", "set", "+1%", NULL};
static const char *dec_light_1[]  = { "brightnessctl", "set", "5%-", NULL};
static const char *history_pop[]  = { "dunstctl", "history-pop", NULL};
static const char *history_clear[] = { "dunstctl", "history-clear", NULL};
static const char *history_close[] = { "dunstctl", "close", NULL};
