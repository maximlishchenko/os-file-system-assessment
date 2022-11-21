#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "filesystem.h"
#include "filesystem.c"

int main()
{
  load("E3Sample.disk", 0);
  // test();
  get_rootblock();
  printf(("num_free_blocks = %d\n"), num_free_blocks());
}