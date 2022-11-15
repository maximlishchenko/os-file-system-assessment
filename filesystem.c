#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "filesystem.h"

rootblock_t * rootblock;
block_t virtualDisk[4096];
block_t block;

/*Formats the disk creating appropriate root blocks, free bitmap blocks,
inode blocks and root directory. returns 0 on success, a negative number on error*/
int format(char *diskname, _u32 block_size, _u32 num_blocks, _u32 num_inodes) {
  
}

/*Loads a filesystem which has already been formatted. The write_buffer_size records 
how many blocks must change before they are written back to the disk. Returns 0 on success.*/
int load(char *diskname, _u32 write_buffer_size) {
  FILE * fp = fopen(diskname, "r+");
  _u32 buffer[4];
  fseek(fp, 0, SEEK_SET);
  if (fread(buffer, sizeof(_u32), 4, fp) < 0) {
    return -1;
  }
  for (int i = 0; i < 4; i++) {
    printf("buffer[%d] equals %d\n", i, buffer[i]);
  }
  fclose(fp);
  rootblock->block_size = buffer[0];
  rootblock->num_blocks = buffer[1];
  rootblock->num_free_bitmap_blocks = buffer[2];
  rootblock->num_inode_table_blocks = buffer[3];
  return 0;
}

int test() {
  printf("size of block is %ld\n", sizeof(block));
  printf("size of rootblock is %ld\n", sizeof(rootblock));
  printf("size of block.rootblock is %ld\n", sizeof(block.rootblock));
  printf("size of block.bitmapblock is %ld\n", sizeof(block.bitmapblock));
  printf("size of block.inodeblock is %ld\n", sizeof(block.inodeblock));
  printf("size of block.datablock is %ld\n", sizeof(block.datablock));
  return 0;
}