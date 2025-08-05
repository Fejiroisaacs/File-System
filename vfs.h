#ifndef vfs_H_
#define vfs_H_

#include <unistd.h>
#include "inode.h"
#include <list>
#include <vector>

using namespace std;
typedef int vfd_t; 

typedef struct vnode {
    int vnode_number;
    char name[128];
    struct vnode *parent;
    int permissions;
    int type;
    int start_pos;
    list<vnode*> children;
    int mounted;
    union {
        struct unix_fs { /* these structs need to be fleshed out by you */
            int inode;
        } unixfs;
        struct dos_fs {
            int fat_ptr;
        } dosfs;
        struct ram_fs {
            unsigned long offset;
        } ramfs;
    } fs_info; /* file system specific info */
} vnode_t;

typedef struct vstat {
  int st_type; // file or directory
  int st_inode; // inode corresponding to file
  int st_perms; // permission mode of the file
  int st_size; // filesize
  int st_blocks; // number of blocks given to the file
} vstat_t;

typedef struct vdir {
    char name[128];
    int isdir;
    int inode;
} vdir_t;

typedef struct open_table_entries {
    char name[128];
    vfd_t fd;
    int position;
    int flags;
    int end;
    int curr_dblock;
    int bytes_read;
    struct vnode *parent;
} open_table;

vfd_t f_open(vnode_t *vn, const char *filename, int flags);
size_t f_read(vnode_t *vn, void *data, size_t size, int num, vfd_t fd);
size_t f_write(vnode_t *vn, void *data, size_t size, int num, vfd_t fd);
int f_close(vnode_t *vn, vfd_t fd);
int f_seek(vnode_t *vn, vfd_t fd, off_t offset, int whence);
int f_stat(vnode_t *vn, vfd_t fd, vstat_t* stats);
int f_remove(vnode_t *vn, vfd_t fd);
vfd_t f_opendir(vnode_t *vn, const char* path);
vdir_t* f_readdir(vnode_t *vn, vfd_t fd);
int f_closedir(vnode_t *vn, vfd_t fd); 
int f_mkdir(vnode_t *vn, const char* name); 
int f_rmdir(vnode_t *vn, const char* path); 
int f_mount(vnode_t *vn, const char* sourcefile, const char* path); 
int f_unmount(vnode_t *vn, const char* path); 
void cleanup_fs();
void update_fs(char* fileName, long fileSize);
int init_fs(char* fileName);
vnode_t* find_dir_vnode(vnode_t* parent, char* path, int isPath);
void f_clean(vnode_t *vn, vfd_t fd);

extern vnode_t* fs_root;
extern int fs_errno;
extern char* file;
extern char* bitmap;
extern superblock *block;
extern inode *inodes;
extern int vnode_num;
extern long fileSize;
extern vector<vnode_t*> vnodes;
extern vector<open_table*> open_entries;

enum {D, F};
enum {EBADF};

#endif
