#ifndef BDC_H
#define BDC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define C 32
#define S 32
#define BUFFER_SIZE 256
#define BLOCK_SIZE 256

void connect_to_BDS(char *argv[]);

int write_block(int block_no, char *buf);

int read_block(int block_no, char *buf);

#endif // BDC_H