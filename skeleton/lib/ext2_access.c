// ext2 definitions from the real driver in the Linux kernel.
#include "ext2fs.h"

// This header allows your project to link against the reference library. If you
// complete the entire project, you should be able to remove this directive and
// still compile your code.
#include "reference_implementation.h"

// Definitions for ext2cat to compile against.
#include "ext2_access.h"



///////////////////////////////////////////////////////////
//  Accessors for the basic components of ext2.
///////////////////////////////////////////////////////////

// Return a pointer to the primary superblock of a filesystem.
struct ext2_super_block * get_super_block(void * fs) {
    //superblock is located at beginning of file system plus a superblock offset
    return fs + SUPERBLOCK_OFFSET;
}


// Return the block size for a filesystem.
__u32 get_block_size(void * fs) {
    //get superblock and use macro to return block size for the file system
    struct ext2_super_block* superBlock = get_super_block(fs);
    return EXT2_BLOCK_SIZE(superBlock);
}


// Return a pointer to a block given its number.
// get_block(fs, 0) == fs;
void * get_block(void * fs, __u32 block_num) {
    //block is located at beginning of file system plus (num * size of blocks)
    return fs + (block_num * get_block_size(fs));
}


// Return a pointer to the first block group descriptor in a filesystem. Real
// ext2 filesystems will have several of these, but, for simplicity, we will
// assume there is only one.
struct ext2_group_desc * get_block_group(void * fs, __u32 block_group_num) {
    //first block group descriptor is located directly after the superblock
    return fs + SUPERBLOCK_OFFSET + SUPERBLOCK_SIZE;
}


// Return a pointer to an inode given its number. In a real filesystem, this
// would require finding the correct block group, but you may assume it's in the
// first one.
struct ext2_inode * get_inode(void * fs, __u32 inode_num) {
    //get superblock to find inode size
    struct ext2_super_block* superBlock = get_super_block(fs);
    //inode table index can be found in block group descriptor
    __u32 inodeTableIndex = get_block_group(fs, 0)->bg_inode_table;
    //get inode table address
    void * tableBlock = get_block(fs, inodeTableIndex);
    //inode number is found at table base address plus (index * inode size)
    return tableBlock + ((inode_num - 1)*EXT2_INODE_SIZE(superBlock)); 
}



///////////////////////////////////////////////////////////
//  High-level code for accessing filesystem components by path.
///////////////////////////////////////////////////////////

// Chunk a filename into pieces.
// split_path("/a/b/c") will return {"a", "b", "c"}.
//
// This one's a freebie.
char ** split_path(char * path) {
    int num_slashes = 0;
    for (char * slash = path; slash != NULL; slash = strchr(slash + 1, '/')) {
        num_slashes++;
    }

    // Copy out each piece by advancing two pointers (piece_start and slash).
    char ** parts = (char **) calloc(num_slashes, sizeof(char *));
    char * piece_start = path + 1;
    int i = 0;
    for (char * slash = strchr(path + 1, '/');
         slash != NULL;
         slash = strchr(slash + 1, '/')) {
        int part_len = slash - piece_start;
        parts[i] = (char *) calloc(part_len + 1, sizeof(char));
        strncpy(parts[i], piece_start, part_len);
        piece_start = slash + 1;
        i++;
    }
    // Get the last piece.
    parts[i] = (char *) calloc(strlen(piece_start) + 1, sizeof(char));
    strncpy(parts[i], piece_start, strlen(piece_start));
    return parts;
}


// Convenience function to get the inode of the root directory.
struct ext2_inode * get_root_dir(void * fs) {
    return get_inode(fs, EXT2_ROOT_INO);
}


// Given the inode for a directory and a filename, return the inode number of
// that file inside that directory, or 0 if it doesn't exist there.
// 
// name should be a single component: "foo.txt", not "/files/foo.txt".
__u32 get_inode_from_dir(void * fs, struct ext2_inode * dir, 
        char * name) {
    //get directory block from first inode block number
    void* directoryBlock = get_block(fs, dir->i_block[0]);
    void* currentBlock = directoryBlock;
    //keep cycling through directory linked list until file type is unknown (reached end of list)
    while (((struct ext2_dir_entry_2*)currentBlock)->file_type != 0)
    {
        struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2*)currentBlock;
        //if names are equal, return inode
        if (strncmp(name, entry->name, entry->name_len) == 0)
            return entry->inode;
        //otherwise, increment by size of directory entry
        else
            currentBlock += entry->rec_len;
    }
    //file not found
    return 0;
}


// Find the inode number for a file by its full path.
// This is the functionality that ext2cat ultimately needs.
__u32 get_inode_by_path(void * fs, char * path) {
    int i;
    int pathLength = strlen(path);
    int levels = 0;
    //determine number of folder levels by number of forward slashes in file path
    for (i = 0; i < pathLength; i++)
    {
        if (path[i] == '/')
            levels++;
    }
    //must have at least one level (root)
    if (levels == 0)
        return 0;
    //get all folders and filename components from file path
    char** pathComponents = split_path(path);
    //get root directory
    struct ext2_inode* folders = get_root_dir(fs);
    __u32 inodeNo;
    //cycle through folder levels, getting inode numbers from each folder or filename
    for (i = 0; i < levels; i++)
    {
        inodeNo = get_inode_from_dir(fs, folders, pathComponents[i]);
        //if found, continue on to next folder
        if (inodeNo != 0)
            folders = get_inode(fs, inodeNo);
        //file not found
        else
            return 0;
    }
    //return inode number of given file
    return inodeNo;
}

