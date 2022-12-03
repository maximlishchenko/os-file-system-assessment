#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "filesystem.h"

FILE * fp = NULL;
char * curr_dir = "/";

/*Loads a filesystem which has already been formatted. The write_buffer_size records 
how many blocks must change before they are written back to the disk. Returns 0 on success.*/
int load(char *diskname, _u32 write_buffer_size) {
  if (fp != NULL) {
    unload();
  }
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
  fp = NULL;
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
    free(rb);
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
    free(rb);
    return -1;
  }
  fclose(fp);
  fp = fopen(diskname, "ab+");
  if (fp == NULL) {
    free(rb);
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
        result *= k;
      }
      result -= 1;
      buffer[num_8_bit_blocks] = result;
    }
    for (int j = num_8_bit_blocks + 1; j < block_size; j++) {
      buffer[j] = 0;
    }
    if (write_block(i + 1, buffer) < 0) {
      free(rb);
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
      free(rb);
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
    free(rb);
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
      free(root_dir_inode);
      free(rb);
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
    free(rb);
    free(root_dir_inode);
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
        free (rb);
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
      free(rb);
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
    free(rb);
    return -1;
  }

  // Creating remaining empty blocks
  for (int i = 1 + rb->num_free_bitmap_blocks + rb->num_inode_table_blocks + 1; i < num_blocks; i++) {
    Byte buffer[block_size];
    for (int j = 0; j < block_size; j++) {
      buffer[j] = 0;
    }
    if (write_block(i, buffer) < 0) {
      free(rb);
      return -1;
    }
  }

  free(rb);
  fclose(fp);
  return 0;
}

/* Opens a file for reading and writing. returns NULL on error. 
filename is an absolute or relative path to a file.*/
my_file *my_fopen(char *filename) {
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return NULL;
  }
  // Read root directory inode
  _u32 inode_buffer[8];
  // hardcoding for root directory for now
  if (read_inode(0, inode_buffer) < 0) {
    free(rb);
    return NULL;
  }
  // Root directory inode
  inode_t inode;
  inode.size = inode_buffer[0];
  for (int i = 0; i < 7; i++) {
    inode.blocks[i] = inode_buffer[i+1];
  }
  // Read root directory
  Byte root_dir_buffer[rb->block_size];
  if (read_block(inode.blocks[0], root_dir_buffer) < 0) {
    free(rb);
    return NULL;
  }
  // Create an array of chars for filename
  int name_length = strlen(filename) + 1;
  char file_name[name_length];
  for (int i = 0 ; i < name_length; i++) {
    file_name[i] = filename[i];
  }
  // Check that file already exists
  for (int i = 0; i < rb->block_size; i++) {
    // Check if entry with type 'F' exists
    if (root_dir_buffer[i] == 70) {
      // Check if length of that entry matches the filename's length
      if (root_dir_buffer[i+1] == name_length) {
        int k = 0;
        for (int j = i + 2; j < i + 2 + name_length; j++) {
          if (root_dir_buffer[j] == file_name[k]) {
            k++;
          }
        }
        if (k == name_length) { // File exists and we need to return info about it
          _u32 u32_buffer[1];
          // Get index of existing file's inode and read it
          fseek(fp, inode.blocks[0] * rb->block_size + i - sizeof(_u32), SEEK_SET);
          if (fread(u32_buffer, sizeof(_u32), 1, fp) < 0) {
            return NULL;
          }
          // Read the existing file's inode
          _u32 existing_file_inode_buffer[8];
          if (read_inode(u32_buffer[0], existing_file_inode_buffer) < 0) {
            return NULL;
          }
          // Creating an inode for the file
          inode_t * new_inode = malloc(sizeof(inode_t));
          new_inode->size = existing_file_inode_buffer[0];
          for (int i = 0; i < 7; i++) {
            new_inode->blocks[i] = existing_file_inode_buffer[i+1];
          }
          // Creating the file
          my_file * file = malloc(sizeof(my_file));
          Byte data[rb->block_size];
          if (read_block(new_inode->blocks[0], data) < 0) {
            return NULL;
          }
          file->inode_num = u32_buffer[0];
          file->inode = new_inode;
          file->pos = 0;
          file->buffer = data;
          file->dirty = 0;
          return file;
        }
      }
    }
  }
  // If we get here, file doesn't exist
  _u32 u32_buffer[1];

  // Read num of directory entries
  fseek(fp, inode.blocks[0] * rb->block_size, SEEK_SET);
  if (fread(u32_buffer, sizeof(_u32), 1, fp) < 0) {
    return NULL;
  }
  
  _u32 num_root_dir_entries[1];
  // Incrementing number of directory entries
  num_root_dir_entries[0] = u32_buffer[0] + 1;
  fseek(fp, inode.blocks[0] * rb->block_size, SEEK_SET);
  if (fwrite(num_root_dir_entries, sizeof(_u32), 1, fp) < 0) {
    return NULL;
  }
  // Creating a new direntry for the file
  direntry_t direntry;
  direntry.inode_num = get_first_free_inode(); // index of the inode where the file data is stored 
  direntry.type = 'F';
  direntry.name_length = strlen(filename) + 1; // +1 for null terminated string
  direntry.name = filename;

  _u32 buff[1];
  buff[0] = direntry.inode_num;
  fseek(fp, inode.blocks[0] * rb->block_size + inode.size, SEEK_SET);
  if (fwrite(buff, sizeof(_u32), 1, fp) < 0) {
    return NULL;
  }
  buff[0] = direntry.type;
  if (fwrite(buff, sizeof(Byte), 1, fp) < 0) {
    return NULL;
  }
  buff[0] = direntry.name_length;
  if (fwrite(buff, sizeof(Byte), 1, fp) < 0) {
    return NULL;
  }
  if (fwrite(direntry.name, sizeof(Byte) * name_length, 1, fp) < 0) {
    return NULL;
  }

  // Updating size of directory
  inode_buffer[0] += 6 + name_length;
  // Hardocding for root directory inode for now
  if(write_inode(0, inode_buffer) < 0) {
    return NULL;
  }
  // Creating an inode for the file
  inode_t * new_inode = malloc(sizeof(inode_t));
  // Initially the size is 0
  new_inode->size = 0;
  // Allocating the first free block for the file's data
  new_inode->blocks[0] = rb->num_blocks - num_free_blocks();
  for (int i = 1; i < 7; i++){
    new_inode->blocks[i] = 0;
  }
  // Changing the free block bitmap
  // For now only works for 1 block
  int num_occupied_blocks = rb->num_blocks - num_free_blocks() + 1;
  // Calculate the number of 8 positive-bit (FF) bytes
  _u32 num_8_bit_blocks = 0;
  for (int i = 7; i < num_occupied_blocks; i = i + 8) {
    num_8_bit_blocks += 1;
  }
  Byte bitmap_buffer[rb->block_size];
  // Write 8 positive-bit (FF) bytes
  for (int i = 0; i < num_8_bit_blocks; i++) {
    bitmap_buffer[i] = 255;
  }
  int remainder = num_occupied_blocks % 8;
  if (remainder != 0) {
    int result = 2;
    for (int i = 1; i < remainder; i++) {
      result *= 2;
    }
    result -= 1;
    bitmap_buffer[num_8_bit_blocks] = result;
  } else {
    bitmap_buffer[num_8_bit_blocks] = 1;
  }
  for (int i = num_8_bit_blocks + 1; i < rb->block_size; i++) {
    bitmap_buffer[i] = 0;
  }
  if (write_block(1, bitmap_buffer) < 0) {
    return NULL;
  }
  // Creating a the file
  my_file * file = malloc(sizeof(my_file));
  Byte data[rb->block_size];
  if (read_block(new_inode->blocks[0], data) < 0) {
    return NULL;
  }
  file->inode_num = direntry.inode_num;
  file->inode = new_inode;
  file->pos = 0;
  file->buffer = data;
  file->dirty = 0;

  _u32 buf_new[8];
  buf_new[0] = new_inode->size;
  for (int i = 1; i < 8; i++) {
    buf_new[i] = new_inode->blocks[i-1];
  }
  // Write the file's inode
  if (write_inode(file->inode_num, buf_new) < 0) {
    return NULL;
  }
  return file;
}

/*writes num bytes from file into buffer. Returns 0 on success.*/
int my_fputc(my_file *file, Byte *buffer, _u32 num) {
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return -1;
  }
  _u32 current_size = file->inode->size;
  // Case when pos == 0
  if (file->pos == 0) {
    // the amount of bytes we write is equal to or exceeds our file size
    if (num >= current_size) {
      file->inode->size = num;
    }
  } else {
    // pos is > 0
    if (file->pos + num > current_size) {
      file->inode->size = file->pos + num;
    }
  }

  _u32 inode_buffer[8];
  if (read_inode(file->inode_num, inode_buffer) < 0) {
    return -1;
  }
  inode_buffer[0] = file->inode->size;

  if (write_inode(file->inode_num, inode_buffer) < 0) {
    return -1;
  }

  fseek(fp, file->inode->blocks[0] * rb->block_size + file->pos, SEEK_SET);
  if (fwrite(buffer, sizeof(Byte), num, fp) < 0) {
    return -1;
  }

  free(rb);
  return 0;
}

/* Closes the file and does any cleanup necessary. Returns 0 on success.*/
int my_fclose(my_file *file) {
  free(file->inode);
  free(file);
  return 0;
}

/*reads num bytes from file into buffer. Returns 0 on success.*/
int my_fgetc(my_file *file, Byte *buffer, _u32 num) {
  if (num < 0 || num > file->inode->size) {
    return -1;
  }
  for (int i = 0; i < num; i++) {
    buffer[i] = file->buffer[i];
  }
  return 0;
}

/*sets the current position for reading/writing to pos within the file. Returns 0 on success.*/
int my_fseek(my_file *file, _u32 pos) {
  if (file == NULL) {
    return -1;
  }
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return -1;
  }
  if (pos < 0 || pos > file->inode->size - 1) {
    free(rb);
    return -1;
  }
  _u32 diff = pos - file->pos;
  file->pos = pos;
  if (diff == 0) {
    return 0;
  } else if (diff > 0) {
    file->buffer += diff;
  } else {
    file->buffer -= diff;
  }
  return 0;
}

/* Makes a directory. name is either a full path (if it begins with '/', 
or a relative path with regards to the current location in the file system. 
All entries except the last must already exist. Returns 0 on success.*/
int mkdir(char *name) {
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return -1;
  }
  // Read root directory inode
  _u32 inode_buffer[8];
  // hardcoding for root directory for now
  if (read_inode(0, inode_buffer) < 0) {
    free(rb);
    return -1;
  }
  // Root directory inode
  inode_t root_dir_inode;
  root_dir_inode.size = inode_buffer[0];
  for (int i = 0; i < 7; i++) {
    root_dir_inode.blocks[i] = inode_buffer[i+1];
  }

  // Read root dir block
  Byte root_dir_buffer[rb->block_size];
  if (read_block(root_dir_inode.blocks[0], root_dir_buffer) < 0) {
    free(rb);
    return -1;
  }

  // Check if the path is absolute
  int is_absolute = 0;
  if (name[0] == '/') {
    is_absolute = 1;
  }

  int name_length = strlen(name) + 1;
  // new_name is only used if the path is absolute!!!
  char new_name[name_length-1];
  if (is_absolute == 1) {
    // decrement the length since we don't include '/'
    name_length -= 1;
    for (int i = 0; i < name_length + 1; i++) {
      new_name[i] = name[i+1];
    }
    name = new_name;
  }
  // Check that directory already exists
  for (int i = 0; i < rb->block_size; i++) {
    // Check if entry with type 'D' exists
    if (root_dir_buffer[i] == 68) {
      // Check if length of that entry matches the directory name's length
      if (root_dir_buffer[i+1] == name_length) {
        int k = 0;
        for (int j = i + 2; j < i + 2 + name_length; j++) {
          if (root_dir_buffer[j] == name[k]) {
            k++;
          }
        }
        if (k == name_length) { // Directory already exists
          return -1;
        }
      }
    }
  }
  // If we're here, directory doesn't exist
  _u32 u32_buffer[1];
  // Read num of directory entries
  fseek(fp, root_dir_inode.blocks[0] * rb->block_size, SEEK_SET);
  if (fread(u32_buffer, sizeof(_u32), 1, fp) < 0) {
    return -1;
  }
  _u32 num_root_dir_entries[1];
  // Incrementing number of directory entries
  num_root_dir_entries[0] = u32_buffer[0] + 1;
  fseek(fp, root_dir_inode.blocks[0] * rb->block_size, SEEK_SET);
  if (fwrite(num_root_dir_entries, sizeof(_u32), 1, fp) < 0) {
    return -1;
  }
  // Creating a new direntry for the directory
  direntry_t direntry;
  direntry.inode_num = get_first_free_inode(); // index of the inode where the directory data will be stored
  direntry.type = 'D';
  direntry.name_length = name_length; // + 1 for null terminated string
  direntry.name = name;

  // Write direntry to disk
  _u32 buff[1];
  buff[0] = direntry.inode_num;
  fseek(fp, root_dir_inode.blocks[0] * rb->block_size + root_dir_inode.size, SEEK_SET);
  if (fwrite(buff, sizeof(_u32), 1, fp) < 0) {
    return -1;
  }
  buff[0] = direntry.type;
  if (fwrite(buff, sizeof(Byte), 1, fp) < 0) {
    return -1;
  }
  buff[0] = direntry.name_length;
  if (fwrite(buff, sizeof(Byte), 1, fp) < 0) {
    return -1;
  }
  if (fwrite(direntry.name, sizeof(Byte) * name_length, 1, fp) < 0) {
    return -1;
  }

  // Updating size of directory
  inode_buffer[0] += 6 + name_length;
  // Hardocding for root directory inode for now
  if (write_inode(0, inode_buffer) < 0) {
    return -1;
  }

  // Creating an inode for the file
  inode_t * new_inode = malloc(sizeof(inode_t));
  // Initially the size of the directory is 0
  new_inode->size = 0;
  // Allocating the first free block for the directory
  new_inode->blocks[0] = rb->num_blocks - num_free_blocks();
  for (int i = 1; i < 7; i++){
    new_inode->blocks[i] = 0;
  }
  _u32 buf_new[8];
  buf_new[0] = new_inode->size;
  for (int i = 1; i < 8; i++) {
    buf_new[i] = new_inode->blocks[i-1];
  }
  // Write the file's inode
  if (write_inode(direntry.inode_num, buf_new) < 0) {
    return -1;
  }

  // Update bitmap
  if (update_bitmap(1) < 0) {
    return -1;
  }

  return 0;
}

/* Returns the full path to the current directory*/
char *cwd(void) {
  if (curr_dir == NULL) {
    return NULL;
  }
  return curr_dir;
}

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
        return free_inode_index;
      }
    }
  }
  return -1;
}

// Reads inode at index in inode table into buffer
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
  free(rb);
  return 0;
}
 // Writes inode to index in inode table
int write_inode(_u32 index, _u32 *buffer) {
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
  if (fwrite(buffer, sizeof(_u32), 8, fp) < 0) {
    return -1;
  }
  free(rb);
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

/* Increments the occupied bits by num
and updates the bitmap accordingly */
int update_bitmap(_u32 num) {
  rootblock_t * rb = get_rootblock();
  if (rb == NULL) {
    return -1;
  }
  // Changing the free block bitmap
  int num_occupied_blocks = rb->num_blocks - num_free_blocks() + num;
  // number of 8 positive-bit (FF) bytes
  _u32 num_8_bit_bytes = 0;
  for (int i = 7; i < num_occupied_blocks; i = i + 8) {
    num_8_bit_bytes += 1;
  }
  int blocks_to_write = 1; // we'll need to write 1 block in any case
  int temp = num_8_bit_bytes;
  for (int i = 0; i < rb->num_free_bitmap_blocks - 1; i++) {
    if (temp <= rb->block_size) {
      break;
    } else if (temp - rb->block_size > 0) {
      temp -= rb->block_size;
      blocks_to_write += 1;
    }
  }
  for (int i = 0; i < blocks_to_write; i++) {
    Byte bitmap_buffer[rb->block_size];
    // Write 8 positive-bit (FF) bytes
    if (num_8_bit_bytes > rb->block_size) { // not the last block
      for (int i = 0; i < rb->block_size; i++) {
        bitmap_buffer[i] = 255;
      }
      num_8_bit_bytes -= rb->block_size;
      if (write_block(i+1, bitmap_buffer) < 0) {
        return -1;
      }
    } else { // last block case
      for (int i = 0; i < num_8_bit_bytes; i++) {
        bitmap_buffer[i] = 255;
      }
      int remainder = num_occupied_blocks % 8;
      if (remainder != 0) {
        int result = 2;
        for (int i = 1; i < remainder; i++) {
          result *= 2;
        }
        result -= 1;
        bitmap_buffer[num_8_bit_bytes] = result;
      } else {
        bitmap_buffer[num_8_bit_bytes] = 1;
      }
      for (int i = num_8_bit_bytes + 1; i < rb->block_size; i++) {
        bitmap_buffer[i] = 0;
      }
      if (write_block(i + 1, bitmap_buffer) < 0) {
        return -1;
      }
    }
  }
  return 0;
}

int test() {

}