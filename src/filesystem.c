#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "filesystem.h"

FILE * fp = NULL;

/*Loads a filesystem which has already been formatted. The write_buffer_size records 
how many blocks must change before they are written back to the disk. Returns 0 on success.*/
int load(char *diskname, _u32 write_buffer_size) {
  fp = fopen(diskname, "r+");
  if (fp == NULL) {
    return -1;
  }
  fseek(fp, 0, SEEK_SET);
  // _u32 buffer[4];
  // if (fread(buffer, sizeof(_u32), 4, fp) < 0) {
  //   return -1;
  // }
  // rootblock_t * rootblock = malloc(sizeof(_u32) * 4);
  // rootblock->block_size = buffer[0];
  // rootblock->num_blocks = buffer[1];
  // rootblock->num_free_bitmap_blocks = buffer[2];
  // rootblock->num_inode_table_blocks = buffer[3];
  return 0;
}

/*returns the rootblock or -1 on failure*/
rootblock_t * get_rootblock() {
  if (fp == NULL) {
    return NULL;
  }
  fseek(fp, 0, SEEK_SET);
  rootblock_t * rootblock = malloc(sizeof(rootblock_t));
  _u32 buffer[4];
  if (fread(buffer, sizeof(_u32), 4, fp) < 0) {
    return NULL;
  }
  rootblock->block_size = buffer[0];
  rootblock->num_blocks = buffer[1];
  rootblock->num_free_bitmap_blocks = buffer[2];
  rootblock->num_inode_table_blocks = buffer[3];
  return rootblock;
}

/*Read a disk block from index, writing it to buffer. 
Returns 0 on success, a negative number on error.*/
int read_block(_u32 index, Byte *buffer) {
  rootblock_t * rb = get_rootblock();
  // Rootblock doesn't exist
  if (rb == NULL) {
    return -1;
  }
  // Out of bounds
  if (index >= rb->num_blocks) {
    free(rb);
    return -1;
  }
  fseek(fp, index * rb->block_size, SEEK_SET);
  if (fread(buffer, rb->block_size, 1, fp) < 0) {
    free(rb);
    return -1;
  }
  free(rb);
  return 0;
}

int write_block(_u32 index, Byte *content) {
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return -1;
  }
  if (index >= rb->num_blocks) {
    free(rb);
    return -1;
  }
  fseek(fp, index * rb->block_size, SEEK_SET);
  // Write full block
  if (fwrite(content, rb->block_size, 1, fp) < 0) {
    free(rb);
    return -1;
  }
  free(rb);
  return 0;
}

_u32 num_free_blocks() {
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return -1;
  }
  int free_blocks = rb->num_blocks;
  for (int i = 1; i < (rb->num_free_bitmap_blocks + 1); i++) {
    Byte buffer[rb->block_size];
    if (read_block(i, buffer) < 0) {
      return -1;
    }
    for (int j = 0; j < rb->block_size; j++) {
      if (buffer[j] != 0) {
        free_blocks -= get_positive_bits(buffer[j]);
      }
    }
  }
  free(rb);
  return free_blocks;
}

_u32 num_free_inodes() {
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return -1;
  }
  int free_inodes = rb->num_inode_table_blocks * (rb->block_size / 32);
  for (int i = rb->num_free_bitmap_blocks + 1; i < rb->num_free_bitmap_blocks + 1 + rb->num_inode_table_blocks; i++) {
    Byte buffer[rb->block_size];
    if (read_block(i, buffer) < 0) {
      return -1;
    }
    int flag = 1;
    for (int j = 0; j < rb->block_size; j = j + 32) {
      if (buffer[j] != 0) {
        flag = 0;
        break;
      }
    }
    if (flag != 1) {
      free_inodes -= 1;
    }
  }
  free(rb);
  return free_inodes;
}

// /*Formats the disk creating appropriate root blocks, free bitmap blocks,
// inode blocks and root directory. returns 0 on success, a negative number on error*/
// int format(char *diskname, _u32 block_size, _u32 num_blocks, _u32 num_inodes) {
//   rootblock_t * rb;
//   rb = malloc(sizeof(_u32) * 4);
//   // rootblock information
//   rb->block_size = block_size;
//   rb->num_blocks = num_blocks;
//   rb->num_free_bitmap_blocks = num_blocks / block_size / 8;
//   rb->num_inode_table_blocks = num_inodes / 4;

//   FILE *fp;
//   fp = fopen(diskname, "wb");
//   if (fp == NULL) {
//     return -1;
//   }

//   _u32 rb_buffer[block_size / 4];

//   rb_buffer[0] = block_size;
//   rb_buffer[1] = num_blocks;
//   rb_buffer[2] = rb->num_free_bitmap_blocks;
//   rb_buffer[3] = rb->num_inode_table_blocks;

//   for (int i = 4; i < block_size / 4; i++) {
//     rb_buffer[i] = 0;
//   }
//   fwrite(rb_buffer, sizeof(Byte), block_size, fp);

//   // free bitmap information
//   int num_occupied_blocks = rb->num_free_bitmap_blocks + rb->num_inode_table_blocks + 2; // 1 for rootblock, 1 for root dir
//   int num_8_bit_blocks = 0;
//   Byte bitmap_buffer[rb->num_free_bitmap_blocks * block_size];
//   for (int i = 7; i < num_occupied_blocks; i = i + 8) {
//     num_8_bit_blocks += 1;
//   }
//   if (num_8_bit_blocks != 0) {
//     for (int i = 0; i < num_8_bit_blocks; i++) {
//       bitmap_buffer[i] = 255;
//     }
//   }
//   if (num_occupied_blocks % 8 != 0) {
//     int result = 2;
//     for (int i = 1; i <= num_occupied_blocks % 8; i++) {
//       result = i * result;
//     }
//     result -= 1;
//     bitmap_buffer[num_8_bit_blocks] = result;
//   }
//   for (int i = num_8_bit_blocks + 1; i < rb->num_free_bitmap_blocks * block_size; i++) {
//     bitmap_buffer[i] = 0;
//   }
//   fwrite(bitmap_buffer, sizeof(Byte), rb->num_free_bitmap_blocks * block_size, fp);

// //   typedef struct directory {
// //     _u32 num_entries;
// //     direntry_t *dir_entries;
// // } directory_t;
//   directory_t root_dir;
//   root_dir.num_entries = 2;
// //   typedef struct direntry {
// //     _u32 inode_num;
// //     Byte type;
// //     Byte name_length;
// //     char *name;
// // } direntry_t;
//   // direntry_t curr_dir;
//   // curr_dir.type = 'D';
//   // curr_dir.name_length = 2;
//   // char string[2] = {'.', '\0'};
//   // curr_dir.name = string;

//   // direntry parent_dir;
//   // curr_dir.type = 'D';
//   // curr_dir.name_length = 3;
//   // char string[3] = {'..', '\0'};
//   // curr_dir.name = string;

//   fclose(fp);
//   return(0);
// }

int format(char *diskname, _u32 block_size, _u32 num_blocks, _u32 num_inodes) {
  if (block_size < sizeof(rootblock_t)) {
    return -1;
  }

  if (block_size % sizeof(inode_t) != 0) {
    return -1;
  }

  fp = fopen(diskname, "wb");
  if (fp == NULL) {
    return -1;
  }
  rootblock_t * rb = malloc(sizeof(rootblock_t));
  rb->block_size = block_size;
  rb->num_blocks = num_blocks;
  rb->num_free_bitmap_blocks = num_blocks / block_size / 8;
  rb->num_inode_table_blocks = num_inodes / 4;
  // Write rootblock to disk
  if (fwrite(rb, sizeof(rootblock_t), 1, fp) < 0) {
    return -1;
  }

  Byte buffer[rb->block_size - sizeof(rootblock_t)];
  for (int i = 0; i < rb->block_size - sizeof(rootblock_t); i++) {
    buffer[i] = 0;
  }

  // Fill the rest of rootblock with 0's
  if (fwrite(buffer, rb->block_size - sizeof(rootblock_t), 1, fp) < 0) {
    return -1;
  }



  fclose(fp);
  return(0);
}

int test() {
  printf("size of rootblock is %ld\n", sizeof(rootblock_t));
  printf("size of inode_t is %ld\n", sizeof(inode_t));
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