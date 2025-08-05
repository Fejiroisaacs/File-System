#include "inode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]){
  if(argc != 2){
    printf("Usage: ./printdisk <disk_image>\n");
    return 1;
  }

  FILE *file = fopen(argv[1], "rb");
  if (!file) {
    perror("Failed to open disk image");
    return 1;
  }

  superblock sb;
  fread(&sb, sizeof(superblock), 1, file);

  printf("Superblock Info:\n");
  printf("  Block size: %d\n", sb.size);
  printf("  Inode offset: %d\n", sb.inode_offset);
  printf("  Bitmap offset: %d\n", sb.bitmap_offset);
  printf("  Data offset: %d\n", sb.data_offset);
  printf("  Free inode: %d\n", sb.free_inode);
  printf("  Free block: %d\n", sb.free_block);
  printf("  Number of inodes: %d\n\n", sb.num_inodes);

  int num_inodes = sb.num_inodes;
  inode* inodes = (inode*) malloc(sizeof(inode) * num_inodes);
  if (!inodes) {
    printf("Failed to allocate memory for inodes\n");
    fclose(file);
    return 1;
  }

  fseek(file, sb.inode_offset, SEEK_SET);
  fread(inodes, sizeof(inode), num_inodes, file);

  int total_used_blocks = 0;
  printf("Used Inodes:\n");

  for(int i = 0; i < num_inodes; i++){
    if(inodes[i].nlink > 0){
      printf("Inode %d:\n", i);
      printf("  Next inode: %d\n", inodes[i].next_inode);
      printf("  Protect: %d\n", inodes[i].protect);
      printf("  Nlink: %d\n", inodes[i].nlink);
      printf("  Size: %d\n", inodes[i].size);
      printf("  Direct data blocks:\n");

      for (int j = 0; j < N_DBLOCKS; j++) {
        if(inodes[i].dblocks[j] != -1) printf("    [%d] %d\n", j, inodes[i].dblocks[j]);
        if (inodes[i].dblocks[j] != -1) total_used_blocks++;
      }
      printf("\n");
    }
  }

  printf("Total used blocks: %d\n", total_used_blocks);

  free(inodes);
  fclose(file);
  return 0;
}
