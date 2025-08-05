#include "inode.h"
#include "vfs.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <math.h>
#include <string.h>

#define BLOCKSIZE 512

int main(int argc, char* argv[]){

  if(argc < 2){
    printf("usage: ./format disk_name -s filesize_in_mb (disk_name is required)\n");
    return 1;
  }
  int NUMBLOCKS = 2048;
  int NUMINODES = 2048;
  if(argc >= 4){
    if(strcmp(argv[2], "-s") == 0){
      NUMBLOCKS = ceil(atof(argv[3]) * 1024 * 2); // mb to blocks
      NUMINODES = ceil(atof(argv[3]) * 1024 * 2); // as many inodes as there are blocks
    }
  } else if (argc != 2){
    printf("usage: ./format disk_name -s filesize_in_mb (disk_name is required)\n");
    return 1;
  }
  
  int inode_block_filler = ((int)ceil((float)(sizeof(inode)*NUMINODES) / (float)BLOCKSIZE) * BLOCKSIZE) - (sizeof(inode)*NUMINODES);
  int bitmap_filler = ((int)ceil((float)(sizeof(char)*NUMBLOCKS) / (float)BLOCKSIZE) * BLOCKSIZE) - (sizeof(char)*NUMBLOCKS);

  char diskName[128];
  char* bitmap = (char *)malloc(sizeof(char) * NUMBLOCKS);
  memset(bitmap, 0, NUMBLOCKS);
  
  memset(diskName, 0, 128);
  strncpy(diskName, argv[1], strlen(argv[1]));

  char super[BLOCKSIZE];
  char* blocks = (char*) malloc(sizeof(char) * BLOCKSIZE * (NUMBLOCKS));

  inode* inodes = (inode*) malloc(sizeof(inode) * NUMINODES);
  superblock* sb = (superblock*) super;

  memset(super, 0, BLOCKSIZE);
  memset(blocks, 0, BLOCKSIZE*(NUMBLOCKS));
  memset(inodes, -1, sizeof(inode)*NUMINODES);

  sb->size = BLOCKSIZE;
  sb->inode_offset = 512;
  sb->bitmap_offset = sb->inode_offset + (sizeof(inode) * NUMINODES) + inode_block_filler;
  sb->data_offset = sb->bitmap_offset + (sizeof(char) * NUMBLOCKS) + bitmap_filler;
  sb->free_inode = 1;
  sb->free_block = 1;
  sb->num_inodes = NUMINODES;

  inodes[0].nlink = 1;
  inodes[0].size = sizeof(vdir_t) * 2;
  inodes[0].dblocks[0] = 0;
  bitmap[inodes[0].dblocks[0]] = 1;

  vdir_t* directory = (vdir_t*) blocks;

  directory[0].inode = 0;
  directory[0].isdir = D;
  strncpy(directory[0].name, "..", 8);

  directory[1].inode = 0;
  directory[1].isdir = D;
  strncpy(directory[1].name, ".", 8);

  FILE *fp;
  fp = fopen(argv[1], "wb");

  char filler[] = {0};

  fwrite(super, 512, 1, fp);
  fwrite(inodes, sizeof(inode), NUMINODES, fp);
  fwrite(filler, sizeof(char), inode_block_filler, fp);
  fwrite(bitmap, sizeof(char)*NUMBLOCKS, 1, fp);
  fwrite(filler, sizeof(char), bitmap_filler, fp);
  fwrite(blocks, sizeof(char) * BLOCKSIZE * (NUMBLOCKS), 1, fp);

  fclose(fp);
  free(inodes);
  free(blocks);
  free(bitmap);

  return 0;
}

