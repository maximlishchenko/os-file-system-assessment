#include "filesystem.h"

int main()
{
    //E3.disk is created with format("E3.disk",128,4096,80)
    load("E3Sample.disk",0);
    rootblock_t *rb=get_rootblock();
    _u32 fb=num_free_blocks();
    _u32 fi=num_free_inodes();

    //printf("%d %d %d %d %d %d\n",rb->block_size,rb->num_blocks,rb->num_free_bitmap_blocks,rb->num_inode_table_blocks,fb,fi);
    if (rb->block_size!=128)
        return -1;
    if (rb->num_blocks!=4096)
        return -1;
    if (rb->num_free_bitmap_blocks!=4)
        return -1;
    if (rb->num_inode_table_blocks!=20)
        return -1;

    if (fb!=4070)
        return -1;
    
    if (fi!=79)
        return -1;
    printf("E1 PASS\n");
    return 0;
}


