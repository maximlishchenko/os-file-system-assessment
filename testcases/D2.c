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
    printf("%d %d",num_free_blocks(),num_free_inodes());
    if (num_free_inodes()!=78)
        return -1;
    if (num_free_blocks()!=4069)
        return -1;

    load("D2.disk",0);
    file=my_fopen("hello_world");
    Byte *buffer=malloc(6);
    my_fgetc(file,buffer,6);
    return strcmp((char *)buffer,"hello");
}