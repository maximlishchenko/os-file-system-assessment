#include <string.h>
#include "filesystem.h"

int main()
{
    format("D2.disk",128,4096,80);
    unload();
    load("D2.disk",0);

    my_file *file=my_fopen("hello_world");
    my_fputc(file,(Byte *) "hello",6);
    my_fclose(file);
    unload();
    load("D2.disk",0);
    my_file *file2=my_fopen("hello_world2");
    my_fputc(file2,(Byte *) "hello",6);
    unload();
    load("D2.disk",0);
    my_fclose(file2);
    mkdir("testdir1");
    mkdir("/testdir1");
    my_file *file3=my_fopen("hello_world3");
    my_fputc(file3,(Byte *) "hello",6);
    my_fclose(file3);
    mkdir("/testdir2");
    unload();
    load("D2.disk",0);
    printf("%d %d\n",num_free_blocks(),num_free_inodes());
    if (num_free_inodes()!=74)
        return -1;
    if (num_free_blocks()!=4065)
        return -1;

    load("D2.disk",0);
    file=my_fopen("hello_world");
    Byte *buffer=malloc(6);
    my_fgetc(file,buffer,6);
    if (strcmp((char *)buffer,"hello") == 0) {
        printf("D2 PASS\n");
    }
    return strcmp((char *)buffer,"hello");
}