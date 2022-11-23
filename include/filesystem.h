#ifndef FILESYS_H
#define FILESYS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/*Convenience definitions*/
typedef uint32_t _u32; 
typedef unsigned char Byte;

/*An inode contains the size (in bytes) of the content, 5 directly referenced blocks, 
1 single indirectly referenced block, and 1 doubly indirectly referenced block.*/
typedef struct inode {
    _u32 size;
    _u32 blocks[7];
} inode_t;

typedef struct rootblock {
    _u32 block_size;
    _u32 num_blocks;
    _u32 num_free_bitmap_blocks;
    _u32 num_inode_table_blocks;
} rootblock_t;

/*A directory entry consists of an index to the inode for the entry. 
The next Byte records whether it is a file ('F') or directory ('D'). 
Following this, we record the number of characters in the entry name, 
and a pointer to a null terminated string containing the name.*/
typedef struct direntry {
    _u32 inode_num;
    Byte type;
    Byte name_length;
    char *name;
} direntry_t;

/*A directory contains the number of direntrys within it and a pointer to the array of direntrys*/
typedef struct directory {
    _u32 num_entries;
    direntry_t *dir_entries;
} directory_t;


/*The my_file structure is used internally when a program opens a file. 
It records the index of the inode associated with the file and - to prevent 
repeatedly reading it from disk - also has the inode itself. pos records the 
current position in the file. The buffer entry is a block size sized buffer 
containing the current block the file is looking at. Dirty records whether 
the buffer has been modified (i.e., written to). */
typedef struct my_file {
    _u32 inode_num;
    inode_t *inode;
    _u32 pos;
    Byte *buffer;
    char dirty;
} my_file;

/**************** PRIMITIVE ACCESS OPERATIONS ********************/
/*Read a disk block from index, writing it to buffer. Returns 0 on success, a negative number on error.*/
int read_block(_u32 index, Byte *buffer);
/*Write a disk block, filling it with content at index. content should be 
the same size as the block size. Returns 0 on success, a negative number on error*/
int write_block(_u32 index, Byte *content);

/**************** INITIALIZATION OPERATIONS *********************/
/*Formats the disk creating appropriate root blocks, free bitmap blocks, 
inode blocks and root directory. returns 0 on success, a negative number on error*/
int format(char *diskname, _u32 block_size, _u32 num_blocks, _u32 num_inodes);

/*Loads a filesystem which has already been formatted. The write_buffer_size 
records how many blocks must change before they are written back to the disk. Returns 0 on success.*/
int load(char *diskname, _u32 write_buffer_size);

/*returns the rootblock*/
rootblock_t *get_rootblock();

/*returns the number of free blocks on the disk*/
_u32 num_free_blocks();

/*returns the number of free inodes on the disk*/
_u32 num_free_inodes();

/*Unloads the loaded file system. Returns 0 on success.*/
int unload(void);

/*This function writes all blocks that need to  be written back to the disk (see the load function's write_buffer_size for detals)*/
// void fsync(void);

/**************** DIRECTORY OPERATIONS *********************/
/* Makes a directory. name is either a full path (if it begins with '/', 
or a relative path with regards to the current location in the file system. 
All entries except the last must already exist. Returns 0 on success.*/
// int mkdir(char *name);
/* Removes a directory. name is either a full path (if it begins with '/', 
or a relative path with regards to the current location in the file system. 
All entries except the last must already exist. If recursive is true, then 
all entries below the removed directory are also removed. If false and the 
directory is not empty, return an error. Returns 0 on success.*/
// int rmdir(char *name,char recursive);
/* Sets the current working directory. name is either a full path 
(if it begins with '/', or a relative path with regards to the current location 
in the file system. All entries must already exist. Returns 0 on success.*/
// int chdir(char *name);
/* Returns the full path to the current directory*/
char *cwd(void);
/* Returns a sorted list of directory entries as a string separated by \n*/
char *ls(void);

/**************** FILE OPERATIONS *********************/
/* removes a file named name. name is either a full path (if it begins with '/', or a relative path with regards to the current location in the file system. Returns 0 on success.*/
int rm(char *name);
/* moves a file (pointed to by the string src) to dest. We assume dest ends with a filename which identifies the new name of the file being moved. Returns 0 on success.*/
int mv(char *src, char *dest);
/* copies a file (pointed to by the string src) to dest.  The file is renamed to the last element of dest. src and dest can be absolute or relative paths. Returns 0 on success.*/
int cp(char *src, char *dest);

/**************** FILE CREATION and I/O OPERATIONS ************/
/* opens a file for reading and writing. returns NULL on error. filename is an absolute or relative path to a file.*/
my_file *my_fopen(char *filename);
/* closes the file and does any cleanup necessary. Returns 0 on success.*/
int my_fclose(my_file *file);
/*reads num bytes from file into buffer. Returns 0 on success.*/
int my_fgetc(my_file *file, Byte *buffer, _u32 num);
/*writes num bytes from file into buffer. Returns 0 on success.*/
int my_fputc(my_file *file, Byte *buffer, _u32 num);
/*sets the current position for reading/writing to pos within the file. Returns 0 on success.*/
int my_fseek(my_file *file, _u32 pos);

/************** FILE LOCKING FUNCTIONS ***********************/
/* locks a file for access. Any other process trying to get the lock will block until the file is unlocked. Returns 0 on success. Note the file must exist, and is referred to by a full or relative path.*/
int lock(char *file);
/* releases the lock. The lock must be held by the process for this to work. Returns 0 on success*/
int unlock(char *file);

/*************** FILE SYSTEM VERIFICATION FUNCTIONS***********/
int fsck(void);

//own helper FUNCTIONS
int get_positive_bits(Byte b);

#endif