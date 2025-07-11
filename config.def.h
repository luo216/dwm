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

static const char supericon[] = "   ";
static const char logotext[]  = "Arch-linux";
/* status bar */
static const char *interface_name[] = {"enp3s0", "wlp0s20f3", "virbr0", "docker0"};
/* config patch */
static const char autostartblocksh[] = "autostart_blocking.sh";
static const char autostartsh[]      = "autostart.sh";
static const char broken[]           = "broken";
static const char dwmdir[]           = "dwm";
static const char dotconfig[]        = ".config";
/* appearance */
static const unsigned int borderpx  = 3;        /* border pixel of windows */
static const unsigned int gappx     = 8;        /* gap pixel between windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const unsigned int systrayiconsizeredunce = 15;   /* Let the icon not be completely attached to the systray-window, leaving some space */
static const unsigned int systrayicony = systrayiconsizeredunce / 2;   /* Let the icon not be completely attached to the systray-window, leaving some space */
static const unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayspacing = 2;   /* systray spacing */
static const int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray        = 1;     /* 0 means no systray */
static const int showbar            = 1;     /* 0 means no bar */
static const int topbar             = 1;     /* 0 means bottom bar */
static const char *fonts[] = {"Hack Nerd Font:size=13", "WenQuanYi Zen Hei:size=9"};
static const unsigned int baralpha    = 0xe0;
static const unsigned int borderalpha = OPAQUE;
static const char *colors[][3]        = {
  /*                fg                     bg                    border   */
  [SchemeNorm]    = {COLOR_FG_PRIMARY,     COLOR_BG_PRIMARY,     COLOR_BORDER_NORMAL},
  [SchemeSel]     = {COLOR_FG_PRIMARY,     COLOR_BG_SECONDARY,   COLOR_BORDER_FOCUS},
  [SchemeBlue]    = {COLOR_ACCENT_BLUE,    COLOR_ACCENT_BLUE,    COLOR_ACCENT_BLUE},
  [SchemePurple]  = {COLOR_ACCENT_PURPLE,  COLOR_ACCENT_PURPLE,  COLOR_ACCENT_PURPLE},
  [SchemeOrange]  = {COLOR_ACCENT_ORANGE,  COLOR_ACCENT_ORANGE,  COLOR_ACCENT_ORANGE},
  [SchemeGreen]   = {COLOR_ACCENT_GREEN,   COLOR_ACCENT_GREEN,   COLOR_ACCENT_GREEN},
  [SchemeRed]     = {COLOR_ACCENT_RED,     COLOR_ACCENT_RED,     COLOR_ACCENT_RED},
  [SchemeFG]      = {COLOR_FG_PRIMARY,     COLOR_FG_PRIMARY,     COLOR_FG_PRIMARY},
};
static const unsigned int alphas[][3] = {
    /*                 fg      bg        border*/
    [SchemeNorm]   = { OPAQUE, baralpha, borderalpha },
	  [SchemeSel]    = { OPAQUE, baralpha, borderalpha },
    [SchemeBlue]   = { OPAQUE, OPAQUE,   borderalpha },
    [SchemePurple] = { OPAQUE, OPAQUE,   borderalpha },
    [SchemeGreen]  = { OPAQUE, OPAQUE,   borderalpha },
    [SchemeOrange] = { OPAQUE, OPAQUE,   borderalpha },
    [SchemeRed]    = { OPAQUE, OPAQUE,   borderalpha },
    [SchemeFG]     = { OPAQUE, OPAQUE,   borderalpha },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class            instance    title       tags mask     isfloating   monitor */
	{ "R-quick-share",  NULL,       NULL,       1 << 8,       0,           -1 },
};

/* layout(s) */
static const float mfact            = 0.65; /* factor of master area size [0.05..0.95] */
static const int LeftEdgeLean       = 1;    /* 1: lean to the left, 0: lean to the right */
static const int nmaster            = 1;    /* number of clients in master area */
static const int resizehints        = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen     = 1;    /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ " 󰕮 ",      tile },    /* first entry is default */
	{ " 󰉨 ",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static const char *roficmd[]      = {"rofi", "-mousable", "-show", NULL};
static const char *browser[]      = {"google-chrome-stable", NULL};
static const char *termcmd[]      = {"wezterm", NULL};
static const char *script_menu[]  = {"script-menu.sh", NULL};
static const char *inc_light[]    = {"light", "-A", "5", NULL};
static const char *inc_light_1[]  = {"light", "-A", "1", NULL};
static const char *dec_light[]    = {"light", "-U", "5", NULL};
static const char *dec_light_1[]  = {"light", "-U", "1", NULL};
static const char *inc_volume[]   = {"pactl", "set-sink-volume", "@DEFAULT_SINK@", "+5%", NULL};
static const char *inc_volume_1[] = {"pactl", "set-sink-volume", "@DEFAULT_SINK@", "+1%", NULL};
static const char *dec_volume[]   = {"pactl", "set-sink-volume", "@DEFAULT_SINK@", "-5%", NULL};
static const char *dec_volume_1[] = {"pactl", "set-sink-volume", "@DEFAULT_SINK@", "-1%", NULL};
static const char *tog_volume[]   = {"pactl", "set-sink-mute", "@DEFAULT_SINK@", "toggle", NULL};
static const char *flameshot[]    = {"flameshot", "gui", NULL};
static const char *filemgr[]      = {"thunar", NULL};
static const char *xdo_click4[]   = {"xdotool", "click", "4", NULL};
static const char *xdo_click5[]   = {"xdotool", "click", "5", NULL};

static const Key keys[] = {
	/* modifier                     key               function                argument */
  { MODKEY,                       XK_F3,            spawn,                  {.v = tog_volume } },
  { MODKEY,                       XK_F5,            spawn,                  {.v = dec_volume } },
  { MODKEY,                       XK_F6,            spawn,                  {.v = inc_volume } },
  { MODKEY,                       XK_F8,            spawn,                  {.v = dec_light } },
  { MODKEY,                       XK_F9,            spawn,                  {.v = inc_light } },
  { MODKEY,                       XK_F10,           spawn,                  {.v = flameshot } },
	{ MODKEY,                       XK_p,             spawn,                  {.v = roficmd } },
  { MODKEY|ShiftMask,             XK_p,             spawn,                  {.v = browser } },
  { MODKEY|ShiftMask,             XK_Return,        spawn,                  {.v = termcmd } },
  { MODKEY,                       XK_e,             spawn,                  {.v = filemgr } },
	{ MODKEY,                       XK_b,             togglebar,              {0} },
	{ MODKEY,                       XK_j,             focusstackvis,          {.i = +1 } },
	{ MODKEY,                       XK_k,             focusstackvis,          {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_j,             focusstackedge,         {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_k,             focusstackedge,         {.i = -1 } },
	{ MODKEY,                       XK_i,             moveclient,             {.i = 1} },
	{ MODKEY,                       XK_d,             moveclient,             {.i = 0} },
	{ MODKEY,                       XK_h,             setmfact,               {.f = -0.05} },
	{ MODKEY,                       XK_l,             setmfact,               {.f = +0.05} },
	{ MODKEY,                       XK_Return,        toggleEdgeLean,         {0} },
	{ MODKEY,                       XK_Tab,           view,                   {0} },
	{ MODKEY,                       XK_Alt_L,         switchtoclient,         {0} },
	{ MODKEY|ShiftMask,             XK_c,             killclient,             {0} },
	{ MODKEY,                       XK_t,             setlayout,              {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,             setlayout,              {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,             setlayout,              {.v = &layouts[2]} },
	{ MODKEY,                       XK_space,         togglehgappx,           {0} },
	{ MODKEY|ShiftMask,             XK_space,         togglefloating,         {0} },
	{ MODKEY|ShiftMask,             XK_f,             togglefullscr,          {0} },
	{ MODKEY,                       XK_0,             view,                   {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,             tag,                    {.ui = ~0 } },
	{ MODKEY,                       XK_comma,         focusmon,               {.i = -1 } },
	{ MODKEY,                       XK_period,        focusmon,               {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,         tagmon,                 {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period,        tagmon,                 {.i = +1 } },
	{ MODKEY,                       XK_s,             show,                   {0} },
	{ MODKEY|ShiftMask,             XK_s,             showall,                {0} },
  { MODKEY,                       XK_o,             showonly,               {0} },
	{ MODKEY|ShiftMask,             XK_h,             hide,                   {0} },
	TAGKEYS(                        XK_1,                                     0)
	TAGKEYS(                        XK_2,                                     1)
	TAGKEYS(                        XK_3,                                     2)
	TAGKEYS(                        XK_4,                                     3)
	TAGKEYS(                        XK_5,                                     4)
	TAGKEYS(                        XK_6,                                     5)
	TAGKEYS(                        XK_7,                                     6)
	TAGKEYS(                        XK_8,                                     7)
	TAGKEYS(                        XK_9,                                     8)
	{ MODKEY|ShiftMask,             XK_q,             quit,                   {0} },
 	{ MODKEY|ShiftMask,             XK_r,             previewallwin,          {0} },
 	{ MODKEY,                       XK_r,             previewindexwin,        {0} },
 	{ MODKEY,                       XK_u,             spawn,                  {.v = xdo_click4} },
 	{ MODKEY,                       XK_n,             spawn,                  {.v = xdo_click5} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function              argument */
  { ClkSuperIcon,         0,              Button1,        togglesupericon,      {0} },
  { ClkWinClass,          0,              Button1,        spawn,                {.v = roficmd} },
  { ClkWinClass,          0,              Button3,        spawn,                {.v = termcmd} },
	{ ClkWinClass,          0,              Button4,        pview,                {0} },
	{ ClkWinClass,          0,              Button5,        nview,                {0} },
	{ ClkTagBar,            0,              Button1,        view,                 {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,           {0} },
	{ ClkTagBar,            0,              Button4,        pview,                {0} },
	{ ClkTagBar,            0,              Button5,        nview,                {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,                  {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,            {0} },
  { ClkLtSymbol,          0,              Button1,        setlayout,            {0} },
  { ClkLtSymbol,          0,              Button3,        setlayout,            {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button1,        togglewin,            {0} },
	{ ClkWinTitle,          0,              Button3,        killorzoom,           {0} },
	{ ClkWinTitle,          0,              Button4,        previewallwin,        {0} },
	{ ClkNullWinTitle,      0,              Button4,        previewallwin,        {0} },
	{ ClkWinTitle,          0,              Button5,        previewindexwin,      {0} },
	{ ClkNullWinTitle,      0,              Button5,        previewindexwin,      {0} },
  { ClkStatusText,        0,              Button1,        handleStatus1,        {0} },
  { ClkStatusText,        0,              Button2,        handleStatus2,        {0} },
  { ClkStatusText,        0,              Button3,        handleStatus3,        {0} },
  { ClkStatusText,        0,              Button4,        handleStatus4,        {0} },
  { ClkStatusText,        0,              Button5,        handleStatus5,        {0} },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,            {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating,       {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,          {0} },
};

