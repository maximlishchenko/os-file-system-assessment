#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "filesystem.h"

rootblock_t * rootblock;
Byte * virtualDisk;

/*Loads a filesystem which has already been formatted. The write_buffer_size records 
how many blocks must change before they are written back to the disk. Returns 0 on success.*/
// int load(char *diskname, _u32 write_buffer_size) {
//   FILE * fp = fopen(diskname, "r+");
//   fseek(fp, 0, SEEK_SET);
//   _u32 buffer[4];
//   if (fread(buffer, sizeof(_u32), 4, fp) < 0) {
//     return -1;
//   }
//   rootblock = malloc(sizeof(_u32) * 4);
//   rootblock->block_size = buffer[0];
//   rootblock->num_blocks = buffer[1];
//   rootblock->num_free_bitmap_blocks = buffer[2];
//   rootblock->num_inode_table_blocks = buffer[3];

//   // Byte buffer2[rootblock.block_size * rootblock.num_free_bitmap_blocks];
//   // fseek(fp, rootblock.block_size, SEEK_SET);
//   // if (fread(buffer2, sizeof(Byte), rootblock.block_size * rootblock.num_free_bitmap_blocks, fp) < 0) {
//   //   return -1;
//   // }
//   // for (int i = 0; i < rootblock.block_size * rootblock.num_free_bitmap_blocks; i++) {
//   //   bitmap[i] = malloc(sizeof(Byte));
//   //   bitmap[i] = buffer2[i];
//   //   printf("buffer2[%d] is %d\n", i, buffer2[i]);
//   // }
//   fclose(fp);
//   return 0;
// }

int load(char *diskname, _u32 write_buffer_size) {
  FILE * fp = fopen(diskname, "r+");
  fseek(fp, 0, SEEK_SET);
  _u32 buffer[4];
  if (fread(buffer, sizeof(_u32), 4, fp) < 0) {
    return -1;
  }
  rootblock = malloc(sizeof(_u32) * 4);
  rootblock->block_size = buffer[0];
  rootblock->num_blocks = buffer[1];
  rootblock->num_free_bitmap_blocks = buffer[2];
  rootblock->num_inode_table_blocks = buffer[3];
  int disk_size = rootblock->num_blocks * rootblock->block_size;
  Byte buffer2[disk_size];
  fseek(fp, 0, SEEK_SET);
  if (fread(buffer2, sizeof(Byte), disk_size, fp) < 0) {
    return -1;
  }
  virtualDisk = (Byte*)malloc(sizeof(Byte) * disk_size);
  for (int i = 0; i < disk_size; i++) {
    virtualDisk[i] = buffer2[i];
  }
  fclose(fp);
  return 0;
}

rootblock_t *get_rootblock() {
  return rootblock;
}

_u32 num_free_blocks() {
  rootblock_t * rb = get_rootblock();
  int free_blocks = rb->num_blocks;
  for (int i = rb->block_size; i < (rb->block_size + rb->num_free_bitmap_blocks * rb->block_size); i++) {
    if (virtualDisk[i] != 0) {
      free_blocks -= get_positive_bits(virtualDisk[i]);
    }
  }
  return free_blocks;
}

_u32 num_free_inodes() {
  rootblock_t * rb = get_rootblock();
}

/*Formats the disk creating appropriate root blocks, free bitmap blocks,
inode blocks and root directory. returns 0 on success, a negative number on error*/
int format(char *diskname, _u32 block_size, _u32 num_blocks, _u32 num_inodes) {
   FILE *fp;
  Byte asd[128];
  for (int i = 0; i < 128; i = i + 4) {
    _u32 lol = 5;
    asd[i] = lol;
  }
   fp = fopen( "file.disk" , "wb" );
   fwrite(asd , sizeof(Byte) , 128 , fp );
   fclose(fp);
   return(0);
}

int test() {
  printf("rootblock.block_size = %d\n", rootblock->block_size);
  printf("rootblock.num_blocks = %d\n", rootblock->num_blocks);
  printf("rootblock.num_free_bitmap_blocks = %d\n", rootblock->num_free_bitmap_blocks);
  printf("rootblock.num_inode_table_blocks = %d\n", rootblock->num_inode_table_blocks);
}

int get_positive_bits(Byte b) {
  int result = 0;
  for (int i = 0; i < 8; i++) {
    if ((b >> i) & 1) {
      result += 1;
    }
  }
  return result;
}