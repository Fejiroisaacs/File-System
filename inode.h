#ifndef _inode_H_
#define _inode_H_

#define N_DBLOCKS 16

typedef struct superblock {
    int size; /* size of blocks in bytes */
    int inode_offset; /* offset of inode region in bytes */
    int data_offset; /* data region offset in bytes */
    int bitmap_offset; /* bitmap region offset in bytes */
    int free_inode; /* head of free inode list, index, if disk is full, -1 */
    int free_block; /* head of free block list, index, if disk is full, -1 */
    int num_inodes;
}superblock;

typedef struct inode {
    int next_inode; /* index of next free inode */
    int protect; /* protection field */
    int nlink; /* number of links to this file */
    int size; /* numer of bytes in file */
    int uid; /* owner’s user ID */
    int gid; /* owner’s group ID */
    int ctime; /* change time */
    int mtime; /* modification time */
    int atime; /* access time */
    int dblocks[N_DBLOCKS]; /* pointers to data blocks */
}inode;

#endif
