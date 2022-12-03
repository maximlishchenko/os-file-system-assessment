#include "filesystem.h"
#include <string.h>

int main()
{
    format("C3.disk",128,4096,80);
    unload();
    load("C3.disk",0);
    mkdir("testdir1");
    mkdir("testdir2");
    if (strcmp(cwd(),"/")!=0)
        return -1;

    chdir("testdir1");
    if (strcmp(cwd(),"/testdir1/")!=0)
        return -1;
    mkdir("testdir3");
    if (strcmp(ls(),".\n..\ntestdir3\n")!=0)
        return -1;

    chdir("testdir3");
    mkdir("testdir4");
    mkdir("testdir5");
    if (strcmp(ls(),".\n..\ntestdir4\ntestdir5\n")!=0)
        return -1;

    
    chdir("..");
    if (strcmp(cwd(),"/testdir1/")!=0)
        return -1;
    
    chdir("..");
    chdir("testdir2");
    if (strcmp(cwd(),"/testdir2/")!=0)
        return -1;

    if (num_free_blocks()!=4065)
        return -1;
    
    if (num_free_inodes()!=74)
        return -1;

    unload();

    
    return 0;

}