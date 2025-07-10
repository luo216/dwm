#include <pthread.h>

/* macros */
#define NODE_NUM 90

/* External declaration of mutex for use by dwm.c */
extern pthread_mutex_t status_mutex;

/* enum */
enum {
  Notify,
  Battery,
  Clock,
  Net,
  Cpu,
  Cores,
  Temp,
  More,
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
static void click_more(const Arg *arg);
static void click_temp(const Arg *arg);
static void click_net(const Arg *arg);
static void click_notify(const Arg *arg);
static void click_cpu(const Arg *arg);
static void click_cores(const Arg *arg);
static void *drawstatusbar();
static void clean_status_pthread();
static int draw_clock(int x, Block *block);
static int draw_net(int x, Block *block);
static int draw_battery(int x, Block *block);
static int draw_notify(int x, Block *block);
static int draw_cpu(int x, Block *block);
static int draw_cores(int x, Block *block);
static int draw_temp(int x, Block *block);
static int draw_more(int x, Block *block);
static int getstatuswidth();
static void handleStatus1(const Arg *arg);
static void handleStatus2(const Arg *arg);
static void handleStatus3(const Arg *arg);
static void handleStatus4(const Arg *arg);
static void handleStatus5(const Arg *arg);
static int get_temp_nums();
static void init_statusbar();

/* variables */
static pthread_t draw_status_thread;
pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER; // Define mutex
static Node Nodes[NODE_NUM];
static int numCores;
static int cores_cwidth;
static int thermal_zone_index = 0;
static int thermal_zone_num = 0;
static int interface_index = 0;
static CoreBlock storage_cores;
static CpuBlock storage_cpu;
static float storage_net[2] = {0, 0};
static Block Blocks[] = {
    [Notify] = {0, NULL, draw_notify, click_notify},
    [Battery] = {0, NULL, draw_battery, NULL},
    [Clock] = {0, NULL, draw_clock, NULL},
    [Net] = {0, &storage_net, draw_net, click_net},
    [Cpu] = {0, &storage_cpu, draw_cpu, click_cpu},
    [Cores] = {0, &storage_cores, draw_cores, click_cores},
    [Temp] = {0, NULL, draw_temp, click_temp},
    [More] = {0, NULL, draw_more, click_more},
};
/* configuration, allows nested code to access above variables */
#include "config.h"

int getstatuswidth() {
  int width = 0;
  for (int i = 0; i < LENGTH(Blocks); i++) {
    width += Blocks[i].bw;
  }

  return width;
}

void click_temp(const Arg *arg) {
  if (arg->i == 1) {
    if (thermal_zone_index < thermal_zone_num) {
      thermal_zone_index++;
    } else {
      thermal_zone_index = 0;
    }
    char thermal_str[30];
    sprintf(thermal_str, "Thermal Zone: %d", thermal_zone_index);
    const char *notify[] = {"notify-send", thermal_str, NULL};
    const Arg v = {.v = notify};
    spawn(&v);
  }
}

void click_more(const Arg *arg) {
  if (arg->i == 1) {
    const Arg v = {.v = script_menu};
    spawn(&v);
  }
}

void click_net(const Arg *arg) {
  if (arg->i == 1) {
    if (interface_index < LENGTH(interface_name) - 1) {
      interface_index++;
    } else {
      interface_index = 0;
    }
    char thermal_str[30];
    sprintf(thermal_str, "Interface: %s", interface_name[interface_index]);
    const char *notify[] = {"notify-send", thermal_str, NULL};
    const Arg v = {.v = notify};
    spawn(&v);
  }
}

void click_cpu(const Arg *arg) {
  if (arg->i == 1) {
    const Arg v = {.v = dec_volume};
    spawn(&v);
  } else if (arg->i == 2) {
    const Arg v = {.v = tog_volume};
    spawn(&v);
  } else if (arg->i == 3) {
    const Arg v = {.v = inc_volume};
    spawn(&v);
  } else if (arg->i == 4) {
    const Arg v = {.v = inc_volume_1};
    spawn(&v);
  } else if (arg->i == 5) {
    const Arg v = {.v = dec_volume_1};
    spawn(&v);
  }
}

void click_cores(const Arg *arg) {
  if (arg->i == 1) {
    const Arg v = {.v = dec_light};
    spawn(&v);
  } else if (arg->i == 3) {
    const Arg v = {.v = inc_light};
    spawn(&v);
  } else if (arg->i == 4) {
    const Arg v = {.v = inc_light_1};
    spawn(&v);
  } else if (arg->i == 5) {
    const Arg v = {.v = dec_light_1};
    spawn(&v);
  }
}

void click_notify(const Arg *arg) {
  if (arg->i == 1) {
    const char *history_pop[] = {"dunstctl", "history-pop", NULL};
    const Arg v = {.v = history_pop};
    for (int i = 0; i < 5; i++) {
      spawn(&v);
    }
  } else if (arg->i == 3) {
    const char *history_clear[] = {"dunstctl", "history-clear", NULL};
    const Arg v = {.v = history_clear};
    spawn(&v);
  } else if (arg->i == 4) {
    const char *history_pop[] = {"dunstctl", "history-pop", NULL};
    const Arg v = {.v = history_pop};
    spawn(&v);
  } else if (arg->i == 5) {
    const char *history_close[] = {"dunstctl", "close", NULL};
    const Arg v = {.v = history_close};
    spawn(&v);

    if (fork() == 0) {
      if (dpy)
        close(ConnectionNumber(dpy));
      setsid();
      execvp(((char **)history_close)[0], (char **)history_close);
      die("dwm: execvp '%s' failed:", ((char **)history_close)[0]);
    }
  }
}

int draw_cores(int x, Block *block) {
  FILE *fp;
  CoreBlock *storage = block->storage;
  fp = fopen("/proc/stat", "r");
  if (fp == NULL) {
    printf("Failed to open /proc/stat\n");
    return 1;
  }
  // Read data line by line from fp
  char line[256];
  fgets(line, sizeof(line), fp);
  for (int i = 0; i < numCores; i++) {
    fgets(line, sizeof(line), fp);
    sscanf(line, "cpu%*d %lu %lu %lu %lu", &storage->curr[i].user,
           &storage->curr[i].nice, &storage->curr[i].system,
           &storage->curr[i].idle);
  }
  fclose(fp);

  unsigned long ua_arr[numCores];
  unsigned long sy_arr[numCores];
  for (int i = 0; i < numCores; i++) {
    unsigned long user_diff = storage->curr[i].user - storage->prev[i].user;
    unsigned long nice_diff = storage->curr[i].nice - storage->prev[i].nice;
    unsigned long system_diff =
        storage->curr[i].system - storage->prev[i].system;
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
  // draw cpu cores usage
  const int tpad = 2;
  const int border = 1;
  const int cw = cores_cwidth;
  const int w = cw * numCores + 2 * border;
  const int h = bh - 2 * tpad;

  drw_setscheme(drw, scheme[SchemeSel]);
  drw_rect(drw, x - w, tpad, w, h, 1, 1);

  // draw user usage
  x -= border;
  drw_setscheme(drw, scheme[SchemeBlue]);
  for (int i = 0; i < numCores; i++) {
    x -= cw;
    const int ch = (h - 2 * border) * ua_arr[i] / 100;
    const int cy = h - ch + tpad - border;
    drw_rect(drw, x, cy, cw, ch, 1, 0);
  }
  // draw system usage
  x = x + (cw * numCores);
  drw_setscheme(drw, scheme[SchemeRed]);
  for (int i = 0; i < numCores; i++) {
    x -= cw;
    const int ch1 = (h - 2 * border) * ua_arr[i] / 100;
    const int cy1 = h - ch1 + tpad - border;
    const int ch2 = (h - 2 * border) * sy_arr[i] / 100;
    const int cy2 = cy1 - ch2;

    drw_rect(drw, x, cy2, cw, ch2, 1, 0);
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  x -= lrpad;
  block->bw = w + lrpad;
  return x;
}

int draw_cpu(int x, Block *block) {
  FILE *fp;
  CpuBlock *storage = block->storage;
  fp = fopen("/proc/stat", "r");
  if (fp == NULL) {
    printf("Failed to open /proc/stat\n");
    return 1;
  }

  // Read cpu usage information
  fscanf(fp, "cpu %lu %lu %lu %lu", &storage->curr->user, &storage->curr->nice,
         &storage->curr->system, &storage->curr->idle);
  fclose(fp);

  // Calculate the cpu usage time difference
  unsigned long user_diff = storage->curr->user - storage->prev->user;
  unsigned long nice_diff = storage->curr->nice - storage->prev->nice;
  unsigned long system_diff = storage->curr->system - storage->prev->system;
  unsigned long idle_diff = storage->curr->idle - storage->prev->idle;
  unsigned long total_diff = user_diff + nice_diff + system_diff + idle_diff;

  // Calculate cpu usage (expressed as a percentage)
  unsigned long user_usage;
  unsigned long system_usage;
  if (total_diff == 0) {
    user_usage = 0;
    system_usage = 0;
  } else {
    user_usage = (user_diff * 100) / total_diff;
    system_usage = (system_diff * 100) / total_diff;
  }

  // Use the ring structure to record the CPU usage of the past ten times
  storage->pointer = storage->pointer->prev;
  storage->pointer->data->user = user_usage;
  storage->pointer->data->system = system_usage;

  // Update the cpu usage time of the previous second
  storage->prev->user = storage->curr->user;
  storage->prev->nice = storage->curr->nice;
  storage->prev->system = storage->curr->system;
  storage->prev->idle = storage->curr->idle;

  // Plot cpu usage
  const int cw = 1;
  const int w = cw * NODE_NUM + 2;
  const int y = 2;
  const int h = bh - 2 * y;
  const int ch = bh - 2 * y - 2;

  drw_setscheme(drw, scheme[SchemeSel]);
  drw_rect(drw, x - w, y, w, h, 1, 1);

  // Plot user cpu usage
  drw_setscheme(drw, scheme[SchemeBlue]);
  x -= 1;
  for (int i = 0; i < NODE_NUM; i++) {
    x -= cw;
    const int ch1 = ch * storage->pointer->data->user / 100;
    if (ch1 == 0 || storage->pointer->data->user > 100) {
      storage->pointer = storage->pointer->next;
      continue;
    }
    const int cy = ch - ch1 + y + 1;
    drw_rect(drw, x, cy, cw, ch1, 1, 0);
    storage->pointer = storage->pointer->next;
  }
  // Plot system cpu usage
  x = x + (cw * NODE_NUM);
  drw_setscheme(drw, scheme[SchemeRed]);
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
    drw_rect(drw, x, cy1, cw, ch2, 1, 0);
    storage->pointer = storage->pointer->next;
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  x -= lrpad;
  block->bw = w + lrpad;
  return x;
}

int draw_temp(int x, Block *block) {
  char temp[18];
  char temp_addr[40];
  sprintf(temp_addr, "/sys/class/thermal/thermal_zone%d/temp",
          thermal_zone_index);
  FILE *fp = fopen(temp_addr, "r");
  if (fp == NULL) {
    return x;
  }
  int tmp;
  fscanf(fp, "%d", &tmp);
  fclose(fp);
  tmp = tmp / 1000;
  sprintf(temp, "%d", tmp);
  block->bw = TEXTW(temp);
  x -= block->bw;
  drw_text(drw, x, 0, block->bw, bh, 0, temp, 0);

  return x;
}

int draw_notify(int x, Block *block) {
  char tag[] = " ";
  block->bw = TEXTW(tag);
  x -= block->bw;
  drw_text(drw, x, 0, block->bw, bh, lrpad, tag, 0);

  return x;
}

int draw_more(int x, Block *block) {
  char tag[] = "󰍻 ";
  block->bw = TEXTW(tag);
  x -= block->bw;
  drw_text(drw, x, 0, block->bw, bh, lrpad * 3 / 4, tag, 0);

  return x;
}

int draw_battery(int x, Block *block) {
  char capacity[4];
  int int_cap;
  char status[20];
  char capacitypatch[] = "/sys/class/power_supply/BAT0/capacity";
  char statuspatch[] = "/sys/class/power_supply/BAT0/status";

  // read capacity and status
  FILE *fp = fopen(capacitypatch, "r");
  if (fp == NULL) {
    return x;
  }
  fscanf(fp, "%s", capacity);
  fclose(fp);
  fp = fopen(statuspatch, "r");
  if (fp == NULL) {
    return x;
  }
  fscanf(fp, "%s", status);
  fclose(fp);
  int_cap = atoi(capacity);

  block->bw = TEXTW(capacity);
  x -= block->bw;
  drw_text(drw, x, 0, block->bw, bh, lrpad, capacity, 0);

  // Draw the battery case
  const int tpad = 4;
  const int border = 1;
  const int battery_sw = 6;
  const int battery_sh = 3;
  const int battery_w = 2 * battery_sw;
  const int battery_h = bh - battery_sh - 2 * tpad;
  x -= battery_w;
  drw_setscheme(drw, scheme[SchemeSel]);
  drw_rect(drw, x, tpad + battery_sh, battery_w, battery_h, 1, 1);
  drw_rect(drw, x + battery_sw / 2, tpad, battery_sw, battery_sh, 1, 1);

  if (status[0] == 'C' || status[0] == 'F') {
    drw_setscheme(drw, scheme[SchemeGreen]);
  } else if (int_cap >= 45) {
    drw_setscheme(drw, scheme[SchemeBlue]);
  } else if (int_cap > 20) {
    drw_setscheme(drw, scheme[SchemeOrange]);
  } else {
    drw_setscheme(drw, scheme[SchemeRed]);
  }

  const int battery_cap_x = x + border;
  int battery_cap_y = battery_sh + tpad;
  const int battery_cap_w = battery_w - 2 * border;
  int battery_cap_h = battery_h - 2 * border;
  battery_cap_h = battery_cap_h * int_cap / 100;
  battery_cap_y = battery_cap_y + (battery_h - border - battery_cap_h);
  drw_rect(drw, battery_cap_x, battery_cap_y, battery_cap_w, battery_cap_h, 1,
           0);
  x -= lrpad;
  block->bw += battery_w + lrpad;

  drw_setscheme(drw, scheme[SchemeNorm]);
  return x;
}

int draw_net(int x, Block *block) {
  char rx[20], tx[20];
  char txpath[50];
  char rxpath[50];
  int null_width = 15;
  sprintf(txpath, "/sys/class/net/%s/statistics/tx_bytes",
          interface_name[interface_index]);
  sprintf(rxpath, "/sys/class/net/%s/statistics/rx_bytes",
          interface_name[interface_index]);

  // read tx and rx
  FILE *fp = fopen(txpath, "r");
  if (fp == NULL) {
    x -= null_width;
    block->bw = null_width;
    return x;
  }
  fscanf(fp, "%s", tx);
  fclose(fp);
  fp = fopen(rxpath, "r");
  if (fp == NULL) {
    x -= null_width;
    block->bw = null_width;
    return x;
  }
  fscanf(fp, "%s", rx);
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

  // Format the values
  if (txi < 1000) {
    sprintf(tx, "%.2f B/s", txi);
  } else if (txi < 1000 * 1000) {
    sprintf(tx, "%.2f KB/s", txi / 1000);
  } else if (txi < 1000 * 1000 * 1000) {
    sprintf(tx, "%.2f MB/s", txi / 1000 / 1000);
  } else {
    sprintf(tx, "%.2f GB/s", txi / 1000 / 1000 / 1000);
  }

  if (rxi < 1000) {
    sprintf(rx, "%.2f B/s", rxi);
  } else if (rxi < 1000 * 1000) {
    sprintf(rx, "%.2f KB/s", rxi / 1000);
  } else if (rxi < 1000 * 1000 * 1000) {
    sprintf(rx, "%.2f MB/s", rxi / 1000 / 1000);
  } else {
    sprintf(rx, "%.2f GB/s", rxi / 1000 / 1000 / 1000);
  }

  drw_setfontset(drw, smallfont);

  drw_text(drw, x - TEXTW(tx), 4, TEXTW(tx), bh / 2, lrpad, tx, 0);
  drw_text(drw, x - TEXTW(rx), bh / 2, TEXTW(rx), bh / 2 - 4, lrpad, rx, 0);
  x -= TEXTW("999.99 KB/s");
  block->bw = TEXTW("999.99 KB/s");

  drw_setfontset(drw, normalfont);

  return x;
}

int draw_clock(int x, Block *block) {
  time_t currentTime = time(NULL);
  struct tm *tm = localtime(&currentTime);
  int hour = tm->tm_hour;
  int minute = tm->tm_min;
  char *meridiem = (hour < 12) ? "AM" : "PM";
  char stext[20];

  if (hour == 0) {
    hour = 12;
  } else if (hour > 12) {
    hour -= 12;
  }

  sprintf(stext, "%02d:%02d-%s", hour, minute, meridiem);
  block->bw = TEXTW(stext);
  x -= block->bw;
  drw_text(drw, x, 0, block->bw, bh, lrpad, stext, 0);
  return x;
}

void handleStatus1(const Arg *arg) {
  Arg a = {.i = 1};
  if (Blocks[arg->i].click) {
    Blocks[arg->i].click(&a);
  }
}
void handleStatus2(const Arg *arg) {
  Arg a = {.i = 2};
  if (Blocks[arg->i].click) {
    Blocks[arg->i].click(&a);
  }
}
void handleStatus3(const Arg *arg) {
  Arg a = {.i = 3};
  if (Blocks[arg->i].click) {
    Blocks[arg->i].click(&a);
  }
}
void handleStatus4(const Arg *arg) {
  Arg a = {.i = 4};
  if (Blocks[arg->i].click) {
    Blocks[arg->i].click(&a);
  }
}
void handleStatus5(const Arg *arg) {
  Arg a = {.i = 5};
  if (Blocks[arg->i].click) {
    Blocks[arg->i].click(&a);
  }
}

int get_temp_nums() {
  const char *path = "/sys/class/thermal";
  DIR *dir;
  struct dirent *entry;
  int max_number = -1;

  // Open directory
  dir = opendir(path);
  if (dir == NULL) {
    perror("opendir");
    return -1;
  }

  // Read directory entry
  while ((entry = readdir(dir)) != NULL) {
    // Check out whether the name of the entry starts with \ "thermal zone \"
    if (strncmp(entry->d_name, "thermal_zone", 12) == 0) {
      // Extract the number and update max number
      int number = -1;
      sscanf(entry->d_name, "thermal_zone%d", &number);
      if (number > max_number) {
        max_number = number;
      }
    }
  }

  // Close the directory
  closedir(dir);
  return max_number;
}

void init_statusbar() {
  normalfont = drw->fonts;
  smallfont = drw->fonts->next;

  numCores = sysconf(_SC_NPROCESSORS_ONLN);
  cores_cwidth = NODE_NUM / numCores;
  // Each node in nodes is connected head to tail, eventually forming a ring
  for (int i = 0; i < NODE_NUM; i++) {
    Nodes[i].next = &Nodes[i + 1];
    Nodes[i].prev = &Nodes[i - 1];
    Nodes[i].data = (Cpuload *)malloc(sizeof(Cpuload));
  }
  Nodes[NODE_NUM - 1].next = &Nodes[0];
  Nodes[0].prev = &Nodes[NODE_NUM - 1];
  storage_cpu.pointer = &Nodes[0];
  storage_cpu.prev = (Cpuload *)malloc(sizeof(Cpuload));
  storage_cpu.curr = (Cpuload *)malloc(sizeof(Cpuload));

  // init cpu Cores
  storage_cores.curr = (Cpuload *)malloc(sizeof(Cpuload) * numCores);
  storage_cores.prev = (Cpuload *)malloc(sizeof(Cpuload) * numCores);

  // get temp nums
  thermal_zone_num = get_temp_nums();
}

void clean_status_pthread() {
  // Cancel thread
  pthread_cancel(draw_status_thread);
  pthread_join(draw_status_thread, NULL);
  
  // Destroy mutex
  pthread_mutex_destroy(&status_mutex);
  
  // Free memory
  if (storage_cores.curr) {
    free(storage_cores.curr);
    storage_cores.curr = NULL;
  }
  if (storage_cores.prev) {
    free(storage_cores.prev);
    storage_cores.prev = NULL;
  }
  
  if (storage_cpu.curr) {
    free(storage_cpu.curr);
    storage_cpu.curr = NULL;
  }
  if (storage_cpu.prev) {
    free(storage_cpu.prev);
    storage_cpu.prev = NULL;
  }
  
  /* Fix: should free all NODE_NUM nodes */
  for (int i = 0; i < NODE_NUM; i++) {
    if (Nodes[i].data) {
      free(Nodes[i].data);
      Nodes[i].data = NULL;
    }
  }
}

void *drawstatusbar() {
  init_statusbar();
  
  while (1) {
    // Use mutex to protect drawing operations
    pthread_mutex_lock(&status_mutex);
    
    // Only draw on the selected monitor
    if (selmon) {
      systrayw = getsystraywidth();
      systandstat = getstatuswidth() + systrayw;
      
      int x = selmon->ww - systrayw;
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_rect(drw, selmon->ww - systandstat, 0, systandstat, bh, 1, 1);

      for (int i = 0; i < LENGTH(Blocks); i++) {
        x = Blocks[i].draw(x, &Blocks[i]);
      }

      drw_map(drw, selmon->barwin, selmon->ww - systandstat, 0, systandstat, bh);
    }
    
    pthread_mutex_unlock(&status_mutex);
    
    // Move sleep outside loop to avoid sleeping for each monitor
    sleep(1);
  }
  return NULL;
}