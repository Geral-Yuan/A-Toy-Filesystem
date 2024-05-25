#ifndef ITEM_H
#define ITEM_H

#include "utils.h"

/*
Item for file or directory (32 bytes):
*/
typedef struct item {
    int16_t inode_id;  // inode id, -1 if invalid
    char name[MAX_ITEM_NAME_LEN];      // file or directory name
} Item;

#endif  // ITEM_H