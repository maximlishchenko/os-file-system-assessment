#include "filesystem.h"

int main()
{
  // load("ReadingExample.disk", 0);
  // test();
  // get_rootblock();
  // printf(("num_free_blocks = %d\n"), num_free_blocks());
  // printf("num_free_inodes = %d\n", num_free_inodes());
  format("test.disk", 128, 4096, 80);
}
