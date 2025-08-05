/*----------------------------------------------
 * Author: Oghenefejiro Anigboro
 * Date: 4/15/2025
 * Description: virtual file system api
 ---------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include "vfs.h"
#include <cstring>
#include <string.h>
#include <math.h>
#include <errno.h>

#define MAXDIR = 3
// helper functions
void update_free_block(int index);
void update_free_inode(int index);
int check_existence(vdir_t* dir, char* fileName, int size);
void add_vnodes_recursive(vnode_t* parent, int index);
int f_create(vnode_t *dir, const char* filename, int flags);
size_t free_space();
size_t file_free_space(inode* node);

vnode_t* fs_root = NULL;
vector<vnode_t*> vnodes;
vector<open_table*> open_entries;
int open_entries_ct = 0;
char* file = NULL;
superblock *block = NULL;
inode *inodes = NULL;
char *bitmap = NULL;
int fs_errno = 0;
int vnode_num = 0;

vfd_t f_open(vnode_t *vn, const char *filename, int flags){
  open_table* dir;

  if(vn->type == D){
    if(f_create(vn, filename, flags) == -1) return -1;
  } else {
    for(open_table* open_entry: open_entries){
      if(strcmp(open_entry->name, filename) == 0 
          && vn->parent == open_entry->parent) return open_entry->fd;
    }
  }

  dir = (open_table*)malloc(sizeof(open_table));
  dir->fd = open_entries_ct++;
  dir->flags = flags;
  dir->parent = (vn->type == D) ? vn :  vn->parent;
  strcpy(dir->name, filename);

  inode* file_inode = NULL;
  for(vnode_t* node: vnodes){
    if(strcmp(node->name, filename) == 0){
      int file_index = node->fs_info.unixfs.inode;
      file_inode = &inodes[file_index]; break;
    }
  }

  if(!file_inode){
    fs_errno = EINVAL;
    return -1;
  } 

  int file_start = block->data_offset + (512 * file_inode->dblocks[0]);
  dir->position = file_start;
  int found_end = 0;

  for(int i = 0; i < 16; i++){
    if(file_inode->dblocks[i] == -1) {
      dir->end = block->data_offset + (512 * file_inode->dblocks[i]);
      found_end = 1;
    }
  }

  if(!found_end) dir->end = block->data_offset + (512 * file_inode->dblocks[15]);
  dir->curr_dblock = 0;
  dir->bytes_read = 0;

  open_entries.push_back(dir);

  return dir->fd;
}

size_t f_read(vnode_t *vn, void *data, size_t size, int num, vfd_t fd) {

  if (vn->type == D) {
    fs_errno = EISDIR;
    return 0;
  }

  if(fd < 0){
    fs_errno = EBADF;
    return 0;
  }

  size_t bytes_read = 0;
  int file_index = vn->fs_info.unixfs.inode;
  inode* file_inode = &inodes[file_index];

  if(size > (size_t)((block->size * 16) - file_inode->size)){
    fs_errno = EFBIG;
    return 0;
  } 

  if(open_entries[fd]->bytes_read >= file_inode->size) return 0;
  for(int i = open_entries[fd]->curr_dblock; i < 16; i++){
    if(file_inode->dblocks[i] == -1) break;

    int curr_pos = open_entries[fd]->position;
    size_t bytes_left = (block->data_offset + (512 * (file_inode->dblocks[i] + 1))) - curr_pos;

    size_t to_read = (size - bytes_read > bytes_left) ? bytes_left : size - bytes_read;
    memcpy((char*)data + bytes_read, file + curr_pos, to_read);
    bytes_read += to_read;

    open_entries[fd]->bytes_read += to_read;

    if(bytes_read < bytes_left){
      open_entries[fd]->position = open_entries[fd]->position + to_read;
    } else {
      if(i < 15){
        open_entries[fd]->position = block->data_offset + (512 * file_inode->dblocks[i+1]);
        open_entries[fd]->curr_dblock = i+1; 
      } 
      else {
        open_entries[fd]->position = block->data_offset + (512 * file_inode->dblocks[i]);
      }
    }
    if(bytes_read >= size || bytes_read >= (size_t)file_inode->size) break;
  }
  return bytes_read;
}

size_t f_write(vnode_t *vn, void *data, size_t size, int num, vfd_t fd){
  if(!vn || size == 0 || strlen((char*)data) < 1){
    fs_errno = EINVAL;
    return 0;
  }
  if(vn->type == D) {
    fs_errno = EISDIR;
    return 0;
  }
  if(size > 512*16 || size > free_space()){
    fs_errno = EFBIG;
    return 0;
  }

  size_t bytes_written = 0;
  int file_index = vn->fs_info.unixfs.inode;
  inode* file_inode = &inodes[file_index];

  if(size > file_free_space(file_inode)){
    fs_errno = EFBIG;
    return 0;
  }

  for(int i = open_entries[fd]->curr_dblock; i < 16; i++){
    open_entries[fd]->curr_dblock = i;
    if(file_inode->dblocks[i] == -1){
      file_inode->dblocks[i] = block->free_block;
      open_entries[fd]->position = block->data_offset + (512 * file_inode->dblocks[i]);
      bitmap[file_inode->dblocks[i]] = 1;
      update_free_block(0);
    }

    int curr_pos = open_entries[fd]->position;
    int space_left = (block->data_offset + (512 * (file_inode->dblocks[i] + 1))) - curr_pos;
    if(space_left < 512){
      size_t to_write = (size - bytes_written > 512) ? 512 : size - bytes_written; 
      memcpy(file + curr_pos, (char*)data + bytes_written, to_write); 
      bytes_written += to_write;
      if(i < 15){
        if(file_inode->dblocks[i+1] != -1){
          printf("THis is what it is: %d\n", file_inode->dblocks[i+1]);
          open_entries[fd]->position = block->data_offset + (512 * file_inode->dblocks[i+1]);
          open_entries[fd]->curr_dblock = i+1;
        }
      } 
      else {
        open_entries[fd]->position = block->data_offset + (512 * file_inode->dblocks[i]);
      }
    } else{
      size_t to_write = (size - bytes_written > 512) ? 512 : size - bytes_written; 
      memcpy(file + curr_pos, (char*)data + bytes_written, to_write); 
      bytes_written += to_write;
      if(bytes_written < size && i < 15) {
        if(file_inode->dblocks[i+1] != -1){
          open_entries[fd]->position = block->data_offset + (512 * file_inode->dblocks[i+1]);
          open_entries[fd]->curr_dblock = i+1;
        }
      }
      else open_entries[fd]->position += to_write;
    }

    if(bytes_written >= size) break;
  }

  file_inode->size += size;
  return size;
}

int f_close(vnode_t *vn, vfd_t fd){
  if(!vn) return -1;
  if(vn->type != F) {
    fs_errno = EISDIR;
    return -1;
  }
  if(strcmp(open_entries[fd]->name, vn->name) != 0 || open_entries[fd]->parent != vn->parent){
    fs_errno = EBADF;
    return -1;
  } 

  int ret = f_seek(vn, fd, 0, SEEK_SET);
  if(ret == -1) return -1;

  memset(open_entries[fd], -1, sizeof(open_table));

  return 0;
}

int f_seek(vnode_t *vn, vfd_t fd, off_t offset, int whence) {
  if (whence < 0 || whence > 2) {
    fs_errno = EINVAL;
    return -1;
  }

  if (vn->type != F){
    fs_errno = EISDIR;
    return -1;
  }

  int file_index = vn->fs_info.unixfs.inode;
  inode *file_inode = &inodes[file_index];
  int block_size = block->size;

  int new_pos;

  if (whence == SEEK_SET) {
    if (offset < 0 || offset > file_inode->size) return -1;
    new_pos = offset;
  } else if (whence == SEEK_CUR) {
    int curr_offset = open_entries[fd]->bytes_read;
    new_pos = curr_offset + offset;
    if (new_pos < 0 || new_pos > file_inode->size) return -1;
  } else if (whence == SEEK_END) new_pos = file_inode->size;

  int file_block = new_pos / block_size;
  int spill = new_pos % block_size;

  if (file_block >= 16 || file_inode->dblocks[file_block] == -1){ 
    fs_errno = ENXIO; 
    return -1;
  }

  open_entries[fd]->position = block->data_offset + (block_size * file_inode->dblocks[file_block]) + spill;
  open_entries[fd]->curr_dblock = file_block;
  open_entries[fd]->bytes_read = new_pos; 

  return new_pos;
}

int f_stat(vnode_t *vn, vfd_t fd, vstat_t* stats){

  if(!vn){
    fs_errno = EINVAL;
    return -1;
  }
  if(vn->type == -1) return -1;
  int file_index = vn->fs_info.unixfs.inode;
  inode* file_inode = &inodes[file_index];

  stats->st_inode = vn->fs_info.unixfs.inode;
  stats->st_perms = vn->permissions;
  stats->st_size = file_inode->size;
  stats->st_type = vn->type;
  int num_blocks = 0;

  for(int i = 0; i < 16; i++){
    if(file_inode->dblocks[i] != -1) num_blocks++;
  }

  stats->st_blocks = num_blocks;

  return 0;
}

int f_remove(vnode_t *vn, vfd_t fd){

  if(fd < 0) {
    fs_errno = EBADF;
    return -1;
  }
  if(vn->type == -1) {
    fs_errno = EINVAL;
    return -1;
  }
  if(vn->type != F) {
    fs_errno = EISDIR;
    return -1;
  }

  for(open_table* open_entry: open_entries){
    if(strcmp(open_entry->name, vn->name) == 0 && vn->parent == open_entry->parent){
      memset(open_entry, -1, sizeof(open_table));
      break;
    }
  }

  int inode_index = vn->fs_info.unixfs.inode;
  inode* curr_inode = &inodes[inode_index];
  for(int i =0; i < 16; i++){
    if(curr_inode->dblocks[i] != -1) bitmap[curr_inode->dblocks[i]] = 0;
  }

  update_free_inode(vn->fs_info.unixfs.inode);
  update_free_block(curr_inode->dblocks[0]);

  vnode_t* dir_vnode = vn->parent;
  if (!dir_vnode) return -1;

  int dir_index = dir_vnode->fs_info.unixfs.inode;
  int file_exist = 0;
  int num_dir_files = (int)(inodes[dir_index].size / sizeof(vdir_t));

  vdir_t* directory = (vdir_t*)(file + block->data_offset + (inodes[dir_index].dblocks[0] * 512));

  for (int i = 0; i < num_dir_files; i++) {
    if (strcmp(directory[i].name, vn->name) == 0) {
      file_exist = 1;
      if (i < num_dir_files - 1) memcpy(&directory[i], &directory[i + 1], sizeof(vdir_t) * (num_dir_files - i - 1));
      memset(&directory[num_dir_files - 1], 0, sizeof(vdir_t));
      break;
    }
  }

  if (!file_exist) return -1;

  inode* dir_inode = &inodes[dir_index];
  dir_inode->size = sizeof(vdir_t) * (num_dir_files-1);

  memset(curr_inode, -1, sizeof(inode));
  memset((void*)vn, -1, sizeof(vnode_t));

  return 0;
}

vfd_t f_opendir(vnode_t *vn, const char* path){

  if(!vn){
    fs_errno = EINVAL;
    return -1;
  } 
  if(vn->type != D){
    fs_errno = ENOTDIR;
    return -1;
  } 

  open_table* dir;

  for(open_table* open_entry: open_entries){
    if(strcmp(open_entry->name, vn->name) == 0 
        && vn->parent == open_entry->parent) return open_entry->fd;
  }

  dir = (open_table*)malloc(sizeof(open_table));
  dir->fd = open_entries_ct++;
  dir->flags = 0;
  dir->parent = vn->parent;
  strcpy(dir->name, vn->name);

  inode* dir_inode = NULL;
  for(vnode_t* node: vnodes){
    if(strcmp(node->name, vn->name) == 0){
      int file_index = node->fs_info.unixfs.inode;
      dir_inode = &inodes[file_index]; break;
    }
  }

  if(!dir_inode){
    free(dir);
    return -1;
  } 

  int dir_start = block->data_offset + (512 * dir_inode->dblocks[0]);
  dir->position = dir_start;
  int found_end = 0;

  for(int i = 0; i < 16; i++){
    if(dir_inode->dblocks[i] == -1) {
      dir->end = block->data_offset + (512 * dir_inode->dblocks[i]);
      found_end = 1;
    }
  }

  if(!found_end) dir->end = block->data_offset + (512 * dir_inode->dblocks[15]);
  dir->curr_dblock = 0;
  dir->bytes_read = 0;

  open_entries.push_back(dir);

  return dir->fd;
}

vdir_t* f_readdir(vnode_t *vn, vfd_t fd){
  if(vn->type != D) {
    fs_errno = ENOTDIR;
    return NULL;
  }

  int file_index = vn->fs_info.unixfs.inode;
  inode* dir_inode = &inodes[file_index];

  if(open_entries[fd]->bytes_read >= dir_inode->size){
    open_entries[fd]->position = block->data_offset + (vn->fs_info.unixfs.inode * 512);
    open_entries[fd]->bytes_read = 0;
    return NULL;
  } 

  int curr_block = open_entries[fd]->curr_dblock;
  int max_block_pos = block->data_offset + (block->size * (dir_inode->dblocks[curr_block] + 1));

  if(open_entries[fd]->position >= max_block_pos){
    if(curr_block < 15){
      if(dir_inode->dblocks[curr_block+1] != -1){
        open_entries[fd]->position = block->data_offset + (block->size * dir_inode->dblocks[curr_block+1]);
        open_entries[fd]->curr_dblock += 1;
      } else {
        fs_errno = EINVAL;
        return NULL;
      }
    } else {
      fs_errno = EINVAL;
      return NULL;
    }
  }
  vdir_t* directory = (vdir_t*) (file + open_entries[fd]->position);

  open_entries[fd]->position += (int)sizeof(vdir_t);
  open_entries[fd]->bytes_read += (int)sizeof(vdir_t);
  if(directory->inode >= 0) return directory;
  else {
    fs_errno = EINVAL;
    return NULL;
  }

  return directory;
}

int f_closedir(vnode_t *vn, vfd_t fd){
  if(!vn) {
    fs_errno = EINVAL;
    return -1;
  }
  if(fd < 0){
    fs_errno = EBADF;
    return -1;
  }
  if(vn->type != F) {
    fs_errno = ENOTDIR;
    return -1;
  }

  if(strcmp(open_entries[fd]->name, vn->name) != 0 || open_entries[fd]->parent != vn->parent) {
    fs_errno = EINVAL;
    return -1;
  }

  memset(open_entries[fd], -1, sizeof(open_table));

  return 0;
}

int f_mkdir(vnode_t *vn, const char* name){
  if(strcmp(name, ".") == 0 || 
      strcmp(name, "..") == 0 ||
      strcmp(name, "\\") == 0 || // protected names^^
      strlen(name) > 127){
    fs_errno = (strlen(name) > 127) ? ENAMETOOLONG : EEXIST;
    return -1;
  }
  if(block->free_block == -1 || block->free_inode == -1){
    fs_errno = ENOSPC;
    return -1;
  }
  if(vn->type != D) {
    fs_errno = ENOTDIR;
    return -1;
  }

  int file_index = vn->fs_info.unixfs.inode;
  inode* dir_inode = &inodes[file_index];

  inode* new_inode = &inodes[block->free_inode];

  if(dir_inode->size + sizeof(vdir_t) > (size_t) block->size){
    fs_errno = ENOSPC;
    return -1;
  }

  vdir_t* directory = (vdir_t*)(file + (block->data_offset + (dir_inode->dblocks[0] * 512)));
  int size = dir_inode->size / sizeof(vdir_t);

  if(check_existence(directory, (char*)name, size) == -1) {
    fs_errno = EEXIST;
    return -1;
  }

  dir_inode->size = (size+1) * sizeof(vdir_t);

  vnode_t* new_vn = (vnode_t *) malloc(sizeof(vnode_t));
  new_vn->fs_info.unixfs.inode = block->free_inode;
  memset(new_vn->name, 0, 128);
  strncpy(new_vn->name, name, strlen(name));
  new_vn->permissions = 1;
  new_vn->type = D;
  new_vn->vnode_number = vnode_num++;
  new_vn->parent = vn;

  new_inode->nlink = 1;
  new_inode->size = 0;
  new_inode->dblocks[0] = block->free_block;

  bitmap[block->free_block] = 1;

  directory[size].inode = block->free_inode;
  directory[size].isdir = D;
  strcpy(directory[size].name, name);

  update_free_inode(0);
  update_free_block(0);

  vnodes.push_back(new_vn);

  return 0;
}

int f_rmdir(vnode_t *vn, const char* name) {
  if (!vn || !name || strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || strcmp(name, "\\") == 0){
    fs_errno = EINVAL;
    return -1;
  }

  if(vn->type != D){
    fs_errno = ENOTDIR;
    return -1;
  }
  int parent_inode_index = vn->fs_info.unixfs.inode;
  inode* parent_inode = &inodes[parent_inode_index];
  vdir_t* dir_entries = (vdir_t*)(file + block->data_offset + (parent_inode->dblocks[0] * 512));
  int num_entries = parent_inode->size / sizeof(vdir_t);

  int target_inode_index = -1;
  int target_entry_index = -1;

  for (int i = 0; i < num_entries; i++) {
    if (strcmp(dir_entries[i].name, name) == 0 && dir_entries[i].isdir == D) {
      target_inode_index = dir_entries[i].inode;
      target_entry_index = i;
      break;
    }
  }

  if (target_inode_index == -1){
    fs_errno = ENOENT;
    return -1;
  } 

  inode* target_inode = &inodes[target_inode_index];
  for (vnode_t* node : vnodes) {
    vnode_t* curr_vnode = find_dir_vnode(vn, (char *)name, 0);
    if (node->parent == curr_vnode) {
      if (node->type == D) {
        int ret = f_rmdir(curr_vnode, node->name);
        if(ret == 0) node->type = -1;
      }
      else f_remove(node, 1);
    }
  }

  for (int i = 0; i < 16; i++) {
    if (target_inode->dblocks[i] != -1) {
      bitmap[target_inode->dblocks[i]] = 0;
      update_free_block(target_inode->dblocks[i]);
    }
  }

  update_free_inode(target_inode_index);
  memset(target_inode, -1, sizeof(inode));

  if (target_entry_index < num_entries - 1) {
    memcpy(&dir_entries[target_entry_index], &dir_entries[target_entry_index + 1],
        sizeof(vdir_t) * (num_entries - target_entry_index - 1));
  }
  memset(&dir_entries[num_entries - 1], 0, sizeof(vdir_t));
  parent_inode->size -= sizeof(vdir_t);
  // vn->type = -1;

  return 0;
}

size_t calculate_mount_size(inode* mount_inodes, int num_inodes){
  size_t total = 0;
  for(int i = 0; i < num_inodes; i++){
    if(mount_inodes[i].size != -1){
      for(int j = 0; j < 16; j++){
        if(mount_inodes[i].dblocks[j] != -1){
          total += block->size;
        } else break;
      }
    }
  }
  return total;
}

void add_mount_vnodes_recursive(vnode_t *parent, char *mount_file, superblock *mount_block, int *node_map, int mount_idx){
  inode *curr_inode = &((inode*)(mount_file + mount_block->inode_offset))[mount_idx];
  if (curr_inode->size == 0) return;

  vdir_t *entries = (vdir_t*)(mount_file + mount_block->data_offset + curr_inode->dblocks[0] * block->size);
  int count = curr_inode->size / sizeof(vdir_t);

  for (int e = 0; e < count; e++) {
    vdir_t entry = entries[e];
    if (!strcmp(entry.name, ".") || !strcmp(entry.name, "..")) continue;

    int mount_index = entry.inode;        
    int copy_index = node_map[mount_index];      
    if (copy_index < 0) continue;   

    vnode_t *vn = (vnode_t*)malloc(sizeof(vnode_t));
    memset((void*)vn, 0, sizeof(*vn));

    vn->fs_info.unixfs.inode = copy_index;
    vn->type  = entry.isdir;
    vn->parent = parent;
    strncpy(vn->name, entry.name, 127);
    vn->vnode_number = vnode_num++;
    vn->permissions = 1;

    vnodes.push_back(vn);

    if (entry.isdir == D) add_mount_vnodes_recursive(vn, mount_file, mount_block, node_map, mount_index);
  }

}

int f_mount(vnode_t *vn, const char* sourcefile, const char* name){
  if (!vn) {
    fs_errno = EINVAL;
    return -1;
  }
  if (vn->type != D) {
    fs_errno = ENOTDIR;
    return -1;
  }
  if (strlen(name) > 127) {
    fs_errno = ENAMETOOLONG;
    return -1;
  }

  if (f_mkdir(vn, name) == -1) return -1;

  FILE *fp = fopen(sourcefile, "rb");
  if (!fp) { 
    fs_errno = ENOENT; 
    return -1; 
  }

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *mount_file = (char*)malloc(size);
  fread(mount_file, size, 1, fp);
  fclose(fp);

  superblock* mount_block = (superblock*)mount_file;
  inode* mount_inodes = (inode*)(mount_file + mount_block->inode_offset);

  if (calculate_mount_size(mount_inodes, mount_block->num_inodes) > free_space()) {
    fs_errno = EFBIG;
    free(mount_file);
    f_rmdir(vn, name);
    return -1;
  }

  int *node_map = (int*)malloc(sizeof(int)*mount_block->num_inodes);
  memset(node_map, -1, sizeof(int)*mount_block->num_inodes);

  for (int i = 0; i < mount_block->num_inodes; i++) {
    if (mount_inodes[i].size == -1) continue;  

    int new_ino = block->free_inode;
    inode *dst = &inodes[new_ino];
    inode *src = &mount_inodes[i];
    dst->size = src->size;

    memcpy(dst, src, sizeof(inode));
    dst->nlink = 1;

    node_map[i] = new_ino;

    for (int j = 0; j < 16; j++) {
      if (src->dblocks[j] == -1) {
        dst->dblocks[j] = -1;
      } else {
        bitmap[block->free_block] = 1;
        int dest_off  = block->data_offset + (block->free_block * block->size);
        int source_off = mount_block->data_offset + src->dblocks[j] * block->size;
        memcpy(file + dest_off, mount_file + source_off, block->size);
        dst->dblocks[j] = block->free_block;
        update_free_block(0);
      }
    }

    update_free_inode(0);
  }

  vnode_t *mt_root = find_dir_vnode(vn, (char*)name, 0);
  memset((void*)mt_root, 0, sizeof(*mt_root));
  update_free_inode(mt_root->fs_info.unixfs.inode);
  mt_root->fs_info.unixfs.inode = node_map[0];
  mt_root->type = D;
  mt_root->parent = vn;
  strncpy(mt_root->name, name, 127);
  mt_root->vnode_number = vnode_num++;

  add_mount_vnodes_recursive(mt_root, mount_file, mount_block, node_map, 0);

  free(node_map);
  free(mount_file);
  return 0;
}


int f_unmount(vnode_t *vn, const char* name){
  if(!vn) {
    fs_errno = EINVAL;
    return -1;
  }
  if(vn->type != D) {
    fs_errno = ENOTDIR;
    return -1;
  }

  vnode_t* mount_node = find_dir_vnode(vn, (char*)name, 0);
  return mount_node->mounted == 1 ? f_rmdir(vn, name) : -1;
}

int f_create(vnode_t *dir, const char* filename, int flags){
  if(block->free_inode == -1 || block->free_block == -1){
    fs_errno = ENOSPC;
    return -1;
  } 
  int dir_index = dir->fs_info.unixfs.inode;
  int size = (int)(inodes[dir_index].size / sizeof(vdir_t));

  if((size + 1) * sizeof(vdir_t) > (size_t) block->size) return -1;
  vdir_t* directory = (vdir_t*)(file + block->data_offset + (inodes[dir_index].dblocks[0] * 512));
  for (int i = 0; i < size; i++) {
    if (strcmp(directory[i].name, filename) == 0){
      fs_errno = EISDIR;
      return -1;
    } 
  }

  vnode_t* file_vnode = (vnode_t *) malloc(sizeof(vnode_t));
  memset(file_vnode->name, 0, 128);
  strncpy(file_vnode->name, filename, strlen(filename));
  file_vnode->type = F;
  file_vnode->vnode_number = vnode_num++;
  file_vnode->parent = dir; 
  file_vnode->permissions = flags; 
  file_vnode->fs_info.unixfs.inode = block->free_inode;

  inode* new_inode = &inodes[block->free_inode];
  new_inode->nlink = 1;
  new_inode->size = 0;
  new_inode->dblocks[0] = block->free_block;
  new_inode->protect = flags;

  bitmap[block->free_block] = 1;

  directory[size].inode = block->free_inode;
  directory[size].isdir = F;
  strcpy(directory[size].name, filename);

  inodes[dir_index].size = (size + 1) * (sizeof(vdir_t));
  update_free_inode(0);
  update_free_block(0);

  vnodes.push_back(file_vnode);

  return 0;

}

void update_free_inode(int index){
  if (index) memset(&inodes[index], -1, sizeof(inode));
  for(int i = 0; i < block->num_inodes; i++){
    if(inodes[i].nlink == 0 || inodes[i].nlink == -1){
      block->free_inode = i;
      return;
    }
  }
  block->free_inode = -1;
}

void update_free_block(int index){
  if(index) bitmap[index] = 0;
  for(int i = 0; i < block->num_inodes; i++){
    if(bitmap[i] == 0){
      block->free_block = i;
      return;
    } 
  }
  block->free_block = -1;
}

int check_existence(vdir_t * dir, char* name, int size){
  for(int i = 0; i < size; i++){
    if(strcmp(name, dir[i].name) == 0) return -1;
  }
  return 0;
}

void update_fs(char* fileName, long fileSize) {
  FILE* fp = fopen(fileName, "wb");
  if (!fp) {
    perror("fopen");
    return;
  }
  fwrite(file, fileSize, 1, fp);
  fclose(fp);
}

vnode_t* find_dir_vnode(vnode_t* parent, char* name, int isPath){
  if(!isPath){
    for(vnode_t * node: vnodes){
      if(strcmp(node->name, name) == 0 && parent == node->parent) return node;
    }
    return NULL;
  }
  char *rest = strdup(name);
  char *token;
  char *dir = NULL;

  token = strtok(rest, "/");
  while (token != NULL) {
    dir = token;
    token = strtok(NULL, "/");
  }
  for(vnode_t * node: vnodes){
    if(strcmp(node->name, dir) == 0){
      free(rest);
      return node;
    }
  }
  free(rest);
  return NULL;
}

void cleanup_fs(){
  for(open_table* entry: open_entries){
    if(entry != NULL) free(entry);
  }

  for(vnode_t* node: vnodes){
    if(node != NULL) free(node);
  }
  free(file);
}

void add_vnodes_recursive(vnode_t* parent, int index) {
  inode* dir_inode = &inodes[index];
  if (dir_inode->size == 0) return;

  int num_entries = dir_inode->size / sizeof(vdir_t);
  vdir_t* entries = (vdir_t*)(file + block->data_offset + (dir_inode->dblocks[0] * 512));

  for (int i = 0; i < num_entries; i++) {
    vdir_t entry = entries[i];

    vnode_t* node = (vnode_t*)malloc(sizeof(vnode_t));
    if (!node) return; 

    strcpy(node->name, entry.name);
    node->fs_info.unixfs.inode = entry.inode;
    node->type = entry.isdir;
    node->vnode_number = vnode_num++;
    node->permissions = 1;

    if (strcmp(entry.name, ".") == 0) node->parent = node;
    else if (strcmp(entry.name, "..") == 0) node->parent = parent ? parent->parent : NULL;
    else node->parent = parent;

    vnodes.push_back(node);

    if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) continue;

    if (entry.isdir == D) add_vnodes_recursive(node, entry.inode);
  }
}

int init_fs(char* fileName) {
  FILE *fp = fopen(fileName, "rb");
  if (!fp) {
    fs_errno = ENOENT;
    return -1;
  }

  fseek(fp, 0, SEEK_END);
  fileSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  if (file) free(file);
  file = (char*)malloc(fileSize);
  fread(file, fileSize, 1, fp);
  fclose(fp);

  block = (superblock*)(file);
  inodes = (inode*)(file + block->inode_offset);
  bitmap = (char*)(file + block->bitmap_offset);

  fs_root = (vnode_t*)malloc(sizeof(vnode_t));
  strcpy(fs_root->name, "\\");
  fs_root->parent = NULL;
  fs_root->fs_info.unixfs.inode = 0;
  fs_root->vnode_number = vnode_num++;
  fs_root->type = D;

  vnodes.push_back(fs_root);
  add_vnodes_recursive(fs_root, 0);

  return 0;
}

size_t free_space(){
  size_t free_space = 0;
  for(int i = 0; i < block->num_inodes; i++){
    free_space += block->size;
  }
  return free_space;
}

size_t file_free_space(inode* node){
  size_t free_space = 0;
  for(int i = 0; i < block->num_inodes; i++){
    free_space += block->size;
  }
  return free_space;
}

void f_clean(vnode_t *vn, vfd_t fd){
  open_entries[fd]->bytes_read = 0;
  open_entries[fd]->curr_dblock = 0;


  int inode_index = vn->fs_info.unixfs.inode;
  inode* curr_inode = &inodes[inode_index];
  curr_inode->size = 0;
  open_entries[fd]->position = block->data_offset + (512 * curr_inode->dblocks[0]);


  for(int i = 0; i < 16; i++){
    if(curr_inode->dblocks[i] == -1) break;
    memset(file+(curr_inode->dblocks[i] * block->size), 0, block->size);
    if(i != 0){
      update_free_block(curr_inode->dblocks[i]);
      curr_inode->dblocks[i] = -1;
    } 
  }

}
