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
  int free_inodes = rb->num_inode_table_blocks * (rb->block_size / 32);
  int start_index = rb->block_size * (rb->num_free_bitmap_blocks + 1); // adding 1 for rootblock
  for (int i = start_index; i < start_index + (rb->num_inode_table_blocks * rb->block_size); i = i + 32) {
    int flag = 1; // 1 means free, 0 occupied
    for (int j = 0; j < 32; j++) {
      if (virtualDisk[i+j] != 0) {
        flag = 0;
        break;
      }
    }
    if (flag == 0) {
      free_inodes -= 1;
    }
  }
  return free_inodes;
}

/*Formats the disk creating appropriate root blocks, free bitmap blocks,
inode blocks and root directory. returns 0 on success, a negative number on error*/
int format(char *diskname, _u32 block_size, _u32 num_blocks, _u32 num_inodes) {
  rootblock_t * rb;
  rb = malloc(sizeof(_u32) * 4);
  // rootblock information
  rb->block_size = block_size;
  rb->num_blocks = num_blocks;
  rb->num_free_bitmap_blocks = num_blocks / block_size / 8;
  rb->num_inode_table_blocks = num_inodes / 4;

  FILE *fp;
  fp = fopen(diskname, "wb");

  _u32 rb_buffer[block_size / 4];

  rb_buffer[0] = block_size;
  rb_buffer[1] = num_blocks;
  rb_buffer[2] = rb->num_free_bitmap_blocks;
  rb_buffer[3] = rb->num_inode_table_blocks;

  for (int i = 4; i < block_size / 4; i++) {
    rb_buffer[i] = 0;
  }
  fwrite(rb_buffer, sizeof(Byte), block_size, fp);

  // free bitmap information
  int num_occupied_blocks = rb->num_free_bitmap_blocks + rb->num_inode_table_blocks + 2; // 1 for rootblock, 1 for root dir
  int num_8_bit_blocks = 0;
  Byte bitmap_buffer[rb->num_free_bitmap_blocks * block_size];
  for (int i = 7; i < num_occupied_blocks; i = i + 8) {
    num_8_bit_blocks += 1;
  }
  if (num_8_bit_blocks != 0) {
    for (int i = 0; i < num_8_bit_blocks; i++) {
      bitmap_buffer[i] = 255;
    }
  }
  if (num_occupied_blocks % 8 != 0) {
    int result = 2;
    for (int i = 1; i <= num_occupied_blocks % 8; i++) {
      result = i * result;
    }
    result -= 1;
    bitmap_buffer[num_8_bit_blocks] = result;
  }
  for (int i = num_8_bit_blocks + 1; i < rb->num_free_bitmap_blocks * block_size; i++) {
    bitmap_buffer[i] = 0;
  }
  fwrite(bitmap_buffer, sizeof(Byte), rb->num_free_bitmap_blocks * block_size, fp);

  directory_t root_dir;
  root_dir.num_entries = 2;
//   typedef struct direntry {
//     _u32 inode_num;
//     Byte type;
//     Byte name_length;
//     char *name;
// } direntry_t;
  direntry_t curr_dir;
  curr_dir.type = 'D';
  curr_dir.name_length = 2;
  curr_dir.name = '.';

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