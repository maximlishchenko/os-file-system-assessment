#include "filesystem.h"

int main()
{
    format("D1.disk",128,4096,80);
    unload();
    load("D1.disk",0);
    mkdir("/testdir1");
    mkdir("/testdir2");
    mkdir("/testdir1");
    mkdir("testdir1");
    my_file *file3=my_fopen("hello_world3");
    my_fputc(file3,(Byte *) "hello",6);
    my_fclose(file3);
    printf("num_free_blocks() = %d\n", num_free_blocks());
    printf("num_free_inodes() = %d\n", num_free_inodes());
    if (num_free_blocks()!=4067)
        return -1;
    if (num_free_inodes()!=76)
        return -1;
    unload();

    //assessment will also examine disk image using premade code, e.g.,
    //load("D2.disk");
    //char *dir=ls("/");
    //return strcmp(dir,".\n..\ntestdir1\ntestdir2\n");
    printf("D1 PASS\n");
    return 0;
}