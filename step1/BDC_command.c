#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
static char buf[BUFFER_SIZE];

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./BDC_command <DiskServerAddress> <Port>\n");
        exit(-1);
    }
    int port = atoi(argv[2]);

    int client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sockfd < 0) {
        perror("Fail to open socket\n");
        exit(-1);
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
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
    while (1) {
        printf("\nEnter your request: ");
        fgets(buf, BUFFER_SIZE, stdin);
        if (feof(stdin)) {
            write(client_sockfd, "E", 1);
            printf("\nExiting ...\n");
            break;
        }
        buf[strcspn(buf, "\n")] = '\0';
        if (strlen(buf) == 0) continue;
        if (write(client_sockfd, buf, strlen(buf)) != strlen(buf)) {
            perror("Write failed");
            close(client_sockfd);
            exit(-1);
        }
        int len = read(client_sockfd, buf, BUFFER_SIZE);
        buf[len] = '\0';
        if (strcmp(buf, "exit") == 0) {
            printf("Exiting ...\n");
            break;
        }
        printf("%s\n", buf);
    }
    close(client_sockfd);
    return 0;
}