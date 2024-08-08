/* See LICENSE file for copyright and license details. */

static const char supericon[] = "   ";
static const char logotext[] = "Arch";
/* status bar */
static const char *interface_name[] = {"enp2s0", "wlp0s20f3", "virbr0", "ztxx4jgrxb", "docker0"};
static const int thermal_zone_num = 2;
/* appearance */
static const unsigned int borderpx  = 3;        /* border pixel of windows */
static const unsigned int gappx     = 6;       /* gap pixel between windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const unsigned int systrayiconsizeredunce = 15;   /* Let the icon not be completely attached to the systray-window, leaving some space */
static const unsigned int systrayicony = systrayiconsizeredunce / 2;   /* Let the icon not be completely attached to the systray-window, leaving some space */
static const unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayspacing = 2;   /* systray spacing */
static const int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray        = 1;     /* 0 means no systray */
static const int showbar            = 1;     /* 0 means no bar */
static const int topbar             = 1;     /* 0 means bottom bar */
static const char *fonts[] = {"Hack Nerd Font:size=14", "Hack Nerd Font:size=9"};
static const char col_white[]       = "#eeeeee";
static const char col_blue1[]       = "#325d9b";
static const char col_blue2[]       = "#2c4a7e";
static const char col_blue3[]       = "#0078d7";
static const char col_green[]       = "#64DD17";
static const char col_orange[]      = "Orange";
static const char col_red[]         = "Red";
static const unsigned int baralpha = 0xd0;
static const unsigned int borderalpha = OPAQUE;
static const char *colors[][3]      = {
  /*               fg           bg          border   */
  [SchemeNorm]   = {col_white,  col_blue1,  col_blue1},
  [SchemeSel]    = {col_white,  col_blue2,  col_orange},
  [SchemeBlue]   = {col_blue3,  col_blue3,  col_blue3},
  [SchemeGreen]  = {col_green,  col_green,  col_green},
  [SchemeOrange] = {col_orange, col_orange, col_orange},
  [SchemeRed]    = {col_red,    col_red,    col_red},
};
static const unsigned int alphas[][3]      = {
    /*               fg      bg        border*/
    [SchemeNorm] = { OPAQUE, baralpha, borderalpha },
	  [SchemeSel]  = { OPAQUE, baralpha, borderalpha },
    [SchemeBlue]   = { OPAQUE, baralpha, borderalpha },
    [SchemeGreen]  = { OPAQUE, baralpha, borderalpha },
    [SchemeOrange] = { OPAQUE, baralpha, borderalpha },
    [SchemeRed]    = { OPAQUE, baralpha, borderalpha },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class            instance    title       tags mask     isfloating   monitor */
	{ "Gimp",           NULL,       NULL,       0,            1,           -1 },
	{ "R-quick-share",  NULL,       NULL,       1 << 8,       0,           -1 },
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

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
static const char *roficmd[] = {"rofi", "-mousable", "-show", NULL};
static const char *tabsurf[] = {"tabbed", "surf", "-e", NULL};
static const char *termcmd[] = {"tabbed", "-r", "2", "st", "-w", "''", NULL};
static const char *termcmd_notab[] = {"st", NULL};
static const char *inc_light[] = {"light", "-A", "5", NULL};
static const char *inc_light_1[] = {"light", "-A", "1", NULL};
static const char *dec_light[] = {"light", "-U", "5", NULL};
static const char *dec_light_1[] = {"light", "-U", "1", NULL};
static const char *inc_kbd_light[] = { "light", "-s", "sysfs/leds/chromeos::kbd_backlight", "-A", "5", NULL};
static const char *dec_kbd_light[] = { "light", "-s", "sysfs/leds/chromeos::kbd_backlight", "-U", "5", NULL};
static const char *inc_volume[] = {"pactl", "set-sink-volume", "@DEFAULT_SINK@", "+5%", NULL};
static const char *inc_volume_1[] = {"pactl", "set-sink-volume", "@DEFAULT_SINK@", "+1%", NULL};
static const char *dec_volume[] = {"pactl", "set-sink-volume", "@DEFAULT_SINK@", "-5%", NULL};
static const char *dec_volume_1[] = {"pactl", "set-sink-volume", "@DEFAULT_SINK@", "-1%", NULL};
static const char *tog_volume[] = {"pactl", "set-sink-mute", "@DEFAULT_SINK@", "toggle", NULL};
static const char *flameshot[] = {"flameshot", "gui", NULL};
static const char *pcmanfm[] = {"pcmanfm", NULL};
static const char *dmlock[] = {"Xsleep", "1", NULL};

static const Key keys[] = {
	/* modifier                     key        function        argument */
  { MODKEY,                       XK_F5,     spawn,          {.v = dec_light}},
  { MODKEY,                       XK_F6,     spawn,          {.v = inc_light}},
  { MODKEY|ShiftMask,             XK_F5,     spawn,          {.v = dec_kbd_light}},
  { MODKEY|ShiftMask,             XK_F6,     spawn,          {.v = inc_kbd_light}},
  { MODKEY,                       XK_F8,     spawn,          {.v = tog_volume}},
  { MODKEY,                       XK_F9,     spawn,          {.v = dec_volume}},
  { MODKEY,                       XK_F10,    spawn,          {.v = inc_volume}},
  { MODKEY,                       XK_Escape, spawn,          {.v = dmlock}},
  { MODKEY,                       XK_Print,  spawn,          {.v = flameshot}},
	{ MODKEY,                       XK_p,      spawn,          {.v = roficmd } },
  { MODKEY|ShiftMask,             XK_p,      spawn,          {.v = tabsurf}},
  { MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd}},
  { MODKEY,                       XK_e,      spawn,          {.v = pcmanfm}},
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	{ MODKEY,                       XK_j,      focusstackvis,  {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstackvis,  {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_j,      focusstackhid,  {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_k,      focusstackhid,  {.i = -1 } },
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
	{ MODKEY,                       XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY|ShiftMask,             XK_c,      killclient,     {0} },
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                       XK_space,  togglehgappx,   {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY|ShiftMask,             XK_f,      togglefullscr,  {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	{ MODKEY,                       XK_s,      show,           {0} },
	{ MODKEY|ShiftMask,             XK_s,      showall,        {0} },
  { MODKEY,                       XK_o,      showonly,       {0}},
	{ MODKEY|ShiftMask,             XK_h,      hide,           {0} },
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
 	{ MODKEY,                       XK_r,      previewallwin,  {0} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
  { ClkWinClass,          0,              Button1,        spawn,          {.v = roficmd}},
  { ClkWinClass,          0,              Button3,        spawn,          {.v = termcmd_notab}},
	{ ClkWinClass,          0,              Button4,        pview,          {0} },
	{ ClkWinClass,          0,              Button5,        nview,          {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            0,              Button4,        pview,          {0} },
	{ ClkTagBar,            0,              Button5,        nview,          {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
  { ClkLtSymbol,          0,              Button1,        setlayout,      {0}},
  { ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]}},
	{ ClkWinTitle,          0,              Button1,        togglewin,      {0} },
	{ ClkWinTitle,          0,              Button3,        killorzoom,     {0} },
	{ ClkWinTitle,          0,              Button5,        previewallwin,  {0} },
	{ ClkNullWinTitle,      0,              Button5,        previewallwin,  {0} },
  { ClkStatusText,        0,              Button1,        handleStatus1,  {0}},
  { ClkStatusText,        0,              Button2,        handleStatus2,  {0}},
  { ClkStatusText,        0,              Button3,        handleStatus3,  {0}},
  { ClkStatusText,        0,              Button4,        handleStatus4,  {0}},
  { ClkStatusText,        0,              Button5,        handleStatus5,  {0}},
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
};

