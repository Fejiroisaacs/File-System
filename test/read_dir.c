#include "vfs.h"
#include "inode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

long fileSize;
int main(){
    const char* disk = "disktest";
    init_fs((char *)disk);
    vfd_t fd = f_opendir(fs_root, "."); // See below for specification
    printf("The fd is: %d\n", fd);
    vdir_t* entry = f_readdir(fs_root, fd);
    // you define vdir so the specifics might look different for you
    while (entry){
        printf("content: %s\n", entry->name); 
        entry = f_readdir(fs_root, fd);
    }
    printf("Making more files and printing\n");

    int ret = f_mkdir(fs_root, "test1");
    printf("the ret is: %d\n", ret);
    entry = f_readdir(fs_root, fd);
    while (entry){
        printf("content: %s\n", entry->name); 
        entry = f_readdir(fs_root, fd);
    }
    ret = f_mkdir(fs_root, "test2");
    assert(ret == -1);
    printf("Could not create test2: %s\n", strerror(fs_errno)); // no space in dir. max 3 dir entries per dir
    printf("the ret is: %d\n", ret);
    
    f_closedir(fs_root, fd);
    cleanup_fs();
}