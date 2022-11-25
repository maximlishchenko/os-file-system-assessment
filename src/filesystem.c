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
    free(rb);
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

/*Formats the disk creating appropriate root blocks, free bitmap blocks, 
inode blocks and root directory. returns 0 on success, a negative number on error*/
int format(char *diskname, _u32 block_size, _u32 num_blocks, _u32 num_inodes) {
  // Rootblock doesn't fit into block
  if (block_size < sizeof(rootblock_t)) {
    return -1;
  }
  fp = fopen(diskname, "wb");
  if (fp == NULL) {
    return -1;
  }
  // Create rootblock
  rootblock_t * rb = malloc(sizeof(rootblock_t));
  rb->block_size = block_size;
  rb->num_blocks = num_blocks;
  rb->num_free_bitmap_blocks = num_blocks / block_size / 8;
  rb->num_inode_table_blocks = num_inodes / (block_size / sizeof(inode_t));
  // Write rootblock to disk
  if (fwrite(rb, sizeof(rootblock_t), 1, fp) < 0) {
    return -1;
  }
  // Create buffer that will hold trailing zeros for rootblock
  Byte zero_buffer[block_size - sizeof(rootblock_t)];
  // Allocate 0's
  for (int i = 0; i < block_size - sizeof(rootblock_t); i++) {
    zero_buffer[i] = 0;
  }
  // Write trailing zeros to rootblock
  if (fwrite(zero_buffer, sizeof(Byte), block_size - sizeof(rootblock_t), fp) < 0) {
    return -1;
  }
  fclose(fp);
  fp = fopen(diskname, "ab+");
  if (fp == NULL) {
    return -1;
  }

  // Number of occupied blocks on disk
  _u32 num_occupied_blocks = 1 + rb->num_free_bitmap_blocks + rb->num_inode_table_blocks + 1; // + 1 for rootblock, + 1 for root dir
  // Calculate the number of 8 positive-bit (FF) bytes
  _u32 num_8_bit_blocks = 0;
  for (int i = 7; i < num_occupied_blocks; i = i + 8) {
    num_8_bit_blocks += 1;
  }
  _u32 num_occupied_bitmap_blocks = 0; // number of bitmap blocks with at least 1 non-zero entry
  int num_bits_per_block = block_size * 8; // number of bits per block
  int current;
  for (int i = 0; i < rb->num_free_bitmap_blocks; i++) {
    current = i * num_bits_per_block;
    if (num_8_bit_blocks > current) {
      num_occupied_bitmap_blocks++;
    }
  }
  // First dealing with non-empty bitmap blocks
  for (int i = 0; i < num_occupied_bitmap_blocks; i++) {
    Byte buffer[block_size];
    for (int j = 0; j < num_8_bit_blocks && j < block_size; j++) {
      buffer[j] = 255;
    }
    if (num_occupied_blocks % 8 != 0) {
      int result = 2;
      for (int k = 1; k <= num_occupied_blocks % 8; k++) {
        result = k * result;
      }
      result -= 1;
      buffer[num_8_bit_blocks] = result;
    }
    for (int j = num_8_bit_blocks + 1; j < block_size; j++) {
      buffer[j] = 0;
    }
    if (write_block(i + 1, buffer) < 0) {
      return -1;
    }
  }
  int fully_free_blocks = rb->num_free_bitmap_blocks - num_occupied_bitmap_blocks;
  // Write the remaining free bitmap blocks
  for (int i = 0; i < fully_free_blocks; i++) {
    Byte buffer[block_size];
    for (int j = 0; j < block_size; j++) {
      buffer[j] = 0;
    }
    if (write_block(i + 1 + num_occupied_bitmap_blocks, buffer) < 0) {
      return -1;
    }
  }

  // Initializing root directory and '.' with '..'
  direntry_t curr_dir;
  curr_dir.inode_num = 0; // 4
  curr_dir.type = 'D'; // 1
  curr_dir.name_length = 2; // 1
  curr_dir.name = ".\0";

  direntry_t parent_dir;
  parent_dir.inode_num = 0; // 4
  parent_dir.type = 'D'; // 1
  parent_dir.name_length = 3; // 1
  parent_dir.name = "..\0";

  direntry_t direntry_array[2];
  direntry_array[0] = curr_dir;
  direntry_array[1] = parent_dir;

  directory_t root_dir;
  root_dir.num_entries = 2; // 4
  root_dir.dir_entries = direntry_array;

  int total_name_len = 0;
  for (int i = 0; i < root_dir.num_entries; i++) {
    total_name_len += strlen(root_dir.dir_entries[i].name) + 1;
  }
  int root_dir_length = root_dir.num_entries * 6 + 4 + total_name_len;

  // Creating root directory inode
  inode_t * root_dir_inode = malloc(sizeof(inode_t));
  if (root_dir_inode == NULL) {
    return -1;
  }
  root_dir_inode->size = root_dir_length;
  root_dir_inode->blocks[0] = 1 + rb->num_free_bitmap_blocks + rb->num_inode_table_blocks;
  for (int i = 1; i < 7; i++) {
    root_dir_inode->blocks[i] = 0;
  }
  // Buffer to be written to disk
  Byte * root_dir_inode_buffer = malloc(block_size);
  memcpy(root_dir_inode_buffer, root_dir_inode, sizeof(inode_t));
  int offset = sizeof(inode_t);
  // Creating remaining empty inodes for the first inode block
  for (int i = 0; i < (block_size / sizeof(inode_t)) - 1; i++) {
    inode_t * empty_inode = malloc(sizeof(inode_t));
    if (empty_inode == NULL) {
      return -1;
    }
    empty_inode->size = 0;
    for (int j = 0; j < 7; j++) {
      empty_inode->blocks[j] = 0;
    }
    memcpy(root_dir_inode_buffer + offset, empty_inode, sizeof(inode_t));
    free(empty_inode);
    offset += sizeof(inode_t);
  }
  // Write the first inode block
  if (write_block(1 + rb->num_free_bitmap_blocks, root_dir_inode_buffer) < 0) {
    return -1;
  }
  free(root_dir_inode);

  // Creating remaining empty inodes for the rest of the blocks
  for (int i = 1; i < rb->num_inode_table_blocks; i++) {
    Byte * empty_inode_buffer = malloc(block_size);
    int offset = 0;
    for (int j = 0; j < (block_size / sizeof(inode_t)); j++) {
      inode_t * empty_inode = malloc(sizeof(inode_t));
      if (empty_inode == NULL) {
        return -1;
      }
      empty_inode->size = 0;
      for (int k = 0; k < 7; k++) {
        empty_inode->blocks[k] = 0;
      }
      memcpy(empty_inode_buffer + offset, empty_inode, sizeof(inode_t));
      free(empty_inode);
      offset += sizeof(inode_t);
    }
    if (write_block(1 + rb->num_free_bitmap_blocks + i, empty_inode_buffer) < 0) {
      return -1;
    }
  }

  Byte * result_buffer = malloc(block_size);

  _u32 num_entries_buffer[1];
  num_entries_buffer[0] = root_dir.num_entries;
  memcpy(result_buffer, num_entries_buffer, sizeof(_u32));
  int current_offset = sizeof(_u32);
  for (int i = 0; i < root_dir.num_entries; i++) {
    _u32 u32_buffer[1]; // stores inode_num
    u32_buffer[0] = root_dir.dir_entries[i].inode_num;
    Byte byte_buffer[2]; // stores type and name_length
    byte_buffer[0] = root_dir.dir_entries[i].type;
    byte_buffer[1] = root_dir.dir_entries[i].name_length;
    memcpy(result_buffer + current_offset, u32_buffer, sizeof(_u32));
    current_offset += sizeof(_u32);
    memcpy(result_buffer + current_offset, byte_buffer, 2 * sizeof(Byte));
    current_offset += 2 * sizeof(Byte);
    memcpy(result_buffer + current_offset, root_dir.dir_entries[i].name, strlen(root_dir.dir_entries[i].name) + 1);
    current_offset += strlen(root_dir.dir_entries[i].name) + 1;
  }
  Byte remainder_buffer[block_size-root_dir_length];
  for (int i = 0; i < block_size-root_dir_length; i++) {
    remainder_buffer[i] = 0;
  }
  memcpy(result_buffer + current_offset, remainder_buffer, block_size-root_dir_length);
  if (write_block(1 + rb->num_free_bitmap_blocks + rb->num_inode_table_blocks, result_buffer) < 0) {
    return -1;
  }

  // Creating remaining empty blocks
  for (int i = 1 + rb->num_free_bitmap_blocks + rb->num_inode_table_blocks + 1; i < num_blocks; i++) {
    Byte buffer[block_size];
    for (int j = 0; j < block_size; j++) {
      buffer[j] = 0;
    }
    if (write_block(i, buffer) < 0) {
      return -1;
    }
  }

  free(rb);
  fclose(fp);
  return 0;
}

/* Makes a directory. name is either a full path (if it begins with '/', 
or a relative path with regards to the current location in the file system. 
All entries except the last must already exist. Returns 0 on success.*/
int mkdir(char *name) {

}

// typedef struct my_file {
//     _u32 inode_num;
//     inode_t *inode;
//     _u32 pos;
//     Byte *buffer;
//     char dirty;
// } my_file;
/* Opens a file for reading and writing. returns NULL on error. 
filename is an absolute or relative path to a file.*/
my_file *my_fopen(char *filename) {
  // Read directory we want to create the file in

  int index;

  // Create a new directory entry for the file we are creating
  // Set all attributes of the new direntry accoridingly
  // Change the directory’s inode’s size element
  // Add new direntry to list of direntries of the directory
  // Write directory back to disk
  // Create my_file struct, set all attributes accordingly and return it
}


// typedef struct inode {
//     _u32 size;
//     _u32 blocks[7];
// } inode_t;

// Gets index of the first free inode or -1 on error.
_u32 get_first_free_inode() {
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return -1;
  }
  // Read each inode block
  for (int i = 1 + rb->num_free_bitmap_blocks; i < 1 + rb->num_free_bitmap_blocks + rb->num_inode_table_blocks; i++) {
    Byte buffer[rb->block_size];
    if (read_block(i, buffer) < 0) {
      return -1;
    }
    // Read each inode
    for (int j = 0; j < rb->block_size / sizeof(inode_t); j++) {
      int flag = 1; // 1 means free, 0 occupied
      // Read each Byte
      for (int k = j * sizeof(inode_t); k < j * sizeof(inode_t) + sizeof(inode_t); k++) {
        if (buffer[k] != 0) {
          flag = 0;
          break;
        }
      }
      if (flag == 1) {
        _u32 free_inode_index = (i - (1 + rb->num_free_bitmap_blocks)) * (rb->block_size / sizeof(inode_t)) + j;
        free(rb);
        printf("free_inode_index is %d\n", free_inode_index);
        return free_inode_index;
      }
    }
  }
  return -1;
}

// Reads inode at index into buffer
int read_inode(_u32 index, _u32 *buffer) {
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return -1;
  }
  // index is out of bounds
  if (index >= rb->num_inode_table_blocks * (rb->block_size / sizeof(inode_t)) || index < 0) {
    free(rb);
    return -1;
  }
  int offset = 1 + rb->num_free_bitmap_blocks;
  fseek(fp, offset * rb->block_size + index * sizeof(inode_t), SEEK_SET);
  if (fread(buffer, sizeof(_u32), 8, fp) < 0) {
    return -1;
  }
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
  load("ReadingExample.disk", 0);
  _u32 buffer[8];
  read_inode(get_first_free_inode() - 1, buffer);
  for (int i = 0; i < 8; i++) {
    printf("buffer[i] = %d\n", buffer[i]);
  }
}