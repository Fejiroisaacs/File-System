#include "vfs.h"
#include "inode.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

long fileSize;

int main() {
  
  const char* disk = "disktest";
  init_fs((char *)disk);

  char data[1200] = "She counted. One. She could hear the steps coming closer. Two. Puffs of breath could be seen coming from his mouth. Three. He stopped beside her. Four. She pulled the trigger of the gun. As she sat watching the world go by, something caught her eye. It wasn't so much its color or shape, but the way it was moving. She squinted to see if she could better understand what it was and where it was going, but it didn't help. As she continued to stare into the distance, she didn't understand why this uneasiness was building inside her body. She felt like she should get up and run. If only she could make out what it was. At that moment, she comprehended what it was and where it was heading, and she knew her life would never be the same. It was so great to hear from you today and it was such weird timing, he said. This is going to sound funny and a little strange, but you were in a dream I had just a couple of days ago. I'd love to get together and tell you about it if you're up for a cup of coffee, he continued, laying the trap he'd been planning for years.\n";
  char read_data[1200] = {0};

  strcat(data, "This is me testing multi block writes\n");
  vfd_t fd = f_open(fs_root, "idan.txt", 1);
  vnode_t* node = find_dir_vnode(fs_root, (char*)"idan.txt", 0);

  size_t write_size = f_write(node, data, strlen(data), 1, fd);
  f_seek(node, fd, 0, SEEK_SET); // return fd to the start of the file

  size_t read_size = f_read(node, read_data, strlen(data), 1, fd);
  assert(read_size == write_size);
  
  printf("%s\n", read_data);
  
  vstat_t file_stats;
  f_stat(node, fd, &file_stats);
  
  assert(strlen(data) == (size_t)file_stats.st_size);
  
  f_seek(node, fd, 0, SEEK_END); // return fd to the start of the file
  printf("\nTesting more write:\n");
  char more_data[100] = "I am appending more to the file. Hopefully this works. I will be so happy!!\n";
  write_size = f_write(node, more_data, strlen(more_data), 1, fd);
    
  f_seek(node, fd, 0, SEEK_SET); // return fd to the start of the file
  read_size = f_read(node, read_data, strlen(data)+strlen(more_data), 1, fd);
  printf("%s\n", read_data);

  f_stat(node, fd, &file_stats);
  
  assert(strlen(data)+strlen(more_data) == (size_t)file_stats.st_size);

  f_remove(node, fd);
  assert(find_dir_vnode(fs_root, (char*)"idan.txt", 0) == NULL);
  
  cleanup_fs();
  return 0;
}