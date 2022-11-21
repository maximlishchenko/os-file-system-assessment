#include "filesystem.h"

int main()
{
    format("D1.disk",128,4096,80);
    unload();
    load("D1.disk",0);
    mkdir("/testdir1");
    mkdir("/testdir2"); 

    if (num_free_blocks()!=4068)
        return -1;
    
    if (num_free_inodes()!=77)
        return -1;
    

    unload();

    //assessment will also examine disk image using premade code, e.g.,
    //load("D2.disk");
    //char *dir=ls("/");
    //return strcmp(dir,".\n..\ntestdir1\ntestdir2\n");
    return 0;

}