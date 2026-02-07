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
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/shape.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)

#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)
#define TEXTWSTATUS(X)          (drw_fontset_getwidth(statusdrw, (X)) + lrpad)
#define ISVISIBLE(C)            ((C) && (C)->mon && (C)->mon->scrollindex && (C)->tagindex >= 0 && (C)->tagindex < LENGTH(tags) && (C)->mon->scrollindex == &(C)->mon->scrolls[(C)->tagindex])

#define SYSTEM_TRAY_REQUEST_DOCK    0
/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON         10
#define XEMBED_MAPPED              (1 << 0)
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2
#define VERSION_MAJOR               0
#define VERSION_MINOR               0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

/* status bar macros */
#define NODE_NUM 100

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeFG, SchemeBlue, SchemeGreen, SchemeOrange, SchemeRed, SchemeYellow }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation, NetSystemTrayOrientationHorz,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetWMWindowTypeDesktop, NetClientList, NetLast }; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XRootPmap, XSetRoot, XLast }; /* Xembed/root atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkNullWinTitle,
       ClkWinClass, ClkSuperIcon, ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

/* preview layout mode enum */
enum {
  PREVIEW_SCROLL,
  PREVIEW_GRID,
};

/* status bar blocks enum */
enum {
  Notify,
  Battery,
  Clock,
  Net,
  Mem,
  Cpu,
  Cores,
  Temp,
  More,
};

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
struct Client {
	char name[256];
	float mina, maxa;
	int floatx, floaty;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
	int ignoreunmap;
	float mfact;
	int tagindex;
	Client *next; /* next pointer for scroll layout linked lists */
	Client *snext;
	Monitor *mon;
	Window win;
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

typedef struct Scroll Scroll;

struct Scroll {
	Client *head;
	int x;
	int singlefill; /* 1 表示单窗铺满，0 表示居中缩小 */
};

struct Monitor {
	char ltsymbol[16];
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	unsigned int sellt;
	int showbar;
	int topbar;
	Client *sel;
	Client *stack;
	Monitor *next;
	Window barwin;
	Window container;
	const Layout *lt[2];
	Scroll *scrolls;
	Scroll *scrollindex;
	int prevtag; /* 上次使用的tag索引 */
	int logotitlew; /* logotitle的实际宽度 */
};


typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	int tagindex;
	int isfloating;
	int monitor;
} Rule;

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	int override_redirect; /* -1: any, 0: normal, 1: override-redirect */
	int radius;
} CornerRule;

typedef struct Systray   Systray;
struct Systray {
	Window win;
	Client *icons;
};

/* status bar block struct*/
typedef struct Block Block;
struct Block {
  int bw;
  void *storage;
  int (*draw)(int x, Block *block, unsigned int timer);
  void (*click)(const Arg *arg);
};

typedef struct {
  unsigned long user;
  unsigned long nice;
  unsigned long system;
  unsigned long idle;
} Cpuload;

typedef struct Node Node;
struct Node {
  Node *prev;
  Node *next;
  Cpuload *data;
};

/*status cpu block struct */
typedef struct {
  Cpuload *prev;
  Cpuload *curr;
  Node *pointer;
} CpuBlock;

typedef struct {
  Cpuload *prev;
  Cpuload *curr;
} CoreBlock;

typedef struct {
	int x;
	int y;
	int w;
	int h;
} MonitorArea;

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void detach(Client *c);
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
static void drawbar(Monitor *m);
static void drawbars(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void ensureclientvisible(Client *c, int minw, int minh);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusonclick(const Arg *arg);
static void focusstep(const Arg *arg);
static void focusstepvisible(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static unsigned int getsystraywidth();
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);

static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void mapnotify(XEvent *e);

static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static void reorderbyx(Scroll *s);

static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void removesystrayicon(Client *i);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizebarwin(Monitor *m);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void resizerequest(XEvent *e);
static void restack(Monitor *m);
static void run(void);
static void runautostart(void);
static void scan(void);
static int sendevent(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void scrollmove(const Arg *arg);

static void setup(void);
static void seturgent(Client *c, int urg);
static void setcontainertitle(Monitor *m);
static int clampcornerradius(int r, int w, int h);
static void clearwindowshape(Window win);
static int setwindowrounded(Window win, int w, int h, int r);
static void applyroundedcorners(Window win);
static void setroundedfromattrs(Window win, XWindowAttributes *wa);
static void drawroundedmask(Pixmap mask, GC gc, int x, int y, int w, int h, int r, int val);
static void setupborderwin(void);
static void updateborderwin(void);
static void previewscroll(const Arg *arg);

static XImage *getwindowximage(Client *c);
static XImage *getwindowximage_safe(Client *c);
static XImage *create_placeholder_image(unsigned int w, unsigned int h);
static XImage *scaleimage(XImage *src, unsigned int nw, unsigned int nh);
static void showhide(Monitor *m);
static void spawn(const Arg *arg);
static Monitor *systraytomon(Monitor *m);
static void tag(const Arg *arg);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void ensureselectedvisible(const Arg *arg);
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
static void viewlast(const Arg *arg);
static void setdefaultfont(void);
static void setsmallfont(void);
static void setstatusdefaultfont(void);
static void setstatussmallfont(void);
static void synccontainerbg(Monitor *m);
static void synccontainerbgs(void);
static void scrolltogglesize(const Arg *arg);
static void drawsupericon(Monitor *m, int *x);
static void drawlogotitle(Monitor *m, int *x);
static void drawtags(Monitor *m, int *x);
static void drawlayout(Monitor *m, int *x);
static void drawclienttabs(Monitor *m, int x, int w, int n);
static void togglesupericon(const Arg *arg);

static void scroll(Monitor *m);
static void scrollmoveothers(const Arg *arg);
static void initshape(void);
static void initcompositor(void);
static void redirectmonitor(Monitor *m);


/* status bar functions */
static void clickmore(const Arg *arg);
static void clickmem(const Arg *arg);
static void clicktemp(const Arg *arg);
static void clicknet(const Arg *arg);
static void clicknotify(const Arg *arg);
static void clickcpu(const Arg *arg);
static void clickcores(const Arg *arg);
static int drawstatusclock(int x, Block *block, unsigned int timer);
static int drawnet(int x, Block *block, unsigned int timer);
static int drawbattery(int x, Block *block, unsigned int timer);
static int drawnotify(int x, Block *block, unsigned int timer);
static int drawcpu(int x, Block *block, unsigned int timer);
static int drawcores(int x, Block *block, unsigned int timer);
static int drawtemp(int x, Block *block, unsigned int timer);
static int drawmem(int x, Block *block, unsigned int timer);
static int drawmore(int x, Block *block, unsigned int timer);
static int getstatuswidth(void);
static void spawnclickcmd(const char *const cmd[]);
static void handleStatus1(const Arg *arg);
static void handleStatus2(const Arg *arg);
static void handleStatus3(const Arg *arg);
static void handleStatus4(const Arg *arg);
static void handleStatus5(const Arg *arg);
static int gettempnums(void);
static void initstatusbar(void);
static void *drawstatusbar(void *arg);
static void cleanstatuspthread(void);
static void updatestatuscache(void);
static void freestatuscache(void);
static void handlestatusclick(const Arg *arg, int button);
static void sendnotify(const char *msg, const char *urgency, int timeout);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static Client *wintosystrayicon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);


/* variables */
static Systray *systray = NULL;
static const char broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh;               /* bar height */
static int lrpad;            /* sum of left and right padding for text */
static int supericonw;
static int systandstat; /* right padding for systray */
static int systrayw;
static int supericonflag = 1;
static int modkey_enabled = 1;
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;

/* status bar global variables */
static pthread_t drawstatusthread;
static int status_thread_started = 0;
static pthread_mutex_t statuscache_mutex = PTHREAD_MUTEX_INITIALIZER;
static Node Nodes[NODE_NUM];
static int numCores;
static int thermalzoneindex = 0;
static int thermalzonenum = 0;
static int interfaceindex = 0;
static CoreBlock storagecores;
static CpuBlock storagecpu;
static int storagenet[2] = {0, 0};
static Block Blocks[] = {
    [Notify] = {0, NULL, drawnotify, clicknotify},
    [Battery] = {0, NULL, drawbattery, NULL},
    [Clock] = {0, NULL, drawstatusclock, NULL},
    [Net] = {0, &storagenet, drawnet, clicknet},
    [Cpu] = {0, &storagecpu, drawcpu, clickcpu},
    [Cores] = {0, &storagecores, drawcores, clickcores},
    [Temp] = {0, NULL, drawtemp, clicktemp},
    [Mem] = {0, NULL, drawmem, clickmem},
    [More] = {0, NULL, drawmore, clickmore},
};
static void (*handler[LASTEvent]) (XEvent *) = {
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
	[MapNotify] = mapnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[ResizeRequest] = resizerequest,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Drw *statusdrw; /* 状态栏绘制上下文 */
static Monitor *mons, *selmon;
static Window root, wmcheckwin;
static int composite_supported = 0;
static int shape_supported = 0;
static Window borderwin = None;

/* status bar cache */
static Pixmap statuscache = None;
static int cachew = 0;
static int cacheh = 0;
static int cachevalid = 0;
static time_t lastupdate = 0;

/* preview mode */
static int previewmode = PREVIEW_SCROLL;  /* will be initialized from config */

/* configuration, allows nested code to access above variables */
#include "config.h"

/* function implementations */
void
applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tagindex = -1;  // 初始化为无效值
	XGetClassHint(dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isfloating = r->isfloating;
			c->tagindex = r->tagindex;
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}

	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	// 如果没有匹配规则，使用当前monitor的scrollindex对应的索引
	if (c->tagindex == -1) {
		for (int i = 0; i < LENGTH(tags); i++) {
			if (c->mon->scrollindex == &c->mon->scrolls[i]) {
				c->tagindex = i;
				break;
			}
		}
	}
	/* 防御越界的 tagindex，避免被 attach/ISVISIBLE 丢弃 */
	if (c->tagindex < 0 || c->tagindex >= LENGTH(tags)) {
		if (c->mon->scrollindex)
			c->tagindex = c->mon->scrollindex - c->mon->scrolls;
		if (c->tagindex < 0 || c->tagindex >= LENGTH(tags))
			c->tagindex = 0;
	}
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - c->w;
		if (*y > sh)
			*y = sh - c->h;
		if (*x + *w < 0)
			*x = 0;
		if (*y + *h < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - c->w;
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - c->h;
		if (*x + *w <= m->wx)
			*x = m->wx;
		if (*y + *h <= m->wy)
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

void
arrange(Monitor *m)
{
	if (m)
		showhide(m);
	else for (m = mons; m; m = m->next)
		showhide(m);
	if (m) {
		arrangemon(m);
		restack(m);
	} else for (m = mons; m; m = m->next)
		arrangemon(m);
	updateborderwin();
}

void
arrangemon(Monitor *m)
{
	snprintf(m->ltsymbol, sizeof m->ltsymbol, "%s", m->lt[m->sellt]->symbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}



void
attach(Client *c)
{
	Client *last;
	int i = c->tagindex;

	if (i < 0 || i >= LENGTH(tags) || !c->mon)
		return;
	
	/* Insert after current selection when possible (non-floating preference) */
	if (!c->isfloating && c->mon->sel && c->mon->sel->tagindex == i) {
		Client **pp = &c->mon->scrolls[i].head;
		for (; *pp && *pp != c->mon->sel; pp = &(*pp)->next);
		if (*pp == c->mon->sel) {
			c->next = c->mon->sel->next;
			c->mon->sel->next = c;
			return;
		}
	}

	/* Find the last client in this tagindex's scroll list */
	last = NULL;
	Client *curr;
	for (curr = c->mon->scrolls[i].head; curr; curr = curr->next)
		last = curr;
	
	/* Append client to the appropriate tagindex list */
	if (last) {
		last->next = c;
	} else {
		c->mon->scrolls[i].head = c;
	}
	c->next = NULL;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
buttonpress(XEvent *e)
{
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
		x = supericonw + selmon->logotitlew;
		do
			x += TEXTW(tags[i]);
		while (ev->x >= x && ++i < LENGTH(tags));
		if (ev->x < supericonw) {
			click = ClkSuperIcon;
		} else if (ev->x < supericonw + selmon->logotitlew) {
			click = ClkWinClass;
		} else if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.i = i;
		} else if (ev->x < x + TEXTW(selmon->ltsymbol)) {
			click = ClkLtSymbol;
		} else if (ev->x > selmon->ww - systandstat) {
			click = ClkStatusText;
			/* 确定点击的是哪个status组件 */
			int stbsw = 0;
			int stx = selmon->ww - ev->x - systrayw;
			for (i = 0; i < LENGTH(Blocks); i++) {
				stbsw += Blocks[i].bw;
				if (stbsw > stx) {
					arg.i = i;
					break;
				}
			}
		} else {
			x += TEXTW(selmon->ltsymbol);
				int n = 0;
				for (c = m->scrollindex->head; c; c = c->next)
					n++;
				if (m->scrollindex->head && n > 0) {
					c = m->scrollindex->head;
					int tabw = (selmon->ww - systandstat - x) / n;
					do {
						x += tabw;
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
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func((click == ClkTagBar || click == ClkWinTitle || click == ClkStatusText) &&
				buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

static void
ensureclientvisible(Client *c, int minw, int minh)
{
	if (!c || !c->mon || !c->mon->scrollindex)
		return;

	Monitor *m = c->mon;

	if (m->lt[m->sellt]->arrange != scroll)
		return;

	if (minw < 1)
		minw = 1;

	int cw = c->w;
	if (minw > cw)
		minw = cw;
	int ch = c->h;
	if (minh > ch)
		minh = ch;

	/* shrink visible band by gaps so checks match layout space */
	int view_left = m->wx + gappx;
	int view_right = m->wx + m->ww - gappx;
	int view_top = m->wy + scrollstartgap + gappx;
	int view_bottom = m->wy + m->wh - scrollstartgap - gappx;
	int cleft = c->x;
	int cright = c->x + cw;
	int ctop = c->y;
	int cbottom = c->y + ch;

	int visible_left = cleft > view_left ? cleft : view_left;
	int visible_right = cright < view_right ? cright : view_right;
	int visible_top = ctop > view_top ? ctop : view_top;
	int visible_bottom = cbottom < view_bottom ? cbottom : view_bottom;
	int visw = visible_right - visible_left;
	int vish = visible_bottom - visible_top;

	if (visw >= minw && vish >= minh)
		return;

	int delta = 0;
	if (cleft < view_left) {
		delta = cleft - view_left;
	} else if (cleft + minw > view_right) {
		delta = cleft + minw - view_right;
	} else if (cright > view_right) {
		delta = cright - view_right;
	}

	if (delta != 0) {
		Arg a = { .i = delta };
		scrollmove(&a);
	}

	/* Vertical visibility fix for floating clients */
	if (vish < minh && c->isfloating) {
		int shortage = minh - vish;
		if (ctop < view_top && cbottom <= view_bottom) {
			int dy = shortage;
			if (cbottom + dy > view_bottom)
				dy = view_bottom - cbottom;
			if (dy > 0)
				resize(c, c->x, c->y + dy, c->w, c->h, 0);
		} else if (cbottom > view_bottom && ctop >= view_top) {
			int dy = shortage;
			if (ctop - dy < view_top)
				dy = ctop - view_top;
			if (dy > 0)
				resize(c, c->x, c->y - dy, c->w, c->h, 0);
		} else {
			/* fallback: clamp to viewport band */
			int newy = c->y;
			if (newy < view_top)
				newy = view_top;
			if (newy + minh > view_bottom)
				newy = view_bottom - minh;
			if (newy != c->y)
				resize(c, c->x, newy, c->w, c->h, 0);
		}
	}
}

void
focusonclick(const Arg *arg)
{
	Client *c = (Client *)arg->v;

	if (!c)
		return;

	if (c->mon != selmon) {
		unfocus(selmon->sel, 1);
		selmon = c->mon;
	}

	if (!c->isfullscreen)
		ensureclientvisible(c, c->w, 50);

	focus(c);
	restack(selmon);
}

void
focusstep(const Arg *arg)
{
	if (!selmon || !selmon->scrollindex)
		return;

	int dir = arg->i;
	if (dir == 0)
		return;

	Client *head = selmon->scrollindex->head;
	Client *sel = selmon->sel;
	Client *target = NULL;

	if (!head)
		return;

	if (dir > 0) {
		Client *start = sel ? sel->next : head;
			for (Client *it = start; it; it = it->next)
				if (!it->isfloating) {
					target = it;
					break;
				}
		} else { /* dir < 0 */
			Client *prev = NULL;
			if (sel) {
				for (Client *it = head; it && it != sel; it = it->next)
					if (!it->isfloating)
						prev = it;
				target = prev;
			} else {
				for (Client *it = head; it; it = it->next)
					if (!it->isfloating)
						target = it;
			}
		}

	if (target && target != selmon->sel) {
		focus(target);
		restack(selmon);
	}
}

void
focusstepvisible(const Arg *arg)
{
	focusstep(arg);
	ensureselectedvisible(arg);
}

static XImage *
scaleimage_sw(XImage *src, unsigned int nw, unsigned int nh)
{
	if (!src || nw == 0 || nh == 0)
		return NULL;

	/* 防止源图像尺寸为0 */
	if (src->width == 0 || src->height == 0)
		return NULL;

	/* 防止过大的目标尺寸导致内存问题 */
	const unsigned int MAX_DIM = 16384;
	if (nw > MAX_DIM || nh > MAX_DIM)
		return NULL;

	/* 检查缩放比例是否合理，防止极端情况 */
	if (nw > src->width * 10 || nh > src->height * 10)
		return NULL;

	XImage *dst = XCreateImage(dpy, DefaultVisual(dpy, screen),
		DefaultDepth(dpy, screen), ZPixmap, 0, NULL, nw, nh, 32, 0);
	if (!dst)
		return NULL;

	/* 检查 bytes_per_line 是否合理 */
	if (dst->bytes_per_line == 0 || dst->bytes_per_line > 65536) {
		XDestroyImage(dst);
		return NULL;
	}

	/* 分配内存，检查是否成功 */
	dst->data = ecalloc(1, nh * dst->bytes_per_line);
	if (!dst->data) {
		XDestroyImage(dst);
		return NULL;
	}

	/* 执行缩放，添加边界检查 */
	for (unsigned int y = 0; y < nh; y++) {
		unsigned int sy = (unsigned int)((uint64_t)y * src->height / nh);
		if (sy >= src->height)
			sy = src->height - 1;
		for (unsigned int x = 0; x < nw; x++) {
			unsigned int sx = (unsigned int)((uint64_t)x * src->width / nw);
			if (sx >= src->width)
				sx = src->width - 1;
			XPutPixel(dst, x, y, XGetPixel(src, sx, sy));
		}
	}

	return dst;
}

static XImage *
scaleimage(XImage *src, unsigned int nw, unsigned int nh)
{
	if (!src || nw == 0 || nh == 0)
		return NULL;

	/* 防止源图像尺寸为0 */
	if (src->width == 0 || src->height == 0)
		return NULL;

	/* 防止过大的目标尺寸 */
	const unsigned int MAX_DIM = 16384;
	if (nw > MAX_DIM || nh > MAX_DIM)
		return scaleimage_sw(src, nw, nh);

	/* 检查缩放比例是否合理 */
	if (nw > src->width * 10 || nh > src->height * 10)
		return scaleimage_sw(src, nw, nh);

	/* 尝试使用 XRender 硬件加速缩放 */
	XRenderPictFormat *vfmt = XRenderFindVisualFormat(dpy, DefaultVisual(dpy, screen));
	if (!vfmt)
		return scaleimage_sw(src, nw, nh);

	/* 创建源 Pixmap */
	Pixmap spix = XCreatePixmap(dpy, root, src->width, src->height, DefaultDepth(dpy, screen));
	if (!spix)
		return scaleimage_sw(src, nw, nh);

	GC gc = XCreateGC(dpy, spix, 0, NULL);
	if (!gc) {
		XFreePixmap(dpy, spix);
		return scaleimage_sw(src, nw, nh);
	}
	XPutImage(dpy, spix, gc, src, 0, 0, 0, 0, src->width, src->height);

	/* 创建目标 Pixmap */
	Pixmap dpix = XCreatePixmap(dpy, root, nw, nh, DefaultDepth(dpy, screen));
	if (!dpix) {
		XFreeGC(dpy, gc);
		XFreePixmap(dpy, spix);
		return scaleimage_sw(src, nw, nh);
	}

	/* 创建 Picture 对象 */
	Picture sp = XRenderCreatePicture(dpy, spix, vfmt, 0, NULL);
	Picture dp = XRenderCreatePicture(dpy, dpix, vfmt, 0, NULL);
	if (!sp || !dp) {
		if (sp)
			XRenderFreePicture(dpy, sp);
		if (dp)
			XRenderFreePicture(dpy, dp);
		XFreePixmap(dpy, dpix);
		XFreeGC(dpy, gc);
		XFreePixmap(dpy, spix);
		return scaleimage_sw(src, nw, nh);
	}

	/* 设置变换矩阵和滤镜 */
	XTransform xform = {{
		{ XDoubleToFixed((double)src->width / (double)nw), 0, 0 },
		{ 0, XDoubleToFixed((double)src->height / (double)nh), 0 },
		{ 0, 0, XDoubleToFixed(1.0) }
	}};
	XRenderSetPictureTransform(dpy, sp, &xform);
	XRenderSetPictureFilter(dpy, sp, "bilinear", NULL, 0);
	XRenderComposite(dpy, PictOpSrc, sp, None, dp, 0, 0, 0, 0, 0, 0, nw, nh);

	/* 获取结果图像 */
	XImage *dst = XGetImage(dpy, dpix, 0, 0, nw, nh, AllPlanes, ZPixmap);

	/* 清理资源 */
	XRenderFreePicture(dpy, sp);
	XRenderFreePicture(dpy, dp);
	XFreePixmap(dpy, dpix);
	XFreeGC(dpy, gc);
	XFreePixmap(dpy, spix);

	/* 如果 XRender 失败，降级到软件缩放 */
	if (!dst)
		return scaleimage_sw(src, nw, nh);
	return dst;
}

static XImage *
create_placeholder_image(unsigned int w, unsigned int h)
{
	if (w < 200)
		w = 200;
	if (h < 150)
		h = 150;

	XImage *img = XCreateImage(dpy, DefaultVisual(dpy, screen),
		DefaultDepth(dpy, screen), ZPixmap, 0, NULL, w, h, 32, 0);
	if (!img)
		return NULL;

	img->data = ecalloc(1, h * img->bytes_per_line);
	if (!img->data) {
		XDestroyImage(img);
		return NULL;
	}

	/* simple vertical gradient */
	for (unsigned int y = 0; y < h; y++) {
		unsigned long shade = 0x1a1a1a + ((y * 0x202020) / h);
		for (unsigned int x = 0; x < w; x++)
			XPutPixel(img, x, y, shade);
	}

	return img;
}

static XImage *
getwindowximage(Client *c)
{
	XWindowAttributes attr;
	if (!c)
		return NULL;
	if (!XGetWindowAttributes(dpy, c->win, &attr))
		return NULL;

	XRenderPictFormat *format = XRenderFindVisualFormat(dpy, attr.visual);
	if (!format)
		return NULL;
	int hasalpha = (format && format->type == PictTypeDirect && format->direct.alphaMask);

	XRenderPictureAttributes pa = { .subwindow_mode = IncludeInferiors };
	Picture picture = XRenderCreatePicture(dpy, c->win, format, CPSubwindowMode, &pa);
	int framew = c->w;
	int frameh = c->h;
	if (framew <= 0 || frameh <= 0)
		return NULL;
	Pixmap pixmap = XCreatePixmap(dpy, root, framew, frameh, 32);
	if (!pixmap) {
		if (picture)
			XRenderFreePicture(dpy, picture);
		return NULL;
	}
	XRenderPictFormat *fmt32 = XRenderFindStandardFormat(dpy, PictStandardARGB32);
	if (!fmt32) {
		if (picture)
			XRenderFreePicture(dpy, picture);
		XFreePixmap(dpy, pixmap);
		return NULL;
	}
	Picture pm = XRenderCreatePicture(dpy, pixmap, fmt32, 0, NULL);
	if (!picture || !pm) {
		if (picture)
			XRenderFreePicture(dpy, picture);
		if (pm)
			XRenderFreePicture(dpy, pm);
		XFreePixmap(dpy, pixmap);
		return NULL;
	}

	XRenderColor clear = { .red = 0, .green = 0, .blue = 0, .alpha = 0 };
	XRenderFillRectangle(dpy, PictOpSrc, pm, &clear, 0, 0, framew, frameh);
	XRenderComposite(dpy, hasalpha ? PictOpOver : PictOpSrc, picture, None, pm,
		0, 0, 0, 0, 0, 0, framew, frameh);

	XImage *img = XGetImage(dpy, pixmap, 0, 0, framew, frameh, AllPlanes, ZPixmap);
	if (img) {
		img->red_mask = fmt32->direct.redMask << fmt32->direct.red;
		img->green_mask = fmt32->direct.greenMask << fmt32->direct.green;
		img->blue_mask = fmt32->direct.blueMask << fmt32->direct.blue;
		img->depth = DefaultDepth(dpy, screen);
	}

	XRenderFreePicture(dpy, picture);
	XRenderFreePicture(dpy, pm);
	XFreePixmap(dpy, pixmap);

	return img;
}

static XImage *
getwindowximage_safe(Client *c)
{
	XImage *res = NULL;
	
	/* 检查客户端有效性 */
	if (!c || !c->mon) {
		return create_placeholder_image(200, 150);
	}

	/* 检查窗口尺寸是否合理 */
	unsigned int w = c->w > 0 ? (unsigned int)c->w : 200;
	unsigned int h = c->h > 0 ? (unsigned int)c->h : 150;
	
	/* 防止极端尺寸 */
	if (w > 8192) w = 8192;
	if (h > 8192) h = 8192;

	/* 设置错误处理器以捕获X错误 */
	XErrorHandler old = XSetErrorHandler(xerrordummy);

	/* 尝试获取窗口图像 */
	XWindowAttributes attr;
	if (XGetWindowAttributes(dpy, c->win, &attr)) {
		/* 检查窗口属性是否有效 */
		if (attr.width > 0 && attr.height > 0 && attr.width < 8192 && attr.height < 8192) {
			res = getwindowximage(c);
		}
	}

	/* 恢复原来的错误处理器 */
	XSetErrorHandler(old);

	/* 如果获取失败，使用占位图像 */
	if (!res) {
		res = create_placeholder_image(w, h);
	}

	return res;
}

typedef struct {
	Client *c;
	XImage *img;
	XImage *scaled;
	int x, y, w, h;
} PreviewItem;

static int
thumbindex(PreviewItem *items, int n, Client *c)
{
	for (int i = 0; i < n; i++)
		if (items[i].c == c)
			return i;
	return -1;
}

static void
centerpreviewselectedy(PreviewItem *items, int *order, int selected, int previewh, int maxoffsety, int *offsety)
{
	if (selected < 0)
		return;

	PreviewItem *sel = &items[order[selected]];
	int next = sel->y + sel->h / 2 - previewh / 2;
	if (next < 0)
		next = 0;
	if (next > maxoffsety)
		next = maxoffsety;
	*offsety = next;
}

static int
findpreviewneighbor(PreviewItem *items, int *order, int n, int selected, int dirx, int diry)
{
	int best_index = -1;
	int min_distance = INT_MAX;
	PreviewItem *current = &items[order[selected]];
	int current_center_x = current->x + current->w / 2;
	int current_center_y = current->y + current->h / 2;

	for (int i = 0; i < n; i++) {
		if (i == selected)
			continue;
		PreviewItem *candidate = &items[order[i]];
		int candidate_center_x = candidate->x + candidate->w / 2;
		int candidate_center_y = candidate->y + candidate->h / 2;
		int dx = candidate_center_x - current_center_x;
		int dy = candidate_center_y - current_center_y;

		if ((dirx < 0 && dx >= 0) || (dirx > 0 && dx <= 0) ||
		    (diry < 0 && dy >= 0) || (diry > 0 && dy <= 0))
			continue;

		int distance = (dirx != 0)
		             ? (abs(dx) + abs(dy) * 2)
		             : (abs(dy) + abs(dx) * 2);
		if (distance < min_distance) {
			min_distance = distance;
			best_index = i;
		}
	}

	return best_index;
}

static void
arrangePreviewsGrid(PreviewItem *items, int n, int pad, int previeww, int previewh, int *totalh, int *totalw)
{
	if (n == 1) {
		int sw = items[0].scaled ? items[0].scaled->width : 0;
		int sh = items[0].scaled ? items[0].scaled->height : 0;
		items[0].x = (previeww - sw) / 2;
		items[0].y = (previewh - sh) / 2;
		if (totalh) *totalh = sh;
		if (totalw) *totalw = sw;
		return;
	}

	if (n <= 4) {
		unsigned int total_gapi = pad * (n - 1);
		unsigned int row_width = 0;
		int maxh = 0;
		for (int i = 0; i < n; i++) {
			int sw = items[i].scaled ? items[i].scaled->width : 0;
			int sh = items[i].scaled ? items[i].scaled->height : 0;
			row_width += sw;
			if (sh > maxh) maxh = sh;
		}
		row_width += total_gapi;

		int cx = (previeww - row_width) / 2;
		int cy = (previewh - maxh) / 2;

		for (int i = 0; i < n; i++) {
			int sw = items[i].scaled ? items[i].scaled->width : 0;
			int sh = items[i].scaled ? items[i].scaled->height : 0;
			items[i].x = cx;
			items[i].y = cy + (maxh - sh) / 2;
			cx += sw + pad;
		}
		if (totalh) *totalh = maxh;
		if (totalw) *totalw = row_width;
		return;
	}

	unsigned int cols, rows;
	for (cols = 0; cols <= n / 2; cols++)
		if (cols * cols >= n)
			break;
	rows = (cols && (cols - 1) * cols >= n) ? cols - 1 : cols;

	/* 检查宽度约束，确保每行能放入画布 */
	while (cols > 1) {
		int estimated_row_width = 0;
		for (int i = 0; i < cols && i < n; i++) {
			int sw = items[i].scaled ? items[i].scaled->width : 0;
			estimated_row_width += sw;
		}
		estimated_row_width += (cols - 1) * pad;
		if (estimated_row_width <= previeww)
			break;
		cols--;
		rows = (n + cols - 1) / cols;
	}

	int idx = 0;
	int cy = 0;
	int maxh = 0;
	int maxw = 0;

	for (unsigned int i = 0; i < rows; i++) {
		int cx = 0;
		int row_maxh = 0;
		int start_idx = idx;

		for (unsigned int j = 0; j < cols; j++) {
			if (idx >= n)
				break;
			int sw = items[idx].scaled ? items[idx].scaled->width : 0;
			int sh = items[idx].scaled ? items[idx].scaled->height : 0;
			items[idx].x = cx;
			if (sh > row_maxh) row_maxh = sh;
			if (sw > maxw) maxw = sw;
			cx += sw + pad;
			idx++;
		}

		int row_width = cx - pad;
		cx = (previeww - row_width) / 2;
		for (unsigned int j = 0; j < cols && start_idx < n; j++) {
			items[start_idx].x += cx;
			items[start_idx].y = cy;
			start_idx++;
		}

		cy += row_maxh + pad;
		if (row_maxh > maxh) maxh = row_maxh;
	}

	if (totalh) *totalh = cy - pad;
	if (totalw) *totalw = maxw;

	/* 垂直居中 */
	int total_content_height = cy - pad;
	if (total_content_height < previewh) {
		int vertical_offset = (previewh - total_content_height) / 2;
		for (int i = 0; i < n; i++) {
			items[i].y += vertical_offset;
		}
	}
}

static void
drawpreview(Window win, Pixmap buf, GC gc, PreviewItem *items, int n, Client **stacklist, int scount,
	int offset, int offsety, int pad, int previeww, int previewh, int *order, int selected,
	int totalw, int totalh, int mode)
{
	/* 参数检查 */
	if (!items || n <= 0 || !order || previeww <= 0 || previewh <= 0)
		return;

	XSetForeground(dpy, gc, scheme[SchemeNorm][ColBg].pixel);
	XFillRectangle(dpy, buf, gc, 0, 0, previeww, previewh);

	/* draw layout windows first (non-floating), using x-order */
	for (int oi = 0; oi < n; oi++) {
		int idx = order[oi];
		if (idx < 0 || idx >= n)
			continue;
		Client *c = items[idx].c;
		if (!c || c->isfloating)
			continue;
		int dx = items[idx].x - offset + pad;
		int dy = items[idx].y - offsety + pad;
		if (dx + items[idx].w < 0 || dx > previeww || dy + items[idx].h < 0 || dy > previewh)
			continue;
		if (items[idx].scaled && items[idx].w > 0 && items[idx].h > 0)
			XPutImage(dpy, buf, gc, items[idx].scaled, 0, 0, dx, dy, items[idx].w, items[idx].h);
	}

	/* then draw floating windows following stack z-order */
	if (stacklist && scount > 0) {
		for (int i = scount - 1; i >= 0; i--) {
			Client *c = stacklist[i];
			if (!c || !c->isfloating)
				continue;
			int idx = thumbindex(items, n, c);
			if (idx < 0)
				continue;
			int dx = items[idx].x - offset + pad;
			int dy = items[idx].y - offsety + pad;
			if (dx + items[idx].w < 0 || dx > previeww || dy + items[idx].h < 0 || dy > previewh)
				continue;
			if (items[idx].scaled && items[idx].w > 0 && items[idx].h > 0)
				XPutImage(dpy, buf, gc, items[idx].scaled, 0, 0, dx, dy, items[idx].w, items[idx].h);
		}
	}

	/* draw selected border */
	if (selected >= 0 && selected < n) {
		int sidx = order[selected];
		if (sidx >= 0 && sidx < n) {
			int dx = items[sidx].x - offset + pad;
			int dy = items[sidx].y - offsety + pad;
			if (!(dx + items[sidx].w < 0 || dx > previeww || dy + items[sidx].h < 0 || dy > previewh)) {
				if (items[sidx].w > 0 && items[sidx].h > 0) {
					XSetForeground(dpy, gc, scheme[SchemeSel][ColBorder].pixel);
					XSetLineAttributes(dpy, gc, borderpx, LineSolid, CapButt, JoinMiter);
					XDrawRectangle(dpy, buf, gc, dx + borderpx/2, dy + borderpx/2,
						items[sidx].w - borderpx, items[sidx].h - borderpx);
					XSetLineAttributes(dpy, gc, 0, LineSolid, CapButt, JoinMiter);
				}
			}
		}
	}

	/* Draw mode indicator */
	(void)mode;

	/* Draw scrollbar - horizontal for scroll mode, vertical for grid mode */
	if (mode == PREVIEW_SCROLL) {
		/* Draw scrollbar at bottom (horizontal) */
		if (totalw > previeww) {
			int scrollbar_height = 3;
			int scrollbar_y = previewh - scrollbar_height - 2;
			int scrollbar_width = previeww - 4;
			int scrollbar_x = 2;

			/* Draw scrollbar track */
			XSetForeground(dpy, gc, scheme[SchemeNorm][ColBorder].pixel);
			XFillRectangle(dpy, buf, gc, scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_height);

			/* Calculate thumb position and size */
			float ratio = (float)previeww / (float)totalw;
			int thumb_width = (int)(scrollbar_width * ratio);
			if (thumb_width < 10) thumb_width = 10;

			float offset_ratio = (float)offset / (float)(totalw - previeww);
			int thumb_x = scrollbar_x + (int)((scrollbar_width - thumb_width) * offset_ratio);

			/* Draw scrollbar thumb */
			XSetForeground(dpy, gc, scheme[SchemeSel][ColBorder].pixel);
			XFillRectangle(dpy, buf, gc, thumb_x, scrollbar_y, thumb_width, scrollbar_height);
		}
	} else {
		/* Draw scrollbar at right (vertical) for grid mode */
		if (totalh > previewh) {
			int scrollbar_width = 3;
			int scrollbar_x = previeww - scrollbar_width - 2;
			int scrollbar_height = previewh - 4;
			int scrollbar_y = 2;

			/* Draw scrollbar track */
			XSetForeground(dpy, gc, scheme[SchemeNorm][ColBorder].pixel);
			XFillRectangle(dpy, buf, gc, scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_height);

			/* Calculate thumb position and size */
			float ratio = (float)previewh / (float)totalh;
			int thumb_height = (int)(scrollbar_height * ratio);
			if (thumb_height < 10) thumb_height = 10;

			float offset_ratio = (float)offsety / (float)(totalh - previewh);
			int thumb_y = scrollbar_y + (int)((scrollbar_height - thumb_height) * offset_ratio);

			/* Draw scrollbar thumb */
			XSetForeground(dpy, gc, scheme[SchemeSel][ColBorder].pixel);
			XFillRectangle(dpy, buf, gc, scrollbar_x, thumb_y, scrollbar_width, thumb_height);
		}
	}

	XCopyArea(dpy, buf, win, gc, 0, 0, previeww, previewh, 0, 0);
}

static void
previewscroll(const Arg *arg)
{
	(void)arg;
	if (!selmon || !selmon->scrollindex)
		return;

	Monitor *m = selmon;
	Client *c;
	int n = 0;
		for (c = m->scrollindex->head; c; c = c->next)
			n++;
	if (n == 0)
		return;

	PreviewItem *items = ecalloc(n, sizeof(PreviewItem));

	int idx = 0;
	int minx = INT_MAX, maxr = INT_MIN, miny = INT_MAX, maxb = INT_MIN;
		for (c = m->scrollindex->head; c && idx < n; c = c->next) {
			items[idx].c = c;
			minx = MIN(minx, c->x);
			maxr = MAX(maxr, c->x + c->w);
			miny = MIN(miny, c->y);
			maxb = MAX(maxb, c->y + c->h);
			idx++;
		}

	if (minx == INT_MAX || maxr <= minx) {
		free(items);
		return;
	}

	int previeww = (m->ww * 3) / 4;
	int pad = gappx * 2;

	/* 确保预览窗口尺寸合理 */
	if (previeww < 200) previeww = 200;
	if (previeww > 4096) previeww = 4096;

	int boundsh = maxb - miny;
	if (boundsh < 1)
		boundsh = 1;

	/* 始终使用滚动模式的高度（m->wh / 4）来计算缩放比例 */
	int scale_previewh = m->wh / 4;
	if (scale_previewh < 100) scale_previewh = 100;
	if (scale_previewh > 2048) scale_previewh = 2048;

	/* 计算缩放比例，防止除零和极端值 */
	float scale = (float)(scale_previewh - 2 * pad) / (float)boundsh;
	if (scale <= 0.0f || scale > 10.0f)
		scale = 0.1f;

	Client *selbefore = selmon->sel;

	for (int i = 0; i < n; i++) {
		c = items[i].c;
		items[i].img = getwindowximage_safe(c);
		
		/* 计算缩放后的尺寸，防止溢出 */
		int sw = (int)((float)(c->w) * scale);
		int sh = (int)((float)(c->h) * scale);

		/* 确保最小尺寸 */
		if (sw < 10) sw = 10;
		if (sh < 10) sh = 10;

		/* 限制最大尺寸，防止内存问题 */
		if (sw > previeww) sw = previeww;
		if (sh > scale_previewh) sh = scale_previewh;
		
		/* 执行缩放，检查结果 */
		items[i].scaled = scaleimage(items[i].img, (unsigned int)sw, (unsigned int)sh);
		if (!items[i].scaled) {
			/* 如果缩放失败，使用原始图像（如果尺寸合适）或创建占位符 */
			if (items[i].img && (unsigned int)sw <= items[i].img->width &&
			    (unsigned int)sh <= items[i].img->height) {
				/* 使用原始图像 */
				items[i].scaled = items[i].img;
				items[i].img = NULL;
			} else {
				/* 创建占位符 */
				if (items[i].img) {
					XDestroyImage(items[i].img);
					items[i].img = NULL;
				}
				items[i].scaled = create_placeholder_image((unsigned int)sw, (unsigned int)sh);
			}
		}

		/* 计算滚动模式的位置（保存用于切换回滚动模式） */
		int scroll_x = (int)((float)(c->x - minx) * scale);
		int scroll_y = (int)((float)(c->y - miny) * scale);

		items[i].x = scroll_x;
		items[i].y = scroll_y;
		items[i].w = sw;
		items[i].h = sh;

		/* 限制位置范围 */
		if (items[i].x < -previeww) items[i].x = -previeww;
		if (items[i].y < -scale_previewh) items[i].y = -scale_previewh;
	}

	/* 保存缩放参数，用于切换回滚动模式 */
	float saved_scale = scale;
	int saved_minx = minx;
	int saved_miny = miny;

	int totalw = 0;
	for (int i = 0; i < n; i++)
		if (items[i].x + items[i].w > totalw)
			totalw = items[i].x + items[i].w;
	totalw += pad * 2;

	/* 根据模式决定实际的预览窗口高度 */
	int previewh = (previewmode == PREVIEW_GRID) ? (m->wh - bh) : scale_previewh;
	/* 网格模式同时调整宽度 */
	if (previewmode == PREVIEW_GRID) {
		previeww = m->ww - 2 * bh;
		if (previeww < 200) previeww = 200;
	}
	/* 确保预览窗口尺寸合理 */
	if (previewh < 100) previewh = 100;
	if (previewh > 2048) previewh = 2048;

	int *order = ecalloc(n, sizeof(int));
	for (int i = 0; i < n; i++)
		order[i] = i;

	int selected = -1;
	for (int i = 0; i < n; i++)
		if (items[order[i]].c == selmon->sel) {
			selected = i;
			break;
		}
	if (selected == -1)
		selected = n / 2;

	int totalh = previewh;
	int maxoffset = totalw > previeww ? totalw - previeww : 0;
	int maxoffsety = 0;
	int offset = 0;
	int offsety = 0;

	if (previewmode == PREVIEW_GRID) {
		arrangePreviewsGrid(items, n, gappx, previeww, previewh, &totalh, &totalw);
		maxoffset = 0;
		offset = 0;
		maxoffsety = totalh > previewh ? totalh - previewh : 0;

		/* 自动滚动到选中项（与水平模式相同的逻辑） */
		if (selected >= 0 && selected < n) {
			centerpreviewselectedy(items, order, selected, previewh, maxoffsety, &offsety);
		} else {
			offsety = 0;
		}

		if (offsety < 0) offsety = 0;
		if (offsety > maxoffsety) offsety = maxoffsety;
	} else {
		maxoffsety = 0;
		int selx = items[order[selected]].x;
		int selw = items[order[selected]].w;
		offset = selx + selw / 2 - previeww / 2;
		if (offset < 0)
			offset = 0;
		if (offset > maxoffset)
			offset = maxoffset;
	}

	XSetWindowAttributes owa = {
		.override_redirect = True,
		.background_pixel = scheme[SchemeSel][ColBg].pixel,
		.border_pixel = 0,
		.event_mask = KeyPressMask | ButtonPressMask | ExposureMask,
	};
	Window overlay = None;
	Window pwin = None;
	GC gc = NULL;
	Pixmap buf = None;
	Client **stacklist = NULL;
	int confirmed = 0;

	overlay = XCreateWindow(dpy, root, 0, 0, sw, sh, 0,
		DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
		CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &owa);
	if (!overlay)
		goto preview_cleanup;
	XMapRaised(dpy, overlay);

	int px = m->wx + ((previewmode == PREVIEW_GRID) ? bh : (m->ww - previeww) / 2);
	int py = m->wy + ((previewmode == PREVIEW_GRID) ? 0 : (m->wh / 4));
	XSetWindowAttributes pwa = {
		.override_redirect = True,
		.background_pixel = scheme[SchemeNorm][ColBg].pixel,
		.border_pixel = scheme[SchemeSel][ColBorder].pixel,
		.event_mask = ExposureMask | ButtonPressMask,
	};
	pwin = XCreateWindow(dpy, overlay, px, py, previeww, previewh, 1,
		DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
		CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &pwa);
	if (!pwin)
		goto preview_cleanup;
	XMapRaised(dpy, pwin);

	XGrabKeyboard(dpy, overlay, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	XGrabPointer(dpy, overlay, True, ButtonPressMask, GrabModeAsync, GrabModeAsync,
		None, cursor[CurNormal]->cursor, CurrentTime);

	gc = XCreateGC(dpy, pwin, 0, NULL);
	if (!gc)
		goto preview_cleanup;

	int scount = 0;
	for (c = m->stack; c; c = c->snext)
		if (ISVISIBLE(c))
			scount++;
	stacklist = ecalloc(scount, sizeof(Client *));
	int si = 0;
	for (c = m->stack; c; c = c->snext)
		if (ISVISIBLE(c))
			stacklist[si++] = c;

	int running = 1;
	int lastselected = selected;
	buf = XCreatePixmap(dpy, pwin, previeww, previewh, DefaultDepth(dpy, screen));
	if (!buf)
		goto preview_cleanup;
	int needredraw = 1;
	int needblit = 0;
	int drawn = 0;
	drawpreview(pwin, buf, gc, items, n, stacklist, scount, offset, offsety, pad, previeww, previewh, order, selected, totalw, totalh, previewmode);
	drawn = 1;
	needredraw = 0;

	while (running) {
		XEvent ev;
		XNextEvent(dpy, &ev);
		needblit = 0;

		if (ev.type == KeyPress) {
			KeySym ks = XKeycodeToKeysym(dpy, (KeyCode)ev.xkey.keycode, 0);
			if (ks == XK_Escape) {
				running = 0;
				confirmed = 0;
			} else if (ks == XK_Return || ks == XK_space) {
				running = 0;
				confirmed = 1;
			} else if (ks == XK_Tab) {
				previewmode = (previewmode == PREVIEW_SCROLL) ? PREVIEW_GRID : PREVIEW_SCROLL;
				int new_previewh = (previewmode == PREVIEW_GRID) ? (m->wh - bh) : (m->wh / 4);
				new_previewh = MAX(new_previewh, 100);
				new_previewh = MIN(new_previewh, 2048);
				int new_py = m->wy + ((previewmode == PREVIEW_GRID) ? 0 : (m->wh / 4));
				int new_previeww = previeww;
				if (previewmode == PREVIEW_GRID) {
					new_previeww = m->ww - 2 * bh;
					if (new_previeww < 200) new_previeww = 200;
				}
				int new_px = m->wx + ((previewmode == PREVIEW_GRID) ? bh : (m->ww - new_previeww) / 2);
				XMoveResizeWindow(dpy, pwin, new_px, new_py, new_previeww, new_previewh);
				previewh = new_previewh;
				previeww = new_previeww;
				px = new_px;
				py = new_py;
				XFreePixmap(dpy, buf);
				buf = XCreatePixmap(dpy, pwin, previeww, previewh, DefaultDepth(dpy, screen));
				if (previewmode == PREVIEW_GRID) {
					arrangePreviewsGrid(items, n, gappx, previeww, previewh, &totalh, &totalw);
					offset = 0;
					offsety = 0;
					maxoffsety = totalh > previewh ? totalh - previewh : 0;
				} else {
					/* 切换回滚动模式，使用保存的缩放参数重新计算位置 */
					totalw = 0;
					for (int i = 0; i < n; i++) {
						Client *c = items[i].c;
						items[i].x = (int)((float)(c->x - saved_minx) * saved_scale);
						items[i].y = (int)((float)(c->y - saved_miny) * saved_scale);
						if (items[i].x + items[i].w > totalw)
							totalw = items[i].x + items[i].w;
					}
					totalw += pad * 2;
					maxoffset = totalw > previeww ? totalw - previeww : 0;
					maxoffsety = 0;
					offsety = 0;
					int selx = items[order[selected]].x;
					int selw = items[order[selected]].w;
					offset = selx + selw / 2 - previeww / 2;
					if (offset < 0) offset = 0;
					if (offset > maxoffset) offset = maxoffset;
				}
				needredraw = 1;
			} else if (ks == XK_h || ks == XK_Left) {
				int best_index = findpreviewneighbor(items, order, n, selected, -1, 0);
					if (best_index != -1) {
						selected = best_index;
						needredraw = 1;
						if (previewmode == PREVIEW_GRID)
							centerpreviewselectedy(items, order, selected, previewh, maxoffsety, &offsety);
					}
			} else if (ks == XK_l || ks == XK_Right) {
				int best_index = findpreviewneighbor(items, order, n, selected, 1, 0);
					if (best_index != -1) {
						selected = best_index;
						needredraw = 1;
						if (previewmode == PREVIEW_GRID)
							centerpreviewselectedy(items, order, selected, previewh, maxoffsety, &offsety);
					}
			} else if (ks == XK_k || ks == XK_Up) {
				int best_index = findpreviewneighbor(items, order, n, selected, 0, -1);
					if (best_index != -1) {
						selected = best_index;
						needredraw = 1;
						if (previewmode == PREVIEW_GRID)
							centerpreviewselectedy(items, order, selected, previewh, maxoffsety, &offsety);
					}
			} else if (ks == XK_j || ks == XK_Down) {
				int best_index = findpreviewneighbor(items, order, n, selected, 0, 1);
					if (best_index != -1) {
						selected = best_index;
						needredraw = 1;
						if (previewmode == PREVIEW_GRID)
							centerpreviewselectedy(items, order, selected, previewh, maxoffsety, &offsety);
					}
			}
		} else if (ev.type == ButtonPress) {
			if (ev.xbutton.button == Button4) {
				if (previewmode == PREVIEW_SCROLL) {
					offset -= previeww / 8;
					if (offset < 0)
						offset = 0;
				} else {
					offsety -= previewh / 8;
					if (offsety < 0)
						offsety = 0;
				}
				needredraw = 1;
			} else if (ev.xbutton.button == Button5) {
				if (previewmode == PREVIEW_SCROLL) {
					offset += previeww / 8;
					if (offset > maxoffset)
						offset = maxoffset;
				} else {
					offsety += previewh / 8;
					if (offsety > maxoffsety)
						offsety = maxoffsety;
				}
				needredraw = 1;
			} else if (ev.xbutton.button == Button1 && ev.xbutton.window == pwin) {
				int cx = ev.xbutton.x + offset - pad;
				int cy = ev.xbutton.y + offsety - pad;
				int hit = -1;
				unsigned int bestarea = UINT_MAX;
				for (int i = scount - 1; i >= 0; i--) {
					int tidx = thumbindex(items, n, stacklist[i]);
					if (tidx < 0)
						continue;
					if (cx >= items[tidx].x && cx <= items[tidx].x + items[tidx].w &&
						cy >= items[tidx].y && cy <= items[tidx].y + items[tidx].h) {
						unsigned int area = (unsigned int)items[tidx].w * (unsigned int)items[tidx].h;
						if (area < bestarea) {
							bestarea = area;
							hit = tidx;
						}
					}
				}
				if (hit >= 0) {
					int hitorder = -1;
					for (int i = 0; i < n; i++)
						if (order[i] == hit) {
							hitorder = i;
							break;
						}
					if (hitorder >= 0) {
						if (hitorder == selected) {
							confirmed = 1;
							running = 0;
						} else {
							selected = hitorder;
							needredraw = 1;
						}
					}
				}
			}
		} else if (ev.type == Expose && ev.xexpose.window == pwin) {
			needblit = 1;
		}

		/* keep newly selected item visible */
		if (selected != lastselected) {
			int sx = items[order[selected]].x;
			if (sx - offset < pad)
				offset = sx - pad;
			if (items[order[selected]].x + items[order[selected]].w - offset > previeww - pad)
				offset = items[order[selected]].x + items[order[selected]].w - (previeww - pad);
			if (offset < 0)
				offset = 0;
			if (offset > maxoffset)
				offset = maxoffset;
			lastselected = selected;
			needredraw = 1;
		}

		if (needredraw) {
			drawpreview(pwin, buf, gc, items, n, stacklist, scount, offset, offsety, pad, previeww, previewh, order, selected, totalw, totalh, previewmode);
			drawn = 1;
			needredraw = 0;
		} else if (needblit && drawn) {
			XCopyArea(dpy, buf, pwin, gc, 0, 0, previeww, previewh, 0, 0);
		}
	}

preview_cleanup:
		XUngrabKeyboard(dpy, CurrentTime);
		XUngrabPointer(dpy, CurrentTime);
		if (gc)
			XFreeGC(dpy, gc);
		if (buf)
			XFreePixmap(dpy, buf);
		if (pwin)
			XDestroyWindow(dpy, pwin);
		if (overlay)
			XDestroyWindow(dpy, overlay);

	if (confirmed && selected >= 0 && selected < n) {
		Client *target = items[order[selected]].c;
		Arg a = { .v = target };
		focusonclick(&a);
	}
	(void)selbefore;

	/* 安全清理资源 */
	for (int i = 0; i < n; i++) {
		/* 检查指针是否有效，避免双重释放 */
		if (items[i].scaled) {
			XDestroyImage(items[i].scaled);
			items[i].scaled = NULL;
		}
		if (items[i].img) {
			XDestroyImage(items[i].img);
			items[i].img = NULL;
		}
	}
	
	/* 清理数组前先清空指针 */
	if (stacklist) {
		free(stacklist);
		stacklist = NULL;
	}
	if (order) {
		free(order);
		order = NULL;
	}
	if (items) {
		free(items);
		items = NULL;
	}
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	view(&a);
	cleanstatuspthread();
	freestatuscache();
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
	if (borderwin != None)
		XDestroyWindow(dpy, borderwin);

	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors); i++)
		drw_scm_free(drw, scheme[i], 3);
	free(scheme);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	drw_free(statusdrw);

	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		if (m)
			m->next = mon->next;
	}
	
	/* 先取消复合重定向，避免子窗口问题 */
	if (composite_supported && mon->container) {
		XCompositeUnredirectSubwindows(dpy, mon->container, CompositeRedirectAutomatic);
	}
	
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
	
	/* 确保container窗口存在且有效 */
	if (mon->container) {
		XUnmapWindow(dpy, mon->container);
		XDestroyWindow(dpy, mon->container);
	}
	
	free(mon->scrolls);
	free(mon);
}

void
clientmessage(XEvent *e)
{
	XWindowAttributes wa;
	XSetWindowAttributes swa;
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (showsystray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
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
			
			c->isfloating = True;
			c->tagindex = 1;
			updatesizehints(c);
			updatesystrayicongeom(c, wa.width, wa.height);
			XAddToSaveSet(dpy, c->win);
			XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
			XReparentWindow(dpy, c->win, systray->win, c->x, c->y);
			/* use parents background color */
			swa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
			XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			/* FIXME not sure if I have to send these events, too */
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_FOCUS_IN, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_MODALITY_ON, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			XSync(dpy, False);
			resizebarwin(selmon);
			updatesystray();
			setclientstate(c, NormalState);
		}
		return;
	}

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
	updateborderwin();
}

void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = 0;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
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
				for (int i = 0; i < LENGTH(tags); i++) {
					for (c = m->scrolls[i].head; c; c = c->next)
						if (c->isfullscreen)
							resizeclient(c, m->mx, m->my, m->mw, m->mh);
				}
				resizebarwin(m);
			}
			focus(NULL);
			arrange(NULL);
		}
	} else if (shape_supported) {
		XWindowAttributes wa;
		int isbar = 0;

		if (ev->window == borderwin || wintoclient(ev->window) || wintosystrayicon(ev->window))
			return;

		for (m = mons; m; m = m->next) {
			if (ev->window == m->barwin || ev->window == m->container) {
				isbar = 1;
				break;
			}
		}
		if (isbar)
			return;

		if (!XGetWindowAttributes(dpy, ev->window, &wa))
			return;
		if (wa.override_redirect)
			setroundedfromattrs(ev->window, &wa);
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			
			/* Ignore external position changes completely */
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if (c->isfloating) {
				int maxx = m->wx + m->ww - c->w;
				int maxy = m->wy + m->wh - c->h;
				if (maxx < m->wx)
					maxx = m->wx;
				if (maxy < m->wy)
					maxy = m->wy;
				if (c->x < m->wx)
					c->x = m->wx;
				else if (c->x > maxx)
					c->x = maxx;
				if (c->y < m->wy)
					c->y = m->wy;
				else if (c->y > maxy)
					c->y = maxy;
			}
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				resizeclient(c, c->x, c->y, c->w, c->h);
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

Monitor *
createmon(void)
{
	Monitor *m;
	XSetWindowAttributes wa;

	m = ecalloc(1, sizeof(Monitor));
	m->showbar = showbar;
	m->topbar = topbar;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	m->scrolls = ecalloc(LENGTH(tags), sizeof(Scroll));
	m->scrollindex = &m->scrolls[0];
	m->prevtag = 0; /* 默认为第一个tag */
	snprintf(m->ltsymbol, sizeof m->ltsymbol, "%s", layouts[0].symbol);

	wa.override_redirect = True;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|ExposureMask;
	m->container = XCreateWindow(dpy, root, 0, 0, 1, 1, 0,
		DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
		CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
	XMapWindow(dpy, m->container);
	redirectmonitor(m);

	return m;
}

void
destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
	else if ((c = wintosystrayicon(ev->window))) {
		removesystrayicon(c);
		resizebarwin(selmon);
		updatesystray();
	}
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	if (!c->mon)
		return;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

void
detach(Client *c)
{
	Client **tc;
	int i = c->tagindex;
	
	if (i < 0 || i >= LENGTH(tags))
		return;
	
	/* Find and remove client from scroll list */
	for (tc = &c->mon->scrolls[i].head; *tc && *tc != c; tc = &(*tc)->next);
	if (*tc) {
		*tc = c->next;
		c->next = NULL;
	}
}

void
drawbar(Monitor *m)
{
	int x, w, n = 0;
	
	Client *c;

	if (!m->showbar)
		return;

	/* count visible clients */
	for (int i = 0; i < LENGTH(tags); i++) {
		for (c = m->scrolls[i].head; c; c = c->next) {
			if (ISVISIBLE(c))
				n++;
		}
	}

	/* draw components in order */
	x = 0;
	drawsupericon(m, &x);
	drawlogotitle(m, &x);
	drawtags(m, &x);
	drawlayout(m, &x);

	/* draw client tabs */
	w = m->ww - systandstat - x;
	if (w > bh && n > 0) {
		drawclienttabs(m, x, w, n);
	} else if (w > bh) {
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, x, 0, w, bh, 1, 1);
	}

	/* Don't draw over statusbar area */
	if (m == selmon) {
		drw_map(drw, m->barwin, 0, 0, m->ww - systandstat, bh);
	} else {
		const char *str = "other monitor";
		drw_setscheme(drw, scheme[SchemeSel]);
		drw_text(drw, m->ww - systandstat, 0, systandstat, bh,
		         (systandstat - TEXTW(str)) / 2, str, 1);
		drw_map(drw, m->barwin, 0, 0, m->ww, bh);
	}
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		if (showsystray)
			updatesystray();
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window))) {
		drawbar(m);
		if (m == selmon) {
			updatesystray();
			/* Copy cache to bar if available */
			pthread_mutex_lock(&statuscache_mutex);
			if (cachevalid && statuscache != None) {
				XCopyArea(dpy, statuscache, selmon->barwin, statusdrw->gc, 
						  0, 0, cachew, bh,
						  selmon->ww - cachew, 0);
			}
			pthread_mutex_unlock(&statuscache_mutex);
		}
	}
}

void
focus(Client *c)
{
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);
	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
	updateborderwin();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

Atom
getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	/* FIXME getatomprop should return the number of items and a pointer to
	 * the stored data instead of this workaround */
	Atom req = XA_ATOM;
	if (prop == xatom[XembedInfo])
		req = xatom[XembedInfo];

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req,
		&da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		if (da == xatom[XembedInfo] && dl == 2)
			atom = ((Atom *)p)[1];
		XFree(p);
	}
	return atom;
}

unsigned int
getsystraywidth()
{
	unsigned int w = 0;
	Client *i;
	if(showsystray)
		for(i = systray->icons; i; w += i->w + systrayspacing, i = i->next) ;
	if(w) {
		/* Add padding based on icon height scaling */
		w += (int)(bh * (1.0 - systrayiconheight));
	}
	return w ? w : 1;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING) {
		size_t len = MIN((size_t)name.nitems, (size_t)size - 1);
		memcpy(text, name.value, len);
		text[len] = '\0';
	} else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success) {
		if (n > 0 && list && *list)
			snprintf(text, size, "%s", *list);
		if (list)
			XFreeStringList(list);
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j, k;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
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
						XGrabKey(dpy, k,
							 keys[i].mod | modifiers[j],
							 root, True,
							 GrabModeAsync, GrabModeAsync);
		XFree(syms);
	}
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--) {
		/* 检查是否有重叠 */
		int overlap_x = MAX(0, MIN(unique[n].x_org + unique[n].width, info->x_org + info->width) - 
		                    MAX(unique[n].x_org, info->x_org));
		int overlap_y = MAX(0, MIN(unique[n].y_org + unique[n].height, info->y_org + info->height) - 
		                    MAX(unique[n].y_org, info->y_org));
		
		/* 如果有重叠，则认为是同一个显示器区域 */
		if (overlap_x > 0 && overlap_y > 0)
			return 0;
	}
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	if (!modkey_enabled)
		return;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;

	if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0 , 0, 0)) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->mfact = mfactdefault;
	c->ignoreunmap = 0;

	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tagindex = t->tagindex;
	} else {
		c->mon = selmon;
		applyrules(c);
	}

	if (c->x + c->w > c->mon->wx + c->mon->ww)
		c->x = c->mon->wx + c->mon->ww - c->w;
	if (c->y + c->h > c->mon->wy + c->mon->wh)
		c->y = c->mon->wy + c->mon->wh - c->h;
	c->x = MAX(c->x, c->mon->wx);
	c->y = MAX(c->y, c->mon->wy);
	int scrollx = (c->mon && c->mon->scrollindex) ? c->mon->scrollindex->x : 0;
	c->floatx = c->x + scrollx;
	c->floaty = c->y;
	wc.border_width = 0;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed || c->h < (c->mon->wh * autofloatthreshold);

	if (c->isfloating)
		XRaiseWindow(dpy, c->win);
	attach(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(c->win), 1);

	/* Reparent client window into container */
	c->ignoreunmap = 2;
	XReparentWindow(dpy, c->win, c->mon->container, c->x - c->mon->wx, c->y - c->mon->wy);
	XMoveResizeWindow(dpy, c->win, c->x - c->mon->wx, c->y - c->mon->wy, c->w, c->h);
	c->ignoreunmap = 0;

	setclientstate(c, NormalState);
	int visible = ISVISIBLE(c);
	if (visible && c->mon == selmon)
		unfocus(selmon->sel, 0);
	if (visible)
		c->mon->sel = c;
	arrange(c->mon);
	if (c->isfloating && c->mon && c->mon->scrollindex)
		reorderbyx(c->mon->scrollindex);
	XMapWindow(dpy, c->win);
	applyroundedcorners(c->win);
	if (visible && c->mon == selmon && !c->isfullscreen)
		ensureclientvisible(c, c->w, 50);
	if (visible && c->mon == selmon) {
		focus(c);
		restack(selmon);
	} else {
		drawbar(c->mon);
	}
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
mapnotify(XEvent *e)
{
	XMapEvent *ev = &e->xmap;
	if (!shape_supported || ev->event != root)
		return;

	Window w = ev->window;
	if (w == borderwin || wintoclient(w) || wintosystrayicon(w))
		return;

	XWindowAttributes wa;
	if (!XGetWindowAttributes(dpy, w, &wa))
		return;

	setroundedfromattrs(w, &wa);
}

void
maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	Client *i;
	if ((i = wintosystrayicon(ev->window))) {
		sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
		resizebarwin(selmon);
		updatesystray();
	}

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;

	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		if (showsystray)
			updatesystray();
		focus(NULL);
	}
	mon = m;
}

void
movemouse(const Arg *arg)
{
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
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / refreshrate))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + c->w)) < snap)
				nx = selmon->wx + selmon->ww - c->w;
			if (abs(selmon->wy - ny) < snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + c->h)) < snap)
				ny = selmon->wy + selmon->wh - c->h;
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
			&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
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
	if (c->isfloating && c->mon && c->mon->scrollindex)
		reorderbyx(c->mon->scrollindex);
}



void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((c = wintosystrayicon(ev->window))) {
		if (ev->atom == XA_WM_NORMAL_HINTS) {
			updatesizehints(c);
			updatesystrayicongeom(c, c->w, c->h);
		}
		else
			updatesystrayiconstate(c, ev);
		resizebarwin(selmon);
		updatesystray();
	}

	if (ev->window == root) {
		if (ev->atom == XA_WM_NAME)
			updatestatus();
		else if (ev->atom == xatom[XRootPmap] || ev->atom == xatom[XSetRoot])
			synccontainerbgs();
	} else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
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
				if (c == c->mon->sel && ISVISIBLE(c))
					drawbar(c->mon);
			}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
quit(const Arg *arg)
{
	running = 0;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
	}
	return r;
}

static void
reorderbyx(Scroll *s)
{
	if (!s || !s->head)
		return;

	Monitor *m = s->head->mon;
	if (!m)
		return;

	Client *sorted = NULL;
	for (Client *c = s->head; c; ) {
		Client *next = c->next;
		XWindowAttributes wa;
		int cx = c->x;
		if (XGetWindowAttributes(dpy, c->win, &wa))
			cx = wa.x;
		Client **pp = &sorted;
		while (*pp) {
			int px = (*pp)->x;
			if (XGetWindowAttributes(dpy, (*pp)->win, &wa))
				px = wa.x;
			if (px > cx)
				break;
			pp = &(*pp)->next;
		}
		c->next = *pp;
		*pp = c;
		c = next;
	}
	s->head = sorted;
	if (m)
		drawbar(m);
}

void
removesystrayicon(Client *i)
{
	Client **ii;

	if (!showsystray || !i)
		return;
	for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next);
	if (ii)
		*ii = i->next;
	free(i);
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact)) {
		resizeclient(c, x, y, w, h);
	}
}

void
resizebarwin(Monitor *m) {
	unsigned int w = m->ww;
	if (showsystray && m == systraytomon(m))
		w -= getsystraywidth();
	XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, w, bh);
	applyroundedcorners(m->barwin);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	
	/* translate to container coordinates */
	wc.x = x - c->mon->wx;
	wc.y = y - c->mon->wy;
	int scrollx = (c->mon && c->mon->scrollindex) ? c->mon->scrollindex->x : 0;
	c->floatx = x + scrollx;
	c->floaty = y;

	wc.border_width = 0;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	applyroundedcorners(c->win);
	if (c == selmon->sel)
		updateborderwin();
}

void
resizerequest(XEvent *e)
{
	XResizeRequestEvent *ev = &e->xresizerequest;
	Client *i;

	if ((i = wintosystrayicon(ev->window))) {
		updatesystrayicongeom(i, ev->width, ev->height);
		resizebarwin(selmon);
		updatesystray();
	}
}

void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;
	int drawnborder = 0;
	int rw = 0, rh = 0;
	GC gc;
	XGCValues gcv;

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
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w - 1, c->h - 1);
	gcv.function = GXxor;
	gcv.foreground = scheme[SchemeSel][ColBorder].pixel;
	gcv.line_width = 1;
	gcv.subwindow_mode = IncludeInferiors;
	gc = XCreateGC(dpy, root, GCFunction|GCForeground|GCLineWidth|GCSubwindowMode, &gcv);
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / refreshrate))
				continue;
			while (XCheckTypedEvent(dpy, MotionNotify, &ev))
				;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx + 1, 1);
			nh = MAX(ev.xmotion.y - ocy + 1, 1);
			if (nw == c->w && nh == c->h)
				break;
			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (drawnborder)
				XDrawRectangle(dpy, root, gc, c->x, c->y, rw - 1, rh - 1);
			rw = nw;
			rh = nh;
			XDrawRectangle(dpy, root, gc, c->x, c->y, rw - 1, rh - 1);
			XFlush(dpy);
			drawnborder = 1;
			break;
		}
	} while (ev.type != ButtonRelease);
	if (drawnborder)
		XDrawRectangle(dpy, root, gc, c->x, c->y, rw - 1, rh - 1);
	XFreeGC(dpy, gc);
	if (drawnborder && (!selmon->lt[selmon->sellt]->arrange || c->isfloating))
		resize(c, c->x, c->y, rw, rh, 1);
	XSync(dpy, False);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w - 1, c->h - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;
	Client **floats = NULL;
	int fcount = 0;
	int i;
	int usearrange;

	drawbar(m);
	if (!m->sel)
		return;

	usearrange = m->lt[m->sellt]->arrange != NULL;

	if (usearrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}

	for (c = m->stack; c; c = c->snext)
		if ((c->isfloating || !usearrange) && ISVISIBLE(c))
			fcount++;
	if (fcount) {
		floats = ecalloc(fcount, sizeof(Client *));
		for (i = 0, c = m->stack; c; c = c->snext)
			if ((c->isfloating || !usearrange) && ISVISIBLE(c))
				floats[i++] = c;

		for (i = fcount - 1; i >= 0; i--)
			XRaiseWindow(dpy, floats[i]->win);

		free(floats);
	}

	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
run(void)
{
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
}

void
runautostart(void)
{
	char *home, *path;

	if (!autostartscript || !*autostartscript)
		return;

	if (!(home = getenv("HOME")))
		return;

	/* Expand ~ to home directory */
	if (autostartscript[0] == '~') {
		size_t pathlen = strlen(home) + strlen(autostartscript);
		path = ecalloc(1, pathlen);
		snprintf(path, pathlen, "%s%s", home, autostartscript + 1);
	} else {
		path = strdup(autostartscript);
		if (!path)
			return;
	}

	/* Run script in background */
	if (access(path, X_OK) == 0) {
		if (fork() == 0) {
			execl(path, path, (char *)NULL);
			_exit(1);
		}
	}

	free(path);
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	
	/* 保存旧的 scroll x 坐标用于调整 floatx */
	int old_scrollx = (c->mon->scrollindex) ? c->mon->scrollindex->x : 0;
	
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	// 设置新monitor的scrollindex对应的索引
	for (int i = 0; i < LENGTH(tags); i++) {
		if (m->scrollindex == &m->scrolls[i]) {
			c->tagindex = i;
			break;
		}
	}
	
	/* 对于 floating 窗口，调整 floatx 以适应新的 monitor 和 scroll */
	if (c->isfloating && m->scrollindex) {
		int new_scrollx = m->scrollindex->x;
		/* 调整 floatx：减去旧 scroll x，加上新 scroll x */
		c->floatx = c->floatx - old_scrollx + new_scrollx;
	}
	
	/* reparent client into target monitor container */
	c->ignoreunmap = 2;
	XReparentWindow(dpy, c->win, m->container, c->x - m->wx, c->y - m->wy);
	attach(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
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
	}
	else {
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

void
setfocus(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *) &(c->win), 1);
	}
	sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldstate = c->isfloating;
		c->isfloating = 1;
		
		/* For fullscreen, make window cover entire monitor */
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen){
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
		c->isfloating = c->oldstate;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

void
setlayout(const Arg *arg)
{
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = (Layout *)arg->v;
	snprintf(selmon->ltsymbol, sizeof selmon->ltsymbol, "%s", selmon->lt[selmon->sellt]->symbol);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

void
setup(void)
{
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
	while (waitpid(-1, NULL, WNOHANG) > 0);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h / 2;
	bh = drw->fonts->h + 10;
	
	/* 创建状态栏绘制上下文 */
	statusdrw = drw_create(dpy, screen, root, sw, bh);
	if (!drw_fontset_create(statusdrw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded for status.");
	
	/* 初始化状态栏 */
	initstatusbar();
	
	/* 启动状态栏线程 */
	if (pthread_create(&drawstatusthread, NULL, drawstatusbar, NULL) != 0) {
		die("failed to create status thread");
	}
	status_thread_started = 1;
	initshape();
	initcompositor();

	/* init preview mode from config */
	previewmode = previewmode_default;
	
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
	netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
	netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
netatom[NetWMWindowTypeDesktop] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
	xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
	xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
	xatom[XRootPmap] = XInternAtom(dpy, "_XROOTPMAP_ID", False);
	xatom[XSetRoot] = XInternAtom(dpy, "_XSETROOT_ID", False);
	/* init cursors */
	/* Try to load from system cursor theme first, fallback to default cursors */
	if (!(cursor[CurNormal] = drw_cur_create_from_theme(drw, "left_ptr")))
		cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	if (!(cursor[CurResize] = drw_cur_create_from_theme(drw, "nwse-resize")))
		cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	if (!(cursor[CurMove] = drw_cur_create_from_theme(drw, "move")))
		cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 3);
	setupborderwin();
	/* init system tray */
	updatesystray();
	systandstat = getsystraywidth();
	systrayw = getsystraywidth();
	/* init bars */
	updatebars();
	synccontainerbgs();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
}

void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

static void
drawroundedmask(Pixmap mask, GC gc, int x, int y, int w, int h, int r, int val)
{
	if (w <= 0 || h <= 0)
		return;
	XSetForeground(dpy, gc, val);
	if (r <= 0) {
		XFillRectangle(dpy, mask, gc, x, y, w, h);
		return;
	}
	int d = 2 * r;
	XFillRectangle(dpy, mask, gc, x + r, y, w - d, h);
	XFillRectangle(dpy, mask, gc, x, y + r, w, h - d);
	XFillArc(dpy, mask, gc, x, y, d, d, 0, 23040);
	XFillArc(dpy, mask, gc, x + w - d, y, d, d, 0, 23040);
	XFillArc(dpy, mask, gc, x, y + h - d, d, d, 0, 23040);
	XFillArc(dpy, mask, gc, x + w - d, y + h - d, d, d, 0, 23040);
}

static void
setupborderwin(void)
{
	if (!shape_supported || borderwin != None)
		return;

	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixel = scheme[SchemeSel][ColBorder].pixel,
		.border_pixel = 0,
	};

	borderwin = XCreateWindow(dpy, root, 0, 0, 1, 1, 0,
		DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
		CWOverrideRedirect|CWBackPixel|CWBorderPixel, &wa);
	XMapRaised(dpy, borderwin);
	XShapeCombineMask(dpy, borderwin, ShapeInput, 0, 0, None, ShapeSet);
	XUnmapWindow(dpy, borderwin);
}

static void
updateborderwin(void)
{
	if (!shape_supported)
		return;
	if (borderwin == None)
		setupborderwin();
	if (borderwin == None)
		return;

	Client *c = selmon ? selmon->sel : NULL;
	if (!c || !ISVISIBLE(c) || c->isfullscreen) {
		XUnmapWindow(dpy, borderwin);
		return;
	}

	int w = c->w;
	int h = c->h;
	int r = clampcornerradius(cornerradius, w, h);
	int t = (int)borderpx;
	if (w <= 0 || h <= 0 || t <= 0) {
		XUnmapWindow(dpy, borderwin);
		return;
	}

	XMoveResizeWindow(dpy, borderwin, c->x, c->y, w, h);
	XSetWindowBackground(dpy, borderwin, scheme[SchemeSel][ColBorder].pixel);
	XClearWindow(dpy, borderwin);

	Pixmap mask = XCreatePixmap(dpy, borderwin, w, h, 1);
	if (!mask) {
		XMapRaised(dpy, borderwin);
		return;
	}

	GC gc = XCreateGC(dpy, mask, 0, NULL);
	if (!gc) {
		XFreePixmap(dpy, mask);
		XMapRaised(dpy, borderwin);
		return;
	}

	XSetForeground(dpy, gc, 0);
	XFillRectangle(dpy, mask, gc, 0, 0, w, h);
	drawroundedmask(mask, gc, 0, 0, w, h, r, 1);

	int innerw = w - 2 * t;
	int innerh = h - 2 * t;
	if (innerw > 0 && innerh > 0) {
		int innerr = clampcornerradius(r - t, innerw, innerh);
		drawroundedmask(mask, gc, t, t, innerw, innerh, innerr, 0);
	}

	XShapeCombineMask(dpy, borderwin, ShapeBounding, 0, 0, mask, ShapeSet);
	XShapeCombineMask(dpy, borderwin, ShapeClip, 0, 0, mask, ShapeSet);
	XFreeGC(dpy, gc);
	XFreePixmap(dpy, mask);

	XMapRaised(dpy, borderwin);
}

static int
clampcornerradius(int r, int w, int h)
{
	if (r <= 0 || w <= 0 || h <= 0)
		return 0;
	if (r * 2 > w)
		r = w / 2;
	if (r * 2 > h)
		r = h / 2;
	return r;
}

static void
clearwindowshape(Window win)
{
	if (!shape_supported || !win)
		return;
	XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, None, ShapeSet);
	XShapeCombineMask(dpy, win, ShapeClip, 0, 0, None, ShapeSet);
}

static int
setwindowrounded(Window win, int w, int h, int r)
{
	if (!shape_supported || !win)
		return 0;

	r = clampcornerradius(r, w, h);
	if (r <= 0 || w <= 0 || h <= 0) {
		clearwindowshape(win);
		return 0;
	}

	Pixmap mask = XCreatePixmap(dpy, win, w, h, 1);
	if (!mask)
		return 0;

	GC gc = XCreateGC(dpy, mask, 0, NULL);
	if (!gc) {
		XFreePixmap(dpy, mask);
		return 0;
	}

	XSetForeground(dpy, gc, 0);
	XFillRectangle(dpy, mask, gc, 0, 0, w, h);
	XSetForeground(dpy, gc, 1);

	int d = 2 * r;
	XFillRectangle(dpy, mask, gc, r, 0, w - d, h);
	XFillRectangle(dpy, mask, gc, 0, r, w, h - d);
	XFillArc(dpy, mask, gc, 0, 0, d, d, 0, 23040);
	XFillArc(dpy, mask, gc, w - d, 0, d, d, 0, 23040);
	XFillArc(dpy, mask, gc, 0, h - d, d, d, 0, 23040);
	XFillArc(dpy, mask, gc, w - d, h - d, d, d, 0, 23040);

	XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
	XShapeCombineMask(dpy, win, ShapeClip, 0, 0, mask, ShapeSet);

	XFreeGC(dpy, gc);
	XFreePixmap(dpy, mask);
	return 1;
}

static int
getcornerradius(Window win, XWindowAttributes *wa)
{
	const char *class = NULL, *instance = NULL, *title = NULL;
	unsigned int i;
	const CornerRule *r;
	XClassHint ch = { NULL, NULL };

	/* 默认使用全局圆角半径 */
	int radius = cornerradius;

	/* 获取窗口类和实例名 */
	if (XGetClassHint(dpy, win, &ch)) {
		class = ch.res_class ? ch.res_class : broken;
		instance = ch.res_name ? ch.res_name : broken;
	}

	/* 获取窗口标题 */
	char *winname = NULL;
	if (XFetchName(dpy, win, &winname) && winname)
		title = winname;

	/* 匹配圆角规则 */
	for (i = 0; i < LENGTH(cornerrules); i++) {
		r = &cornerrules[i];
		if ((r->override_redirect == -1 || r->override_redirect == wa->override_redirect)
		&& (!r->title || (title && strstr(title, r->title)))
		&& (!r->class || (class && strstr(class, r->class)))
		&& (!r->instance || (instance && strstr(instance, r->instance))))
		{
			radius = r->radius;
			break;
		}
	}

	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	if (winname)
		XFree(winname);

	return radius;
}

static void
applyroundedcorners(Window win)
{
	XWindowAttributes wa;

	if (!shape_supported || !win)
		return;
	if (!XGetWindowAttributes(dpy, win, &wa))
		return;

	int outerw = wa.width + 2 * wa.border_width;
	int outerh = wa.height + 2 * wa.border_width;
	int radius = getcornerradius(win, &wa);

	setwindowrounded(win, outerw, outerh, radius);
}

static void
setroundedfromattrs(Window win, XWindowAttributes *wa)
{
	if (!wa || !shape_supported || !win)
		return;

	for (Monitor *m = mons; m; m = m->next)
		if (win == m->container)
			return;

	int outerw = wa->width + 2 * wa->border_width;
	int outerh = wa->height + 2 * wa->border_width;
	int radius = getcornerradius(win, wa);

	setwindowrounded(win, outerw, outerh, radius);
}

void
showhide(Monitor *m)
{
	for (int i = 0; i < LENGTH(tags); i++) {
		Scroll *s = &m->scrolls[i];
		int inview = (m->scrollindex == s);

		for (Client *c = s->head; c; c = c->next) {
			if (inview) {
				XMoveWindow(dpy, c->win, c->x - m->wx, c->y - m->wy);
				if (!m->lt[m->sellt]->arrange && !c->isfullscreen)
					resize(c, c->x, c->y, c->w, c->h, 0);
			} else {
				XMoveWindow(dpy, c->win, c->w * -2, c->y);
			}
		}
	}
}

void
spawn(const Arg *arg)
{
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

void
tag(const Arg *arg)
{
	if (!selmon->sel || arg->i < 0 || arg->i >= LENGTH(tags))
		return;

	Client *c = selmon->sel;

	/* 保存旧的 scroll x 坐标用于调整 floatx */
	int old_scrollx = (selmon->scrollindex) ? selmon->scrollindex->x : 0;
	int old_tagindex = c->tagindex;

	detach(selmon->sel);
	selmon->sel->tagindex = arg->i;
	attach(selmon->sel);

	/* switch view to the new tag */
	Arg v = { .i = arg->i };
	view(&v);

	/* 对于 floating 窗口，调整 floatx 以适应新的 scroll x 坐标 */
	if (c->isfloating && selmon->scrollindex && old_tagindex != arg->i) {
		int new_scrollx = selmon->scrollindex->x;
		/* 调整 floatx：减去旧 scroll x，加上新 scroll x */
		c->floatx = c->floatx - old_scrollx + new_scrollx;
		
		/* 更新窗口位置 */
		resizeclient(c, c->floatx - selmon->scrollindex->x, c->floaty, c->w, c->h);
		
		reorderbyx(selmon->scrollindex);
	}
}

void
setdefaultfont(void)
{
	drw_setfontset(drw, drw_fontset_create(drw, fonts, LENGTH(fonts)));
}



void
setsmallfont(void)
{
	static Fnt *smallfont = NULL;
	
	if (!smallfont) {
		smallfont = drw_fontset_create(drw, &fonts[1], 1);
		if (!smallfont) return;
	}
	
	drw_setfontset(drw, smallfont);
}

void
setstatusdefaultfont(void)
{
	if (statusdrw) {
		drw_setfontset(statusdrw, drw_fontset_create(statusdrw, fonts, LENGTH(fonts)));
	}
}

void
setstatussmallfont(void)
{
	static Fnt *status_smallfont = NULL;
	
	if (!status_smallfont && statusdrw) {
		status_smallfont = drw_fontset_create(statusdrw, &fonts[1], 1);
	}
	
	if (statusdrw && status_smallfont) {
		drw_setfontset(statusdrw, status_smallfont);
	}
}

static void
initshape(void)
{
	int evb = 0, erb = 0;
	shape_supported = XShapeQueryExtension(dpy, &evb, &erb);
}

static void
initcompositor(void)
{
	composite_supported = enableoffscreen;
	if (!enableoffscreen)
		return;
	int evb = 0, erb = 0;
	int major = 0, minor = 4;
	if (!(XCompositeQueryExtension(dpy, &evb, &erb)
		&& XCompositeQueryVersion(dpy, &major, &minor)))
		composite_supported = 0;
}

static void
redirectmonitor(Monitor *m)
{
	if (!composite_supported || !m)
		return;
	XCompositeRedirectSubwindows(dpy, m->container, CompositeRedirectAutomatic);
}

void
setcontainertitle(Monitor *m)
{
	if (!m || !m->container)
		return;

	char title[64];
	snprintf(title, sizeof(title), "dwm-container-%d", m->num);
	XStoreName(dpy, m->container, title);
}

void
synccontainerbg(Monitor *m)
{
	Atom actual = None;
	int format = 0;
	unsigned long n = 0, extra = 0;
	unsigned char *data = NULL;
	Pixmap pm = None;

	if (netatom[NetWMWindowTypeDesktop])
		XChangeProperty(dpy, m->container, netatom[NetWMWindowType], XA_ATOM, 32,
			PropModeReplace, (unsigned char *)&netatom[NetWMWindowTypeDesktop], 1);

	if (XGetWindowProperty(dpy, root, xatom[XRootPmap], 0, 1, False,
		XA_PIXMAP, &actual, &format, &n, &extra, &data) == Success && data) {
		if (actual == XA_PIXMAP && format == 32 && n == 1)
			pm = *(Pixmap *)data;
		XFree(data);
	}
	if (pm != None) {
		XSetWindowBackgroundPixmap(dpy, m->container, pm);
		XChangeProperty(dpy, m->container, xatom[XRootPmap], XA_PIXMAP, 32,
			PropModeReplace, (unsigned char *)&pm, 1);
		XClearWindow(dpy, m->container);
	}
}

void
synccontainerbgs(void)
{
	for (Monitor *m = mons; m; m = m->next)
		synccontainerbg(m);
}

void
drawsupericon(Monitor *m, int *x)
{
	supericonw = TEXTW(supericon);
	drw_setscheme(drw, (modkey_enabled) ? scheme[SchemeNorm] : scheme[SchemeSel]);
	drw_text(drw, *x, 0, supericonw, bh, lrpad, supericon, 0);
	*x += supericonw;
}

void
drawlogotitle(Monitor *m, int *x)
{
	drw_setscheme(drw, scheme[SchemeNorm]);

	if (m->sel) {
		const char *winclass;
		XClassHint ch = {NULL, NULL};
		XGetClassHint(dpy, m->sel->win, &ch);
		winclass = ch.res_class ? ch.res_class : broken;
		m->logotitlew = TEXTW(winclass) + lrpad;
		drw_text(drw, *x, 0, m->logotitlew, bh, lrpad, winclass, 0);
		if (ch.res_class)
			XFree(ch.res_class);
		if (ch.res_name)
			XFree(ch.res_name);
	} else {
		m->logotitlew = TEXTW(logotext) + lrpad;
		drw_text(drw, *x, 0, m->logotitlew, bh, lrpad, logotext, 0);
	}
	*x += m->logotitlew;
}

void
drawtags(Monitor *m, int *x)
{
	unsigned int i;
	int w, tagstop = 3, tagslpad = 2;
	Client *c;
	int hasclients, hasurgent;
	
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
		
		/* 检查这个scroll是否有客户端和紧急窗口 */
		hasclients = 0;
		hasurgent = 0;
		for (c = m->scrolls[i].head; c; c = c->next) {
			hasclients = 1;
			if (c->isurgent)
				hasurgent = 1;
		}
		
		/* 检查是否是当前选中的scroll */
		if (m->scrollindex == &m->scrolls[i])
			drw_setscheme(drw, scheme[SchemeSel]);
		else
			drw_setscheme(drw, scheme[SchemeNorm]);
			
		drw_text(drw, *x, 0, w, bh, lrpad / 2, tags[i], hasurgent);
		if (hasclients)
			drw_rect(drw, *x + tagslpad, tagstop, w - tagslpad * 2, 1, 1, 0);
		*x += w;
	}
}

void
drawlayout(Monitor *m, int *x)
{
	int w = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[SchemeNorm]);
	*x = drw_text(drw, *x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);
}

void
drawclienttabs(Monitor *m, int x, int w, int n)
{
	Client *c;
	int scm, remainder = w % n;
	int tabw = (1.0 / (double)n) * w + 1;
	
	setsmallfont();
	for (c = m->scrollindex->head; c; c = c->next) {
		scm = (m->sel == c) ? SchemeSel : SchemeNorm;
		drw_setscheme(drw, scheme[scm]);

		if (remainder >= 0) {
			if (remainder == 0)
				tabw--;
			remainder--;
		}
		
		int titletextw = TEXTW(c->name);
		int offset = (tabw - titletextw) / 2;
		if (offset >= 0) {
			drw_rect(drw, x, 0, offset, bh, 1, 1);
			drw_text(drw, x + offset, 0, tabw - offset, bh, lrpad / 2, c->name, 0);
			if (c->isfloating)
				drw_rect(drw, x + offset, 0, titletextw, 2, 1, 0);
		} else {
			int padding = 5;
			drw_text(drw, x, 0, tabw, bh, lrpad / 2, c->name, 0);
			if (c->isfloating)
				drw_rect(drw, x + padding, 0, tabw - 2 * padding, 2, 1, 0);
		}
		x += tabw;
	}
	setdefaultfont();
}

void
togglebar(const Arg *arg)
{
	selmon->showbar = !selmon->showbar;
	updatebarpos(selmon);
	resizebarwin(selmon);
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

void
togglefloating(const Arg *arg)
{
	Client *c = selmon->sel;
	if (!c)
		return;
	if (c->isfullscreen) /* no support for fullscreen windows */
		return;
	c->isfloating = !c->isfloating || c->isfixed;
	if (c->isfloating)
		resize(c, c->x, c->y, c->w, c->h, 0);
	arrange(selmon);
	if (c->isfloating && c->mon && c->mon->scrollindex)
		reorderbyx(c->mon->scrollindex);
	ensureclientvisible(c, c->w, 50);
	focus(c);
	restack(c->mon);
}

void
ensureselectedvisible(const Arg *arg)
{
	(void)arg;
	if (!selmon || !selmon->sel)
		return;
  Client *c = selmon->sel;
	ensureclientvisible(c, c->w, 50);
  focus(c);
  restack(selmon);
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	updateborderwin();
}

void
unmanage(Client *c, int destroyed)
{
	Monitor *m = c->mon;
	XWindowChanges wc;

	detach(c);
	detachstack(c);
	if (!destroyed) {
		wc.border_width = 0;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XSelectInput(dpy, c->win, NoEventMask);
		
		/* Reparent client window back to root and restore original border */
		XReparentWindow(dpy, c->win, root, c->x, c->y);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);

		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);
}

void
unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (c->ignoreunmap) {
			c->ignoreunmap--;
			return;
		}
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	}
	else if ((c = wintosystrayicon(ev->window))) {
		/* KLUDGE! sometimes icons occasionally unmap their windows, but do
		 * _not_ destroy them. We map those windows back */
		XMapRaised(dpy, c->win);
		updatesystray();
	}
}

void
updatebars(void)
{
	unsigned int w;
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"dwm", "dwm"};
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		w = m->ww;
		if (showsystray && m == systraytomon(m))
			w -= getsystraywidth();
		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, w, bh, 0, DefaultDepth(dpy, screen),
				CopyFromParent, DefaultVisual(dpy, screen),
				CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		if (showsystray && m == systraytomon(m))
			XMapRaised(dpy, systray->win);
		XMapRaised(dpy, m->barwin);
		applyroundedcorners(m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
	}
}

void
updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh -= bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + bh : m->wy;
	} else
		m->by = -bh;

	/* keep container aligned to window area */
	XMoveResizeWindow(dpy, m->container, m->wx, m->wy, m->ww, m->wh);
}

void
updateclientlist(void)
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (int i = 0; i < LENGTH(tags); i++)
			for (c = m->scrolls[i].head; c; c = c->next)
				XChangeProperty(dpy, root, netatom[NetClientList],
					XA_WINDOW, 32, PropModeAppend,
					(unsigned char *) &(c->win), 1);
}

int
updategeom(void)
{
	int dirty = 0;
	int existing = 0;
	int targetcount = 0;
	Monitor *m;
	Client *c;
	MonitorArea *areas = NULL;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, nn;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = ecalloc(nn, sizeof(XineramaScreenInfo));

		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);

		/* 如果原始显示器数量大于唯一显示器数量，说明有重叠 */
		if (nn > j) {
			/* 合并所有重叠的显示器为一个大的屏幕区域 */
			free(unique);
			targetcount = 1;
			areas = ecalloc(1, sizeof(MonitorArea));
			areas[0].x = 0;
			areas[0].y = 0;
			areas[0].w = sw;
			areas[0].h = sh;
		} else {
			targetcount = j;
			areas = ecalloc(targetcount, sizeof(MonitorArea));
			for (i = 0; i < targetcount; i++) {
				areas[i].x = unique[i].x_org;
				areas[i].y = unique[i].y_org;
				areas[i].w = unique[i].width;
				areas[i].h = unique[i].height;
			}
			free(unique);
		}
	}
#endif /* XINERAMA */

	if (!areas || targetcount == 0) {
		free(areas);
		targetcount = 1;
		areas = ecalloc(1, sizeof(MonitorArea));
		areas[0].x = 0;
		areas[0].y = 0;
		areas[0].w = sw;
		areas[0].h = sh;
	}

	for (existing = 0, m = mons; m; m = m->next, existing++);

	for (int i = existing; i < targetcount; i++) {
		for (m = mons; m && m->next; m = m->next);
		if (m)
			m->next = createmon();
		else
			mons = createmon();
	}

	int index = 0;
	for (m = mons; index < targetcount && m; m = m->next, index++) {
		if (index >= existing
		|| areas[index].x != m->mx || areas[index].y != m->my
		|| areas[index].w != m->mw || areas[index].h != m->mh)
		{
			dirty = 1;
			m->num = index;
			m->mx = m->wx = areas[index].x;
			m->my = m->wy = areas[index].y;
			m->mw = m->ww = areas[index].w;
			m->mh = m->wh = areas[index].h;
			updatebarpos(m);
		} else {
			m->num = index;
		}
		setcontainertitle(m);
	}

	while (existing > targetcount) {
		Monitor *last = mons;
		Monitor *prev = NULL;
		for (; last && last->next; prev = last, last = last->next);
		if (!last)
			break;

		for (int t = 0; t < LENGTH(tags); t++) {
			while ((c = last->scrolls[t].head)) {
				dirty = 1;
				last->scrolls[t].head = c->next;
				detachstack(c);
				
				/* 保存旧的 scroll x 坐标用于调整 floatx */
				int old_scrollx = (last->scrollindex) ? last->scrollindex->x : 0;
				
				c->mon = mons;
				
				/* 对于 floating 窗口，调整 floatx 以适应新的 monitor */
				if (c->isfloating && mons->scrollindex) {
					int new_scrollx = mons->scrollindex->x;
					/* 调整 floatx：减去旧 scroll x，加上新 scroll x */
					c->floatx = c->floatx - old_scrollx + new_scrollx;
				}
				
				/* 重父化窗口到新的container */
				c->ignoreunmap = 2;
				XReparentWindow(dpy, c->win, mons->container, 
				                c->x - mons->wx, c->y - mons->wy);
				attach(c);
				attachstack(c);
			}
		}

		if (last == selmon)
			selmon = mons;
		if (prev)
			prev->next = NULL;
		cleanupmon(last);
		existing--;
	}

	free(areas);

	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
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

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		snprintf(stext, sizeof(stext), "dwm-"VERSION);
	drawbar(selmon);
	updatesystray();
}


void
updatesystrayicongeom(Client *i, int w, int h)
{
	if (i) {
		i->h = (int)(bh * systrayiconheight);
		if (w == h)
			i->w = (int)(bh * systrayiconheight);
		else if (h == bh)
			i->w = w;
		else
			i->w = (int) ((float)(bh * systrayiconheight) * ((float)w / (float)h));
		applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
		/* force icons into the systray dimensions if they don't want to */
		if (i->h > (int)(bh * systrayiconheight)) {
			if (i->w == i->h)
				i->w = (int)(bh * systrayiconheight);
			else
				i->w = (int) ((float)(bh * systrayiconheight) * ((float)i->w / (float)i->h));
			i->h = (int)(bh * systrayiconheight);
		}
		/* center icon vertically */
		i->y = (bh - i->h) / 2;
	}
}

void
updatesystrayiconstate(Client *i, XPropertyEvent *ev)
{
	long flags;
	int code = 0;

	if (!showsystray || !i || ev->atom != xatom[XembedInfo] ||
			!(flags = getatomprop(i, xatom[XembedInfo])))
		return;

	if (flags & XEMBED_MAPPED && i->tagindex == -1) {
		i->tagindex = 0;
		code = XEMBED_WINDOW_ACTIVATE;
		XMapRaised(dpy, i->win);
		setclientstate(i, NormalState);
	}
	else if (!(flags & XEMBED_MAPPED) && i->tagindex != -1) {
		i->tagindex = -1;
		code = XEMBED_WINDOW_DEACTIVATE;
		XUnmapWindow(dpy, i->win);
		setclientstate(i, WithdrawnState);
	}
	else
		return;
	sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
			systray->win, XEMBED_EMBEDDED_VERSION);
}

void
updatesystray(void)
{
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
		systray->win = XCreateSimpleWindow(dpy, root, x, m->by, w, bh, 0, 0, scheme[SchemeSel][ColBg].pixel);
		wa.event_mask        = ButtonPressMask | ExposureMask;
		wa.override_redirect = True;
		wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
		XSelectInput(dpy, systray->win, SubstructureNotifyMask);
		XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *)&netatom[NetSystemTrayOrientationHorz], 1);
		XChangeWindowAttributes(dpy, systray->win, CWEventMask|CWOverrideRedirect|CWBackPixel, &wa);
		XMapRaised(dpy, systray->win);
		XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
		if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
			sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
			XSync(dpy, False);
		}
		else {
			fprintf(stderr, "dwm: unable to obtain system tray.\n");
			free(systray);
			systray = NULL;
			return;
		}
	}
	/* Calculate padding based on icon height scaling */
	int padding = (int)(bh * (1.0 - systrayiconheight) / 2);
	w = padding;
	for (i = systray->icons; i; i = i->next) {
		/* make sure the background color stays the same */
		wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
		XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
		XMapRaised(dpy, i->win);
		i->x = w;
		/* center icon vertically */
		i->y = (bh - i->h) / 2;
		XMoveResizeWindow(dpy, i->win, i->x, i->y, i->w, i->h);
		w += i->w;
		if (i->next)
			w += systrayspacing;  /* Only add spacing between icons */
		if (i->mon != m)
			i->mon = m;
	}
	/* Add padding at the end */
	if (w > padding)
		w += padding;
	else
		w = 1;
	x -= w;
	XMoveResizeWindow(dpy, systray->win, x, m->by, w, bh);
	wc.x = x; wc.y = m->by; wc.width = w; wc.height = bh;
	wc.stack_mode = Above; wc.sibling = m->barwin;
	XConfigureWindow(dpy, systray->win, CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &wc);
	XMapWindow(dpy, systray->win);
	XMapSubwindows(dpy, systray->win);
	/* redraw background */
	XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBg].pixel);
	XFillRectangle(dpy, systray->win, drw->gc, 0, 0, w, bh);
	XSync(dpy, False);
	/* now that tray moved, resize bars so drawbar maps correct width */
	for (Monitor *mm = mons; mm; mm = mm->next)
		resizebarwin(mm);
}

void
updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		snprintf(c->name, sizeof(c->name), "%s", broken);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
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

void
view(const Arg *arg)
{
	/* 如果目标tag与当前tag相同，直接返回，不更新prevtag */
	if (arg->i >= 0 && arg->i < LENGTH(tags) && selmon->scrollindex == &selmon->scrolls[arg->i])
		return;
	
	/* 保存当前tag作为"上一个tag"（只有在真正切换时才更新） */
	if (selmon->scrollindex) {
		for (int i = 0; i < LENGTH(tags); i++) {
			if (selmon->scrollindex == &selmon->scrolls[i]) {
				selmon->prevtag = i;
				break;
			}
		}
	}
	
	if (arg->i >= 0 && arg->i < LENGTH(tags))
		selmon->scrollindex = &selmon->scrolls[arg->i];
	focus(NULL);
	arrange(selmon);
}

void
viewlast(const Arg *arg)
{
	if (selmon->prevtag == -1)
		return; /* 没有上一个tag */
	
	/* 切换到上一个tag */
	Arg lastarg = { .i = selmon->prevtag };
	view(&lastarg);
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (int i = 0; i < LENGTH(tags); i++)
			for (c = m->scrolls[i].head; c; c = c->next)
				if (c->win == w)
					return c;
	return NULL;
}

Client *
wintosystrayicon(Window w) {
	Client *i = NULL;

	if (!showsystray || !w)
		return i;
	for (i = systray->icons; i && i->win != w; i = i->next) ;
	return i;
}

Monitor *
wintomon(Window w)
{
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
void
sendnotify(const char *msg, const char *urgency, int timeout)
{
	char timeout_str[16];
	snprintf(timeout_str, sizeof(timeout_str), "%d", timeout);

	const char *notify[] = {"dunstify", "-u", urgency, "-t", timeout_str, msg, NULL};
	const Arg v = {.v = notify};
	spawn(&v);
}

int
xerror(Display *dpy, XErrorEvent *ee)
{
	/* 忽略常见的无害错误 */
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;

	/* 对于其他错误，使用 dunstify 通知而不是直接退出 */
	char msg[512];
	snprintf(msg, sizeof(msg), "dwm: X error - request=%d, error=%d, resource=0x%lx",
	         ee->request_code, ee->error_code, ee->resourceid);
	fprintf(stderr, "%s\n", msg);

	/* 使用 dunstify 发送通知 */
	sendnotify(msg, "critical", 5000);

	/* 不调用 xerrorxlib，避免 dwm 退出 */
	return 0;
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

Monitor *
systraytomon(Monitor *m) {
	Monitor *t;
	int i, n;
	if(!systraypinning) {
		if(!m)
			return selmon;
		return m == selmon ? m : NULL;
	}
	for(n = 1, t = mons; t && t->next; n++, t = t->next) ;
	for(i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next) ;
	if(systraypinningfailfirst && n < systraypinning)
		return mons;
	return t;
}

void
togglesupericon(const Arg *arg)
{
	supericonflag = !supericonflag;
	modkey_enabled = !supericonflag;
	drawbars();
}



void
scroll(Monitor *m)
{
	Client *c;
	int n = 0;
	int x, w;
	int h = 0, y = 0;

	if (!m->scrollindex)
		return;

	/* Place floating clients relative to scroll offset (ignore fullscreen) */
	for (c = m->scrollindex->head; c; c = c->next) {
		if (!c->isfloating || c->isfullscreen)
			continue;
		resizeclient(c, c->floatx - m->scrollindex->x, c->floaty, c->w, c->h);
	}

	/* Count clients in the current scrollindex's scroll list */
	for (c = m->scrollindex->head; c; c = c->next)
		if (!c->isfloating)
			n++;
	
	if (n == 0)
		return;

	/* single client special handling */
	if (n == 1) {
		for (c = m->scrollindex->head; c; c = c->next) {
			if (c->isfloating)
				continue;

			if (m->scrollindex->singlefill) {
				x = m->wx + gappx;
				w = m->ww - 2 * gappx;
				if (w < 100)
					w = 100;
				h = m->wh - 2 * scrollstartgap;
				y = m->wy + scrollstartgap;
			} else {
				int topgap = 30;
				int bottomgap = 60;
				int minw = 100;

				h = m->wh - topgap - bottomgap;
				if (h < 100)
					h = 100;
				if (h > m->wh - 2 * scrollstartgap)
					h = m->wh - 2 * scrollstartgap;

				w = (h * 3) / 2; /* enforce 3:2 aspect */
				if (w > m->ww - 2 * gappx)
					w = m->ww - 2 * gappx;
				if (w < minw)
					w = minw;

				y = m->wy + topgap;
				x = m->wx + (m->ww - w) / 2;
			}
			resizeclient(c, x, y, w, h);
			return;
		}
	}

	x = m->wx - m->scrollindex->x + scrollstartgap;
	for (c = m->scrollindex->head; c; c = c->next) {
		if (c->isfloating)
			continue;

		/* multi-client layout: height fills window area, width from per-client mfact */
		h = m->wh - 2 * scrollstartgap;
		if (h < bh)
			h = bh;

		int layoutw = m->ww * c->mfact;
		if (layoutw < 100)
			layoutw = 100;
		if (layoutw > m->ww - 2 * gappx)
			layoutw = m->ww - 2 * gappx;

		y = m->wy + scrollstartgap;
		resizeclient(c, x, y, layoutw, h);
		x += layoutw + gappx;
	}
}



void
setmfact(const Arg *arg)
{
	float delta = arg->f;
	Client *c = selmon->sel;

	if (!c)
		return;

	c->mfact += delta;
	if (c->mfact < 0.05) c->mfact = 0.05;
	if (c->mfact > 0.95) c->mfact = 0.95;

	arrange(selmon);
}

void
scrollmove(const Arg *arg)
{
	Scroll *s = selmon->scrollindex;
	if (!s)
		return;

	s->x += arg->i;
	if (s->x < 0)
		s->x = 0;

	scroll(selmon);
}

void
scrollmoveothers(const Arg *arg)
{
	if (!selmon || !selmon->scrollindex || !selmon->sel)
		return;
	int offset = arg->i;
	if (offset == 0)
		return;

	Scroll *s = selmon->scrollindex;
	Client *sel = selmon->sel;
	int dy = 50;

	if (!sel->isfloating) {
		togglefloating(&(Arg){0});
		resizeclient(sel, sel->x, sel->y + dy, sel->w, sel->h);
	}

	s->x += offset;
	sel->floatx += offset;
	if (s->x < 0) {
		sel->floatx -= s->x;
		s->x = 0;
	}

	scroll(selmon);
	reorderbyx(s);
}

void
scrolltogglesize(const Arg *arg)
{
	/* If not in scroll layout, keep original setlayout behavior */
	if (selmon->lt[selmon->sellt]->arrange != scroll) {
		setlayout(arg);
		return;
	}

	int n = 0;
	Client *c;
	for (c = selmon->scrollindex ? selmon->scrollindex->head : NULL; c; c = c->next)
		if (!c->isfloating)
			n++;

	if (n > 1 && selmon->sel && !selmon->sel->isfloating) {
		/* Align with setmfact clamp upper bound; allow tiny rounding error */
		const float target = 0.95f;
		const float eps = 0.0001f;

		if (selmon->sel->mfact > target - eps)
			selmon->sel->mfact = mfactdefault;
		else
			selmon->sel->mfact = target;
	} else {
		Scroll *s = selmon->scrollindex;
		if (s)
			s->singlefill = !s->singlefill;
	}
	scroll(selmon);
}

/* status bar functions */
int
getstatuswidth(void) 
{
  int width = 0;
  for (int i = 0; i < LENGTH(Blocks); i++) {
    width += Blocks[i].bw;
  }
  return width ? width : 1;  /* Ensure minimum width of 1 */
}

static void
spawnclickcmd(const char *const cmd[])
{
  const Arg v = {.v = cmd};
  spawn(&v);
}

void
clicktemp(const Arg *arg)
{
  if (arg->i == 1) {
    if (thermalzoneindex < thermalzonenum) {
      thermalzoneindex++;
    } else {
      thermalzoneindex = 0;
    }
    char thermal_str[30];
    snprintf(thermal_str, sizeof(thermal_str), "Thermal Zone: %d", thermalzoneindex);
    sendnotify(thermal_str, "normal", 3000);
  }
}

void
clickmore(const Arg *arg)
{
  if (arg->i == 1)
    spawnclickcmd(script_menu);
}

void
clickmem(const Arg *arg)
{
  if (arg->i == 1)
    spawnclickcmd(sys_monitor);
}

void
clicknet(const Arg *arg)
{
  if (arg->i == 1) {
    if (interfaceindex < LENGTH(interface_name) - 1) {
      interfaceindex++;
    } else {
      interfaceindex = 0;
    }
    char thermal_str[64];
    snprintf(thermal_str, sizeof(thermal_str), "Interface: %s", interface_name[interfaceindex]);
    sendnotify(thermal_str, "normal", 3000);
  }
}

void
clickcpu(const Arg *arg)
{
  if (arg->i == 1) {
    spawnclickcmd(dec_volume);
  } else if (arg->i == 2) {
    spawnclickcmd(tog_volume);
  } else if (arg->i == 3) {
    spawnclickcmd(inc_volume);
  } else if (arg->i == 4) {
    spawnclickcmd(inc_volume_1);
  } else if (arg->i == 5) {
    spawnclickcmd(dec_volume_1);
  }
}

void
clickcores(const Arg *arg)
{
  if (arg->i == 1) {
    spawnclickcmd(dec_light);
  } else if (arg->i == 3) {
    spawnclickcmd(inc_light);
  } else if (arg->i == 4) {
    spawnclickcmd(inc_light_1);
  } else if (arg->i == 5) {
    spawnclickcmd(dec_light_1);
  }
}

void
clicknotify(const Arg *arg)
{
  if (arg->i == 1) {
    for (int i = 0; i < 5; i++) {
      spawnclickcmd(history_pop);
    }
  } else if (arg->i == 3) {
    spawnclickcmd(history_clear);
  } else if (arg->i == 4) {
    spawnclickcmd(history_pop);
  } else if (arg->i == 5) {
    spawnclickcmd(history_close);
  }
}

int
drawstatusclock(int x, Block *block, unsigned int timer)
{
  time_t currentTime = time(NULL);
  struct tm *tm = localtime(&currentTime);
  if (!tm) {
    block->bw = 0;
    return x;
  }
  int hour = tm->tm_hour;
  int minute = tm->tm_min;
  char *meridiem = (hour < 12) ? "AM" : "PM";
  char stext[20];

  if (hour == 0) {
    hour = 12;
  } else if (hour > 12) {
    hour -= 12;
  }

  snprintf(stext, sizeof(stext), "%02d:%02d-%s", hour, minute, meridiem);
  block->bw = TEXTWSTATUS(stext);
  x -= block->bw;
  drw_text(statusdrw, x, 0, block->bw, bh, lrpad, stext, 0);
  return x;
}

int
drawnotify(int x, Block *block, unsigned int timer)
{
  char tag[] = " ";
  block->bw = TEXTWSTATUS(tag);
  x -= block->bw;
  drw_text(statusdrw, x, 0, block->bw, bh, lrpad, tag, 0);
  return x;
}

int
drawmore(int x, Block *block, unsigned int timer)
{
  char tag[] = "󰍻 ";
  block->bw = TEXTWSTATUS(tag);
  x -= block->bw;
  drw_text(statusdrw, x, 0, block->bw, bh, lrpad * 3 / 4, tag, 0);
  return x;
}

int
drawcores(int x, Block *block, unsigned int timer)
{
  FILE *fp;
  CoreBlock *storage = block->storage;
  fp = fopen("/proc/stat", "r");
  if (fp == NULL) {
    block->bw = 0;
    return x;
  }
  char line[256];
  if (!fgets(line, sizeof(line), fp)) {
    fclose(fp);
    block->bw = 0;
    return x;
  }
  for (int i = 0; i < numCores; i++) {
    if (!fgets(line, sizeof(line), fp)) {
      fclose(fp);
      block->bw = 0;
      return x;
    }
    if (sscanf(line, "cpu%*d %lu %lu %lu %lu", &storage->curr[i].user,
           &storage->curr[i].nice, &storage->curr[i].system,
           &storage->curr[i].idle) != 4) {
      fclose(fp);
      block->bw = 0;
      return x;
    }
  }
  fclose(fp);

  unsigned long ua_arr[numCores];
  unsigned long sy_arr[numCores];
  for (int i = 0; i < numCores; i++) {
    unsigned long user_diff = storage->curr[i].user - storage->prev[i].user;
    unsigned long nice_diff = storage->curr[i].nice - storage->prev[i].nice;
    unsigned long system_diff = storage->curr[i].system - storage->prev[i].system;
    unsigned long idle_diff = storage->curr[i].idle - storage->prev[i].idle;
    unsigned long total_diff = user_diff + nice_diff + system_diff + idle_diff;

    if (total_diff == 0) {
      ua_arr[i] = 0;
      sy_arr[i] = 0;
    } else {
      ua_arr[i] = (user_diff * 100) / total_diff;
      sy_arr[i] = (system_diff * 100) / total_diff;
    }

    storage->prev[i].user = storage->curr[i].user;
    storage->prev[i].nice = storage->curr[i].nice;
    storage->prev[i].system = storage->curr[i].system;
    storage->prev[i].idle = storage->curr[i].idle;
  }
  
  const int tpad = 2;
  const int border = 1;
  const int bar_w = 100;  // Same width as mem module
  const int cw = (bar_w - 2 * border) / numCores;
  const int w = bar_w;
  const int h = bh - 2 * tpad;

  drw_setscheme(statusdrw, scheme[SchemeSel]);
  drw_rect(statusdrw, x - w, tpad, w, h, 1, 1);

  x -= border;
  drw_setscheme(statusdrw, scheme[SchemeBlue]);
  for (int i = 0; i < numCores; i++) {
    x -= cw;
    const int ch = (h - 2 * border) * ua_arr[i] / 100;
    const int cy = h - ch + tpad - border;
    drw_rect(statusdrw, x, cy, cw, ch, 1, 0);
  }
  
  x = x + (cw * numCores);
  drw_setscheme(statusdrw, scheme[SchemeRed]);
  for (int i = 0; i < numCores; i++) {
    x -= cw;
    const int ch1 = (h - 2 * border) * ua_arr[i] / 100;
    const int cy1 = h - ch1 + tpad - border;
    const int ch2 = (h - 2 * border) * sy_arr[i] / 100;
    const int cy2 = cy1 - ch2;

    drw_rect(statusdrw, x, cy2, cw, ch2, 1, 0);
  }

  drw_setscheme(statusdrw, scheme[SchemeNorm]);
  x -= lrpad;
  block->bw = w + lrpad;
  return x;
}

int
drawcpu(int x, Block *block, unsigned int timer)
{
  FILE *fp;
  CpuBlock *storage = block->storage;
  fp = fopen("/proc/stat", "r");
  if (fp == NULL) {
    block->bw = 0;
    return x;
  }

  if (fscanf(fp, "cpu %lu %lu %lu %lu", &storage->curr->user, &storage->curr->nice,
         &storage->curr->system, &storage->curr->idle) != 4) {
    fclose(fp);
    block->bw = 0;
    return x;
  }
  fclose(fp);

  unsigned long user_diff = storage->curr->user - storage->prev->user;
  unsigned long nice_diff = storage->curr->nice - storage->prev->nice;
  unsigned long system_diff = storage->curr->system - storage->prev->system;
  unsigned long idle_diff = storage->curr->idle - storage->prev->idle;
  unsigned long total_diff = user_diff + nice_diff + system_diff + idle_diff;

  unsigned long user_usage;
  unsigned long system_usage;
  if (total_diff == 0) {
    user_usage = 0;
    system_usage = 0;
  } else {
    user_usage = (user_diff * 100) / total_diff;
    system_usage = (system_diff * 100) / total_diff;
  }

  storage->pointer = storage->pointer->prev;
  storage->pointer->data->user = user_usage;
  storage->pointer->data->system = system_usage;

  storage->prev->user = storage->curr->user;
  storage->prev->nice = storage->curr->nice;
  storage->prev->system = storage->curr->system;
  storage->prev->idle = storage->curr->idle;

  const int cw = 1;
  const int w = cw * NODE_NUM + 2;
  const int y = 2;
  const int h = bh - 2 * y;
  const int ch = bh - 2 * y - 2;

  drw_setscheme(statusdrw, scheme[SchemeSel]);
  drw_rect(statusdrw, x - w, y, w, h, 1, 1);

  drw_setscheme(statusdrw, scheme[SchemeBlue]);
  x -= 1;
  for (int i = 0; i < NODE_NUM; i++) {
    x -= cw;
    const int ch1 = ch * storage->pointer->data->user / 100;
    if (ch1 == 0 || storage->pointer->data->user > 100) {
      storage->pointer = storage->pointer->next;
      continue;
    }
    const int cy = ch - ch1 + y + 1;
    drw_rect(statusdrw, x, cy, cw, ch1, 1, 0);
    storage->pointer = storage->pointer->next;
  }
  
  x = x + (cw * NODE_NUM);
  drw_setscheme(statusdrw, scheme[SchemeRed]);
  for (int i = 0; i < NODE_NUM; i++) {
    x -= cw;
    const int ch2 = ch * storage->pointer->data->system / 100;
    if (ch2 == 0 || storage->pointer->data->system > 100) {
      storage->pointer = storage->pointer->next;
      continue;
    }
    const int ch1 = ch * storage->pointer->data->user / 100;
    const int cy = ch - ch1 + y + 1;
    const int cy1 = cy - ch2;
    drw_rect(statusdrw, x, cy1, cw, ch2, 1, 0);
    storage->pointer = storage->pointer->next;
  }

  drw_setscheme(statusdrw, scheme[SchemeNorm]);
  x -= lrpad;
  block->bw = w + lrpad;
  return x;
}

int
drawtemp(int x, Block *block, unsigned int timer)
{
  static char temp[18] = "--°C";

  if (timer % 5 == 0) {
        char temp_addr[40];
        snprintf(temp_addr, sizeof(temp_addr), "/sys/class/thermal/thermal_zone%d/temp",
                 thermalzoneindex);
        FILE *fp = fopen(temp_addr, "r");
        if (fp == NULL) {
          block->bw = 0;
          return x;
        }
        int tmp;
        if (fscanf(fp, "%d", &tmp) != 1) {
          fclose(fp);
          return x;  // Keep previous value on error
        }
        fclose(fp);
        tmp = tmp / 1000;
        snprintf(temp, sizeof(temp), "%d°C", tmp);
      }

  block->bw = TEXTWSTATUS(temp);
  x -= block->bw;
  drw_text(statusdrw, x, 0, block->bw, bh, 0, temp, 0);

  return x;
}

int
drawmem(int x, Block *block, unsigned int timer)
{
    static long mem_total = 0, mem_free = 0, mem_active = 0, mem_inactive = 0, mem_cached = 0;

    if (timer % 2 == 0) { // Update every 2 seconds
        char line[256];
        FILE *fp = fopen("/proc/meminfo", "r");
        if (fp == NULL) {
            perror("fopen /proc/meminfo");
            return x;
        }
        
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "MemTotal: %ld kB", &mem_total) == 1) continue;
            if (sscanf(line, "MemFree: %ld kB", &mem_free) == 1) continue;
            if (sscanf(line, "Active: %ld kB", &mem_active) == 1) continue;
            if (sscanf(line, "Inactive: %ld kB", &mem_inactive) == 1) continue;
            if (sscanf(line, "Cached: %ld kB", &mem_cached) == 1) continue;
        }
        fclose(fp);
    }

    if (mem_total == 0) { // Avoid division by zero on first run
        
        return x;
    }
    
    

    // Drawing logic
    const int bar_w = 100;
    const int bar_h = bh - 6;
    const int bar_x = x - bar_w;
    const int bar_y = (bh - bar_h) / 2;
    int current_x = bar_x;

    float free_perc = (float)mem_free / mem_total;
    float active_perc = (float)mem_active / mem_total;
    float inactive_perc = (float)mem_inactive / mem_total;

    int free_w = free_perc * bar_w;
    int active_w = active_perc * bar_w;
    int inactive_w = inactive_perc * bar_w;

    // Green: Free
    drw_setscheme(statusdrw, scheme[SchemeGreen]);
    drw_rect(statusdrw, current_x, bar_y, free_w, bar_h, 1, 1);
    current_x += free_w;

    // Yellow: Active
    drw_setscheme(statusdrw, scheme[SchemeOrange]); // Assuming SchemeOrange is yellow
    drw_rect(statusdrw, current_x, bar_y, active_w, bar_h, 1, 1);
    current_x += active_w;

    // Blue: Inactive
    drw_setscheme(statusdrw, scheme[SchemeBlue]);
    drw_rect(statusdrw, current_x, bar_y, inactive_w, bar_h, 1, 1);
    current_x += inactive_w;

    // Red: Unaccounted (includes cached, etc.)
    drw_setscheme(statusdrw, scheme[SchemeRed]);
    drw_rect(statusdrw, current_x, bar_y, bar_w - (current_x - bar_x), bar_h, 1, 1);
    
    // Draw a border around the whole bar
    drw_setscheme(statusdrw, scheme[SchemeFG]);
    drw_rect(statusdrw, bar_x, bar_y, bar_w, bar_h, 0, 1);

    x -= (bar_w + lrpad);
    block->bw = bar_w + lrpad;

    drw_setscheme(statusdrw, scheme[SchemeNorm]);
    
    return x;
}

int
drawnet(int x, Block *block, unsigned int timer)
{
  char rx[20], tx[20];
  char txpath[50];
  char rxpath[50];
  int null_width = 15;
  snprintf(txpath, sizeof(txpath), "/sys/class/net/%s/statistics/tx_bytes",
           interface_name[interfaceindex]);
  snprintf(rxpath, sizeof(rxpath), "/sys/class/net/%s/statistics/rx_bytes",
           interface_name[interfaceindex]);

  FILE *fp = fopen(txpath, "r");
  if (fp == NULL) {
    x -= null_width;
    block->bw = null_width;
    return x;
  }
  if (fscanf(fp, "%19s", tx) != 1) {
    fclose(fp);
    x -= null_width;
    block->bw = null_width;
    return x;
  }
  fclose(fp);
  fp = fopen(rxpath, "r");
  if (fp == NULL) {
    x -= null_width;
    block->bw = null_width;
    return x;
  }
  if (fscanf(fp, "%19s", rx) != 1) {
    fclose(fp);
    x -= null_width;
    block->bw = null_width;
    return x;
  }
  fclose(fp);

  float txi = atof(tx);
  float rxi = atof(rx);
  float txi_tmp = txi;
  float rxi_tmp = rxi;
  float *f_arr = block->storage;

  txi = txi - f_arr[0];
  rxi = rxi - f_arr[1];

  f_arr[0] = txi_tmp;
  f_arr[1] = rxi_tmp;

  if (txi < 1000) {
    snprintf(tx, sizeof(tx), "%.2f B/s", txi);
  } else if (txi < 1000 * 1000) {
    snprintf(tx, sizeof(tx), "%.2f KB/s", txi / 1000);
  } else if (txi < 1000 * 1000 * 1000) {
    snprintf(tx, sizeof(tx), "%.2f MB/s", txi / 1000 / 1000);
  } else {
    snprintf(tx, sizeof(tx), "%.2f GB/s", txi / 1000 / 1000 / 1000);
  }

  if (rxi < 1000) {
    snprintf(rx, sizeof(rx), "%.2f B/s", rxi);
  } else if (rxi < 1000 * 1000) {
    snprintf(rx, sizeof(rx), "%.2f KB/s", rxi / 1000);
  } else if (rxi < 1000 * 1000 * 1000) {
    snprintf(rx, sizeof(rx), "%.2f MB/s", rxi / 1000 / 1000);
  } else {
    snprintf(rx, sizeof(rx), "%.2f GB/s", rxi / 1000 / 1000 / 1000);
  }

  setstatussmallfont();
  unsigned int font_w, font_h;
  drw_font_getexts(statusdrw->fonts, "M", 1, &font_w, &font_h);
  
  int line_spacing = -4;
  int total_text_height = font_h * 2 + line_spacing;
  int start_y = (bh - total_text_height) / 2;
  
  drw_text(statusdrw, x - TEXTWSTATUS(tx), start_y, TEXTWSTATUS(tx), font_h, lrpad, tx, 0);
  drw_text(statusdrw, x - TEXTWSTATUS(rx), start_y + font_h + line_spacing, TEXTWSTATUS(rx), font_h, lrpad, rx, 0);
  x -= TEXTWSTATUS("999.99 KB/s");
  block->bw = TEXTWSTATUS("999.99 KB/s");

  setstatusdefaultfont();

  return x;
}

int
drawbattery(int x, Block *block, unsigned int timer)
{
  static char bat_perc[5] = "??";
  static char bat_status[20] = "Unknown";

  if (timer % 10 == 0) {
    char capacity[4];
    char status[20];
    char capacitypatch[] = "/sys/class/power_supply/BAT0/capacity";
    char statuspatch[] = "/sys/class/power_supply/BAT0/status";

    // read capacity and status
    FILE *fp = fopen(capacitypatch, "r");
    if (fp == NULL) {
      block->bw = 0;
      return x;
    }
    if (fscanf(fp, "%3s", capacity) != 1) {
      fclose(fp);
      
      return x;  // Keep previous values on error
    }
    fclose(fp);
    fp = fopen(statuspatch, "r");
    if (fp == NULL) {
      block->bw = 0;
      return x;
    }
    if (fscanf(fp, "%19s", status) != 1) {
      fclose(fp);
      
      return x;  // Keep previous values on error
    }
    fclose(fp);
    snprintf(bat_perc, sizeof(bat_perc), "%s", capacity);
    snprintf(bat_status, sizeof(bat_status), "%s", status);
    
  }

  int int_cap = atoi(bat_perc);

  char battery_text[8];
  snprintf(battery_text, sizeof(battery_text), "%s%%", bat_perc);
  int text_width = TEXTWSTATUS(battery_text);
  
  const int border = 1;
  const int battery_h = statusdrw->fonts->h - 6;
  const int battery_w = battery_h * 2;
  const int battery_x = x - battery_w - 5;
  const int battery_y = (bh - battery_h) / 2;
  const int text_x = battery_x - text_width - 3;

  drw_setscheme(statusdrw, scheme[SchemeFG]);
  drw_rect(statusdrw, battery_x, battery_y, battery_w, battery_h, 0, 1);
  drw_rect(statusdrw, battery_x + battery_w, battery_y + 4, 2, battery_h - 8, 1, 1);

  if (bat_status[0] == 'C' || bat_status[0] == 'F') {
    drw_setscheme(statusdrw, scheme[SchemeGreen]);
  } else if (int_cap <= 15) {
    drw_setscheme(statusdrw, scheme[SchemeRed]);
  } else if (int_cap <= 30) {
    drw_setscheme(statusdrw, scheme[SchemeOrange]);
  } else if (int_cap <= 60) {
    drw_setscheme(statusdrw, scheme[SchemeYellow]);
  } else {
    drw_setscheme(statusdrw, scheme[SchemeBlue]);
  }

  int drawable_w = battery_w - 2 * border;
  int num_segments = (int_cap + 9) / 10;
  if (int_cap > 0 && num_segments == 0)
    num_segments = 1;
  int battery_cap_w = num_segments * drawable_w / 10;
  drw_rect(statusdrw, battery_x + border, battery_y + border, battery_cap_w,
           battery_h - 2 * border, 1, 1);

  drw_setscheme(statusdrw, scheme[SchemeNorm]);
  drw_text(statusdrw, text_x, 0, text_width, bh, lrpad, battery_text, 0);

  block->bw = text_width + 3 + battery_w + 5;
  x -= block->bw;
  
  

  return x;

}

int
gettempnums(void)
{
  const char *path = "/sys/class/thermal";
  DIR *dir;
  struct dirent *entry;
  int max_number = -1;

  dir = opendir(path);
  if (dir == NULL) {
    return -1;
  }

  while ((entry = readdir(dir)) != NULL) {
    if (strncmp(entry->d_name, "thermal_zone", 12) == 0) {
      int number = -1;
      sscanf(entry->d_name, "thermal_zone%d", &number);
      if (number > max_number) {
        max_number = number;
      }
    }
  }

  closedir(dir);
  return max_number;
}

void
initstatusbar(void)
{
  long online = sysconf(_SC_NPROCESSORS_ONLN);
  numCores = (online > 0) ? (int)online : 1;
  
  for (int i = 0; i < NODE_NUM; i++) {
    Nodes[i].next = &Nodes[(i + 1) % NODE_NUM];
    Nodes[i].prev = &Nodes[(i + NODE_NUM - 1) % NODE_NUM];
    Nodes[i].data = calloc(1, sizeof(Cpuload));
    if (!Nodes[i].data)
      die("fatal: could not malloc() %u bytes\n", (unsigned int)sizeof(Cpuload));
  }
  storagecpu.pointer = &Nodes[0];
  storagecpu.prev = calloc(1, sizeof(Cpuload));
  storagecpu.curr = calloc(1, sizeof(Cpuload));
  storagecores.curr = calloc((size_t)numCores, sizeof(Cpuload));
  storagecores.prev = calloc((size_t)numCores, sizeof(Cpuload));
  if (!storagecpu.prev || !storagecpu.curr || !storagecores.curr || !storagecores.prev)
    die("fatal: could not allocate status bar cpu buffers\n");

  thermalzonenum = gettempnums();
}

void
cleanstatuspthread(void)
{
  if (status_thread_started) {
    running = 0;
    pthread_join(drawstatusthread, NULL);
    status_thread_started = 0;
  }

  /* Free memory */
  if (storagecores.curr) {
    free(storagecores.curr);
    storagecores.curr = NULL;
  }
  if (storagecores.prev) {
    free(storagecores.prev);
    storagecores.prev = NULL;
  }

  if (storagecpu.curr) {
    free(storagecpu.curr);
    storagecpu.curr = NULL;
  }
  if (storagecpu.prev) {
    free(storagecpu.prev);
    storagecpu.prev = NULL;
  }

  /* Free all NODE_NUM nodes */
  for (int i = 0; i < NODE_NUM; i++) {
    if (Nodes[i].data) {
      free(Nodes[i].data);
      Nodes[i].data = NULL;
    }
  }
}

void
updatestatuscache(void)
{
  if (!selmon)
    return;

  pthread_mutex_lock(&statuscache_mutex);

  /* Update global dimensions */
  systrayw = getsystraywidth();
  systandstat = getstatuswidth() + systrayw;

  /* Recreate cache if size changed */
  if (statuscache == None || cachew != systandstat || cacheh != bh) {
    if (statuscache != None) {
      XFreePixmap(dpy, statuscache);
      statuscache = None;
    }
    cachevalid = 0;
    cachew = systandstat;
    cacheh = bh;
    statuscache = XCreatePixmap(dpy, root, cachew, cacheh, DefaultDepth(dpy, screen));
    if (statuscache == None) {
      pthread_mutex_unlock(&statuscache_mutex);
      return;
    }
  }

  /* Draw status to cache */
  drw_setscheme(statusdrw, scheme[SchemeNorm]);
  drw_rect(statusdrw, 0, 0, cachew, bh, 1, 1);

  int x = cachew - systrayw;
  for (int i = 0; i < LENGTH(Blocks); i++) {
    x = Blocks[i].draw(x, &Blocks[i], 0);
  }

  drw_map(statusdrw, statuscache, 0, 0, cachew, bh);
  cachevalid = 1;
  time(&lastupdate);
  pthread_mutex_unlock(&statuscache_mutex);
}

void
freestatuscache(void)
{
  pthread_mutex_lock(&statuscache_mutex);
  if (statuscache != None) {
    XFreePixmap(dpy, statuscache);
    statuscache = None;
  }
  cachevalid = 0;
  pthread_mutex_unlock(&statuscache_mutex);
}

void *
drawstatusbar(void *arg)
{
  (void)arg;
  time_t now;

  while (running) {
    if (selmon) {
      int needs_update = 0;
      time(&now);
      pthread_mutex_lock(&statuscache_mutex);
      needs_update = (!cachevalid || (now - lastupdate) >= 1);
      pthread_mutex_unlock(&statuscache_mutex);

      /* Only update cache if needed (invalid or 1s passed) */
      if (needs_update) {
        updatestatuscache();
      }
      
      /* Copy cache to barwin if available */
      pthread_mutex_lock(&statuscache_mutex);
      if (cachevalid && statuscache != None) {
        XCopyArea(dpy, statuscache, selmon->barwin, statusdrw->gc, 
                  0, 0, cachew, bh,
                  selmon->ww - cachew, 0);
      }
      pthread_mutex_unlock(&statuscache_mutex);
    }

    sleep(1);
  }
  return NULL;
}

void
handlestatusclick(const Arg *arg, int button)
{
  Arg a = {.i = button};
  if (Blocks[arg->i].click)
    Blocks[arg->i].click(&a);
}

void
handleStatus1(const Arg *arg)
{
  handlestatusclick(arg, 1);
}

void
handleStatus2(const Arg *arg)
{
  handlestatusclick(arg, 2);
}

void
handleStatus3(const Arg *arg)
{
  handlestatusclick(arg, 3);
}

void
handleStatus4(const Arg *arg)
{
  handlestatusclick(arg, 4);
}

void
handleStatus5(const Arg *arg)
{
  handlestatusclick(arg, 5);
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!XInitThreads())
		die("dwm: XInitThreads failed");
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
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
