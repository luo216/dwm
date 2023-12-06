#include <pthread.h>

enum {
  Notify,
  Battery,
  Clock,
  Net,
  Cpu,
  Cores,
  Temp,
}; /*status bar blocks*/

/* status bar block struct*/
typedef struct Block Block;
struct Block {
  int bw;
  void *storage;
  int (*draw)(int x, Block *block);
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

/* function declarations */
static void click_temp(const Arg *arg);
static void click_notify(const Arg *arg);
static void click_cpu(const Arg *arg);
static void *drawstatusbar();
static void clean_status_pthread();
static int draw_clock(int x, Block *block);
static int draw_net(int x, Block *block);
static int draw_battery(int x, Block *block);
static int draw_notify(int x, Block *block);
static int draw_cpu(int x, Block *block);
static int draw_cores(int x, Block *block);
static int draw_temp(int x, Block *block);
static int getstatuswidth();
static void handleStatus1(const Arg *arg);
static void handleStatus2(const Arg *arg);
static void handleStatus3(const Arg *arg);
static void handleStatus4(const Arg *arg);
static void handleStatus5(const Arg *arg);
static void init_statusbar();

/* variables */
static pthread_t draw_status_thread;
static Node Nodes[10];
static int numCores;
static CoreBlock storage_cores;
static CpuBlock storage_cpu;
static float storage_net[2] = {0, 0};
static Block Blocks[] = {
    [Notify] = {0, NULL, draw_notify, click_notify},
    [Battery] = {0, NULL, draw_battery, NULL},
    [Clock] = {0, NULL, draw_clock, NULL},
    [Net] = {0, &storage_net, draw_net, NULL},
    [Cpu] = {0, &storage_cpu, draw_cpu, click_cpu},
    [Cores] = {0, &storage_cores, draw_cores, click_cpu},
    [Temp] = {0, NULL, draw_temp, click_temp},
};
