#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#define MAX_PATH_LEN 256
#define MAX_BUFFER_SIZE 65536

char READ_BUFFER[MAX_BUFFER_SIZE];
char WRITE_BUFFER[MAX_BUFFER_SIZE];

int is_blank_string(const char *str) {
    while (*str) {
        if (!isspace((unsigned char)*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./FC <ServerAddress> <FSPort>\n");
        exit(-1);
    }
    int FSPort = atoi(argv[2]);

    int client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sockfd < 0) {
        perror("Fail to open socket\n");
        exit(-1);
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(FSPort);
    if (strcmp(argv[1], "localhost") == 0) {
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    } else {
        if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
            printf("Invalid address\n");
            exit(-1);
        }
    }
    if (connect(client_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Fail to connect to server\n");
        exit(-1);
    }

    int last_cmd_empty = 0;
    char crt_path[MAX_PATH_LEN];
    memset(crt_path, 0, MAX_PATH_LEN);

    printf("Welcome to Geral's filesystem\n");
    while (1) {
        if (!last_cmd_empty){
            memset(READ_BUFFER, 0, MAX_BUFFER_SIZE);
            read(client_sockfd, READ_BUFFER, MAX_BUFFER_SIZE);
            strcpy(crt_path, READ_BUFFER);
        }
        printf("\033[1m\033[32mGeral-fs\033[0m:\033[1m\033[34m%s\033[0m$ ", crt_path);
        memset(WRITE_BUFFER, 0, MAX_BUFFER_SIZE);
        fgets(WRITE_BUFFER, MAX_BUFFER_SIZE, stdin);
        if (feof(stdin)) {
            printf("\nExiting ...\n");
            break;
        }
        last_cmd_empty = 0;
        if (is_blank_string(WRITE_BUFFER)) {
            last_cmd_empty = 1;
            continue;
        }
        write(client_sockfd, WRITE_BUFFER, strlen(WRITE_BUFFER));
        memset(READ_BUFFER, 0, MAX_BUFFER_SIZE);
        read(client_sockfd, READ_BUFFER, MAX_BUFFER_SIZE);
        if (strcmp(READ_BUFFER, "Exit") == 0) {
            printf("Exiting ...\n");
            break;
        }
        printf("%s", READ_BUFFER);
        write(client_sockfd, "OK", 3);
    }
}