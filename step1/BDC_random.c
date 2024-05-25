#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_SIZE 1024
static char buf[BUFFER_SIZE];

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./BDC_random <DiskServerAddress> <Port>\n");
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

    int N;
    int C, S;
    int rqst, c, s;
    char CONTENT[257];
    srand(time(NULL));
    printf("\nRequest: I\n");
    write(client_sockfd, "I", 1);
    int len = read(client_sockfd, buf, BUFFER_SIZE);
    buf[len] = '\0';
    printf("%s\n", buf);
    sscanf(buf, "%d %d", &C, &S);
    printf("\nEnter the number of R/W requests: ");
    scanf("%d", &N);
    for (int i = 0; i < N; ++i) {
        rqst = rand() % 2;
        c = rand() % C;
        s = rand() % S;
        if (rqst == 0) {  // read
            sprintf(buf, "R %d %d", c, s);
        } else {  // write
            for (int i = 0; i < 256; ++i) {
                char c = 33 + rand() % (126 - 33 + 1);
                CONTENT[i] = c;
            }
            CONTENT[256] = '\0';
            sprintf(buf, "W %d %d 256 %s", c, s, CONTENT);
        }
        printf("\nRequest: %s\n", buf);
        write(client_sockfd, buf, strlen(buf));
        len = read(client_sockfd, buf, BUFFER_SIZE);
        printf("%c\n", buf[0]);
    }
    printf("All requests are sent!\nExiting ...\n");
    write(client_sockfd, "E", 1);
    close(client_sockfd);
    return 0;
}