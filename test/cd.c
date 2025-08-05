#include "vfs.h"
#include "inode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

long fileSize;
vnode_t* current_dir;
vnode_t* print_file_contents(const char* filename, vfd_t fd) {
    vnode_t* node = find_dir_vnode(current_dir, (char*)filename, 0);
    char buffer[64] = {0};
    f_seek(node, fd, 0, SEEK_SET);
    f_read(node, buffer, 64, 1, fd);
    printf("\nContents of %s: %s\n", filename, buffer);
    return node;
}


int main() {
    const char* disk = "disktest";
    init_fs((char *)disk);

    current_dir = fs_root;
    printf("root contents:\n");
    vfd_t fd = f_opendir(fs_root, ".");
    vdir_t* entry = f_readdir(fs_root, fd);
    while (entry) {
        printf("  %s\n", entry->name);
        entry = f_readdir(fs_root, fd);
    }
    f_closedir(fs_root, fd);

    int ret = f_mkdir(fs_root, "home");
    assert(ret == 0);
    printf("\nCreated directory: home\n");

    vnode_t* home = find_dir_vnode(fs_root, (char*)"home", 0);
    assert(home && home->type == D);
    current_dir = home;
    printf("cd into home\n");

    printf("Creating files: file1.txt, file2.txt, file3.txt\n");
    vfd_t fd1 = f_open(current_dir, "file1.txt", 1);
    vfd_t fd2 = f_open(current_dir, "file2.txt", 1);
    vfd_t fd3 = f_open(current_dir, "file3.txt", 1);
    assert(fd1 >= 0 && fd2 >= 0 && fd3 >= 0);

    char message[25] = "Hello from file 1";
    f_write(find_dir_vnode(current_dir, (char*)"file1.txt", 0), message, strlen(message), 1, fd1);
    message[16] = '2';
    f_write(find_dir_vnode(current_dir, (char*)"file2.txt", 0), message, strlen(message), 1, fd2);
    message[16] = '3';
    f_write(find_dir_vnode(current_dir, (char*)"file3.txt", 0), message, strlen(message), 1, fd3);

    printf("\nContents of home:\n");
    fd = f_opendir(current_dir, ".");
    entry = f_readdir(current_dir, fd);
    while (entry) {
        printf("  %s\n", entry->name);
        entry = f_readdir(current_dir, fd);
    }
    f_closedir(current_dir, fd);

    f_close(print_file_contents((char*)"file1.txt", fd1), fd1);
    f_close(print_file_contents((char*)"file2.txt", fd2), fd2);
    f_close(print_file_contents((char*)"file3.txt", fd3), fd3);

    cleanup_fs();

    return 0;
}
