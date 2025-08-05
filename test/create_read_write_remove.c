#include "vfs.h"
#include "inode.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

long fileSize;

int main() {
  
  const char* disk = "disktest";
  init_fs((char *)disk);

  char data[64] = {0};
  char read_data[64] = {0};

  strncpy(data, "This is me testing single block writes", 63);
  vfd_t fd = f_open(fs_root, "idan.txt", 1);
  vnode_t* node = find_dir_vnode(fs_root, (char*)"idan.txt", 0);

  printf("the allocated block is: %d\n", node->fs_info.unixfs.inode);

  size_t write_size = f_write(node, data, 64, 1, fd);
  f_seek(node, fd, 0, SEEK_SET); // return fd to the start of the file

  size_t read_size = f_read(node, read_data, 64, 1, fd);
  assert(read_size == write_size);
  
  printf("%s\n", read_data);
  printf("The sizes are: %ld-%ld\n", write_size, read_size);

  f_remove(node, fd);

  assert(find_dir_vnode(fs_root, (char*)"idan.txt", 0) == NULL);

  cleanup_fs();
  return 0;
}