#include "status.h"
/* configuration, allows nested code to access above variables */
#include "config.h"

int getstatuswidth() {
  int width = 0;
  for (int i = 0; i < LENGTH(Blocks); i++) {
    width += Blocks[i].bw;
  }

  return width;
}
