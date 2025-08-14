# File System – Final Project (CMSC 355: Operating Systems)

A custom **hierarchical file system** implemented from scratch in C, complete with its own **shell**, **virtual file system (VFS)** interface, and **formatting utility**.  
The project simulates a "disk" as a single Unix file (disk image) and supports a wide range of file system operations including permissions, mounting, and directory navigation.

---

## Overview

This project is a from-scratch implementation of a file system inspired by **MS-DOS FAT** and **Unix inode-based** designs.  
It includes:

- **A Virtual File System (VFS)** layer to abstract physical file storage.
- **A custom shell** to interact with the file system.
- **A disk format utility** to initialize and manage virtual disks.

The shell operates independently of the host operating system’s file commands, making all file operations occur within the simulated disk.

---

## Features

### File System Core

- Hierarchical directory structure
- File permissions (absolute & symbolic modes)
- Persistent storage via disk image files
- Support for large files (multi-block allocation)
- Mounting/unmounting different file systems

### Shell Commands

- **Basic Commands:** `ls`, `cat`, `touch`, `mkdir`, `rmdir`, `pwd`, `cd`, `rm`, `stat`

- **Advanced Commands:** `mount`, `umount`
- **Redirection Support:** `>`, `>>` for writing/appending to files
- **Error Handling:** Graceful handling of invalid inputs and operations

### Developer API (libvfs.so)

- `f_open`, `f_read`, `f_write`, `f_close`
- `f_seek`, `f_rewind`, `f_stat`
- `f_remove`, `f_mkdir`, `f_rmdir`
- `f_opendir`, `f_readdir`, `f_closedir`
- `f_mount`, `f_umount`

---

## Implementation Details

- **Disk Image Structure:**
  - Boot block
  - Superblock
  - Free space management
  - Root directory initialization
- **Block size:** 512 bytes (configurable)
- **File Allocation Strategies:** FAT-12 or early Unix V7-style inodes
- **VFS Representation:** `vnode` structure for file/directory metadata
- **Persistence:** Disk state saved to file upon shell exit

## Note

- This project was developed as part of the Final Project for the course.

- All file operations occur within the simulated file system — no direct OS file calls are used.

---

## Usage

```bash

# Build the project
make

# Format a Disk Image
./format mydisk.img

# Optional: specify size in MB
./format -s 5 mydisk.img

#Run the Shell
./shell

# Example Session
myshell$ ls
. ..
myshell$ mkdir docs
myshell$ cd docs
myshell$ echo "Hello World" > hello.txt
myshell$ cat hello.txt

Repository Structure
.
├── format.c        # Disk formatting utility
├── shell.c         # Interactive shell
├── vfs.c           # Virtual File System implementation
├── vfs.h           # VFS API definitions
├── Makefile        # Build configuration
└── tests/          # Unit tests for VFS functions
