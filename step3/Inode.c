#include <stdio.h>
#include "include/BDC.h"
#include "include/Inode.h"
#include "include/Item.h"

SuperBlock sb;
Inode inode_list[MAX_INODE_CNT];
extern int socket_to_BDS;

void store_superblock() {
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, &sb, sizeof(sb));
    printf("Store superblock at block 0\n");
    write_block(0, buf);
}

void init_superblock() {
    sb.VerificationCode = VERIFICATION_CODE;
    sb.free_inode_cnt = MAX_INODE_CNT;
    sb.free_block_cnt = MAX_BLOCK_CNT - REVERSE_BLOCK_CNT;
    memset(sb.inode_bitmap, 0, sizeof(sb.inode_bitmap));
    memset(sb.block_bitmap, 0, sizeof(sb.block_bitmap));
    sb.block_bitmap[0] = (uint16_t)~0;
    sb.block_bitmap[1] = (uint16_t)~0;
    sb.block_bitmap[2] = (uint16_t)1;
    store_superblock();
}

int load_superblock() {
    char buf[BLOCK_SIZE];
    read_block(0, (char *)&sb);
    if (sb.VerificationCode != VERIFICATION_CODE) {
        printf("File system not formatted\n");
        return -1;
    }
    printf("Superblock loaded\n");
    return 0;
}

void init_root_inode() {
    inode_list[0].id = 0;
    time(&inode_list[0].update_time);
    inode_list[0].mode = 1;
    inode_list[0].file_size = 0;
    inode_list[0].block_cnt = 0;
    inode_list[0].parent = -1;
    memset(inode_list[0].direct, -1, sizeof(inode_list[0].direct));
    inode_list[0].indirect = -1;
    sb.inode_bitmap[0] = (uint16_t)1;
    sb.free_inode_cnt--;
    store_superblock();
}

void init_inode(Inode *inode, int16_t id, uint16_t mode, int16_t parent) {
    inode->id = id;
    time(&inode->update_time);
    inode->mode = mode;
    inode->file_size = 0;
    inode->block_cnt = 0;
    inode->parent = parent;
    memset(inode->direct, -1, sizeof(inode->direct));
    inode->indirect = -1;
}

void init_inodes() {
    init_root_inode();
    for (int16_t i = 1; i < MAX_INODE_CNT; ++i) {
        inode_list[i].id = i;
        store_inode(&inode_list[i]);
    }
}

void load_inodes() {
    for (int i = 0; i < (MAX_INODE_CNT / INODE_PER_BLOCK); i++) {
        read_block(1 + i, (char *)&inode_list[i * INODE_PER_BLOCK]);
    }
    printf("Inodes loaded\n");
}

int16_t alloc_inode() {
    if (sb.free_inode_cnt == 0) {
        printf("No free inode.\n");
        return -1;
    }
    for (int16_t i = 0; i < MAX_INODE_CNT; i++) {
        if (sb.inode_bitmap[i / BITMAP_LEN] & (1 << (i % BITMAP_LEN))) continue;
        sb.inode_bitmap[i / BITMAP_LEN] |= (1 << (i % BITMAP_LEN));
        sb.free_inode_cnt--;
        printf("Alloc inode %d\n", i);
        store_superblock();
        return i;
    }
}

int free_inode(int16_t id) {
    if (id < 0 || id >= MAX_INODE_CNT) {
        printf("Invalid inode id.\n");
        return -1;
    }
    if (!(sb.inode_bitmap[id / BITMAP_LEN] & (1 << (id % BITMAP_LEN)))) {
        printf("Inode %d is already free.\n", id);
        return -1;
    }
    if (inode_list[id].block_cnt > 0) {
        for (int i = 0; i < DIRECT_PTR_CNT; ++i) {
            if (inode_list[id].direct[i] != -1) {
                free_block(inode_list[id].direct[i]);
            }
        }
        if (inode_list[id].indirect != -1) {
            int16_t indirect_blocks[BLOCK_ID_PER_BLOCK];
            read_block(inode_list[id].indirect, (char *)indirect_blocks);
            for (int i = 0; i < BLOCK_ID_PER_BLOCK; ++i) {
                if (indirect_blocks[i] != -1) {
                    free_block(indirect_blocks[i]);
                }
            }
            free_block(inode_list[id].indirect);
        }
    }
    sb.inode_bitmap[id / BITMAP_LEN] &= ~(1 << (id % BITMAP_LEN));
    sb.free_inode_cnt++;
    printf("Free inode %d\n", id);
    store_superblock();
    return 0;
}

int16_t alloc_block() {
    if (sb.free_block_cnt == 0) {
        printf("No free block.\n");
        return -1;
    }
    for (int16_t i = 0; i < MAX_BLOCK_CNT; i++) {
        if (sb.block_bitmap[i / BITMAP_LEN] & (1 << (i % BITMAP_LEN))) continue;
        sb.block_bitmap[i / BITMAP_LEN] |= (1 << (i % BITMAP_LEN));
        sb.free_block_cnt--;
        printf("Alloc block %d\n", i);
        store_superblock();
        return i;
    }
}

int free_block(int16_t id) {
    if (id < REVERSE_BLOCK_CNT || id >= MAX_BLOCK_CNT) {
        printf("Invalid block id.\n");
        return -1;
    }
    if (!(sb.block_bitmap[id / BITMAP_LEN] & (1 << (id % BITMAP_LEN))) || id < REVERSE_BLOCK_CNT) {
        printf("Block %d is already free.\n", id);
        return -1;
    }
    sb.block_bitmap[id / BITMAP_LEN] &= ~(1 << (id % BITMAP_LEN));
    sb.free_block_cnt++;
    printf("Free block %d\n", id);
    store_superblock();
    return 0;
}

void init_items_in_block(int16_t id) {
    Item items[ITEM_PER_BLOCK];
    for (int i = 0; i < ITEM_PER_BLOCK; ++i) {
        items[i].inode_id = -1;
    }
    printf("Store initialized items in block %d\n", id);
    write_block(id, (char *)items);
}

void store_inode(Inode *inode) {
    int block_no = 1 + inode->id / INODE_PER_BLOCK;
    int offset = (inode->id % INODE_PER_BLOCK) * INODE_SIZE;
    char buf[BLOCK_SIZE];
    read_block(block_no, buf);
    memcpy(buf + offset, inode, INODE_SIZE);
    printf("Store inode %d in block %d\n", inode->id, block_no);
    write_block(block_no, buf);
}

void store_inodes() {
    for (int16_t i = 0; i < MAX_INODE_CNT; i++) {
        store_inode(&inode_list[i]);
    }
}

int16_t search_in_dir(Inode *dir_inode, char *name, uint16_t mode) {
    if (dir_inode->mode != 1) {
        printf("Not a directory.\n");
        return -2;
    }
    for (int16_t i = 0; i < DIRECT_PTR_CNT; ++i) {
        if (dir_inode->direct[i] == -1) continue;
        Item items[ITEM_PER_BLOCK];
        read_block(dir_inode->direct[i], (char *)items);
        for (int j = 0; j < ITEM_PER_BLOCK; ++j) {
            if (items[j].inode_id == -1) continue;
            if (strcmp(items[j].name, name) == 0 && mode == inode_list[items[j].inode_id].mode) {
                return items[j].inode_id;
            }
        }
    }
    return -1;
}

int add_to_dir(Inode *dir_inode, char *name, uint16_t mode) {
    if (dir_inode->mode != 1) {
        printf("Not a directory.\n");
        return -1;
    }
    for (int i = 0; i < DIRECT_PTR_CNT; ++i) {
        if (dir_inode->direct[i] == -1) {
            int id = alloc_block();
            if (id < 0) return -1;
            init_items_in_block(id);
            dir_inode->direct[i] = id;
            dir_inode->block_cnt++;
            time(&dir_inode->update_time);
            store_inode(dir_inode);
        }
        Item items[ITEM_PER_BLOCK];
        read_block(dir_inode->direct[i], (char *)items);
        for (int j = 0; j < ITEM_PER_BLOCK; ++j) {
            if (items[j].inode_id == -1) {
                int id = alloc_inode();
                if (id < 0) return -1;
                init_inode(&inode_list[id], id, mode, dir_inode->id);
                store_inode(&inode_list[id]);

                items[j].inode_id = id;
                strcpy(items[j].name, name);
                printf("Store item %s in block %d\n", name, dir_inode->direct[i]);
                write_block(dir_inode->direct[i], (char *)items);
                time(&dir_inode->update_time);
                dir_inode->file_size += ITEM_SIZE;
                store_inode(dir_inode);
                return 0;
            }
        }
    }
    printf("No space in the directory.\n");
    return -1;
}

int remove_from_dir(Inode *dir_inode, char *name, uint16_t mode) {
    if (dir_inode->mode != 1) {
        printf("Not a directory.\n");
        return -1;
    }
    for (int i = 0; i < DIRECT_PTR_CNT; ++i) {
        if (dir_inode->direct[i] == -1) continue;
        Item items[ITEM_PER_BLOCK];
        read_block(dir_inode->direct[i], (char *)items);
        for (int j = 0; j < ITEM_PER_BLOCK; ++j) {
            if (items[j].inode_id == -1) continue;
            if (strcmp(items[j].name, name) == 0 && mode == inode_list[items[j].inode_id].mode) {
                if (mode == 1 && inode_list[items[j].inode_id].file_size > 0) {
                    printf("Directory is not empty.\n");
                    return -1;
                }
                if (free_inode(items[j].inode_id) < 0) return -1;
                items[j].inode_id = -1;
                printf("Remove item %s from block %d\n", name, dir_inode->direct[i]);
                write_block(dir_inode->direct[i], (char *)items);
                time(&dir_inode->update_time);
                dir_inode->file_size -= ITEM_SIZE;
                store_inode(dir_inode);
                return 0;
            }
        }
    }
    if (mode == 0) {
        printf("No such file.\n");
    } else {
        printf("No such directory.\n");
    }
    return -1;
}

int list_dir(Inode *dir_inode, char *message) {
    if (dir_inode->mode != 1) {
        printf("Not a directory.\n");
        return -1;
    }
    for (int i = 0; i < DIRECT_PTR_CNT; ++i) {
        if (dir_inode->direct[i] == -1) continue;
        Item items[ITEM_PER_BLOCK];
        read_block(dir_inode->direct[i], (char *)items);
        for (int j = 0; j < ITEM_PER_BLOCK; ++j) {
            if (items[j].inode_id == -1) continue;
            // printf("%s\n", items[j].name);
            char buffer[80];
            int len;
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&inode_list[items[j].inode_id].update_time));
            if (inode_list[items[j].inode_id].mode == 0) {
                len = strlen(message);
                sprintf(message + len, "%5uB \t%s\t%s\n", (unsigned int)inode_list[items[j].inode_id].file_size, buffer, items[j].name);
            } else {
                len = strlen(message);
                sprintf(message + len, "\t%s\t\033[1m\033[32m%s\033[0m\n", buffer, items[j].name);
            }
        }
    }
    return 0;
}

int read_file(Inode *file_node, char *buf) {
    if (file_node->mode != 0) {
        printf("Not a file.\n");
        return -1;
    }
    int size = file_node->file_size;
    int block_cnt = file_node->block_cnt;
    for (int i = 0; i < block_cnt && i < DIRECT_PTR_CNT; ++i) {
        read_block(file_node->direct[i], buf + i * BLOCK_SIZE);
    }
    if (block_cnt > DIRECT_PTR_CNT) {
        int16_t indirect_blocks[BLOCK_ID_PER_BLOCK];
        read_block(file_node->indirect, (char *)indirect_blocks);
        for (int i = 0; i < block_cnt - DIRECT_PTR_CNT; ++i) {
            read_block(indirect_blocks[i], buf + (i + DIRECT_PTR_CNT) * BLOCK_SIZE);
        }
    }
    memset(buf + size, 0, MAX_FILE_SIZE - size);
    return size;
}

int write_file(Inode *file_inode, char *buf, uint16_t size) {
    if (file_inode->mode != 0) {
        printf("Not a file.\n");
        return -1;
    }
    int need_block_cnt = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (need_block_cnt > MAX_FILE_BLOCK_CNT) {
        printf("Too much content.\n");
        return -1;
    }
    int crt_block_cnt = file_inode->block_cnt;
    if (need_block_cnt <= crt_block_cnt) {
        for (int i = 0; i < DIRECT_PTR_CNT && i < need_block_cnt; ++i) {
            printf("Store data in block %d\n", file_inode->direct[i]);
            write_block(file_inode->direct[i], buf + i * BLOCK_SIZE);
        }
        if (need_block_cnt > DIRECT_PTR_CNT) {
            int16_t indirect_blocks[BLOCK_ID_PER_BLOCK];
            read_block(file_inode->indirect, (char *)indirect_blocks);
            for (int i = 0; i < BLOCK_ID_PER_BLOCK; ++i) {
                printf("Store data in block %d\n", indirect_blocks[i]);
                write_block(indirect_blocks[i], buf + (i + DIRECT_PTR_CNT) * BLOCK_SIZE);
            }
        }
        for (int i = need_block_cnt; i < DIRECT_PTR_CNT && i < crt_block_cnt; ++i) {
            free_block(file_inode->direct[i]);
        }
        if (need_block_cnt > DIRECT_PTR_CNT) {
            int16_t indirect_blocks[BLOCK_ID_PER_BLOCK];
            read_block(file_inode->indirect, (char *)indirect_blocks);
            for (int i = need_block_cnt - DIRECT_PTR_CNT; i < crt_block_cnt - DIRECT_PTR_CNT; ++i) {
                free_block(indirect_blocks[i]);
            }
        } else if (crt_block_cnt > DIRECT_PTR_CNT) {
            int16_t indirect_blocks[BLOCK_ID_PER_BLOCK];
            read_block(file_inode->indirect, (char *)indirect_blocks);
            for (int i = 0; i < crt_block_cnt - DIRECT_PTR_CNT; ++i) {
                free_block(indirect_blocks[i]);
            }
            free_block(file_inode->indirect);
        }
    } else {
        for (int i = crt_block_cnt; i < DIRECT_PTR_CNT && i < need_block_cnt; ++i) {
            int id = alloc_block();
            if (id < 0) return -1;
            file_inode->direct[i] = id;
        }
        if (crt_block_cnt > DIRECT_PTR_CNT) {
            int16_t indirect_blocks[BLOCK_ID_PER_BLOCK];
            read_block(file_inode->indirect, (char *)indirect_blocks);
            for (int i = crt_block_cnt - DIRECT_PTR_CNT; i < need_block_cnt - DIRECT_PTR_CNT; ++i) {
                int id = alloc_block();
                if (id < 0) return -1;
                indirect_blocks[i] = id;
            }
            printf("Store indirect block IDs in block %d\n", file_inode->indirect);
            write_block(file_inode->indirect, (char *)indirect_blocks);
        } else if (need_block_cnt > DIRECT_PTR_CNT) {
            int id = alloc_block();
            if (id < 0) return -1;
            file_inode->indirect = id;
            int16_t indirect_blocks[BLOCK_ID_PER_BLOCK];
            memset(indirect_blocks, -1, sizeof(indirect_blocks));
            for (int i = 0; i < need_block_cnt - DIRECT_PTR_CNT; ++i) {
                int id = alloc_block();
                if (id < 0) return -1;
                indirect_blocks[i] = id;
            }
            printf("Store indirect block IDs in block %d\n", file_inode->indirect);
            write_block(file_inode->indirect, (char *)indirect_blocks);
        }
        for (int i = 0; i < DIRECT_PTR_CNT && i < need_block_cnt; ++i) {
            printf("Store data in block %d\n", file_inode->direct[i]);
            write_block(file_inode->direct[i], buf + i * BLOCK_SIZE);
        }
        if (need_block_cnt > DIRECT_PTR_CNT) {
            int16_t indirect_blocks[BLOCK_ID_PER_BLOCK];
            read_block(file_inode->indirect, (char *)indirect_blocks);
            for (int i = 0; i < need_block_cnt - DIRECT_PTR_CNT; ++i) {
                printf("Store data in block %d\n", indirect_blocks[i]);
                write_block(indirect_blocks[i], buf + (i + DIRECT_PTR_CNT) * BLOCK_SIZE);
            }
        }
    }
    file_inode->file_size = size;
    file_inode->block_cnt = need_block_cnt;
    time(&file_inode->update_time);
    store_inode(file_inode);
    return 0;
}