/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <dirent.h>
#include <locale.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>

#include "drw.h"
#include "util.h"

/* External declaration of status bar mutex */
extern pthread_mutex_t status_mutex;

/* macros */
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask)                                                        \
  (mask & ~(numlockmask | LockMask) &                                          \
   (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask |      \
    Mod5Mask))
#define INTERSECT(x, y, w, h, m)                                               \
  (MAX(0, MIN((x) + (w), (m)->wx + (m)->ww) - MAX((x), (m)->wx)) *             \
   MAX(0, MIN((y) + (h), (m)->wy + (m)->wh) - MAX((y), (m)->wy)))
#define ISVISIBLE(C) ((C->tags & C->mon->tagset[C->mon->seltags]))
#define HIDDEN(C) ((getstate(C->win) == IconicState))
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X) ((X)->w + 2 * (X)->bw)
#define HEIGHT(X) ((X)->h + 2 * (X)->bw)
#define TAGMASK ((1 << LENGTH(tags)) - 1)
#define TEXTW(X) (drw_fontset_getwidth(drw, (X)) + lrpad)
#define OPAQUE 0xffU
#define SYSTEM_TRAY_REQUEST_DOCK 0
/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_FOCUS_IN 4
#define XEMBED_MODALITY_ON 10
#define XEMBED_MAPPED (1 << 0)
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_WINDOW_DEACTIVATE 2
#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum {
  SchemeNorm,
  SchemeSel,
  SchemeBlue,
  SchemePurple,
  SchemeGreen,
  SchemeOrange,
  SchemeYellow,
  SchemeRed,
  SchemeFG
}; /* color schemes */
enum {
  NetSupported,
  NetWMName,
  NetWMState,
  NetWMCheck,
  NetSystemTray,
  NetSystemTrayOP,
  NetSystemTrayOrientation,
  NetSystemTrayOrientationHorz,
  NetWMFullscreen,
  NetActiveWindow,
  NetWMWindowType,
  NetWMWindowTypeDialog,
  NetClientList,
  NetLast
}; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum {
  WMProtocols,
  WMDelete,
  WMState,
  WMTakeFocus,
  WMLast
}; /* default atoms */
enum {
  ClkTagBar,
  ClkLtSymbol,
  ClkStatusText,
  ClkWinTitle,
  ClkNullWinTitle,
  ClkWinClass,
  ClkSuperIcon,
  ClkClientWin,
  ClkRootWin,
  ClkLast
}; /* clicks */

typedef union {
  int i;
  unsigned int ui;
  float f;
  const void *v;
} Arg;

typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(const Arg *arg);
  const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;

typedef struct Preview Preview;
struct Preview {
  XImage *orig_image;
  XImage *scaled_image;
  Window win;
  unsigned int x, y;
  Preview *next;
};

struct Client {
  char name[256];
  float mina, maxa, mfact;
  int x, y, w, h;
  int oldx, oldy, oldw, oldh;
  int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
  int bw, oldbw;
  unsigned int tags;
  int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen,
      isLeftEdgeLean, isAtEdge;
  Client *next;
  Client *snext;
  Monitor *mon;
  Window win;
  Preview pre;
};

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct {
  const char *symbol;
  void (*arrange)(Monitor *);
} Layout;

struct Monitor {
  char ltsymbol[16];
  float mfact;
  int isLeftEdgeLean;
  int num;
  int by;             /* bar geometry */
  int btw;            /* width of tasks portion of bar */
  int bt;             /* number of tasks */
  int mx, my, mw, mh; /* screen size */
  int wx, wy, ww, wh; /* window area  */
  unsigned int seltags;
  unsigned int sellt;
  unsigned int tagset[2];
  int showbar;
  int topbar;
  int hidsel;
  Client *clients;
  Client *visible;
  Client *sel;
  Client *stack;
  Monitor *next;
  Window barwin;
  const Layout *lt[2];
};

typedef struct {
  const char *class;
  const char *instance;
  const char *title;
  unsigned int tags;
  int isfloating;
  int monitor;
} Rule;

typedef struct Systray Systray;
struct Systray {
  Window win;
  Client *icons;
};

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h,
                          int interact);
static void arrange(Monitor *m);
static void arrangeClients(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachbottom(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void transferFocusAttributes(Client *unfocus, Client *focus);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstackvis(const Arg *arg);
static void focusstackedge(const Arg *arg);
static void focusstack(int inc);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static unsigned int getsystraywidth();
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void hide(const Arg *arg);
static void hidewin(Client *c);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static Client *nextpreview(Client *c);
static Client *nexttiled(Client *c);
static Client *nextvisible(Client *c);
static void pop(Client *c);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void removesystrayicon(Client *i);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void resizerequest(XEvent *e);
static void restack(Monitor *m);
static void run(void);
static void runautostart(void);
static void scan(void);
static int sendevent(Window w, Atom proto, int m, long d0, long d1, long d2,
                     long d3, long d4);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void show(const Arg *arg);
static void showall(const Arg *arg);
static void showonly(const Arg *arg);
static void showwin(Client *c);
static void showhide(Client *c);
static void spawn(const Arg *arg);
static Monitor *systraytomon(Monitor *m);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *m);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglehgappx(const Arg *arg);
static void togglefullscr(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void togglewin(const Arg *arg);
static void togglesupericon(const Arg *arg);
static void toggleEdgeLean(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatesystray(void);
static void updatesystrayicongeom(Client *i, int w, int h);
static void updatesystrayiconstate(Client *i, XPropertyEvent *ev);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static void switchtoclient(const Arg *arg);
static void nview(const Arg *arg);
static void pview(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static Client *wintosystrayicon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void xinitvisual();
static void zoom(const Arg *arg);
static void moveclient(const Arg *arg);
static void killorzoom(const Arg *arg);
static void previewallwin();
static void previewindexwin();
static void arrangePreviews(unsigned int n, Monitor *m, unsigned int gappo,
                            unsigned int gappi);
static void arrangeIndexPreviews(unsigned int n, Monitor *m, unsigned int gappo,
                                 unsigned int gappi);
static XImage *getwindowximage(Client *c);
static XImage *getwindowximage_safe(Client *c);
static XImage *create_placeholder_image(unsigned int w, unsigned int h);
static XImage *scaledownimage(XImage *orig_image, unsigned int cw,
                              unsigned int ch);

/* variables */
static int supericonflag = 1;
static Systray *systray = NULL;
static int systandstat; /* right padding for systray */
static int systrayw = 100;
static int logotitlew;
static int supericonw;
static int screen;
static int sw, sh; /* X display screen geometry width, height */
static int bh;     /* bar height */
static int lrpad;  /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent])(XEvent *) = {
    [ButtonPress] = buttonpress,
    [ClientMessage] = clientmessage,
    [ConfigureRequest] = configurerequest,
    [ConfigureNotify] = configurenotify,
    [DestroyNotify] = destroynotify,
    [EnterNotify] = enternotify,
    [Expose] = expose,
    [FocusIn] = focusin,
    [KeyPress] = keypress,
    [MappingNotify] = mappingnotify,
    [MapRequest] = maprequest,
    [MotionNotify] = motionnotify,
    [PropertyNotify] = propertynotify,
    [ResizeRequest] = resizerequest,
    [UnmapNotify] = unmapnotify};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;
static Fnt *normalfont;
static Fnt *smallfont;

static int useargb = 0;
static Visual *visual;
static int depth;
static Colormap cmap;

/* configuration, allows nested code to access above variables */
#include "status.c"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags {
  char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

/* function implementations */
void applyrules(Client *c) {
  const char *class, *instance;
  unsigned int i;
  const Rule *r;
  Monitor *m;
  XClassHint ch = {NULL, NULL};

  /* rule matching */
  c->isfloating = 0;
  c->tags = 0;
  XGetClassHint(dpy, c->win, &ch);
  class = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name ? ch.res_name : broken;

  for (i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if ((!r->title || strstr(c->name, r->title)) &&
        (!r->class || strstr(class, r->class)) &&
        (!r->instance || strstr(instance, r->instance))) {
      c->isfloating = r->isfloating;
      c->tags |= r->tags;
      for (m = mons; m && m->num != r->monitor; m = m->next)
        ;
      if (m)
        c->mon = m;
    }
  }
  if (ch.res_class)
    XFree(ch.res_class);
  if (ch.res_name)
    XFree(ch.res_name);
  c->tags =
      c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact) {
  int baseismin;
  Monitor *m = c->mon;

  /* set minimum possible */
  *w = MAX(1, *w);
  *h = MAX(1, *h);
  if (interact) {
    if (*x > sw)
      *x = sw - WIDTH(c);
    if (*y > sh)
      *y = sh - HEIGHT(c);
    if (*x + *w + 2 * c->bw < 0)
      *x = 0;
    if (*y + *h + 2 * c->bw < 0)
      *y = 0;
  } else {
    if (*x >= m->wx + m->ww)
      *x = m->wx + m->ww - WIDTH(c);
    if (*y >= m->wy + m->wh)
      *y = m->wy + m->wh - HEIGHT(c);
    if (*x + *w + 2 * c->bw <= m->wx)
      *x = m->wx;
    if (*y + *h + 2 * c->bw <= m->wy)
      *y = m->wy;
  }
  if (*h < bh)
    *h = bh;
  if (*w < bh)
    *w = bh;
  if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    if (!c->hintsvalid)
      updatesizehints(c);
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if (!baseismin) { /* temporarily remove base dimensions */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for aspect limits */
    if (c->mina > 0 && c->maxa > 0) {
      if (c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if (c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if (baseismin) { /* increment calculation requires this */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for increment value */
    if (c->incw)
      *w -= *w % c->incw;
    if (c->inch)
      *h -= *h % c->inch;
    /* restore base dimensions */
    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if (c->maxw)
      *w = MIN(*w, c->maxw);
    if (c->maxh)
      *h = MIN(*h, c->maxh);
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void arrange(Monitor *m) {
  if (m)
    showhide(m->stack);
  else
    for (m = mons; m; m = m->next)
      showhide(m->stack);
  if (m) {
    arrangemon(m);
    restack(m);
  } else
    for (m = mons; m; m = m->next)
      arrangemon(m);
}

void arrangeClients(Monitor *m) {
  Client *h1 = NULL, *t1 = NULL;
  Client *h2 = NULL, *t2 = NULL;
  Client *current = m->clients;
  Client *next_node;

  while (current != NULL) {
    next_node = current->next;
    current->next = NULL;

    if (ISVISIBLE(current)) {
      if (h2 == NULL) {
        h2 = t2 = current;
      } else {
        t2->next = current;
        t2 = current;
      }
    } else {
      if (h1 == NULL) {
        h1 = t1 = current;
      } else {
        t1->next = current;
        t1 = current;
      }
    }

    current = next_node;
  }

  if (h1 != NULL) {
    t1->next = h2;
    m->clients = h1;
  } else {
    m->clients = h2;
  }

  m->visible = h2;
}

void arrangemon(Monitor *m) {
  strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
  if (m->lt[m->sellt]->arrange)
    m->lt[m->sellt]->arrange(m);
}

void attach(Client *c) {
  if (ISVISIBLE(c->mon->clients)) {
    c->next = c->mon->clients;
    c->mon->clients = c;
  } else {
    Client *tc;
    for (tc = c->mon->clients; tc && !ISVISIBLE(tc->next); tc = tc->next)
      ;
    c->next = tc->next;
    tc->next = c;
  }
}

void attachbottom(Client *c) {
  if (c->mon->sel) {
    c->next = c->mon->sel->next;
    c->mon->sel->next = c;
  } else {
    if (!c->mon->clients)
      c->mon->clients = c;
    else {
      Client *tc;
      for (tc = c->mon->clients; tc->next; tc = tc->next)
        ;
      c->next = NULL;
      tc->next = c;
    }
  }
}

void attachstack(Client *c) {
  c->snext = c->mon->stack;
  c->mon->stack = c;
}

void buttonpress(XEvent *e) {
  unsigned int i, x, click;
  Arg arg = {0};
  Client *c;
  Monitor *m;
  XButtonPressedEvent *ev = &e->xbutton;

  click = ClkRootWin;
  /* focus monitor if necessary */
  if ((m = wintomon(ev->window)) && m != selmon) {
    unfocus(selmon->sel, 1);
    selmon = m;
    focus(NULL);
  }
  if (ev->window == selmon->barwin) {
    i = 0;
    x = supericonw + logotitlew;
    do
      x += TEXTW(tags[i]);
    while (ev->x >= x && ++i < LENGTH(tags));
    if (ev->x < supericonw) {
      click = ClkSuperIcon;
    } else if (ev->x < supericonw + logotitlew) {
      click = ClkWinClass;
    } else if (i < LENGTH(tags)) {
      click = ClkTagBar;
      arg.ui = 1 << i;
    } else if (ev->x < x + TEXTW(selmon->ltsymbol)) {
      click = ClkLtSymbol;
    } else if (ev->x > selmon->ww - systandstat) {
      int stbsw = 0;
      int stx = selmon->ww - ev->x - systrayw;
      for (int j = 0; j < LENGTH(Blocks); j++) {
        stbsw += Blocks[j].bw;
        if (stbsw > stx) {
          arg.i = j;
          break;
        }
      }
      click = ClkStatusText;
    } else {
      x += TEXTW(selmon->ltsymbol);
      c = m->clients;

      if (c && m->bt) {
        do {
          if (!ISVISIBLE(c))
            continue;
          else
            x += (1.0 / (double)m->bt) * m->btw;
        } while (ev->x > x && (c = c->next));

        click = ClkWinTitle;
        arg.v = c;
      } else {
        click = ClkNullWinTitle;
      }
    }
  } else if ((c = wintoclient(ev->window))) {
    focus(c);
    restack(selmon);
    XAllowEvents(dpy, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }
  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func &&
        buttons[i].button == ev->button &&
        CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
      buttons[i].func((click == ClkTagBar || click == ClkWinTitle ||
                       click == ClkStatusText) &&
                              buttons[i].arg.i == 0
                          ? &arg
                          : &buttons[i].arg);
}

void checkotherwm(void) {
  xerrorxlib = XSetErrorHandler(xerrorstart);
  /* this causes an error if some other window manager is running */
  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
  XSync(dpy, False);
  XSetErrorHandler(xerror);
  XSync(dpy, False);
}

void cleanup(void) {
  Arg a = {.ui = ~0};
  Layout foo = {"", NULL};
  Monitor *m;
  size_t i;

  view(&a);
  selmon->lt[selmon->sellt] = &foo;
  for (m = mons; m; m = m->next)
    while (m->stack)
      unmanage(m->stack, 0);
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  while (mons)
    cleanupmon(mons);

  if (showsystray) {
    XUnmapWindow(dpy, systray->win);
    XDestroyWindow(dpy, systray->win);
    free(systray);
  }

  for (i = 0; i < CurLast; i++)
    drw_cur_free(drw, cursor[i]);
  for (i = 0; i < LENGTH(colors); i++)
    free(scheme[i]);
  free(scheme);
  XDestroyWindow(dpy, wmcheckwin);
  clean_status_pthread();
  drw_free(drw);
  XSync(dpy, False);
  XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void cleanupmon(Monitor *mon) {
  Monitor *m;

  if (mon == mons)
    mons = mons->next;
  else {
    for (m = mons; m && m->next != mon; m = m->next)
      ;
    m->next = mon->next;
  }
  XUnmapWindow(dpy, mon->barwin);
  XDestroyWindow(dpy, mon->barwin);
  free(mon);
}

void clientmessage(XEvent *e) {
  XWindowAttributes wa;
  XSetWindowAttributes swa;
  XClientMessageEvent *cme = &e->xclient;
  Client *c = wintoclient(cme->window);

  if (showsystray && cme->window == systray->win &&
      cme->message_type == netatom[NetSystemTrayOP]) {
    /* add systray icons */
    if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
      if (!(c = (Client *)calloc(1, sizeof(Client))))
        die("fatal: could not malloc() %u bytes\n", sizeof(Client));
      if (!(c->win = cme->data.l[2])) {
        free(c);
        return;
      }
      c->mon = selmon;
      c->next = systray->icons;
      systray->icons = c;
      if (!XGetWindowAttributes(dpy, c->win, &wa)) {
        /* use sane defaults */
        wa.width = bh;
        wa.height = bh;
        wa.border_width = 0;
      }
      c->x = c->oldx = c->y = c->oldy = 0;
      c->w = c->oldw = wa.width;
      c->h = c->oldh = wa.height;
      c->oldbw = wa.border_width;
      c->bw = 0;
      c->isfloating = True;
      /* reuse tags field as mapped status */
      c->tags = 1;
      updatesizehints(c);
      updatesystrayicongeom(c, wa.width, wa.height);
      XAddToSaveSet(dpy, c->win);
      XSelectInput(dpy, c->win,
                   StructureNotifyMask | PropertyChangeMask |
                       ResizeRedirectMask);
      XReparentWindow(dpy, c->win, systray->win, 0, 0);
      XClassHint ch = {"dwmsystray", "dwmsystray"};
      XSetClassHint(dpy, c->win, &ch);
      /* use parents background color */
      swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
      XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime,
                XEMBED_EMBEDDED_NOTIFY, 0, systray->win,
                XEMBED_EMBEDDED_VERSION);
      /* FIXME not sure if I have to send these events, too */
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime,
                XEMBED_FOCUS_IN, 0, systray->win, XEMBED_EMBEDDED_VERSION);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime,
                XEMBED_WINDOW_ACTIVATE, 0, systray->win,
                XEMBED_EMBEDDED_VERSION);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime,
                XEMBED_MODALITY_ON, 0, systray->win, XEMBED_EMBEDDED_VERSION);
      XSync(dpy, False);
      updatesystray();
      setclientstate(c, NormalState);
    }
    return;
  }

  if (!c)
    return;
  if (cme->message_type == netatom[NetWMState]) {
    if (cme->data.l[1] == netatom[NetWMFullscreen] ||
        cme->data.l[2] == netatom[NetWMFullscreen])
      setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                        || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ &&
                            !c->isfullscreen)));
  } else if (cme->message_type == netatom[NetActiveWindow]) {
    if (c != selmon->sel && !c->isurgent)
      seturgent(c, 1);
  }
}

void configure(Client *c) {
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = dpy;
  ce.event = c->win;
  ce.window = c->win;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->w;
  ce.height = c->h;
  ce.border_width = c->bw;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void configurenotify(XEvent *e) {
  Monitor *m;
  Client *c;
  XConfigureEvent *ev = &e->xconfigure;
  int dirty;

  /* TODO: updategeom handling sucks, needs to be simplified */
  if (ev->window == root) {
    dirty = (sw != ev->width || sh != ev->height);
    sw = ev->width;
    sh = ev->height;
    if (updategeom() || dirty) {
      drw_resize(drw, sw, bh);
      updatebars();
      for (m = mons; m; m = m->next) {
        XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
        for (c = m->clients; c; c = c->next)
          if (c->isfullscreen)
            resizeclient(c, m->mx, m->my, m->mw, m->mh);
      }
      focus(NULL);
      arrange(NULL);
    }
  }
}

void configurerequest(XEvent *e) {
  Client *c;
  Monitor *m;
  XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;

  if ((c = wintoclient(ev->window))) {
    if (ev->value_mask & CWBorderWidth)
      c->bw = ev->border_width;
    else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
      m = c->mon;
      if (ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x = m->mx + ev->x;
      }
      if (ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y = m->my + ev->y;
      }
      if (ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w = ev->width;
      }
      if (ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h = ev->height;
      }
      if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
        c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
      if ((c->y + c->h) > m->my + m->mh && c->isfloating)
        c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
      if ((ev->value_mask & (CWX | CWY)) &&
          !(ev->value_mask & (CWWidth | CWHeight)))
        configure(c);
      if (ISVISIBLE(c))
        XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
    } else
      configure(c);
  } else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
  }
  XSync(dpy, False);
}

Monitor *createmon(void) {
  Monitor *m;

  m = ecalloc(1, sizeof(Monitor));
  m->tagset[0] = m->tagset[1] = 1;
  m->mfact = mfact;
  m->isLeftEdgeLean = LeftEdgeLean;
  m->showbar = showbar;
  m->topbar = topbar;
  m->lt[0] = &layouts[0];
  m->lt[1] = &layouts[1 % LENGTH(layouts)];
  strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
  return m;
}

void destroynotify(XEvent *e) {
  Client *c;
  XDestroyWindowEvent *ev = &e->xdestroywindow;

  if ((c = wintoclient(ev->window)))
    unmanage(c, 1);
  else if ((c = wintosystrayicon(ev->window))) {
    removesystrayicon(c);
    updatesystray();
  }
}

void detach(Client *c) {
  Client **tc;

  for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next)
    ;
  *tc = c->next;
}

void detachstack(Client *c) {
  Client **tc, *t;

  for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext)
    ;
  *tc = c->snext;

  if (c == c->mon->sel) {
    for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
      ;
    c->mon->sel = t;
  }
}

Monitor *dirtomon(int dir) {
  Monitor *m = NULL;

  if (dir > 0) {
    if (!(m = selmon->next))
      m = mons;
  } else if (selmon == mons)
    for (m = mons; m->next; m = m->next)
      ;
  else
    for (m = mons; m->next != selmon; m = m->next)
      ;
  return m;
}

void drawbar(Monitor *m) {
  int x, w, tw = 0, stw = 0, n = 0, scm, tagstop = 3, tagslpad = 2;
  unsigned int i, occ = 0, urg = 0;
  Client *c;

  if (!m->showbar)
    return;

  // Lock mutex to protect status bar area if this is the selected monitor
  if (m == selmon) {
    pthread_mutex_lock(&status_mutex);
  }

  if (showsystray && m == systraytomon(m))
    stw = systandstat;

  for (c = m->clients; c; c = c->next) {
    if (ISVISIBLE(c))
      n++;
    occ |= c->tags;
    if (c->isurgent)
      urg |= c->tags;
  }
  x = 0;
  supericonw = TEXTW(supericon);
  drw_setscheme(drw, (supericonflag) ? scheme[SchemeNorm] : scheme[SchemeBlue]);
  drw_text(drw, x, 0, supericonw, bh, lrpad, supericon, 0);

  drw_setscheme(drw, scheme[SchemeNorm]);
  x += supericonw;
  if (selmon->sel) {
    const char *winclass;
    XClassHint ch = {NULL, NULL};
    XGetClassHint(dpy, selmon->sel->win, &ch);
    winclass = ch.res_class ? ch.res_class : broken;
    logotitlew = TEXTW(winclass) + lrpad;
    drw_text(drw, x, 0, logotitlew, bh, lrpad, winclass, 0);
  } else {
    logotitlew = TEXTW(logotext) + lrpad;
    drw_text(drw, x, 0, logotitlew, bh, lrpad, logotext, 0);
  }
  x += logotitlew;

  for (i = 0; i < LENGTH(tags); i++) {
    w = TEXTW(tags[i]);
    drw_setscheme(
        drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
    drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
    if (occ & 1 << i)
      drw_rect(drw, x + tagslpad, tagstop, w - tagslpad * 2, 1, 1, 0);
    x += w;
  }
  w = TEXTW(m->ltsymbol);
  drw_setscheme(drw, scheme[SchemeNorm]);
  x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

  if ((w = m->ww - tw - stw - x) > bh) {
    if (n > 0) {
      drw_setfontset(drw, smallfont);
      int remainder = w % n;
      int tabw = (1.0 / (double)n) * w + 1;
      for (c = m->clients; c; c = c->next) {
        if (!ISVISIBLE(c))
          continue;
        if (m->sel == c)
          scm = SchemeSel;
        else
          scm = SchemeNorm;
        drw_setscheme(drw, scheme[scm]);

        if (remainder >= 0) {
          if (remainder == 0) {
            tabw--;
          }
          remainder--;
        }
        int titletextw = TEXTW(c->name);
        int offset = (tabw - titletextw) / 2;
        if (offset >= 0) {
          drw_rect(drw, x, 0, offset, bh, 1, 1);
          drw_text(drw, x + offset, 0, tabw - offset, bh, lrpad / 2, c->name,
                   0);
          // If it is a floating window, mark
          if (c->isfloating) {
            drw_rect(drw, x + offset, 0, titletextw, 2, 1, 0);
          }
          if (HIDDEN(c)) {
            drw_rect(drw, x + offset, bh / 2, titletextw, 1, 1, 0);
          }
        } else {
          int padding = 5;
          drw_text(drw, x, 0, tabw, bh, lrpad / 2, c->name, 0);
          // If it is a floating window, mark
          if (c->isfloating) {
            drw_rect(drw, x + padding, 0, tabw - 2 * padding, 2, 1, 0);
          }
          if (HIDDEN(c)) {
            drw_rect(drw, x + padding, bh / 2, tabw - 2 * padding, 1, 1, 0);
          }
        }
        x += tabw;
      }
      drw_setfontset(drw, normalfont);
    } else {
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_rect(drw, x, 0, w, bh, 1, 1);
    }
  }
  m->bt = n;
  m->btw = w;
  drw_map(drw, m->barwin, 0, 0, m->ww - stw, bh);

  // Release mutex if this is the selected monitor
  if (m == selmon) {
    pthread_mutex_unlock(&status_mutex);
  }
}

void drawbars(void) {
  Monitor *m;

  for (m = mons; m; m = m->next)
    drawbar(m);
}

void enternotify(XEvent *e) {
  Client *c;
  Monitor *m;
  XCrossingEvent *ev = &e->xcrossing;

  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) &&
      ev->window != root)
    return;
  c = wintoclient(ev->window);
  m = c ? c->mon : wintomon(ev->window);
  if (m != selmon) {
    unfocus(selmon->sel, 1);
    selmon = m;
  } else if (!c || c == selmon->sel)
    return;
  focus(c);
}

void expose(XEvent *e) {
  Monitor *m;
  XExposeEvent *ev = &e->xexpose;

  if (ev->count == 0 && (m = wintomon(ev->window))) {
    drawbar(m);
    if (m == selmon)
      updatesystray();
  }
}

void transferFocusAttributes(Client *unfocus, Client *focus) {
  if (unfocus && unfocus->isfloating) {
    return;
  }
  if (unfocus && focus && focus->tags == unfocus->tags && unfocus != focus) {
    focus->isLeftEdgeLean = unfocus->isLeftEdgeLean;
    for (Client *c = nexttiled(focus->mon->clients); c;
         c = nexttiled(c->next)) {
      if (c == focus) {
        focus->isAtEdge = focus->isLeftEdgeLean ? 1 : 0;
        return;
      }
      if (c == unfocus) {
        focus->isAtEdge = focus->isLeftEdgeLean ? 0 : 1;
        return;
      }
    }
  }
}

void focus(Client *c) {
  transferFocusAttributes(selmon->sel, c);
  if (!c || !ISVISIBLE(c))
    for (c = selmon->stack; c && (!ISVISIBLE(c) || HIDDEN(c)); c = c->snext)
      ;
  if (selmon->sel && selmon->sel != c) {
    unfocus(selmon->sel, 0);

    if (selmon->hidsel) {
      hidewin(selmon->sel);
      if (c)
        arrange(c->mon);
      selmon->hidsel = 0;
    }
  }
  if (c) {
    if (c->mon != selmon)
      selmon = c->mon;
    if (c->isurgent)
      seturgent(c, 0);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
    setfocus(c);
  } else {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
  selmon->sel = c;
  drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void focusin(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;

  if (selmon->sel && ev->window != selmon->sel->win)
    setfocus(selmon->sel);
}

void focusmon(const Arg *arg) {
  Monitor *m;

  if (!mons->next)
    return;
  if ((m = dirtomon(arg->i)) == selmon)
    return;
  unfocus(selmon->sel, 0);
  selmon = m;
  focus(NULL);
}

void focusstackvis(const Arg *arg) {
  focusstack(arg->i);
  arrange(selmon);
}

void focusstackedge(const Arg *arg) {
  if (selmon->sel) {
    focusstack(arg->i);
    if (!selmon->sel->isAtEdge) {
      selmon->sel->mon->isLeftEdgeLean = !selmon->sel->isLeftEdgeLean;
      selmon->sel->isLeftEdgeLean = !selmon->sel->isLeftEdgeLean;
      selmon->sel->isAtEdge = 1;
    }
    arrange(selmon);
  }
}

void focusstack(int inc) {
  Client *c = NULL, *i;
  // if no client selected AND exclude hidden client; if client selected but
  // fullscreened
  if (!selmon->sel ||
      (selmon->sel && selmon->sel->isfullscreen && lockfullscreen))
    return;
  if (!selmon->clients)
    return;
  if (inc > 0) {
    if (selmon->sel)
      for (c = selmon->sel->next; c && (!ISVISIBLE(c) || HIDDEN(c));
           c = c->next)
        ;
    if (!c)
      return;
  } else {
    if (selmon->sel) {
      for (i = selmon->clients; i != selmon->sel; i = i->next)
        if (ISVISIBLE(i) && !HIDDEN(i))
          c = i;
    } else
      return;
  }
  if (c) {
    focus(c);
    restack(selmon);
    if (HIDDEN(c)) {
      showwin(c);
      c->mon->hidsel = 1;
    }
  }
}

Atom getatomprop(Client *c, Atom prop) {
  int di;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da, atom = None;

  /* FIXME getatomprop should return the number of items and a pointer to
   * the stored data instead of this workaround */
  Atom req = XA_ATOM;
  if (prop == xatom[XembedInfo])
    req = xatom[XembedInfo];

  if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req, &da,
                         &di, &dl, &dl, &p) == Success &&
      p) {
    atom = *(Atom *)p;
    if (da == xatom[XembedInfo] && dl == 2)
      atom = ((Atom *)p)[1];
    XFree(p);
  }
  return atom;
}

int getrootptr(int *x, int *y) {
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long getstate(Window w) {
  int format;
  long result = -1;
  unsigned char *p = NULL;
  unsigned long n, extra;
  Atom real;

  if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False,
                         wmatom[WMState], &real, &format, &n, &extra,
                         (unsigned char **)&p) != Success)
    return -1;
  if (n != 0)
    result = *p;
  XFree(p);
  return result;
}

unsigned int getsystraywidth() {
  unsigned int w = 0;
  Client *i;
  if (showsystray)
    for (i = systray->icons; i; w += i->w + systrayspacing, i = i->next)
      ;
  return w ? w + systrayspacing : 1;
}

int gettextprop(Window w, Atom atom, char *text, unsigned int size) {
  char **list = NULL;
  int n;
  XTextProperty name;

  if (!text || size == 0)
    return 0;
  text[0] = '\0';
  if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
    return 0;
  if (name.encoding == XA_STRING) {
    strncpy(text, (char *)name.value, size - 1);
  } else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success &&
             n > 0 && *list) {
    strncpy(text, *list, size - 1);
    XFreeStringList(list);
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}

void grabbuttons(Client *c, int focused) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = {0, LockMask, numlockmask,
                                numlockmask | LockMask};
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    if (!focused)
      XGrabButton(dpy, AnyButton, AnyModifier, c->win, False, BUTTONMASK,
                  GrabModeSync, GrabModeSync, None, None);
    for (i = 0; i < LENGTH(buttons); i++)
      if (buttons[i].click == ClkClientWin)
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabButton(dpy, buttons[i].button, buttons[i].mask | modifiers[j],
                      c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync,
                      None, None);
  }
}

void grabkeys(void) {
  updatenumlockmask();
  {
    unsigned int i, j, k;
    unsigned int modifiers[] = {0, LockMask, numlockmask,
                                numlockmask | LockMask};
    int start, end, skip;
    KeySym *syms;

    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XDisplayKeycodes(dpy, &start, &end);
    syms = XGetKeyboardMapping(dpy, start, end - start + 1, &skip);
    if (!syms)
      return;
    for (k = start; k <= end; k++)
      for (i = 0; i < LENGTH(keys); i++)
        /* skip modifier codes, we do that ourselves */
        if (keys[i].keysym == syms[(k - start) * skip])
          for (j = 0; j < LENGTH(modifiers); j++)
            XGrabKey(dpy, k, keys[i].mod | modifiers[j], root, True,
                     GrabModeAsync, GrabModeAsync);
    XFree(syms);
  }
}

void hide(const Arg *arg) {
  hidewin(selmon->sel);
  focus(NULL);
  arrange(selmon);
}

void hidewin(Client *c) {
  if (!c || HIDDEN(c))
    return;

  Window w = c->win;
  static XWindowAttributes ra, ca;
  
  // Clean up existing preview image before creating a new one
  if (c->pre.orig_image) {
    XDestroyImage(c->pre.orig_image);
    c->pre.orig_image = NULL;
  }
  
  c->pre.orig_image = getwindowximage_safe(c);

  // more or less taken directly from blackbox's hide() function
  XGrabServer(dpy);
  XGetWindowAttributes(dpy, root, &ra);
  XGetWindowAttributes(dpy, w, &ca);
  // prevent UnmapNotify events
  XSelectInput(dpy, root, ra.your_event_mask & ~SubstructureNotifyMask);
  XSelectInput(dpy, w, ca.your_event_mask & ~StructureNotifyMask);
  XUnmapWindow(dpy, w);
  setclientstate(c, IconicState);
  XSelectInput(dpy, root, ra.your_event_mask);
  XSelectInput(dpy, w, ca.your_event_mask);
  XUngrabServer(dpy);
}

#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo *unique, size_t n,
                        XineramaScreenInfo *info) {
  while (n--)
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org &&
        unique[n].width == info->width && unique[n].height == info->height)
      return 0;
  return 1;
}
#endif /* XINERAMA */

void keypress(XEvent *e) {
  if (!supericonflag)
    return;

  unsigned int i;
  KeySym keysym;
  XKeyEvent *ev;

  ev = &e->xkey;
  keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
  for (i = 0; i < LENGTH(keys); i++)
    if (keysym == keys[i].keysym &&
        CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func)
      keys[i].func(&(keys[i].arg));
}

void killclient(const Arg *arg) {
  if (!selmon->sel)
    return;

  if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask,
                 wmatom[WMDelete], CurrentTime, 0, 0, 0)) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, selmon->sel->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
}

void manage(Window w, XWindowAttributes *wa) {
  Client *c, *t = NULL;
  Window trans = None;
  XWindowChanges wc;

  c = ecalloc(1, sizeof(Client));
  c->isLeftEdgeLean = selmon->isLeftEdgeLean;
  c->isAtEdge = selmon->isLeftEdgeLean ? 0 : 1;
  c->mfact = mfact;
  c->win = w;
  // Initialize preview fields
  c->pre.orig_image = NULL;
  c->pre.scaled_image = NULL;
  c->pre.win = 0;
  /* geometry */
  c->x = c->oldx = wa->x;
  c->y = c->oldy = wa->y;
  c->w = c->oldw = wa->width;
  c->h = c->oldh = wa->height;
  c->oldbw = wa->border_width;

  updatetitle(c);
  if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
    c->mon = t->mon;
    c->tags = t->tags;
  } else {
    c->mon = selmon;
    applyrules(c);
  }

  if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
    c->x = c->mon->wx + c->mon->ww - WIDTH(c);
  if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
    c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
  c->x = MAX(c->x, c->mon->wx);
  c->y = MAX(c->y, c->mon->wy);
  c->bw = borderpx;

  wc.border_width = c->bw;
  XConfigureWindow(dpy, w, CWBorderWidth, &wc);
  XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
  configure(c); /* propagates border_width, if size doesn't change */
  updatewindowtype(c);
  updatesizehints(c);
  updatewmhints(c);
  XSelectInput(dpy, w,
               EnterWindowMask | FocusChangeMask | PropertyChangeMask |
                   StructureNotifyMask);
  grabbuttons(c, 0);
  if (!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  if (c->isfloating)
    XRaiseWindow(dpy, c->win);
  attachbottom(c);
  attachstack(c);
  XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
                  PropModeAppend, (unsigned char *)&(c->win), 1);
  XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w,
                    c->h); /* some windows require this */
  if (!HIDDEN(c))
    setclientstate(c, NormalState);
  if (c->mon == selmon)
    unfocus(selmon->sel, 0);
  c->mon->sel = c;
  arrange(c->mon);
  if (!HIDDEN(c))
    XMapWindow(dpy, c->win);
  focus(NULL);
}

void mappingnotify(XEvent *e) {
  XMappingEvent *ev = &e->xmapping;

  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard)
    grabkeys();
}

void maprequest(XEvent *e) {
  static XWindowAttributes wa;
  XMapRequestEvent *ev = &e->xmaprequest;

  Client *i;
  if ((i = wintosystrayicon(ev->window))) {
    sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime,
              XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
    updatesystray();
  }

  if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
    return;
  if (!wintoclient(ev->window))
    manage(ev->window, &wa);
}

void monocle(Monitor *m) {
  unsigned int n = 0;
  Client *c;

  for (c = m->clients; c; c = c->next)
    if (ISVISIBLE(c))
      n++;
  if (n > 0) /* override layout symbol */
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
  for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void motionnotify(XEvent *e) {
  updatesystray();
  static Monitor *mon = NULL;
  Monitor *m;
  XMotionEvent *ev = &e->xmotion;

  if (ev->window != root)
    return;
  if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
    unfocus(selmon->sel, 1);
    selmon = m;
    focus(NULL);
  }
  mon = m;
}

void movemouse(const Arg *arg) {
  int x, y, ocx, ocy, nx, ny;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                   None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
    return;
  if (!getrootptr(&x, &y))
    return;
  do {
    XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if (abs(selmon->wx - nx) < snap)
        nx = selmon->wx;
      else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
        nx = selmon->wx + selmon->ww - WIDTH(c);
      if (abs(selmon->wy - ny) < snap)
        ny = selmon->wy;
      else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
        ny = selmon->wy + selmon->wh - HEIGHT(c);
      if (!c->isfloating && selmon->lt[selmon->sellt]->arrange &&
          (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
        togglefloating(NULL);
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, nx, ny, c->w, c->h, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(dpy, CurrentTime);
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

Client *nextpreview(Client *c) {
  for (; c && !ISVISIBLE(c); c = c->next)
    ;
  return c;
}

Client *nexttiled(Client *c) {
  for (; c && (c->isfloating || !ISVISIBLE(c) || HIDDEN(c)); c = c->next)
    ;
  return c;
}

Client *nextvisible(Client *c) {
  for (; c && !ISVISIBLE(c); c = c->next)
    ;
  return c;
}

void pop(Client *c) {
  detach(c);
  attach(c);
  focus(c);
  arrange(c->mon);
}

void propertynotify(XEvent *e) {
  Client *c;
  Window trans;
  XPropertyEvent *ev = &e->xproperty;

  if ((c = wintosystrayicon(ev->window))) {
    if (ev->atom == XA_WM_NORMAL_HINTS) {
      updatesizehints(c);
      updatesystrayicongeom(c, c->w, c->h);
    } else
      updatesystrayiconstate(c, ev);
    updatesystray();
  }

  if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
    updatestatus();
  } else if (ev->state == PropertyDelete) {
    return; /* ignore */
  } else if ((c = wintoclient(ev->window))) {
    switch (ev->atom) {
    default:
      break;
    case XA_WM_TRANSIENT_FOR:
      if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
          (c->isfloating = (wintoclient(trans)) != NULL))
        arrange(c->mon);
      break;
    case XA_WM_NORMAL_HINTS:
      c->hintsvalid = 0;
      break;
    case XA_WM_HINTS:
      updatewmhints(c);
      drawbars();
      break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
      updatetitle(c);
      if (c == c->mon->sel)
        drawbar(c->mon);
    }
    if (ev->atom == netatom[NetWMWindowType])
      updatewindowtype(c);
  }
}

void quit(const Arg *arg) {
  // fix: reloading dwm keeps all the hidden clients hidden
  Monitor *m;
  Client *c;
  for (m = mons; m; m = m->next) {
    if (m) {
      for (c = m->stack; c; c = c->next)
        if (c && HIDDEN(c))
          showwin(c);
    }
  }

  running = 0;
}

Monitor *recttomon(int x, int y, int w, int h) {
  Monitor *m, *r = selmon;
  int a, area = 0;

  for (m = mons; m; m = m->next)
    if ((a = INTERSECT(x, y, w, h, m)) > area) {
      area = a;
      r = m;
    }
  return r;
}

void removesystrayicon(Client *i) {
  Client **ii;

  if (!showsystray || !i)
    return;
  for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next)
    ;
  if (ii)
    *ii = i->next;
  free(i);
}

void resize(Client *c, int x, int y, int w, int h, int interact) {
  if (applysizehints(c, &x, &y, &w, &h, interact))
    resizeclient(c, x, y, w, h);
}

void resizeclient(Client *c, int x, int y, int w, int h) {
  XWindowChanges wc;

  c->oldx = c->x;
  c->x = wc.x = x;
  c->oldy = c->y;
  c->y = wc.y = y;
  c->oldw = c->w;
  c->w = wc.width = w;
  c->oldh = c->h;
  c->h = wc.height = h;
  wc.border_width = c->bw;
  XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth,
                   &wc);
  configure(c);
  XSync(dpy, False);
}

void resizemouse(const Arg *arg) {
  int ocx, ocy, nw, nh;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                   None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
    return;
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
               c->h + c->bw - 1);
  do {
    XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
      nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
      if (c->mon->wx + nw >= selmon->wx &&
          c->mon->wx + nw <= selmon->wx + selmon->ww &&
          c->mon->wy + nh >= selmon->wy &&
          c->mon->wy + nh <= selmon->wy + selmon->wh) {
        if (!c->isfloating && selmon->lt[selmon->sellt]->arrange &&
            (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
          togglefloating(NULL);
      }
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, c->x, c->y, nw, nh, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
               c->h + c->bw - 1);
  XUngrabPointer(dpy, CurrentTime);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
    ;
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

void resizerequest(XEvent *e) {
  XResizeRequestEvent *ev = &e->xresizerequest;
  Client *i;

  if ((i = wintosystrayicon(ev->window))) {
    updatesystrayicongeom(i, ev->width, ev->height);
    updatesystray();
  }
}

void restack(Monitor *m) {
  Client *c;
  XEvent ev;
  XWindowChanges wc;

  drawbar(m);
  if (!m->sel)
    return;
  if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
    XRaiseWindow(dpy, m->sel->win);
  if (m->lt[m->sellt]->arrange) {
    wc.stack_mode = Below;
    wc.sibling = m->barwin;
    for (c = m->stack; c; c = c->snext)
      if (!c->isfloating && ISVISIBLE(c)) {
        XConfigureWindow(dpy, c->win, CWSibling | CWStackMode, &wc);
        wc.sibling = c->win;
      }
  }
  XSync(dpy, False);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
    ;
}

void run(void) {
  XEvent ev;
  /* main event loop */
  XSync(dpy, False);
  while (running && !XNextEvent(dpy, &ev))
    if (handler[ev.type])
      handler[ev.type](&ev); /* call handler */
}

void runautostart(void) {
  char *pathpfx;
  char *path;
  char *xdgdatahome;
  char *home;
  struct stat sb;

  if ((home = getenv("HOME")) == NULL)
    /* this is almost impossible */
    return;

  /* if $XDG_DATA_HOME is set and not empty, use $XDG_DATA_HOME/dwm,
   * otherwise use ~/.local/share/dwm as autostart script directory
   */
  xdgdatahome = getenv("XDG_DATA_HOME");
  if (xdgdatahome != NULL && *xdgdatahome != '\0') {
    /* space for path segments, separators and nul */
    pathpfx = ecalloc(1, strlen(xdgdatahome) + strlen(dwmdir) + 2);

    if (sprintf(pathpfx, "%s/%s", xdgdatahome, dwmdir) <= 0) {
      free(pathpfx);
      return;
    }
  } else {
    /* space for path segments, separators and nul */
    pathpfx = ecalloc(1, strlen(home) + strlen(dotconfig) + strlen(dwmdir) + 3);

    if (sprintf(pathpfx, "%s/%s/%s", home, dotconfig, dwmdir) < 0) {
      free(pathpfx);
      return;
    }
  }

  /* check if the autostart script directory exists */
  if (!(stat(pathpfx, &sb) == 0 && S_ISDIR(sb.st_mode))) {
    /* the XDG conformant path does not exist or is no directory
     * so we try ~/.dwm instead
     */
    char *pathpfx_new = realloc(pathpfx, strlen(home) + strlen(dwmdir) + 3);
    if (pathpfx_new == NULL) {
      free(pathpfx);
      return;
    }
    pathpfx = pathpfx_new;

    if (sprintf(pathpfx, "%s/.%s", home, dwmdir) <= 0) {
      free(pathpfx);
      return;
    }
  }

  /* try the blocking script first */
  path = ecalloc(1, strlen(pathpfx) + strlen(autostartblocksh) + 2);
  if (sprintf(path, "%s/%s", pathpfx, autostartblocksh) <= 0) {
    free(path);
    free(pathpfx);
  }

  if (access(path, X_OK) == 0)
    system(path);

  /* now the non-blocking script */
  if (sprintf(path, "%s/%s", pathpfx, autostartsh) <= 0) {
    free(path);
    free(pathpfx);
  }

  if (access(path, X_OK) == 0)
    system(strcat(path, " &"));

  free(pathpfx);
  free(path);
}

void scan(void) {
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect ||
          XGetTransientForHint(dpy, wins[i], &d1))
        continue;
      if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
        manage(wins[i], &wa);
    }
    for (i = 0; i < num; i++) { /* now the transients */
      if (!XGetWindowAttributes(dpy, wins[i], &wa))
        continue;
      if (XGetTransientForHint(dpy, wins[i], &d1) &&
          (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
        manage(wins[i], &wa);
    }
    if (wins)
      XFree(wins);
  }
}

void sendmon(Client *c, Monitor *m) {
  if (c->mon == m)
    return;
  unfocus(c, 1);
  detach(c);
  detachstack(c);
  c->mon = m;
  c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  attachbottom(c);
  attachstack(c);
  focus(NULL);
  arrange(NULL);
}

void setclientstate(Client *c, long state) {
  long data[] = {state, None};

  XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
                  PropModeReplace, (unsigned char *)data, 2);
}

int sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2,
              long d3, long d4) {
  int n;
  Atom *protocols, mt;
  int exists = 0;
  XEvent ev;

  if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
    mt = wmatom[WMProtocols];
    if (XGetWMProtocols(dpy, w, &protocols, &n)) {
      while (!exists && n--)
        exists = protocols[n] == proto;
      XFree(protocols);
    }
  } else {
    exists = True;
    mt = proto;
  }

  if (exists) {
    ev.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = mt;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = d0;
    ev.xclient.data.l[1] = d1;
    ev.xclient.data.l[2] = d2;
    ev.xclient.data.l[3] = d3;
    ev.xclient.data.l[4] = d4;
    XSendEvent(dpy, w, False, mask, &ev);
  }
  return exists;
}

void setfocus(Client *c) {
  if (!c->neverfocus) {
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&(c->win), 1);
  }
  sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus],
            CurrentTime, 0, 0, 0);
}

void setfullscreen(Client *c, int fullscreen) {
  if (fullscreen && !c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&netatom[NetWMFullscreen],
                    1);
    c->isfullscreen = 1;
    c->oldstate = c->isfloating;
    c->oldbw = c->bw;
    c->bw = 0;
    c->isfloating = 1;
    resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    XRaiseWindow(dpy, c->win);
  } else if (!fullscreen && c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)0, 0);
    c->isfullscreen = 0;
    c->isfloating = c->oldstate;
    c->bw = c->oldbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    arrange(c->mon);
  }
}

void setlayout(const Arg *arg) {
  if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
    selmon->sellt ^= 1;
  if (arg && arg->v)
    selmon->lt[selmon->sellt] = (Layout *)arg->v;
  strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol,
          sizeof selmon->ltsymbol);
  if (selmon->sel)
    arrange(selmon);
  else
    drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(const Arg *arg) {
  float f;

  if (!arg || !selmon->lt[selmon->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + selmon->sel->mfact : arg->f - 1.0;
  if (f < 0.05 || f > 0.95)
    return;
  selmon->sel->mfact = f;
  arrange(selmon);
}

void setup(void) {
  int i;
  XSetWindowAttributes wa;
  Atom utf8string;
  struct sigaction sa;

  /* do not transform children into zombies when they terminate */
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
  sa.sa_handler = SIG_IGN;
  sigaction(SIGCHLD, &sa, NULL);

  /* clean up any zombies (inherited from .xinitrc etc) immediately */
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  /* init screen */
  screen = DefaultScreen(dpy);
  sw = DisplayWidth(dpy, screen);
  sh = DisplayHeight(dpy, screen);
  root = RootWindow(dpy, screen);
  xinitvisual();
  drw = drw_create(dpy, screen, root, sw, sh, visual, depth, cmap);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
  lrpad = drw->fonts->h / 2;
  bh = drw->fonts->h + 10;
  updategeom();
  /* init atoms */
  utf8string = XInternAtom(dpy, "UTF8_STRING", False);
  wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
  wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
  netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
  netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
  netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
  netatom[NetSystemTrayOrientation] =
      XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
  netatom[NetSystemTrayOrientationHorz] =
      XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
  netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen] =
      XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog] =
      XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
  xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
  xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
  /* init cursors */
  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurMove] = drw_cur_create(drw, XC_fleur);
  /* init appearance */
  scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
  for (i = 0; i < LENGTH(colors); i++)
    scheme[i] = drw_scm_create(drw, colors[i], alphas[i], 3);
  /* init system tray */
  updatesystray();
  /* init bars */
  updatebars();
  updatestatus();
  /* supporting window for NetWMCheck */
  wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
                  PropModeReplace, (unsigned char *)"LG3D",
                  4); /* Changed to solve broken java GUIs */
  XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  /* EWMH support per view */
  XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)netatom, NetLast);
  XDeleteProperty(dpy, root, netatom[NetClientList]);
  /* select events */
  wa.cursor = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                  ButtonPressMask | PointerMotionMask | EnterWindowMask |
                  LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
  XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
  XSelectInput(dpy, root, wa.event_mask);
  grabkeys();
  focus(NULL);
}

void seturgent(Client *c, int urg) {
  XWMHints *wmh;

  c->isurgent = urg;
  if (!(wmh = XGetWMHints(dpy, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(dpy, c->win, wmh);
  XFree(wmh);
}

void show(const Arg *arg) {
  if (selmon->hidsel)
    selmon->hidsel = 0;
  showwin(selmon->sel);
}

void showall(const Arg *arg) {
  Client *c = NULL;
  selmon->hidsel = 0;
  for (c = selmon->clients; c; c = c->next) {
    if (ISVISIBLE(c))
      showwin(c);
  }
  if (!selmon->sel) {
    for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next)
      ;
    if (c)
      focus(c);
  }
  restack(selmon);
}

void showonly(const Arg *arg) {
  Client *c = NULL;
  selmon->hidsel = 0;
  showwin(selmon->sel);
  for (c = selmon->clients; c; c = c->next) {
    if (ISVISIBLE(c) && c != selmon->sel)
      hidewin(c);
  }
  arrange(selmon);
}

void showwin(Client *c) {
  if (!c || !HIDDEN(c))
    return;

  XMapWindow(dpy, c->win);
  setclientstate(c, NormalState);
  arrange(c->mon);
}

void showhide(Client *c) {
  if (!c)
    return;
  if (ISVISIBLE(c)) {
    /* show clients top down */
    XMoveWindow(dpy, c->win, c->x, c->y);
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) &&
        !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {
    /* hide clients bottom up */
    showhide(c->snext);
    XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
  }
}

void spawn(const Arg *arg) {
  struct sigaction sa;

  if (fork() == 0) {
    if (dpy)
      close(ConnectionNumber(dpy));
    setsid();

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sa, NULL);

    execvp(((char **)arg->v)[0], (char **)arg->v);
    die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
  }
}

void tag(const Arg *arg) {
  if (selmon->sel && arg->ui & TAGMASK) {
    selmon->sel->tags = arg->ui & TAGMASK;
    view(arg);
  }
}

void tagmon(const Arg *arg) {
  if (!selmon->sel || !mons->next)
    return;
  sendmon(selmon->sel, dirtomon(arg->i));
}

void tile(Monitor *m) {
  unsigned int n, j, x, y, w, h, sw = 0;
  Client *c, *i;
  h = m->wh - (borderpx + gappx) * 2;
  y = m->wy + gappx;

  for (i = m->stack; i && (!ISVISIBLE(i) || i->isfloating || HIDDEN(i));
       i = i->snext)
    ;
  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    if (c == i)
      j = n;
  if (n == 0)
    return;

  if (n == 1) {
    if (i->mfact < 0.9) {
      h = m->wh * 8 / 9;           /* 8/9 of monitor height,height */
      w = h * 3 / 2;               /* 4/3 of monitor width,width */
      x = m->wx + (m->ww - w) / 2; /* center the window,x */
      y = m->wy + (m->wh - h) / 3; /* center the window,y */
      resize(i, x, y, w, h, False);
      return;
    } else {
      w = m->ww - (borderpx + gappx) * 2;
      x = m->wx + gappx;
      resize(i, x, y, w, h, False);
      return;
    }
  }

  Client *arr[n];
  int k;
  for (k = 0, c = nexttiled(m->clients); c && k < n; c = nexttiled(c->next)) {
    arr[k++] = c;
  }
  if (i->isLeftEdgeLean) {
    if (!i->isAtEdge && j > 0)
      j--;
    for (k = 0; k < n; k++) {
      if (k < j || sw >= m->ww) {
        XMoveWindow(dpy, arr[k]->win, WIDTH(arr[k]) * -2, arr[k]->y);
        continue;
      }
      w = m->ww * arr[k]->mfact - (borderpx + gappx) * 2;
      x = m->wx + sw + gappx;
      resize(arr[k], x, y, w, h, False);
      sw += w + borderpx + gappx;
    }
  } else {
    if (!i->isAtEdge && j < n - 1)
      j++;
    for (k = n - 1; k >= 0; k--) {
      if (k > j || sw >= m->ww) {
        XMoveWindow(dpy, arr[k]->win, WIDTH(arr[k]) * -2, arr[k]->y);
        continue;
      }
      w = m->ww * arr[k]->mfact - (borderpx + gappx) * 2;
      x = m->wx + m->ww - sw - w - gappx - borderpx * 2;
      resize(arr[k], x, y, w, h, False);
      sw += w + borderpx + gappx;
    }
  }
}

void togglebar(const Arg *arg) {
  selmon->showbar = !selmon->showbar;
  updatebarpos(selmon);
  XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww,
                    bh);
  if (showsystray) {
    XWindowChanges wc;
    if (!selmon->showbar)
      wc.y = -bh;
    else if (selmon->showbar) {
      wc.y = 0;
      if (!selmon->topbar)
        wc.y = selmon->mh - bh;
    }
    XConfigureWindow(dpy, systray->win, CWY, &wc);
  }
  arrange(selmon);
}

void togglefloating(const Arg *arg) {
  if (!selmon->sel)
    return;
  if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
    return;
  selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  if (selmon->sel->isfloating)
    resize(selmon->sel, selmon->sel->x, selmon->sel->y, selmon->sel->w,
           selmon->sel->h, 0);
  arrange(selmon);
}

void togglehgappx(const Arg *arg) {
  if (selmon->sel) {
    if (selmon->sel->mfact > 0.9)
      selmon->sel->mfact = mfact;
    else
      selmon->sel->mfact = 0.95;
  }
  arrange(selmon);
}

void togglefullscr(const Arg *arg) {
  if (selmon->sel)
    setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
}

void toggletag(const Arg *arg) {
  unsigned int newtags;

  if (!selmon->sel)
    return;
  newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    selmon->sel->tags = newtags;
    focus(NULL);
    arrangeClients(selmon);
    arrange(selmon);
  }
}

void toggleview(const Arg *arg) {
  unsigned int newtagset =
      selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

  if (newtagset) {
    selmon->tagset[selmon->seltags] = newtagset;
    focus(NULL);
    arrangeClients(selmon);
    arrange(selmon);
  }
}

void togglewin(const Arg *arg) {
  Client *c = (Client *)arg->v;

  if (c == selmon->sel) {
    hidewin(c);
    focus(NULL);
    arrange(c->mon);
  } else {
    if (HIDDEN(c))
      showwin(c);
    focus(c);
    restack(selmon);
    arrange(selmon);
  }
}

void togglesupericon(const Arg *arg) {
  supericonflag = !supericonflag;
  drawbars();
}

void unfocus(Client *c, int setfocus) {
  if (!c)
    return;
  grabbuttons(c, 0);
  XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
  if (setfocus) {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
}

void toggleEdgeLean(const Arg *arg) {
  if (selmon->sel) {
    selmon->isLeftEdgeLean = !selmon->sel->isLeftEdgeLean;
    selmon->sel->isLeftEdgeLean = !selmon->sel->isLeftEdgeLean;
    selmon->sel->isAtEdge = !selmon->sel->isAtEdge;
    arrange(selmon);
  }
}

void unmanage(Client *c, int destroyed) {
  Monitor *m = c->mon;
  XWindowChanges wc;

  detach(c);
  detachstack(c);
  if (!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(dpy); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XSelectInput(dpy, c->win, NoEventMask);
    XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }

  // Clean up preview image resources
  if (c->pre.orig_image) {
    XDestroyImage(c->pre.orig_image);
    c->pre.orig_image = NULL;
  }
  if (c->pre.scaled_image) {
    XDestroyImage(c->pre.scaled_image);
    c->pre.scaled_image = NULL;
  }
  if (c->pre.win) {
    XDestroyWindow(dpy, c->pre.win);
    c->pre.win = 0;
  }

  free(c);
  focus(NULL);
  updateclientlist();
  arrange(m);
}

void unmapnotify(XEvent *e) {
  Client *c;
  XUnmapEvent *ev = &e->xunmap;

  if ((c = wintoclient(ev->window))) {
    if (ev->send_event)
      setclientstate(c, WithdrawnState);
    else
      unmanage(c, 0);
  } else if ((c = wintosystrayicon(ev->window))) {
    /* KLUDGE! sometimes icons occasionally unmap their windows, but do
     * _not_ destroy them. We map those windows back */
    XMapRaised(dpy, c->win);
    updatesystray();
  }
}

void updatebars(void) {
  Monitor *m;
  XSetWindowAttributes wa = {.override_redirect = True,
                             .background_pixel = 0,
                             .border_pixel = 0,
                             .colormap = cmap,
                             .event_mask = ButtonPressMask | ExposureMask};
  XClassHint ch = {"dwm", "dwm"};
  for (m = mons; m; m = m->next) {
    if (m->barwin)
      continue;
    m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, depth,
                              InputOutput, visual,
                              CWOverrideRedirect | CWBackPixel | CWBorderPixel |
                                  CWColormap | CWEventMask,
                              &wa);
    // XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
    if (showsystray && m == systraytomon(m))
      XMapRaised(dpy, systray->win);
    XMapRaised(dpy, m->barwin);
    XSetClassHint(dpy, m->barwin, &ch);
  }
}

void updatebarpos(Monitor *m) {
  m->wy = m->my;
  m->wh = m->mh;
  if (m->showbar) {
    m->wh -= bh;
    m->by = m->topbar ? m->wy : m->wy + m->wh;
    m->wy = m->topbar ? m->wy + bh : m->wy;
  } else
    m->by = -bh;
}

void updateclientlist(void) {
  Client *c;
  Monitor *m;

  XDeleteProperty(dpy, root, netatom[NetClientList]);
  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
                      PropModeAppend, (unsigned char *)&(c->win), 1);
}

int updategeom(void) {
  int dirty = 0;

#ifdef XINERAMA
  if (XineramaIsActive(dpy)) {
    int i, j, n, nn;
    Client *c;
    Monitor *m;
    XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
    XineramaScreenInfo *unique = NULL;

    for (n = 0, m = mons; m; m = m->next, n++)
      ;
    /* only consider unique geometries as separate screens */
    unique = ecalloc(nn, sizeof(XineramaScreenInfo));
    for (i = 0, j = 0; i < nn; i++)
      if (isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nn = j;

    /* new monitors if nn > n */
    for (i = n; i < nn; i++) {
      for (m = mons; m && m->next; m = m->next)
        ;
      if (m)
        m->next = createmon();
      else
        mons = createmon();
    }
    for (i = 0, m = mons; i < nn && m; m = m->next, i++)
      if (i >= n || unique[i].x_org != m->mx || unique[i].y_org != m->my ||
          unique[i].width != m->mw || unique[i].height != m->mh) {
        dirty = 1;
        m->num = i;
        m->mx = m->wx = unique[i].x_org;
        m->my = m->wy = unique[i].y_org;
        m->mw = m->ww = unique[i].width;
        m->mh = m->wh = unique[i].height;
        updatebarpos(m);
      }
    /* removed monitors if n > nn */
    for (i = nn; i < n; i++) {
      for (m = mons; m && m->next; m = m->next)
        ;
      while ((c = m->clients)) {
        dirty = 1;
        m->clients = c->next;
        detachstack(c);
        c->mon = mons;
        attachbottom(c);
        attachstack(c);
      }
      if (m == selmon)
        selmon = mons;
      cleanupmon(m);
    }
    free(unique);
  } else
#endif /* XINERAMA */
  {    /* default monitor setup */
    if (!mons)
      mons = createmon();
    if (mons->mw != sw || mons->mh != sh) {
      dirty = 1;
      mons->mw = mons->ww = sw;
      mons->mh = mons->wh = sh;
      updatebarpos(mons);
    }
  }
  if (dirty) {
    selmon = mons;
    selmon = wintomon(root);
  }
  return dirty;
}

void updatenumlockmask(void) {
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(dpy);
  for (i = 0; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
          XKeysymToKeycode(dpy, XK_Num_Lock))
        numlockmask = (1 << i);
  XFreeModifiermap(modmap);
}

void updatesizehints(Client *c) {
  long msize;
  XSizeHints size;

  if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = PSize;
  if (size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  } else if (size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  } else
    c->basew = c->baseh = 0;
  if (size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  } else
    c->incw = c->inch = 0;
  if (size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  } else
    c->maxw = c->maxh = 0;
  if (size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  } else if (size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  } else
    c->minw = c->minh = 0;
  if (size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    c->maxa = c->mina = 0.0;
  c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
  c->hintsvalid = 1;
}

void updatestatus(void) {
  drawbar(selmon);
  updatesystray();
}

void updatesystrayicongeom(Client *i, int w, int h) {
  const int tmp = bh - systrayiconsizeredunce;
  if (i) {
    i->h = tmp;
    if (w == h)
      i->w = tmp;
    else if (h == tmp)
      i->w = w;
    else
      i->w = (int)((float)tmp * ((float)w / (float)h));
    applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
    /* force icons into the systray dimensions if they don't want to */
    if (i->h > tmp) {
      if (i->w == i->h)
        i->w = tmp;
      else
        i->w = (int)((float)tmp * ((float)i->w / (float)i->h));
      i->h = tmp;
    }
  }
}

void updatesystrayiconstate(Client *i, XPropertyEvent *ev) {
  long flags;
  int code = 0;

  if (!showsystray || !i || ev->atom != xatom[XembedInfo] ||
      !(flags = getatomprop(i, xatom[XembedInfo])))
    return;

  if (flags & XEMBED_MAPPED && !i->tags) {
    i->tags = 1;
    code = XEMBED_WINDOW_ACTIVATE;
    XMapRaised(dpy, i->win);
    setclientstate(i, NormalState);
  } else if (!(flags & XEMBED_MAPPED) && i->tags) {
    i->tags = 0;
    code = XEMBED_WINDOW_DEACTIVATE;
    XUnmapWindow(dpy, i->win);
    setclientstate(i, WithdrawnState);
  } else
    return;
  sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
            systray->win, XEMBED_EMBEDDED_VERSION);
}

void updatesystray(void) {
  XSetWindowAttributes wa;
  XWindowChanges wc;
  Client *i;
  Monitor *m = systraytomon(NULL);
  unsigned int x = m->mx + m->mw;
  unsigned int w = 1;

  if (!showsystray)
    return;
  if (!systray) {
    /* init systray */
    if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
      die("fatal: could not malloc() %u bytes\n", sizeof(Systray));
    systray->win = XCreateSimpleWindow(dpy, root, x, m->by, w, bh, 0, 0,
                                       scheme[SchemeSel][ColBg].pixel);
    wa.event_mask = ButtonPressMask | ExposureMask;
    wa.override_redirect = True;
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    XSelectInput(dpy, systray->win, SubstructureNotifyMask);
    XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&netatom[NetSystemTrayOrientationHorz], 1);
    XChangeWindowAttributes(
        dpy, systray->win, CWEventMask | CWOverrideRedirect | CWBackPixel, &wa);
    XMapRaised(dpy, systray->win);
    XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
    if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
      sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime,
                netatom[NetSystemTray], systray->win, 0, 0);
      XSync(dpy, False);
    } else {
      fprintf(stderr, "dwm: unable to obtain system tray.\n");
      free(systray);
      systray = NULL;
      return;
    }
  }
  for (w = 0, i = systray->icons; i; i = i->next) {
    /* make sure the background color stays the same */
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
    XMapRaised(dpy, i->win);
    w += systrayspacing;
    i->x = w;
    XMoveResizeWindow(dpy, i->win, i->x, systrayicony, i->w, i->h);
    w += i->w;
    if (i->mon != m)
      i->mon = m;
  }
  w = w ? w + systrayspacing : 1;
  x -= w;
  XMoveResizeWindow(dpy, systray->win, x, m->by, w, bh);
  wc.x = x;
  wc.y = m->by;
  wc.width = w;
  wc.height = bh;
  wc.stack_mode = Above;
  wc.sibling = m->barwin;
  XConfigureWindow(dpy, systray->win,
                   CWX | CWY | CWWidth | CWHeight | CWSibling | CWStackMode,
                   &wc);
  XMapWindow(dpy, systray->win);
  XMapSubwindows(dpy, systray->win);
  /* redraw background */
  XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBg].pixel);
  XFillRectangle(dpy, systray->win, drw->gc, 0, 0, w, bh);
  XSync(dpy, False);
}

void updatetitle(Client *c) {
  if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  if (c->name[0] == '\0') /* hack to mark broken clients */
    strcpy(c->name, broken);
}

void updatewindowtype(Client *c) {
  Atom state = getatomprop(c, netatom[NetWMState]);
  Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

  if (state == netatom[NetWMFullscreen])
    setfullscreen(c, 1);
  if (wtype == netatom[NetWMWindowTypeDialog])
    c->isfloating = 1;
}

void updatewmhints(Client *c) {
  XWMHints *wmh;

  if ((wmh = XGetWMHints(dpy, c->win))) {
    if (c == selmon->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(dpy, c->win, wmh);
    } else
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    if (wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = 0;
    XFree(wmh);
  }
}

void view(const Arg *arg) {
  if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
    return;
  selmon->seltags ^= 1; /* toggle sel tagset */
  if (arg->ui & TAGMASK)
    selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
  focus(NULL);
  arrangeClients(selmon);
  arrange(selmon);
}

void switchtoclient(const Arg *arg) {
  Client *c;
  XWindowAttributes wa;

  if (!selmon->sel)
    return;

  for (c = selmon->sel->snext; c; c = c->snext) {
    if (ISVISIBLE(c) && !HIDDEN(c) && XGetWindowAttributes(dpy, c->win, &wa) &&
        wa.x < 0) {
      focus(c);
      if (!c->isAtEdge) {
        c->mon->isLeftEdgeLean = !c->isLeftEdgeLean;
        c->isLeftEdgeLean = !c->isLeftEdgeLean;
        c->isAtEdge = 1;
      }
      arrange(selmon);
      return;
    }
  }
}

void nview(const Arg *arg) {
  unsigned int tmp = 1 << (LENGTH(tags) - 1);
  if (selmon->tagset[selmon->seltags] == tmp)
    return;
  else
    selmon->tagset[selmon->seltags] = selmon->tagset[selmon->seltags] << 1;
  focus(NULL);
  arrange(selmon);
}

void pview(const Arg *arg) {
  unsigned int tmp = 1;
  if (selmon->tagset[selmon->seltags] == tmp)
    return;
  else
    selmon->tagset[selmon->seltags] = selmon->tagset[selmon->seltags] >> 1;
  focus(NULL);
  arrange(selmon);
}

Client *wintoclient(Window w) {
  Client *c;
  Monitor *m;

  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      if (c->win == w)
        return c;
  return NULL;
}

Client *wintosystrayicon(Window w) {
  Client *i = NULL;

  if (!showsystray || !w)
    return i;
  for (i = systray->icons; i && i->win != w; i = i->next)
    ;
  return i;
}

Monitor *wintomon(Window w) {
  int x, y;
  Client *c;
  Monitor *m;

  if (w == root && getrootptr(&x, &y))
    return recttomon(x, y, 1, 1);
  for (m = mons; m; m = m->next)
    if (w == m->barwin)
      return m;
  if ((c = wintoclient(w)))
    return c->mon;
  return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display *dpy, XErrorEvent *ee) {
  if (ee->error_code == BadWindow ||
      (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch) ||
      (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolyFillRectangle &&
       ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolyFillRectangle && ee->error_code == BadMatch) ||
      (ee->request_code == X_PolySegment && ee->error_code == BadDrawable) ||
      (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch) ||
      (ee->request_code == X_GrabButton && ee->error_code == BadAccess) ||
      (ee->request_code == X_GrabKey && ee->error_code == BadAccess) ||
      (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
          ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee); /* may call exit */
}

int xerrordummy(Display *dpy, XErrorEvent *ee) { return 0; }

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display *dpy, XErrorEvent *ee) {
  die("dwm: another window manager is already running");
  return -1;
}

Monitor *systraytomon(Monitor *m) {
  Monitor *t;
  int i, n;
  if (!systraypinning) {
    if (!m)
      return selmon;
    return m == selmon ? m : NULL;
  }
  for (n = 1, t = mons; t && t->next; n++, t = t->next)
    ;
  for (i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next)
    ;
  if (systraypinningfailfirst && n < systraypinning)
    return mons;
  return t;
}

void xinitvisual() {
  XVisualInfo *infos;
  XRenderPictFormat *fmt;
  int nitems;
  int i;

  XVisualInfo tpl = {.screen = screen, .depth = 32, .class = TrueColor};
  long masks = VisualScreenMask | VisualDepthMask | VisualClassMask;

  infos = XGetVisualInfo(dpy, masks, &tpl, &nitems);
  visual = NULL;
  for (i = 0; i < nitems; i++) {
    fmt = XRenderFindVisualFormat(dpy, infos[i].visual);
    if (fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
      visual = infos[i].visual;
      depth = infos[i].depth;
      cmap = XCreateColormap(dpy, root, visual, AllocNone);
      useargb = 1;
      break;
    }
  }

  XFree(infos);

  if (!visual) {
    visual = DefaultVisual(dpy, screen);
    depth = DefaultDepth(dpy, screen);
    cmap = DefaultColormap(dpy, screen);
  }
}

void zoom(const Arg *arg) {
  Client *c = selmon->sel;

  if (!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating)
    return;
  if (c == nexttiled(selmon->clients) && !(c = nexttiled(c->next)))
    return;
  pop(c);
}

void moveclient(const Arg *arg) {
  int direction = arg->i;
  Client *iter, *original_target = selmon->sel;
  Client *target = original_target;
  Client *nodes[3] = {NULL, NULL, NULL};

  if (!selmon->lt[selmon->sellt]->arrange || !target || target->isfloating)
    return;

  if (!direction) {
    target = target->next;
    if (!target)
      return;
  }

  for (iter = selmon->clients; iter; iter = iter->next) {
    nodes[2] = nodes[1];
    nodes[1] = nodes[0];
    nodes[0] = iter;
    if (iter == target)
      break;
  }

  if (!nodes[0] || !nodes[1] || !ISVISIBLE(nodes[0]) || !ISVISIBLE(nodes[1]))
    return;

  if (nodes[2]) {
    nodes[2]->next = nodes[0];
    if (ISVISIBLE(nodes[2]))
      selmon->visible = nodes[0];
  } else {
    selmon->clients = nodes[0];
    selmon->visible = nodes[0];
  }
  nodes[1]->next = nodes[0]->next;
  nodes[0]->next = nodes[1];

  arrange(original_target->mon);
}

void killorzoom(const Arg *arg) {
  Client *c = (Client *)arg->v;
  if (c == nextvisible(selmon->clients))
    killclient(&(Arg){0});
  else
    zoom(arg);
}

void previewindexwin() {
  Monitor *m = selmon;
  Client *c, *focus_c = NULL, *current_c = NULL;
  unsigned int n;

  // Array to store all clients for keyboard navigation
  Client **clients_array = NULL;
  int selected_index = -1;

  // First pass: count visible windows
  for (n = 0, c = nextpreview(m->clients); c; c = nextpreview(c->next), n++) {
    /* If you hit actualfullscreen patch Unlock the notes below */
    if (c->isfullscreen)
      togglefullscr(&(Arg){0});
    /* If you hit awesomebar patch Unlock the notes below */
    if (HIDDEN(c)) {
      // For hidden windows, use the stored image if available
      if (!c->pre.orig_image) {
        // If no stored image, create a placeholder
        c->pre.orig_image = create_placeholder_image(c->w > 0 ? c->w : 200,
                                                     c->h > 0 ? c->h : 150);
      }
    } else {
      // For visible windows, capture the current image
      // Clean up existing image first to prevent memory leak
      if (c->pre.orig_image) {
        XDestroyImage(c->pre.orig_image);
        c->pre.orig_image = NULL;
      }
      c->pre.orig_image = getwindowximage_safe(c);
      // getwindowximage_safe now always returns an image (real or placeholder)
    }

    // Record current selected window
    if (c == selmon->sel)
      current_c = c;
  }

  if (n == 0)
    return;

  // Allocate clients array
  clients_array = ecalloc(n, sizeof(Client *));

  // Second pass: fill array
  int i = 0;
  for (c = nextpreview(m->clients); c; c = nextpreview(c->next)) {
    // Now all clients should have an image (real or placeholder)
    clients_array[i] = c;
    if (c == current_c) {
      selected_index = i; // Default to current window
    }
    i++;
  }

  // If current window not found, default to first
  if (selected_index == -1 && n > 0)
    selected_index = 0;

  arrangeIndexPreviews(n, m, 60, 15);

  // Create preview windows
  for (i = 0; i < n; i++) {
    c = clients_array[i];
    if (!c->pre.win)
      c->pre.win = XCreateSimpleWindow(
          dpy, root, c->pre.x, c->pre.y, c->pre.scaled_image->width,
          c->pre.scaled_image->height, 1, BlackPixel(dpy, screen),
          WhitePixel(dpy, screen));
    else
      XMoveResizeWindow(dpy, c->pre.win, c->pre.x, c->pre.y,
                        c->pre.scaled_image->width,
                        c->pre.scaled_image->height);

    // Set border color, selected window uses selected color
    if (i == selected_index)
      XSetWindowBorder(dpy, c->pre.win, scheme[SchemeSel][ColBorder].pixel);
    else
      XSetWindowBorder(dpy, c->pre.win, scheme[SchemeNorm][ColBorder].pixel);

    XSetWindowBorderWidth(dpy, c->pre.win, borderpx);
    XUnmapWindow(dpy, c->win);

    if (c->pre.win) {
      XSelectInput(dpy, c->pre.win,
                   ButtonPress | EnterWindowMask | LeaveWindowMask |
                       KeyPressMask);
      XMapWindow(dpy, c->pre.win);
      GC gc = XCreateGC(dpy, c->pre.win, 0, NULL);
      XPutImage(dpy, c->pre.win, gc, c->pre.scaled_image, 0, 0, 0, 0,
                c->pre.scaled_image->width, c->pre.scaled_image->height);
      XFreeGC(dpy, gc);
    }
  }

  // Grab keyboard for input
  XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);

  // Move cursor to selected window
  if (selected_index >= 0 && selected_index < n) {
    Client *sel_c = clients_array[selected_index];
    XWarpPointer(dpy, None, sel_c->pre.win, 0, 0, 0, 0,
                 sel_c->pre.scaled_image->width / 2,
                 sel_c->pre.scaled_image->height / 2);
  }

  XEvent event;
  KeySym keysym;
  int prev_selected = selected_index;

  while (1) {
    XNextEvent(dpy, &event);

    if (event.type == KeyPress) {
      keysym = XKeycodeToKeysym(dpy, event.xkey.keycode, 0);
      prev_selected = selected_index;

      // Position-based navigation logic
      if (keysym == XK_h || keysym == XK_Left) {
        // Move left: find nearest window to the left
        int best_index = -1;
        int min_distance = INT_MAX;
        Client *current = clients_array[selected_index];
        int current_center_x =
            current->pre.x + current->pre.scaled_image->width / 2;
        int current_center_y =
            current->pre.y + current->pre.scaled_image->height / 2;

        for (i = 0; i < n; i++) {
          if (i == selected_index)
            continue;
          Client *candidate = clients_array[i];
          int candidate_center_x =
              candidate->pre.x + candidate->pre.scaled_image->width / 2;
          int candidate_center_y =
              candidate->pre.y + candidate->pre.scaled_image->height / 2;

          // Only consider windows to the left
          if (candidate_center_x < current_center_x) {
            int dx = current_center_x - candidate_center_x;
            int dy = abs(current_center_y - candidate_center_y);
            int distance = dx + dy * 2; // Weight vertical distance more

            if (distance < min_distance) {
              min_distance = distance;
              best_index = i;
            }
          }
        }

        if (best_index != -1) {
          selected_index = best_index;
        }
      } else if (keysym == XK_l || keysym == XK_Right) {
        // Move right: find nearest window to the right
        int best_index = -1;
        int min_distance = INT_MAX;
        Client *current = clients_array[selected_index];
        int current_center_x =
            current->pre.x + current->pre.scaled_image->width / 2;
        int current_center_y =
            current->pre.y + current->pre.scaled_image->height / 2;

        for (i = 0; i < n; i++) {
          if (i == selected_index)
            continue;
          Client *candidate = clients_array[i];
          int candidate_center_x =
              candidate->pre.x + candidate->pre.scaled_image->width / 2;
          int candidate_center_y =
              candidate->pre.y + candidate->pre.scaled_image->height / 2;

          // Only consider windows to the right
          if (candidate_center_x > current_center_x) {
            int dx = candidate_center_x - current_center_x;
            int dy = abs(current_center_y - candidate_center_y);
            int distance = dx + dy * 2; // Weight vertical distance more

            if (distance < min_distance) {
              min_distance = distance;
              best_index = i;
            }
          }
        }

        if (best_index != -1) {
          selected_index = best_index;
        }
      } else if (keysym == XK_k || keysym == XK_Up) {
        // Move up: find nearest window above
        int best_index = -1;
        int min_distance = INT_MAX;
        Client *current = clients_array[selected_index];
        int current_center_x =
            current->pre.x + current->pre.scaled_image->width / 2;
        int current_center_y =
            current->pre.y + current->pre.scaled_image->height / 2;

        for (i = 0; i < n; i++) {
          if (i == selected_index)
            continue;
          Client *candidate = clients_array[i];
          int candidate_center_x =
              candidate->pre.x + candidate->pre.scaled_image->width / 2;
          int candidate_center_y =
              candidate->pre.y + candidate->pre.scaled_image->height / 2;

          // Only consider windows above
          if (candidate_center_y < current_center_y) {
            int dx = abs(current_center_x - candidate_center_x);
            int dy = current_center_y - candidate_center_y;
            int distance = dy + dx * 2; // Weight horizontal distance more

            if (distance < min_distance) {
              min_distance = distance;
              best_index = i;
            }
          }
        }

        if (best_index != -1) {
          selected_index = best_index;
        }
      } else if (keysym == XK_j || keysym == XK_Down) {
        // Move down: find nearest window below
        int best_index = -1;
        int min_distance = INT_MAX;
        Client *current = clients_array[selected_index];
        int current_center_x =
            current->pre.x + current->pre.scaled_image->width / 2;
        int current_center_y =
            current->pre.y + current->pre.scaled_image->height / 2;

        for (i = 0; i < n; i++) {
          if (i == selected_index)
            continue;
          Client *candidate = clients_array[i];
          int candidate_center_x =
              candidate->pre.x + candidate->pre.scaled_image->width / 2;
          int candidate_center_y =
              candidate->pre.y + candidate->pre.scaled_image->height / 2;

          // Only consider windows below
          if (candidate_center_y > current_center_y) {
            int dx = abs(current_center_x - candidate_center_x);
            int dy = candidate_center_y - current_center_y;
            int distance = dy + dx * 2; // Weight horizontal distance more

            if (distance < min_distance) {
              min_distance = distance;
              best_index = i;
            }
          }
        }

        if (best_index != -1) {
          selected_index = best_index;
        }
      } else if (keysym == XK_Return || keysym == XK_space) {
        // Confirm selection
        focus_c = clients_array[selected_index];
        break;
      } else if (keysym == XK_Escape) {
        // Cancel and return
        focus_c = current_c;
        break;
      }

      // Update border colors
      if (prev_selected != selected_index) {
        if (prev_selected >= 0 && prev_selected < n)
          XSetWindowBorder(dpy, clients_array[prev_selected]->pre.win,
                           scheme[SchemeNorm][ColBorder].pixel);

        XSetWindowBorder(dpy, clients_array[selected_index]->pre.win,
                         scheme[SchemeSel][ColBorder].pixel);
      }
    } else if (event.type == ButtonPress && event.xbutton.button == Button1) {
      // Mouse click selection
      for (i = 0; i < n; i++) {
        if (event.xbutton.window == clients_array[i]->pre.win) {
          focus_c = clients_array[i];
          break;
        }
      }
      if (focus_c)
        break;
    } else if (event.type == EnterNotify) {
      // Mouse hover highlight
      for (i = 0; i < n; i++) {
        if (event.xcrossing.window == clients_array[i]->pre.win &&
            i != selected_index) {
          XSetWindowBorder(dpy, clients_array[i]->pre.win,
                           scheme[SchemeSel][ColBorder].pixel);
          break;
        }
      }
    } else if (event.type == LeaveNotify) {
      // Mouse leave restore color
      for (i = 0; i < n; i++) {
        if (event.xcrossing.window == clients_array[i]->pre.win &&
            i != selected_index) {
          XSetWindowBorder(dpy, clients_array[i]->pre.win,
                           scheme[SchemeNorm][ColBorder].pixel);
          break;
        }
      }
    }
  }

  // Release keyboard
  XUngrabKeyboard(dpy, CurrentTime);

  // Cleanup preview windows
  for (i = 0; i < n; i++) {
    c = clients_array[i];
    XUnmapWindow(dpy, c->pre.win);
    if (!HIDDEN(c))
      XMapWindow(dpy, c->win);
    // Don't destroy orig_image for hidden windows as it's stored for reuse
    if (!HIDDEN(c)) {
      XDestroyImage(c->pre.orig_image);
      c->pre.orig_image = NULL;
    }
    XDestroyImage(c->pre.scaled_image);
    c->pre.scaled_image = NULL;
  }

  // Free clients array
  free(clients_array);

  // Switch to selected window if any
  if (focus_c) {
    // Check if target tags are different from current tags (like view function)
    if (focus_c->tags != selmon->tagset[selmon->seltags]) {
      selmon->seltags ^= 1; /* toggle sel tagset */
      selmon->tagset[selmon->seltags] = focus_c->tags;
    }
    focus(NULL);

    if (HIDDEN(focus_c))
      showwin(focus_c);
  }

  focus(focus_c);
  if (focus_c && !focus_c->isAtEdge) {
    focus_c->mon->isLeftEdgeLean = !focus_c->isLeftEdgeLean;
    focus_c->isLeftEdgeLean = !focus_c->isLeftEdgeLean;
    focus_c->isAtEdge = 1;
  }
  arrange(m);
}

void previewallwin() {
  Monitor *m = selmon;
  Client *c, *focus_c = NULL, *current_c = NULL;
  unsigned int n;

  // Array to store all clients for keyboard navigation
  Client **clients_array = NULL;
  int selected_index = -1;

  // First pass: count visible windows
  for (n = 0, c = m->clients; c; c = c->next) {
    if (c->isfullscreen)
      togglefullscr(&(Arg){0});
    if (HIDDEN(c)) {
      // For hidden windows, use the stored image if available
      if (!c->pre.orig_image) {
        // If no stored image, create a placeholder
        c->pre.orig_image = create_placeholder_image(c->w > 0 ? c->w : 200,
                                                     c->h > 0 ? c->h : 150);
      }
    } else {
      // For visible windows, capture the current image
      // Clean up existing image first to prevent memory leak
      if (c->pre.orig_image) {
        XDestroyImage(c->pre.orig_image);
        c->pre.orig_image = NULL;
      }
      c->pre.orig_image = getwindowximage_safe(c);
      // getwindowximage_safe now always returns an image (real or placeholder)
    }
    n++;

    // Record current selected window
    if (c == selmon->sel)
      current_c = c;
  }

  if (n == 0)
    return;

  // Allocate clients array
  clients_array = ecalloc(n, sizeof(Client *));

  // Second pass: fill array
  int i = 0;
  for (c = m->clients; c; c = c->next) {
    if (c->isfullscreen)
      continue;
    // Now all clients should have an image (real or placeholder)
    clients_array[i] = c;
    if (c == current_c) {
      selected_index = i; // Default to current window
    }
    i++;
  }

  // If current window not found, default to first
  if (selected_index == -1 && n > 0)
    selected_index = 0;

  arrangePreviews(n, m, 60, 15);

  // Create preview windows
  for (i = 0; i < n; i++) {
    c = clients_array[i];
    if (!c->pre.win)
      c->pre.win = XCreateSimpleWindow(
          dpy, root, c->pre.x, c->pre.y, c->pre.scaled_image->width,
          c->pre.scaled_image->height, 1, BlackPixel(dpy, screen),
          WhitePixel(dpy, screen));
    else
      XMoveResizeWindow(dpy, c->pre.win, c->pre.x, c->pre.y,
                        c->pre.scaled_image->width,
                        c->pre.scaled_image->height);

    // Set border color, selected window uses selected color
    if (i == selected_index)
      XSetWindowBorder(dpy, c->pre.win, scheme[SchemeSel][ColBorder].pixel);
    else
      XSetWindowBorder(dpy, c->pre.win, scheme[SchemeNorm][ColBorder].pixel);

    XSetWindowBorderWidth(dpy, c->pre.win, borderpx);
    XUnmapWindow(dpy, c->win);

    if (c->pre.win) {
      XSelectInput(dpy, c->pre.win,
                   ButtonPress | EnterWindowMask | LeaveWindowMask |
                       KeyPressMask);
      XMapWindow(dpy, c->pre.win);
      GC gc = XCreateGC(dpy, c->pre.win, 0, NULL);
      XPutImage(dpy, c->pre.win, gc, c->pre.scaled_image, 0, 0, 0, 0,
                c->pre.scaled_image->width, c->pre.scaled_image->height);
      XFreeGC(dpy, gc);
    }
  }

  // Grab keyboard for input
  XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);

  // Move cursor to selected window
  if (selected_index >= 0 && selected_index < n) {
    Client *sel_c = clients_array[selected_index];
    XWarpPointer(dpy, None, sel_c->pre.win, 0, 0, 0, 0,
                 sel_c->pre.scaled_image->width / 2,
                 sel_c->pre.scaled_image->height / 2);
  }

  XEvent event;
  KeySym keysym;
  int prev_selected = selected_index;

  while (1) {
    XNextEvent(dpy, &event);

    if (event.type == KeyPress) {
      keysym = XKeycodeToKeysym(dpy, event.xkey.keycode, 0);
      prev_selected = selected_index;

      // Position-based navigation logic
      if (keysym == XK_h || keysym == XK_Left) {
        // Move left: find nearest window to the left
        int best_index = -1;
        int min_distance = INT_MAX;
        Client *current = clients_array[selected_index];
        int current_center_x =
            current->pre.x + current->pre.scaled_image->width / 2;
        int current_center_y =
            current->pre.y + current->pre.scaled_image->height / 2;

        for (i = 0; i < n; i++) {
          if (i == selected_index)
            continue;
          Client *candidate = clients_array[i];
          int candidate_center_x =
              candidate->pre.x + candidate->pre.scaled_image->width / 2;
          int candidate_center_y =
              candidate->pre.y + candidate->pre.scaled_image->height / 2;

          // Only consider windows to the left
          if (candidate_center_x < current_center_x) {
            int dx = current_center_x - candidate_center_x;
            int dy = abs(current_center_y - candidate_center_y);
            int distance = dx + dy * 2; // Weight vertical distance more

            if (distance < min_distance) {
              min_distance = distance;
              best_index = i;
            }
          }
        }

        if (best_index != -1) {
          selected_index = best_index;
        }
      } else if (keysym == XK_l || keysym == XK_Right) {
        // Move right: find nearest window to the right
        int best_index = -1;
        int min_distance = INT_MAX;
        Client *current = clients_array[selected_index];
        int current_center_x =
            current->pre.x + current->pre.scaled_image->width / 2;
        int current_center_y =
            current->pre.y + current->pre.scaled_image->height / 2;

        for (i = 0; i < n; i++) {
          if (i == selected_index)
            continue;
          Client *candidate = clients_array[i];
          int candidate_center_x =
              candidate->pre.x + candidate->pre.scaled_image->width / 2;
          int candidate_center_y =
              candidate->pre.y + candidate->pre.scaled_image->height / 2;

          // Only consider windows to the right
          if (candidate_center_x > current_center_x) {
            int dx = candidate_center_x - current_center_x;
            int dy = abs(current_center_y - candidate_center_y);
            int distance = dx + dy * 2; // Weight vertical distance more

            if (distance < min_distance) {
              min_distance = distance;
              best_index = i;
            }
          }
        }

        if (best_index != -1) {
          selected_index = best_index;
        }
      } else if (keysym == XK_k || keysym == XK_Up) {
        // Move up: find nearest window above
        int best_index = -1;
        int min_distance = INT_MAX;
        Client *current = clients_array[selected_index];
        int current_center_x =
            current->pre.x + current->pre.scaled_image->width / 2;
        int current_center_y =
            current->pre.y + current->pre.scaled_image->height / 2;

        for (i = 0; i < n; i++) {
          if (i == selected_index)
            continue;
          Client *candidate = clients_array[i];
          int candidate_center_x =
              candidate->pre.x + candidate->pre.scaled_image->width / 2;
          int candidate_center_y =
              candidate->pre.y + candidate->pre.scaled_image->height / 2;

          // Only consider windows above
          if (candidate_center_y < current_center_y) {
            int dx = abs(current_center_x - candidate_center_x);
            int dy = current_center_y - candidate_center_y;
            int distance = dy + dx * 2; // Weight horizontal distance more

            if (distance < min_distance) {
              min_distance = distance;
              best_index = i;
            }
          }
        }

        if (best_index != -1) {
          selected_index = best_index;
        }
      } else if (keysym == XK_j || keysym == XK_Down) {
        // Move down: find nearest window below
        int best_index = -1;
        int min_distance = INT_MAX;
        Client *current = clients_array[selected_index];
        int current_center_x =
            current->pre.x + current->pre.scaled_image->width / 2;
        int current_center_y =
            current->pre.y + current->pre.scaled_image->height / 2;

        for (i = 0; i < n; i++) {
          if (i == selected_index)
            continue;
          Client *candidate = clients_array[i];
          int candidate_center_x =
              candidate->pre.x + candidate->pre.scaled_image->width / 2;
          int candidate_center_y =
              candidate->pre.y + candidate->pre.scaled_image->height / 2;

          // Only consider windows below
          if (candidate_center_y > current_center_y) {
            int dx = abs(current_center_x - candidate_center_x);
            int dy = candidate_center_y - current_center_y;
            int distance = dy + dx * 2; // Weight horizontal distance more

            if (distance < min_distance) {
              min_distance = distance;
              best_index = i;
            }
          }
        }

        if (best_index != -1) {
          selected_index = best_index;
        }
      } else if (keysym == XK_Return || keysym == XK_space) {
        // Confirm selection
        focus_c = clients_array[selected_index];
        break;
      } else if (keysym == XK_Escape) {
        // Cancel and return
        focus_c = current_c;
        break;
      }

      // Update border colors
      if (prev_selected != selected_index) {
        if (prev_selected >= 0 && prev_selected < n)
          XSetWindowBorder(dpy, clients_array[prev_selected]->pre.win,
                           scheme[SchemeNorm][ColBorder].pixel);

        XSetWindowBorder(dpy, clients_array[selected_index]->pre.win,
                         scheme[SchemeSel][ColBorder].pixel);
      }
    } else if (event.type == ButtonPress && event.xbutton.button == Button1) {
      // Mouse click selection
      for (i = 0; i < n; i++) {
        if (event.xbutton.window == clients_array[i]->pre.win) {
          focus_c = clients_array[i];
          break;
        }
      }
      if (focus_c)
        break;
    } else if (event.type == EnterNotify) {
      // Mouse hover highlight
      for (i = 0; i < n; i++) {
        if (event.xcrossing.window == clients_array[i]->pre.win &&
            i != selected_index) {
          XSetWindowBorder(dpy, clients_array[i]->pre.win,
                           scheme[SchemeSel][ColBorder].pixel);
          break;
        }
      }
    } else if (event.type == LeaveNotify) {
      // Mouse leave restore color
      for (i = 0; i < n; i++) {
        if (event.xcrossing.window == clients_array[i]->pre.win &&
            i != selected_index) {
          XSetWindowBorder(dpy, clients_array[i]->pre.win,
                           scheme[SchemeNorm][ColBorder].pixel);
          break;
        }
      }
    }
  }

  // Release keyboard
  XUngrabKeyboard(dpy, CurrentTime);

  // Cleanup preview windows
  for (i = 0; i < n; i++) {
    c = clients_array[i];
    XUnmapWindow(dpy, c->pre.win);
    if (!HIDDEN(c))
      XMapWindow(dpy, c->win);
    // Don't destroy orig_image for hidden windows as it's stored for reuse
    if (!HIDDEN(c)) {
      XDestroyImage(c->pre.orig_image);
      c->pre.orig_image = NULL;
    }
    XDestroyImage(c->pre.scaled_image);
    c->pre.scaled_image = NULL;
  }

  // Free clients array
  free(clients_array);

  // Switch to selected window if any
  if (focus_c) {
    // Check if target tags are different from current tags (like view function)
    if (focus_c->tags != selmon->tagset[selmon->seltags]) {
      selmon->seltags ^= 1; /* toggle sel tagset */
      selmon->tagset[selmon->seltags] = focus_c->tags;
    }
    focus(NULL);

    if (HIDDEN(focus_c))
      showwin(focus_c);
  }

  focus(focus_c);
  arrangeClients(m);
  if (focus_c && !focus_c->isAtEdge) {
    focus_c->mon->isLeftEdgeLean = !focus_c->isLeftEdgeLean;
    focus_c->isLeftEdgeLean = !focus_c->isLeftEdgeLean;
    focus_c->isAtEdge = 1;
  }
  arrange(m);
}

void arrangeIndexPreviews(unsigned int n, Monitor *m, unsigned int gappo,
                          unsigned int gappi) {
  unsigned int i, j;
  unsigned int cx, cy, cw, ch, cmaxh;
  unsigned int cols, rows;
  Client *c, *tmpc;

  if (n == 1) {
    c = nextpreview(m->clients);
    unsigned int cw = (m->ww - 2 * gappo) * 8 / 10;
    unsigned int ch = (m->wh - 2 * gappo) * 9 / 10;
    // Clean up existing scaled image before creating new one
    if (c->pre.scaled_image) {
      XDestroyImage(c->pre.scaled_image);
      c->pre.scaled_image = NULL;
    }
    c->pre.scaled_image = scaledownimage(c->pre.orig_image, cw, ch);
    c->pre.x = m->mx + (m->mw - c->pre.scaled_image->width) / 2;
    c->pre.y = m->my + (m->mh - c->pre.scaled_image->height) / 2;
    return;
  }

  if (n <= 4) {
    unsigned int total_gapi = gappi * (n - 1);
    cw = (m->ww - 2 * gappo - total_gapi) / n;
    ch = (m->wh - 2 * gappo) * 7 / 10;

    unsigned int total_width = 0;
    for (c = nextpreview(m->clients); c; c = nextpreview(c->next)) {
      // Clean up existing scaled image before creating new one
      if (c->pre.scaled_image) {
        XDestroyImage(c->pre.scaled_image);
        c->pre.scaled_image = NULL;
      }
      c->pre.scaled_image = scaledownimage(c->pre.orig_image, cw, ch);
      total_width += c->pre.scaled_image->width;
    }
    total_width += total_gapi;

    cx = m->mx + (m->mw - total_width) / 2;
    cy = m->my + (m->mh - ch) / 2;
    for (c = nextpreview(m->clients); c; c = nextpreview(c->next)) {
      c->pre.x = cx;
      c->pre.y = cy + (ch - c->pre.scaled_image->height) / 2;
      cx += c->pre.scaled_image->width + gappi;
    }
    return;
  }

  for (cols = 0; cols <= n / 2; cols++)
    if (cols * cols >= n)
      break;
  rows = (cols && (cols - 1) * cols >= n) ? cols - 1 : cols;
  ch = (m->wh - 2 * gappo) / rows;
  cw = (m->ww - 2 * gappo) / cols;
  c = nextpreview(m->clients);
  cy = 0;
  for (i = 0; i < rows; i++) {
    cx = 0;
    cmaxh = 0;
    tmpc = c;
    for (int j = 0; j < cols; j++) {
      if (!c)
        break;
      // Clean up existing scaled image before creating new one
      if (c->pre.scaled_image) {
        XDestroyImage(c->pre.scaled_image);
        c->pre.scaled_image = NULL;
      }
      c->pre.scaled_image = scaledownimage(c->pre.orig_image, cw, ch);
      c->pre.x = cx;
      cmaxh = c->pre.scaled_image->height > cmaxh ? c->pre.scaled_image->height
                                                  : cmaxh;
      cx += c->pre.scaled_image->width + gappi;
      c = nextpreview(c->next);
    }
    c = tmpc;
    cx = m->wx + (m->ww - cx) / 2;
    for (j = 0; j < cols; j++) {
      if (!c)
        break;
      c->pre.x += cx;
      c->pre.y = cy + (cmaxh - c->pre.scaled_image->height) / 2;
      c = nextpreview(c->next);
    }
    cy += cmaxh + gappi;
  }
  cy = m->wy + (m->wh - cy) / 2;
  for (c = nextpreview(m->clients); c; c = nextpreview(c->next))
    c->pre.y += cy;
}

void arrangePreviews(unsigned int n, Monitor *m, unsigned int gappo,
                     unsigned int gappi) {
  unsigned int i, j;
  unsigned int cx, cy, cw, ch, cmaxh;
  unsigned int cols, rows;
  Client *c, *tmpc;

  if (n == 1) {
    c = m->clients;
    unsigned int cw = (m->ww - 2 * gappo) * 8 / 10;
    unsigned int ch = (m->wh - 2 * gappo) * 9 / 10;
    // Clean up existing scaled image before creating new one
    if (c->pre.scaled_image) {
      XDestroyImage(c->pre.scaled_image);
      c->pre.scaled_image = NULL;
    }
    c->pre.scaled_image = scaledownimage(c->pre.orig_image, cw, ch);
    c->pre.x = m->mx + (m->mw - c->pre.scaled_image->width) / 2;
    c->pre.y = m->my + (m->mh - c->pre.scaled_image->height) / 2;
    return;
  }

  if (n <= 4) {
    unsigned int total_gapi = gappi * (n - 1);
    cw = (m->ww - 2 * gappo - total_gapi) / n;
    ch = (m->wh - 2 * gappo) * 7 / 10;

    unsigned int total_width = 0;
    for (c = m->clients; c; c = c->next) {
      // Clean up existing scaled image before creating new one
      if (c->pre.scaled_image) {
        XDestroyImage(c->pre.scaled_image);
        c->pre.scaled_image = NULL;
      }
      c->pre.scaled_image = scaledownimage(c->pre.orig_image, cw, ch);
      total_width += c->pre.scaled_image->width;
    }
    total_width += total_gapi;

    cx = m->mx + (m->mw - total_width) / 2;
    cy = m->my + (m->mh - ch) / 2;
    for (c = m->clients; c; c = c->next) {
      c->pre.x = cx;
      c->pre.y = cy + (ch - c->pre.scaled_image->height) / 2;
      cx += c->pre.scaled_image->width + gappi;
    }
    return;
  }

  for (cols = 0; cols <= n / 2; cols++)
    if (cols * cols >= n)
      break;
  rows = (cols && (cols - 1) * cols >= n) ? cols - 1 : cols;
  ch = (m->wh - 2 * gappo) / rows;
  cw = (m->ww - 2 * gappo) / cols;
  c = m->clients;
  cy = 0;
  for (i = 0; i < rows; i++) {
    cx = 0;
    cmaxh = 0;
    tmpc = c;
    for (int j = 0; j < cols; j++) {
      if (!c)
        break;
      // Clean up existing scaled image before creating new one
      if (c->pre.scaled_image) {
        XDestroyImage(c->pre.scaled_image);
        c->pre.scaled_image = NULL;
      }
      c->pre.scaled_image = scaledownimage(c->pre.orig_image, cw, ch);
      c->pre.x = cx;
      cmaxh = c->pre.scaled_image->height > cmaxh ? c->pre.scaled_image->height
                                                  : cmaxh;
      cx += c->pre.scaled_image->width + gappi;
      c = c->next;
    }
    c = tmpc;
    cx = m->wx + (m->ww - cx) / 2;
    for (j = 0; j < cols; j++) {
      if (!c)
        break;
      c->pre.x += cx;
      c->pre.y = cy + (cmaxh - c->pre.scaled_image->height) / 2;
      c = c->next;
    }
    cy += cmaxh + gappi;
  }
  cy = m->wy + (m->wh - cy) / 2;
  for (c = m->clients; c; c = c->next)
    c->pre.y += cy;
}

XImage *getwindowximage(Client *c) {
  XWindowAttributes attr;
  XGetWindowAttributes(dpy, c->win, &attr);
  XRenderPictFormat *format = XRenderFindVisualFormat(dpy, attr.visual);
  int hasAlpha = (format->type == PictTypeDirect && format->direct.alphaMask);
  XRenderPictureAttributes pa;
  pa.subwindow_mode = IncludeInferiors;
  Picture picture =
      XRenderCreatePicture(dpy, c->win, format, CPSubwindowMode, &pa);
  Pixmap pixmap = XCreatePixmap(dpy, root, c->w, c->h, 32);
  XRenderPictureAttributes pa2;
  XRenderPictFormat *format2 =
      XRenderFindStandardFormat(dpy, PictStandardARGB32);
  Picture pixmapPicture = XRenderCreatePicture(dpy, pixmap, format2, 0, &pa2);
  XRenderColor color;
  color.red = 0x0000;
  color.green = 0x0000;
  color.blue = 0x0000;
  color.alpha = 0x0000;
  XRenderFillRectangle(dpy, PictOpSrc, pixmapPicture, &color, 0, 0, c->w, c->h);
  XRenderComposite(dpy, hasAlpha ? PictOpOver : PictOpSrc, picture, 0,
                   pixmapPicture, 0, 0, 0, 0, 0, 0, c->w, c->h);
  XImage *temp = XGetImage(dpy, pixmap, 0, 0, c->w, c->h, AllPlanes, ZPixmap);
  temp->red_mask = format2->direct.redMask << format2->direct.red;
  temp->green_mask = format2->direct.greenMask << format2->direct.green;
  temp->blue_mask = format2->direct.blueMask << format2->direct.blue;
  temp->depth = DefaultDepth(dpy, screen);

  // Clean up created resources
  XRenderFreePicture(dpy, picture);
  XRenderFreePicture(dpy, pixmapPicture);
  XFreePixmap(dpy, pixmap);

  return temp;
}

XImage *getwindowximage_safe(Client *c) {
  XImage *result = NULL;
  XErrorHandler old_handler;

  // Set error handler to catch possible X errors
  old_handler = XSetErrorHandler(xerrordummy);

  // Check if window still exists
  XWindowAttributes attr;
  if (XGetWindowAttributes(dpy, c->win, &attr)) {
    // Window exists, try to get image
    result = getwindowximage(c);
  }

  // Restore original error handler
  XSetErrorHandler(old_handler);

  // If unable to get image, create placeholder image
  if (!result) {
    result =
        create_placeholder_image(c->w > 0 ? c->w : 200, c->h > 0 ? c->h : 150);
  }

  return result;
}

XImage *create_placeholder_image(unsigned int w, unsigned int h) {
  // Ensure minimum size
  if (w < 200)
    w = 200;
  if (h < 150)
    h = 150;

  XImage *placeholder =
      XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), 24, ZPixmap, 0,
                   NULL, w, h, 32, 0);

  if (!placeholder) {
    return NULL;
  }

  placeholder->data = malloc(placeholder->height * placeholder->bytes_per_line);
  if (!placeholder->data) {
    XDestroyImage(placeholder);
    return NULL;
  }

  // 创建渐变背景 (从深灰到浅灰)
  for (unsigned int y = 0; y < h; y++) {
    for (unsigned int x = 0; x < w; x++) {
      unsigned char gray_value = 60 + (y * 40 / h); // 从60到100的灰度值
      unsigned long pixel = (gray_value << 16) | (gray_value << 8) | gray_value;
      XPutPixel(placeholder, x, y, pixel);
    }
  }

  // 绘制边框
  unsigned long border_color = 0x808080; // 中等灰色
  for (unsigned int x = 0; x < w; x++) {
    XPutPixel(placeholder, x, 0, border_color);     // 上边框
    XPutPixel(placeholder, x, h - 1, border_color); // 下边框
  }
  for (unsigned int y = 0; y < h; y++) {
    XPutPixel(placeholder, 0, y, border_color);     // 左边框
    XPutPixel(placeholder, w - 1, y, border_color); // 右边框
  }

  // 绘制中央的 "X" 标记表示无法获取图像
  unsigned long x_color = 0xFFFFFF; // 白色
  unsigned int center_x = w / 2;
  unsigned int center_y = h / 2;
  unsigned int size = MIN(w, h) / 4; // X的大小

  // 绘制对角线 "X"
  for (int i = -size / 2; i <= size / 2; i++) {
    // 主对角线
    if (center_x + i >= 0 && center_x + i < w && center_y + i >= 0 &&
        center_y + i < h) {
      XPutPixel(placeholder, center_x + i, center_y + i, x_color);
      if (i > -size / 2 && i < size / 2) { // 加粗线条
        XPutPixel(placeholder, center_x + i + 1, center_y + i, x_color);
        XPutPixel(placeholder, center_x + i, center_y + i + 1, x_color);
      }
    }
    // 反对角线
    if (center_x + i >= 0 && center_x + i < w && center_y - i >= 0 &&
        center_y - i < h) {
      XPutPixel(placeholder, center_x + i, center_y - i, x_color);
      if (i > -size / 2 && i < size / 2) { // 加粗线条
        XPutPixel(placeholder, center_x + i + 1, center_y - i, x_color);
        XPutPixel(placeholder, center_x + i, center_y - i - 1, x_color);
      }
    }
  }

  placeholder->depth = DefaultDepth(dpy, screen);
  return placeholder;
}

XImage *scaledownimage(XImage *orig_image, unsigned int cw, unsigned int ch) {
  int factor_w = orig_image->width / cw + 1;
  int factor_h = orig_image->height / ch + 1;
  int scale_factor = factor_w > factor_h ? factor_w : factor_h;
  int scaled_width = orig_image->width / scale_factor;
  int scaled_height = orig_image->height / scale_factor;
  XImage *scaled_image = XCreateImage(
      dpy, DefaultVisual(dpy, DefaultScreen(dpy)), orig_image->depth, ZPixmap,
      0, NULL, scaled_width, scaled_height, 32, 0);
  if (!scaled_image) {
    return NULL;
  }
  scaled_image->data =
      malloc(scaled_image->height * scaled_image->bytes_per_line);
  if (!scaled_image->data) {
    XDestroyImage(scaled_image);
    return NULL;
  }
  for (int y = 0; y < scaled_height; y++) {
    for (int x = 0; x < scaled_width; x++) {
      int orig_x = x * scale_factor;
      int orig_y = y * scale_factor;
      unsigned long pixel = XGetPixel(orig_image, orig_x, orig_y);
      XPutPixel(scaled_image, x, y, pixel);
    }
  }
  scaled_image->depth = orig_image->depth;
  return scaled_image;
}

int main(int argc, char *argv[]) {
  if (argc == 2 && !strcmp("-v", argv[1]))
    die("dwm-" VERSION);
  else if (argc != 1)
    die("usage: dwm [-v]");
  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);
  if (!(dpy = XOpenDisplay(NULL)))
    die("dwm: cannot open display");
  checkotherwm();
  setup();
#ifdef __OpenBSD__
  if (pledge("stdio rpath proc exec", NULL) == -1)
    die("pledge");
#endif /* __OpenBSD__ */
  scan();
  runautostart();
  if (pthread_create(&draw_status_thread, NULL, *drawstatusbar, NULL) != 0)
    die("pthread_create error");
  run();
  cleanup();
  clean_status_pthread();
  XCloseDisplay(dpy);
  return EXIT_SUCCESS;
}
