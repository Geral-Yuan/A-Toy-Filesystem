#include "include/BDC.h"

char BLOCK_WRITE_BUFFER[BUFFER_SIZE];
char BLOCK_READ_BUFFER[BUFFER_SIZE];

int socket_to_BDS;

void connect_to_BDS(char *argv[]) {
    int BDSPort = atoi(argv[2]);
    socket_to_BDS = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_to_BDS < 0) {
        perror("Fail to open socket\n");
        exit(-1);
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BDSPort);
    if (strcmp(argv[1], "localhost") == 0) {
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    } else {
        if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
            printf("Invalid address\n");
            exit(-1);
        }
    }
    if (connect(socket_to_BDS, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Fail to connect to server\n");
        exit(-1);
    }
}

int write_block(int block_no, char *buf) {
    memset(BLOCK_WRITE_BUFFER, 0, BUFFER_SIZE);
    sprintf(BLOCK_WRITE_BUFFER, "W %d %d", block_no / S, block_no % S);
    write(socket_to_BDS, BLOCK_WRITE_BUFFER, strlen(BLOCK_WRITE_BUFFER));
    // printf("Send write request at %d %d\n", block_no / S, block_no % S); // for debugging
    memset(BLOCK_READ_BUFFER, 0, BUFFER_SIZE);
    read(socket_to_BDS, BLOCK_READ_BUFFER, BUFFER_SIZE);
    if (strcmp(BLOCK_READ_BUFFER, "OK") != 0) {
        printf("Fail to write block %d\n", block_no);
        return -1;
    }
    write(socket_to_BDS, buf, BLOCK_SIZE);
    // printf("Send block data\n"); // for debugging
    memset(BLOCK_READ_BUFFER, 0, BUFFER_SIZE);
    read(socket_to_BDS, BLOCK_READ_BUFFER, BUFFER_SIZE);
    // printf("Receive response\n"); // for debugging
    if (strcmp(BLOCK_READ_BUFFER, "No") == 0) {
        printf("Fail to write block %d\n", block_no);
        return -1;
    }
    printf("Write block %d\n", block_no); // for debugging
    return 0;
}

int read_block(int block_no, char *buf) {
    memset(BLOCK_WRITE_BUFFER, 0, BUFFER_SIZE);
    sprintf(BLOCK_WRITE_BUFFER, "R %d %d", block_no / S, block_no % S);
    write(socket_to_BDS, BLOCK_WRITE_BUFFER, strlen(BLOCK_WRITE_BUFFER));
    // printf("Send read request at %d %d\n", block_no / S, block_no % S); // for debugging
    memset(BLOCK_READ_BUFFER, 0, BUFFER_SIZE);
    read(socket_to_BDS, BLOCK_READ_BUFFER, BUFFER_SIZE);
    // printf("Receive response\n"); // for debugging
    if (strcmp(BLOCK_READ_BUFFER, "No") == 0) {
        printf("Fail to read block %d\n", block_no);
        return -1;
    }
    write(socket_to_BDS, "OK", 3);
    read(socket_to_BDS, buf, BLOCK_SIZE);
    // printf("Receive block data\n"); // for debugging
    printf("Read block %d\n", block_no); // for debugging
    return 0;
}