#ifndef INODE_H
#define INODE_H

#include <time.h>
#include <string.h>
#include "utils.h"

/*
I-node (32 bytes):
*/
typedef struct inode {
    time_t update_time;              // last update time
    int16_t id;                      // inode id, -1 if invalid (not allocated)
    uint16_t file_size;              // size of the file in bytes
    uint8_t mode;                    // 0: file, 1: directory
    uint8_t block_cnt;               // number of blocks
    int16_t parent;                  // parent inode id, -1 if null
    int16_t direct[DIRECT_PTR_CNT];  // direct pointers pointing to blocks, -1 if null
    int16_t indirect;                // single indirect pointer, -1 if null (only for file)
} Inode;

/*
SuperBlock recording universal infomation (164 + 64 = 228 bytes):
*/
typedef struct superblock {
    uint64_t VerificationCode;  // check if the file system is formatted
    uint16_t free_inode_cnt;    // number of free inodes
    uint16_t free_block_cnt;    // number of free blocks

    uint16_t inode_bitmap[MAX_INODE_CNT / BITMAP_LEN];  // inode bitmap size in blocks
    uint16_t block_bitmap[MAX_BLOCK_CNT / BITMAP_LEN];  // block bitmap size in blocks
} SuperBlock;

void store_superblock();

void init_superblock();

int load_superblock();

void init_root_inode();

void init_inode(Inode *inode, int16_t id, uint16_t mode, int16_t parent);

void init_inodes();

void load_inodes();

int16_t alloc_inode();

int free_inode(int16_t id);

int16_t alloc_block();

int free_block(int16_t id);

void init_items_in_block(int16_t id);

void store_inode(Inode *inode);

void store_inodes();

/*
search for a file or directory in a directory
return: Inode id if found, -1 if not found, -2 if error
*/
int16_t search_in_dir(Inode *dir_inode, char *name, uint16_t mode);

int add_to_dir(Inode *dir_inode, char *name, uint16_t mode);

int remove_from_dir(Inode *dir_inode, char *name, uint16_t mode);

int list_dir(Inode *dir_inode, char *message);

int read_file(Inode *file_node, char *buf);

int write_file(Inode *file_inode, char *buf, uint16_t size);

#endif  // INODE_H