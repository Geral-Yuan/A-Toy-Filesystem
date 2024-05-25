#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <ctype.h>

#define INODE_SIZE 32
#define BLOCK_SIZE 256
#define ITEM_SIZE 32

#define INODE_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)
#define ITEM_PER_BLOCK (BLOCK_SIZE / ITEM_SIZE)
#define BLOCK_ID_PER_BLOCK (BLOCK_SIZE / sizeof(int16_t))

#define MAX_INODE_CNT 256
#define MAX_BLOCK_CNT 1024  // c = 32, s = 32
#define DIRECT_PTR_CNT 7

#define BITMAP_LEN 16

#define REVERSE_BLOCK_CNT (1 + MAX_BLOCK_CNT / INODE_PER_BLOCK)  // 1 + 32 = 33

#define MAX_ITEM_NAME_LEN 30
#define MAX_PASSWORD_LEN 11

#define MAX_BLOCK_PER_INODE (DIRECT_PTR_CNT + 1 + BLOCK_ID_PER_BLOCK)
#define MAX_FILE_BLOCK_CNT (DIRECT_PTR_CNT + BLOCK_ID_PER_BLOCK)
#define MAX_FILE_SIZE (MAX_FILE_BLOCK_CNT * BLOCK_SIZE)
#define MAX_BUFFER_SIZE 65536

#define VERIFICATION_CODE 0xdeadbeef114514ULL

int is_letter(char c);

int is_digits(char c);

int is_blank_string(const char *str);

#endif  // UTILS_H