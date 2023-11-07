/* See LICENSE file for copyright and license details. */
/* status bar */
static const char interface_name[] = "wlp0s20f3";
/* appearance */
static const unsigned int borderpx = 1; /* border pixel of windows */
static const unsigned int snap = 32;    /* snap pixel */
static const unsigned int systraypinning =
    0; /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor
          X */
static const unsigned int systrayonleft =
    0; /* 0: systray in the right corner, >0: systray on left of status text */
static const unsigned int systrayspacing = 2; /* systray spacing */
static const int systraypinningfailfirst =
    1; /* 1: if pinning fails, display systray on the first monitor, False:
          display systray on the last monitor*/
static const int showsystray = 1;   /* 0 means no systray */
static const int systrayrpad = 620; /* right padding for systray */
static const int showbar = 1;       /* 0 means no bar */
static const int topbar = 1;        /* 0 means bottom bar */
static const char *fonts[] = {"Hack Nerd Font:size=14",
                              "Hack Nerd Font:size=9"};
static const char col_white[] = "#eeeeee";
static const char col_blue1[] = "#325d9b";
static const char col_blue2[] = "#1c3a5e";
static const char col_Green[] = "Green";
static const char col_Orange[] = "Orange";
static const char col_Red[] = "Red";
static const char *colors[][3] = {
    /*               fg         bg         border   */
    [SchemeNorm] = {col_white, col_blue1, col_blue1},
    [SchemeSel] = {col_white, col_blue2, col_blue1},
    [SchemeGreen] = {col_Green, col_blue1, col_blue1},
    [SchemeOrange] = {col_Orange, col_blue1, col_blue1},
    [SchemeRed] = {col_Red, col_blue1, col_blue1},
};

/* tagging */
static const char *tags[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};

static const Rule rules[] = {
    /* xprop(1):
     *	WM_CLASS(STRING) = instance, class
     *	WM_NAME(STRING) = title
     */
    /* class      instance    title       tags mask     isfloating   monitor */
    {"Gimp", NULL, NULL, 0, 1, -1},
    {"Firefox", NULL, NULL, 1 << 8, 0, -1},
};

/* layout(s) */
static const float mfact = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster = 1;    /* number of clients in master area */
static const int resizehints =
    1; /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen =
    1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
    /* symbol     arrange function */
    {" 󰕮 ", tile}, /* first entry is default */
    {" 󰉨 ", NULL}, /* no layout function means floating behavior */
    {" 󱢈 ", monocle},
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY, TAG)                                                      \
  {MODKEY, KEY, view, {.ui = 1 << TAG}},                                       \
      {MODKEY | ControlMask, KEY, toggleview, {.ui = 1 << TAG}},               \
      {MODKEY | ShiftMask, KEY, tag, {.ui = 1 << TAG}},                        \
      {MODKEY | ControlMask | ShiftMask, KEY, toggletag, {.ui = 1 << TAG}},

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd)                                                             \
  {                                                                            \
    .v = (const char *[]) { "/bin/sh", "-c", cmd, NULL }                       \
  }

/* commands */
static const char *roficmd[] = {"rofi", "-show", NULL};
static const char *termcmd[] = {"st", NULL};
static const char *inc_light[] = {"light", "-A", "5", NULL};
static const char *dec_light[] = {"light", "-U", "5", NULL};
static const char *inc_volume[] = {"pactl", "set-sink-volume", "@DEFAULT_SINK@",
                                   "+5%", NULL};
static const char *dec_volume[] = {"pactl", "set-sink-volume", "@DEFAULT_SINK@",
                                   "-5%", NULL};
static const char *tog_volume[] = {"pactl", "set-sink-mute", "@DEFAULT_SINK@",
                                   "toggle", NULL};
static const char *flameshot[] = {"flameshot", "gui", NULL};

static const Key keys[] = {
    /* modifier                     key        function        argument */
    {MODKEY, XK_F5, spawn, {.v = dec_light}},
    {MODKEY, XK_F6, spawn, {.v = inc_light}},
    {MODKEY, XK_F8, spawn, {.v = tog_volume}},
    {MODKEY, XK_F9, spawn, {.v = dec_volume}},
    {MODKEY, XK_F10, spawn, {.v = inc_volume}},
    {MODKEY, XK_Print, spawn, {.v = flameshot}},
    {MODKEY, XK_p, spawn, {.v = roficmd}},
    {MODKEY | ShiftMask, XK_Return, spawn, {.v = termcmd}},
    {MODKEY, XK_b, togglebar, {0}},
    {MODKEY, XK_j, focusstack, {.i = +1}},
    {MODKEY, XK_k, focusstack, {.i = -1}},
    {MODKEY, XK_i, incnmaster, {.i = +1}},
    {MODKEY, XK_d, incnmaster, {.i = -1}},
    {MODKEY, XK_h, setmfact, {.f = -0.05}},
    {MODKEY, XK_l, setmfact, {.f = +0.05}},
    {MODKEY, XK_Return, zoom, {0}},
    {MODKEY, XK_Tab, view, {0}},
    {MODKEY | ShiftMask, XK_c, killclient, {0}},
    {MODKEY, XK_t, setlayout, {.v = &layouts[0]}},
    {MODKEY, XK_f, setlayout, {.v = &layouts[1]}},
    {MODKEY, XK_m, setlayout, {.v = &layouts[2]}},
    {MODKEY, XK_space, setlayout, {0}},
    {MODKEY | ShiftMask, XK_space, togglefloating, {0}},
    {MODKEY, XK_0, view, {.ui = ~0}},
    {MODKEY | ShiftMask, XK_0, tag, {.ui = ~0}},
    {MODKEY, XK_comma, focusmon, {.i = -1}},
    {MODKEY, XK_period, focusmon, {.i = +1}},
    {MODKEY | ShiftMask, XK_comma, tagmon, {.i = -1}},
    {MODKEY | ShiftMask, XK_period, tagmon, {.i = +1}},
    TAGKEYS(XK_1, 0) TAGKEYS(XK_2, 1) TAGKEYS(XK_3, 2) TAGKEYS(XK_4, 3)
        TAGKEYS(XK_5, 4) TAGKEYS(XK_6, 5) TAGKEYS(XK_7, 6) TAGKEYS(XK_8, 7)
            TAGKEYS(XK_9, 8){MODKEY | ShiftMask, XK_q, quit, {0}},
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
 * ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
    /* click                event mask      button          function argument */
    {ClkLtSymbol, 0, Button1, setlayout, {0}},
    {ClkLtSymbol, 0, Button3, setlayout, {.v = &layouts[2]}},
    {ClkWinTitle, 0, Button2, zoom, {0}},
    //{ClkStatusText, 0, Button1, fifostatusbar, {.i = 1}},
    {ClkWinTitle, 0, Button1, spawn, {.v = roficmd}},
    {ClkWinTitle, 0, Button3, spawn, {.v = termcmd}},
    {ClkClientWin, MODKEY, Button1, movemouse, {0}},
    {ClkClientWin, MODKEY, Button2, togglefloating, {0}},
    {ClkClientWin, MODKEY, Button3, resizemouse, {0}},
    {ClkTagBar, 0, Button1, view, {0}},
    {ClkTagBar, 0, Button3, toggleview, {0}},
    {ClkTagBar, MODKEY, Button1, tag, {0}},
    {ClkTagBar, MODKEY, Button3, toggletag, {0}},
};
