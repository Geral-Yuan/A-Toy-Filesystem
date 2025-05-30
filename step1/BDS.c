#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <math.h>

#define BLOCKSIZE 256  // block size is 256 bytes
#define BUFFER_SIZE 1024

static char RW_buf[BLOCKSIZE + 1];
static char buf[BUFFER_SIZE];
static int C, S, delay, port;
static int crt_track = 0;
static char filename[32];
static int FILESIZE;

char *initDisk(int fd) {
    FILESIZE = C * S * BLOCKSIZE;
    if (lseek(fd, FILESIZE - 1, SEEK_SET) == -1) {
        perror("Error calling lseek() to 'stretch' the file");
        close(fd);
        exit(-1);
    }
    if (write(fd, "", 1) != 1) {
        perror("Error writing last byte of the file");
        close(fd);
        exit(-1);
    }
    char *diskfile = (char *)mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (diskfile == MAP_FAILED) {
        printf("Error: Could not map file.\n");
        close(fd);
        exit(-1);
    }
    return diskfile;
}

// TO DO: support input string (to write into the disk) with space
void ParseCommand(char *cmd, int *argcnt, char *argval[]) {
    char *arg;
    *argcnt = 0;
    arg = strtok(cmd, " ");
    while (arg) {
        argval[(*argcnt)++] = arg;
        arg = strtok(NULL, " ");
        if (*argcnt == 5) break;
    }
}

void Serve(int client_sockfd, char *diskfile) {
    int argcnt;
    char *argval[5];
    while (1) {
        int len = read(client_sockfd, buf, BUFFER_SIZE);
        buf[len] = '\0';
        printf("\nReceive request: %s\n", buf);
        ParseCommand(buf, &argcnt, argval);
        if (argcnt == 0) {
            write(client_sockfd, "Empty command. Try again", 25);
            continue;
        }
        if (argcnt == 1 && strcmp(argval[0], "E") == 0) {
            printf("Exiting ...\n");
            write(client_sockfd, "exit", 5);
            break;
        } else if (argcnt == 1 && strcmp(argval[0], "I") == 0) {
            sprintf(buf, "%d %d", C, S);
            printf("%s\n", buf);
            write(client_sockfd, buf, strlen(buf));
        } else if (argcnt == 3 && strcmp(argval[0], "R") == 0) {
            int c, s;
            c = atoi(argval[1]);
            s = atoi(argval[2]);
            if (c >= 0 && c < C && s >= 0 && s < S) {
                if (crt_track != c) {
                    printf("Seeking from track %d to track %d\n", crt_track, c);
                    usleep(abs(crt_track - c) * delay);
                    crt_track = c;
                }
                memcpy(RW_buf, diskfile + BLOCKSIZE * (c * S + s), BLOCKSIZE);
                sprintf(buf, "Yes %s", RW_buf);
                printf("%s\n", buf);
                write(client_sockfd, buf, strlen(buf));
            } else {
                puts("No");
                write(client_sockfd, "No", 3);
            }
        } else if (argcnt == 5 && strcmp(argval[0], "W") == 0) {
            int c, s, l;
            c = atoi(argval[1]);
            s = atoi(argval[2]);
            l = atoi(argval[3]);
            memset(RW_buf, 0, sizeof(RW_buf));
            strncpy(RW_buf, argval[4], BLOCKSIZE);
            if (c >= 0 && c < C && s >= 0 && s < S && l >= strlen(RW_buf) && l <= BLOCKSIZE) {
                if (crt_track != c) {
                    printf("Seeking from track %d to track %d\n", crt_track, c);
                    usleep(abs(crt_track - c) * delay);
                    crt_track = c;
                }
                memcpy(diskfile + BLOCKSIZE * (c * S + s), RW_buf, l);
                memset(diskfile + BLOCKSIZE * (c * S + s) + l, 0, BLOCKSIZE - l);
                puts("Yes");
                write(client_sockfd, "Yes", 4);
            } else {
                puts("No");
                write(client_sockfd, "No", 3);
            }
        } else {
            printf("Error: Invalid command\n");
            write(client_sockfd, "Error: Invalid command", 23);
        }
    }
}

void handle_sigint(int sig) {
    printf("\nReceived SIGINT (signal %d). Exiting cleanly...\n", sig);
    exit(0);
}

int main(int argc, char *argv[]) {
    // parse the arguments to main()
    if (argc != 6) {
        printf("Usage: ./BDS <DiskFIleName> <#cylinders> <#sectors per cylinder> <track-to-track delay> <Port>\n");
        exit(-1);
    }
    strcpy(filename, argv[1]);
    C = atoi(argv[2]);
    S = atoi(argv[3]);
    delay = atoi(argv[4]);
    port = atoi(argv[5]);

    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    int fd = open(filename, O_RDWR | O_CREAT, 0);
    if (fd < 0) {
        printf("Error: Could not open file '%s'.\n", filename);
        exit(-1);
    }
    char *diskfile = initDisk(fd);

    int server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("Fail to open socket\n");
        exit(-1);
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr));
    server_addr.sin_port = htons(port);
    bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_sockfd, 5);
    printf("Listen on port %d\nWaiting for connections ...\n", port);
    while (1) {
        int client_sockfd;
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &addr_len);
        printf("Connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        Serve(client_sockfd, diskfile);
        close(fd);
        close(client_sockfd);
    }
    munmap(diskfile, FILESIZE);
    return 0;
}