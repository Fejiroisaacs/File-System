int f_mkdir(vnode_t *vn, const char* path){
    if(strcmp(vn->name, ".") == 0 || 
       strcmp(vn->name, "..") == 0 ||
       strcmp(vn->name, "\\") == 0 || // protected names^^
       strlen(vn->name) > 127 ||
       block->free_block == -1 || block->free_inode == -1) return -1;
  
    if(vn->type != D) return -1;
    
    char *rest = strdup(path);
    char *token;
    char *dir = NULL;
  
    token = strtok(rest, "/");
    while (token != NULL) {
      dir = token;
      token = strtok(NULL, "/");
    }
  
    int path_found = 0;
    vdir_t* directory;
    inode* new_inode;
    int size;
  
    if(strcmp(dir, "\\") == 0){
      size = inodes[0].size / sizeof(vdir_t);
      directory = (vdir_t*)(file + block->data_offset);
      if(check_existence(directory, vn->name, size) == -1) return -1;
      inodes[0].size = (size+1) * sizeof(vdir_t);
      path_found = 1;
  
    } else {
  
      for(int i = 0; i < block->num_inodes; i++){
        if(inodes[i].nlink == -1) break; // getting to region of never initialised inodes i.e. path doesn't exist
        else {
          if(inodes[i].dblocks[0] < 0){
            printf("dblocks < 0\n");
            return -1;
          }
          directory = (vdir_t*)(file + (block->data_offset + (inodes[i].dblocks[0] * 512)));
          if(strcmp(directory[0].name, dir) == 0){
            size = inodes[i].size / sizeof(vdir_t);
            if(check_existence(directory, vn->name, size) == -1) return -1;
            inodes[i].size = (size+1) * sizeof(vdir_t);
            path_found = 1;
            break;
          }
        }
      }
    }
  
    if(!path_found) return -1; // should never get here
  
    vn->fs_info.unixfs.inode = block->free_inode;
  
    new_inode = &inodes[block->free_inode];
    new_inode->nlink = 1;
    new_inode->size = 0;
    new_inode->dblocks[0] = block->free_block;
  
    bitmap[block->free_block] = 1;
  
    directory[size].inode = block->free_inode;
    directory[size].isdir = D;
    strcpy(directory[size].name, vn->name);
  
    update_free_inode(0);
    update_free_block(0);
    free(rest);
  
    vnodes.push_back(vn);
  
    return 0;
  }

  

int f_rmdir(vnode_t *vn, const char* path){
    if((strcmp(vn->name, ".") == 0 || 
    strcmp(vn->name, "..") == 0 ||
    strcmp(vn->name, "\\") == 0)) return -1; // cant remove root
  
    vnode_t* dir_vnode = find_dir_vnode((char *)path, 1);
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
  
    dir_index = vn->fs_info.unixfs.inode;
    inode* curr_inode = &inodes[dir_index];
  
    directory = (vdir_t*)(file + block->data_offset + (inodes[dir_index].dblocks[0] * 512));
    num_dir_files = (int)(inodes[dir_index].size / sizeof(vdir_t));
  
    // need test
    for (int i = 0; i < num_dir_files; i++) {
      char* newPath = (char*)malloc(sizeof(char)*128);
      sprintf(newPath, "%s/%s", path, vn->name);
      for(vnode_t* node: vnodes){
        if(strcmp(node->name, directory[i].name) == 0) f_rmdir(node, newPath);
      }
      free(newPath);
    }
  
    for(int i = 0; i < 16; i++){ // free blocks
      if(curr_inode->dblocks[i] != -1) bitmap[curr_inode->dblocks[i]] = 0;
    }
  
    update_free_inode(vn->fs_info.unixfs.inode);
    update_free_block(curr_inode->dblocks[0]);
  
    memset(curr_inode, -1, sizeof(inode));
    vn = NULL;
  
    return 0;
  }
  

  int f_mkdir(vnode_t *vn, const char* name) {
    // 1. Validation
    if (!vn || vn->type != D || !name) return -1;
  
    // if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || strcmp(name, "\\") == 0)
    //   return -1;
  
    if (strlen(name) > 127 || block->free_block == -1 || block->free_inode == -1)
      return -1;
  
    printf("Here\n");
    inode *parent_inode = &inodes[vn->fs_info.unixfs.inode];
  
    if (parent_inode->dblocks[0] < 0) return -1;
  
    // 2. Load directory entries
    vdir_t *directory = (vdir_t*)(file + (block->data_offset + parent_inode->dblocks[0] * 512));
    int num_entries = parent_inode->size / sizeof(vdir_t);
  
    // 3. Check for existing name
    // if (check_existence(directory, (char *)name, num_entries) != -1) return -1;
  
    // 4. Setup new inode
    inode *new_inode = &inodes[block->free_inode];
    new_inode->nlink = 1;
    new_inode->size = 0;
    new_inode->dblocks[0] = block->free_block;
  
    // 5. Add new directory entry
    vdir_t *new_entry = &directory[num_entries];
    new_entry->inode = block->free_inode;
    new_entry->isdir = D;
    strcpy(new_entry->name, name);
  
    parent_inode->size += sizeof(vdir_t);
  
    // 6. Update free maps
    
    // 7. Create vnode
    vnode_t *new_vn = (vnode_t*) malloc(sizeof(vnode_t));
    strcpy(new_vn->name, name);
    // new_vn->name = strdup(name);
    new_vn->type = D;
    new_vn->fs_info.unixfs.inode = block->free_inode;
    new_vn->parent = vn;
    new_vn->permissions = 1;
    new_vn->vnode_number = vnode_num++;
  
    bitmap[block->free_block] = 1;
    update_free_inode(0);
    update_free_block(0);
  
    vnodes.push_back(new_vn);
  
    return 0;
  }
  
  int f_rmdir(vnode_t *vn, const char* name) {
    if (!vn || !name || vn->type != D) return -1;
  
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || strcmp(name, "\\") == 0)
      return -1; // Can't remove special or root dirs
  
    inode *parent_inode = &inodes[vn->fs_info.unixfs.inode];
    if (parent_inode->dblocks[0] < 0) return -1;
  
    vdir_t *entries = (vdir_t *)(file + block->data_offset + (parent_inode->dblocks[0] * 512));
    int num_entries = parent_inode->size / sizeof(vdir_t);
  
    // Locate the directory to remove
    int found = -1;
    for (int i = 0; i < num_entries; i++) {
      if (strcmp(entries[i].name, name) == 0 && entries[i].isdir == D) {
        found = i;
        break;
      }
    }
  
    if (found == -1) return -1; 
  
    int target_inode_idx = entries[found].inode;
    inode *target_inode = &inodes[target_inode_idx];
    vdir_t *target_entries = (vdir_t *)(file + block->data_offset + (target_inode->dblocks[0] * 512));
    int target_num = target_inode->size / sizeof(vdir_t);
  
    for (int i = 0; i < target_num; i++) {
      if (target_entries[i].isdir == D) {
        vnode_t *sub_vn = (vnode_t *) malloc(sizeof(vnode_t));
        strcpy(sub_vn->name, target_entries[i].name);
        sub_vn->type = D;
        sub_vn->fs_info.unixfs.inode = target_entries[i].inode;
        f_rmdir(sub_vn, target_entries[i].name);
      }
    }
  
    // Free target inode blocks
    for (int i = 0; i < 16; i++) {
      if (target_inode->dblocks[i] != -1)
        bitmap[target_inode->dblocks[i]] = 0;
    }
  
    update_free_inode(target_inode_idx);
    update_free_block(target_inode->dblocks[0]);
  
    memset(target_inode, -1, sizeof(inode));
  
    // Remove from parent's directory entries
    if (found < num_entries - 1) {
      memmove(&entries[found], &entries[found + 1], sizeof(vdir_t) * (num_entries - found - 1));
    }
    memset(&entries[num_entries - 1], 0, sizeof(vdir_t));
    parent_inode->size -= sizeof(vdir_t);
  
  
    return 0;
  }