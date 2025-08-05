#include <assert.h>
#include <fcntl.h>
#include "vfs.h"

long fileSize;
int main() {
  extern vnode_t* fs_root;
  const char* disk = "disktest";
  init_fs((char *)disk);
  int ret = f_open(fs_root, "newfile.txt", 0644 | O_CREAT);
  assert(ret == 0);
  cleanup_fs();
  return 0;
}

