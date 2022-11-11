#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "filesystem.h"

rootblock_t * rootblock;

/*Formats the disk creating appropriate root blocks, free bitmap blocks,
inode blocks and root directory. returns 0 on success, a negative number on error*/
int format(char *diskname, _u32 block_size, _u32 num_blocks, _u32 num_inodes) {
  
}

/*Loads a filesystem which has already been formatted. The write_buffer_size records 
how many blocks must change before they are written back to the disk. Returns 0 on success.*/
int load(char *diskname, _u32 write_buffer_size) {
  // fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
  FILE * fp;
  fp = fopen(diskname, "w+");
  fread(rootblock, 1, write_buffer_size, fp);
  fclose(fp);
  return 0;
}