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
  return 0;
}

/*Returns the rootblock or NULL on failure*/
rootblock_t * get_rootblock() {
  if (fp == NULL) {
    return NULL;
  }
  fseek(fp, 0, SEEK_SET);
  rootblock_t * rb = malloc(sizeof(rootblock_t));
  _u32 buffer[4];
  if (fread(buffer, sizeof(_u32), 4, fp) < 0) {
    return NULL;
  }
  rb->block_size = buffer[0];
  rb->num_blocks = buffer[1];
  rb->num_free_bitmap_blocks = buffer[2];
  rb->num_inode_table_blocks = buffer[3];
  return rb;
}

/*Read a disk block from index, writing it to buffer. 
Returns 0 on success, a negative number on error.*/
int read_block(_u32 index, Byte *buffer) {
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return -1;
  }
  if (index >= rb->num_blocks) {
    free(rb);
    return -1;
  }
  fseek(fp, index * rb->block_size, SEEK_SET);
  // Read full block
  if (fread(buffer, sizeof(Byte), rb->block_size, fp) < 0) {
    free(rb);
    return -1;
  }
  free(rb);
  return 0;
}

/*Write a disk block, filling it with content at index. content should be 
the same size as the block size. Returns 0 on success, a negative number on error*/
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
  if (fwrite(content, sizeof(Byte), rb->block_size, fp) < 0) {
    free(rb);
    return -1;
  }
  free(rb);
  return 0;
}

/*Returns the number of free blocks on the disk or -1 on failure.*/
_u32 num_free_blocks() {
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return -1;
  }
  _u32 free_blocks = rb->num_blocks;
  // Iterate through free bitmap blocks
  for (int i = 1; i < (rb->num_free_bitmap_blocks + 1); i++) {
    Byte buffer[rb->block_size];
    if (read_block(i, buffer) < 0) {
      return -1;
    }
    // Iterate through each byte of a single block
    for (int j = 0; j < rb->block_size; j++) {
      if (buffer[j] != 0) {
        free_blocks -= get_positive_bits(buffer[j]);
      }
    }
  }
  free(rb);
  return free_blocks;
}

/*Returns the number of free inodes on the disk or -1 on failure.*/
_u32 num_free_inodes() {
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return -1;
  }
  _u32 free_inodes = rb->num_inode_table_blocks * (rb->block_size / 32);
  // Iterate through inode blocks
  for (int i = rb->num_free_bitmap_blocks + 1; i < (1 + rb->num_free_bitmap_blocks + rb->num_inode_table_blocks); i++) {
    Byte buffer[rb->block_size];
    int num_inodes_per_block = free_inodes / rb->num_inode_table_blocks;
    // Read num_inodes_per_block inodes into buffer
    if (read_block(i, buffer) < 0) {
      return -1;
    }
    // Iterate through each inode in current block
    for (int j = 0; j < num_inodes_per_block; j++) {
      // flag = 1 means inode is free, 0 occupied
      int flag = 1;
      // Check all entries of each inode
      for (int k = 0; k < sizeof(inode_t); k++) {
        if (buffer[j*sizeof(inode_t)+k] != 0) {
          flag = 0;
          break;
        }
      }
      if (flag != 1) {
        free_inodes -= 1;
      }
    }
  }
  free(rb);
  return free_inodes;
}

/*Unloads the loaded file system. Returns 0 on success.*/
int unload(void) {
  if (fp == NULL) {
    return -1;
  }
  if (fclose(fp) != 0) {
    return -1;
  }
  return 0;
}

int format(char *diskname, _u32 block_size, _u32 num_blocks, _u32 num_inodes) {
  if (block_size < sizeof(rootblock_t)) {
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
  rb->num_inode_table_blocks = num_inodes / (block_size / sizeof(inode_t));
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

  // Free bitmap block information
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
  // Write the bitmap blocks
  if (fwrite(bitmap_buffer, rb->num_free_bitmap_blocks * block_size, 1, fp) < 0) {
    return -1;
  }

  // inode blocks information
  inode_t * root_dir_inode = malloc(sizeof(inode_t));
  if (root_dir_inode == NULL) {
    return -1;
  }
  root_dir_inode->size = 21;
  root_dir_inode->blocks[0] = 1 + rb->num_free_bitmap_blocks + rb->num_inode_table_blocks;
  for (int i = 1; i < 8; i++) {
    root_dir_inode->blocks[i] = 0;
  }
  // write the root inode
  if (fwrite(root_dir_inode, sizeof(inode_t), 1, fp) < 0) {
    return -1;
  };
  free(root_dir_inode);

  // write rest of the inodes
  for (int i = 1; i < num_inodes; i++) {
    inode_t * empty_inode = malloc(sizeof(inode_t));
    if (empty_inode == NULL) {
      return -1;
    }
    empty_inode->size = 0;
    for (int i = 0; i < 8; i++) {
      root_dir_inode->blocks[i] = 0;
    }
    if (fwrite(empty_inode, sizeof(inode_t), 1, fp) < 0) {
      return -1;
    };
    free(empty_inode);
  }

  // directory and direntry information
  direntry_t curr_dir;
  curr_dir.inode_num = 0; // 4
  curr_dir.type = 'D'; // 1
  curr_dir.name_length = 2; // 1
  // char curr_dir_name[2] = {'.', '\0'}; // 2
  // curr_dir.name = curr_dir_name;
  curr_dir.name = ".\0";

  direntry_t parent_dir;
  parent_dir.inode_num = 0; // 4
  parent_dir.type = 'D'; // 1
  parent_dir.name_length = 3; // 1
  // char parent_dir_name[3] = {'.', '.', '\0'}; // 3
  // parent_dir.name = parent_dir_name;
  parent_dir.name = "..\0";
  

  direntry_t direntry_array[2];
  direntry_array[0] = curr_dir;
  direntry_array[1] = parent_dir;

  directory_t root_dir;
  root_dir.num_entries = 2; // 4
  root_dir.dir_entries = direntry_array;

  _u32 u32_temp[1];
  u32_temp[0] = root_dir.num_entries;
  if (fwrite(u32_temp, sizeof(_u32), 1, fp) < 0) {
    return -1;
  }
  for (int i = 0; i < root_dir.num_entries; i++) {
    _u32 u32_temp[1];
    u32_temp[0] = root_dir.dir_entries[i].inode_num;
    if (fwrite(u32_temp, sizeof(_u32), 1, fp) < 0) {
      return -1;
    }
    Byte byte_temp[2];
    byte_temp[0] = root_dir.dir_entries[i].type;
    byte_temp[1] = root_dir.dir_entries[i].name_length;
    if (fwrite(byte_temp, 2, 1, fp) < 0) {
      return -1;
    }
    int buffer_length = (int)root_dir.dir_entries[i].name_length; // 1 for type, 1 for name_length
    if (fwrite(root_dir.dir_entries[i].name, buffer_length, 1, fp) < 0) {
      return -1;
    }
  }

  // Fill rest with 0's
  Byte zero_buffer[rb->block_size - 21];
  for (int i = 0; i < rb->block_size - 21; i++) {
    zero_buffer[i] = 0;
  }
  if (fwrite(zero_buffer, rb->block_size - 21, 1, fp) < 0) {
    return -1;
  }
  for (int i = 2 + rb->num_free_bitmap_blocks + rb->num_inode_table_blocks; i < rb->num_blocks; i++) {
    Byte zero_buffer[rb->block_size];
    for (int i = 0; i < rb->block_size; i++) {
      zero_buffer[i] = 0;
    }
    if (fwrite(zero_buffer, rb->block_size, 1, fp) < 0) {
      return -1;
    }
  }
  free(rb);
  fclose(fp);
  return 0;
}

/*Given a decimal number 0 <= b <= 255, calculate the 
number of 1's if this number is converted to binary.*/
int get_positive_bits(Byte b) {
  int result = 0;
  for (int i = 0; i < 8; i++) {
    if ((b >> i) & 1) {
      result += 1;
    }
  }
  return result;
}

int test() {
  printf("%d\n", get_positive_bits(123));
}