#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "include/BDC.h"
#include "include/Inode.h"

#define MAX_PATH_LEN 256
int16_t crt_dir_inode_id;
char crt_path[MAX_PATH_LEN];
int format_flag;
int crt_usr_inode_id;
char READ_BUFFER[MAX_BUFFER_SIZE];
char WRITE_BUFFER[MAX_BUFFER_SIZE];

extern Inode inode_list[MAX_INODE_CNT];

int legal_username(char* name, int FC_socket) {
    int len = 0;
    int i = 0;
    while (name[i] == ' ') ++i;
    while (name[i] != '\0' && name[i] != ' ') {
        if (is_letter(name[i]) || is_digits(name[i])) {
            ++len;
            ++i;
        } else {
            write(FC_socket, "Illegal name: only letters and digits are legal\n", 49);
            return 0;
        }
    }
    while (name[i] != '\0' && name[i] == ' ') ++i;
    if (name[i] != '\0') {
        write(FC_socket, "Illegal name: no space is allowed in the name\n", 47);
        return 0;
    }
    if (len > MAX_ITEM_NAME_LEN) {
        write(FC_socket, "Illegal name: the length of the name is at most 30 characters\n", 63);
        return 0;
    }
    return 1;
}

int legal_name(char* name, int FC_socket) {
    int len = 0;
    int i = 0;
    while (name[i] == ' ') ++i;
    while (name[i] != '\0' && name[i] != ' ') {
        if (is_letter(name[i]) || is_digits(name[i]) || name[i] == '_' || name[i] == '.') {
            ++len;
            ++i;
        } else {
            write(FC_socket, "Illegal name: only letters, digits, _ and . are legal\n", 55);
            return 0;
        }
    }
    while (name[i] != '\0' && name[i] == ' ') ++i;
    if (name[i] != '\0') {
        write(FC_socket, "Illegal name: no space is allowed in the name\n", 47);
        return 0;
    }
    if (len > MAX_ITEM_NAME_LEN) {
        write(FC_socket, "Illegal name: the length of the name is at most 30 characters\n", 63);
        return 0;
    }
    return 1;
}

int is_descendant(int16_t ancestor, int16_t descendant) {
    int16_t parent_id = inode_list[descendant].parent;
    while (parent_id >= 0) {
        if (parent_id == ancestor) return 1;
        parent_id = inode_list[parent_id].parent;
    }
    return 0;
}

int check_permission(int16_t dir_inode_id) {
    int16_t public_inode_id = search_in_dir(&inode_list[0], "public", 1);
    if (dir_inode_id == public_inode_id || is_descendant(public_inode_id, dir_inode_id)) return 1;
    if (dir_inode_id == crt_dir_inode_id || is_descendant(crt_dir_inode_id, dir_inode_id)) return 1;
    return 0;
}

int f_handler(char* args, int FC_socket) {
    init_superblock();
    init_inodes();
    crt_usr_inode_id = crt_dir_inode_id = search_in_dir(&inode_list[0], "public", 1);
    memset(crt_path, 0, sizeof(crt_path));
    strcpy(crt_path, "/public");
    format_flag = 1;
    write(FC_socket, "Format the file system successfully\n", 37);
    return 0;
}

int mk_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    if (!legal_name(args, FC_socket)) return 0;
    int16_t ret = search_in_dir(&inode_list[crt_dir_inode_id], args, 0);
    if (ret == -2) return 0;
    if (ret > 0) {
        sprintf(WRITE_BUFFER, "File \"%s\" already exists\n", args);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
        return 0;
    }
    if (add_to_dir(&inode_list[crt_dir_inode_id], args, 0) >= 0) {
        sprintf(WRITE_BUFFER, "Create file \"%s\" successfully\n", args);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
    }
    return 0;
}

int mkdir_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    if (!legal_name(args, FC_socket)) return 0;
    int16_t ret = search_in_dir(&inode_list[crt_dir_inode_id], args, 1);
    if (ret == -2) return 0;
    if (ret > 0) {
        sprintf(WRITE_BUFFER, "Directory \"%s\" already exists\n", args);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
        return 0;
    }
    if (add_to_dir(&inode_list[crt_dir_inode_id], args, 1) >= 0) {
        sprintf(WRITE_BUFFER, "Create directory \"%s\" successfully\n", args);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
    }
    return 0;
}

int rm_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    if (!legal_name(args, FC_socket)) return 0;
    if (remove_from_dir(&inode_list[crt_dir_inode_id], args, 0) == 0) {
        sprintf(WRITE_BUFFER, "Remove file \"%s\" successfully\n", args);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
    }
    return 0;
}

int cd_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    int16_t new_dir_inode_id = crt_dir_inode_id;
    char new_path[MAX_PATH_LEN];
    strcpy(new_path, crt_path);

    if (args[0] == '/') {
        new_dir_inode_id = 0;
        strcpy(new_path, "/");
        args++;
    }

    char* dir = strtok(args, "/");
    while (dir != NULL) {
        if (strcmp(dir, "..") == 0) {
            int16_t parent_id = inode_list[crt_dir_inode_id].parent;
            if (parent_id >= 0) {
                new_dir_inode_id = parent_id;
                char* tail = strrchr(new_path, '/');
                if (new_dir_inode_id != 0) {
                    *tail = '\0';
                } else {
                    *(tail + 1) = '\0';
                }
            }
        } else if (strcmp(dir, ".") == 0) {
            dir = strtok(NULL, "/");
            continue;
        } else {
            int16_t ret = search_in_dir(&inode_list[new_dir_inode_id], dir, 1);
            if (ret == -2) return 0;
            if (ret < 0) {
                write(FC_socket, "Directory not found\n", 21);
                return 0;
            }
            if (new_dir_inode_id != 0) {
                strcat(new_path, "/");
            }
            strcat(new_path, dir);
            new_dir_inode_id = ret;
        }
        dir = strtok(NULL, "/");
    }
    if (check_permission(new_dir_inode_id) == 0) {
        write(FC_socket, "Permission denied\n", 19);
        return 0;
    }
    crt_dir_inode_id = new_dir_inode_id;
    strcpy(crt_path, new_path);
    write(FC_socket, "\0", 1);
    return 0;
}

int rmdir_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    if (!legal_name(args, FC_socket)) return 0;
    if (remove_dir_recursively_from_dir(&inode_list[crt_dir_inode_id], args) == 0) {
        sprintf(WRITE_BUFFER, "Remove directory \"%s\" successfully\n", args);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
    }
    return 0;
}

// maybe to handle ls [path] later
int ls_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    memset(WRITE_BUFFER, 0, MAX_BUFFER_SIZE);
    list_dir(&inode_list[crt_dir_inode_id], WRITE_BUFFER);
    if (strlen(WRITE_BUFFER) == 0) {
        write(FC_socket, "\0", 1);
        return 0;
    }
    write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
    return 0;
}

int cat_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    int16_t file_inode_id;
    char content[MAX_FILE_SIZE];
    if (!legal_name(args, FC_socket)) return 0;
    if ((file_inode_id = search_in_dir(&inode_list[crt_dir_inode_id], args, 0)) < 0) {
        sprintf(WRITE_BUFFER, "File \"%s\" not found\n", args);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
        return 0;
    }
    if (read_file(&inode_list[file_inode_id], content) > 0) {
        sprintf(WRITE_BUFFER, "%s\n", content);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
    }
    return 0;
}

int w_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    char file[MAX_ITEM_NAME_LEN];
    int16_t file_inode_id;
    int len;
    char content[MAX_FILE_SIZE];
    memset(content, 0, MAX_FILE_SIZE);
    sscanf(args, "%s %d %[^\n]", file, &len, content);
    if (!legal_name(file, FC_socket)) return 0;
    if ((file_inode_id = search_in_dir(&inode_list[crt_dir_inode_id], file, 0)) < 0) {
        sprintf(WRITE_BUFFER, "File \"%s\" not found\n", file);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
        return 0;
    }
    if (len <= 0) {
        write(FC_socket, "Illegal length\n", 16);
        return 0;
    } else if (len < strlen(content)) {
        write(FC_socket, "Content longer than provided length\n", 37);
        return 0;
    }
    len = strlen(content);
    if (write_file(&inode_list[file_inode_id], content, (uint16_t)len) == 0) {
        sprintf(WRITE_BUFFER, "Write file \"%s\" successfully\n", file);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
    }
    return 0;
}

int i_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    char file[MAX_ITEM_NAME_LEN];
    int16_t file_inode_id;
    int pos, len;
    char content[MAX_FILE_SIZE];
    char insert_content[MAX_FILE_SIZE];
    sscanf(args, "%s %d %d %[^\n]", file, &pos, &len, insert_content);
    if (!legal_name(file, FC_socket)) return 0;
    if ((file_inode_id = search_in_dir(&inode_list[crt_dir_inode_id], file, 0)) < 0) {
        sprintf(WRITE_BUFFER, "File \"%s\" not found\n", file);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
        return 0;
    }
    if (pos < 0) {
        write(FC_socket, "Illegal position\n", 18);
        return 0;
    }
    int file_size = inode_list[file_inode_id].file_size;
    if (pos > file_size) pos = file_size;
    if (len <= 0) {
        write(FC_socket, "Illegal length\n", 16);
        return 0;
    } else if (len < strlen(insert_content)) {
        write(FC_socket, "Content to insert longer than provided length\n", 47);
        return 0;
    }
    len = strlen(insert_content);
    if (file_size + len > MAX_FILE_SIZE) {
        write(FC_socket, "Exceeds the file size after inserting\n", 39);
        return 0;
    }
    read_file(&inode_list[file_inode_id], content);
    memmove(content + pos + len, content + pos, file_size - pos);
    memcpy(content + pos, insert_content, len);
    if (write_file(&inode_list[file_inode_id], content, (uint16_t)(file_size + len)) == 0) {
        sprintf(WRITE_BUFFER, "Insert content to file \"%s\" successfully\n", file);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
    }
    return 0;
}

int d_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    char file[MAX_ITEM_NAME_LEN];
    int16_t file_inode_id;
    int pos, len;
    char content[MAX_FILE_SIZE];
    sscanf(args, "%s %d %d", file, &pos, &len);
    if (!legal_name(file, FC_socket)) return 0;
    if ((file_inode_id = search_in_dir(&inode_list[crt_dir_inode_id], file, 0)) < 0) {
        sprintf(WRITE_BUFFER, "File \"%s\" not found\n", file);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
        return 0;
    }
    if (pos < 0) {
        write(FC_socket, "Illegal position\n", 18);
        return 0;
    }
    if (len <= 0) {
        write(FC_socket, "Illegal length\n", 16);
        return 0;
    }
    int file_size = inode_list[file_inode_id].file_size;
    if (pos >= file_size) {
        write(FC_socket, "Position exceeds the file size. Delete nothing\n", 48);
        return 0;
    }
    if (pos + len > file_size) len = file_size - pos;
    read_file(&inode_list[file_inode_id], content);
    memmove(content + pos, content + pos + len, file_size - pos - len);
    if (write_file(&inode_list[file_inode_id], content, (uint16_t)(file_size - len)) == 0) {
        sprintf(WRITE_BUFFER, "Delete content from file \"%s\" successfully\n", file);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
    }
    return 0;
}

int e_handler(char* args, int FC_socket) {
    if (!is_blank_string(args)) {
        write(FC_socket, "Command not found\n", 19);
        return 0;
    }
    write(FC_socket, "Exit", 5);
    return -1;
}

int useradd_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    char username[MAX_ITEM_NAME_LEN];
    char password[MAX_PASSWORD_LEN];
    memset(username, 0, MAX_ITEM_NAME_LEN);
    memset(password, 0, MAX_PASSWORD_LEN);
    sscanf(args, "%s %[^\n]", username, password);
    if (!legal_username(username, FC_socket) || !legal_name(password, FC_socket)) return 0;
    if (password[0] == '\0') {
        write(FC_socket, "Password cannot be empty\n", 26);
        return 0;
    }
    if (password[MAX_PASSWORD_LEN - 1] != '\0') {
        write(FC_socket, "Password is at most 10 characters (letters, digits, _ and .)\n", 62);
        return 0;
    }
    int16_t usr_dir_id = search_in_dir(&inode_list[0], "usr", 1);
    if (search_in_dir(&inode_list[usr_dir_id], username, 0) >= 0) {
        write(FC_socket, "User already exists\n", 21);
        return 0;
    }
    int16_t usr_file_id = add_to_dir(&inode_list[usr_dir_id], username, 0);
    write_file(&inode_list[usr_file_id], password, MAX_PASSWORD_LEN);
    int16_t home_dir_id = search_in_dir(&inode_list[0], "home", 1);
    add_to_dir(&inode_list[home_dir_id], username, 1);
    sprintf(WRITE_BUFFER, "Add user \033[1m\033[34m%s\033[0m successfully\n", username);
    write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
    return 0;
}

int userdel_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    char username[MAX_ITEM_NAME_LEN];
    char password[MAX_PASSWORD_LEN];
    memset(username, 0, MAX_ITEM_NAME_LEN);
    memset(password, 0, MAX_PASSWORD_LEN);
    sscanf(args, "%s %[^\n]", username, password);
    if (!legal_username(username, FC_socket) || !legal_name(password, FC_socket)) return 0;
    if (password[0] == '\0') {
        write(FC_socket, "Password cannot be empty\n", 26);
        return 0;
    }
    if (password[MAX_PASSWORD_LEN - 1] != '\0') {
        write(FC_socket, "Password is at most 10 characters (letters, digits, _ and .)\n", 62);
        return 0;
    }
    int16_t usr_dir_id = search_in_dir(&inode_list[0], "usr", 1);
    int16_t usr_file_id = search_in_dir(&inode_list[usr_dir_id], username, 0);
    if (usr_file_id == -1) {
        write(FC_socket, "User doesn't exit\n", 19);
        return 0;
    }
    read_file(&inode_list[usr_file_id], READ_BUFFER);
    if (strncmp(READ_BUFFER, password, MAX_PASSWORD_LEN) != 0) {
        write(FC_socket, "Password incorrect\n", 20);
        return 0;
    }
    remove_from_dir(&inode_list[usr_dir_id], username, 0);
    int16_t home_dir_id = search_in_dir(&inode_list[0], "home", 1);
    if (remove_dir_recursively_from_dir(&inode_list[home_dir_id], username) == 0) {
        sprintf(WRITE_BUFFER, "Delete user \033[1m\033[34m%s\033[0m successfully\n", username);
        write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
    };
    return 0;
}

int su_handler(char* args, int FC_socket) {
    if (!format_flag) {
        write(FC_socket, "Please format the file system first\n", 37);
        return 0;
    }
    char username[MAX_ITEM_NAME_LEN];
    char password[MAX_PASSWORD_LEN];
    memset(username, 0, MAX_ITEM_NAME_LEN);
    memset(password, 0, MAX_PASSWORD_LEN);
    sscanf(args, "%s %[^\n]", username, password);
    if (!legal_username(username, FC_socket) || !legal_name(password, FC_socket)) return 0;
    if (password[0] == '\0') {
        write(FC_socket, "Password cannot be empty\n", 26);
        return 0;
    }
    if (password[MAX_PASSWORD_LEN - 1] != '\0') {
        write(FC_socket, "Password is at most 10 characters (letters, digits, _ and .)\n", 62);
        return 0;
    }
    int16_t usr_dir_id = search_in_dir(&inode_list[0], "usr", 1);
    int16_t usr_file_id = search_in_dir(&inode_list[usr_dir_id], username, 0);
    if (usr_file_id == -1) {
        write(FC_socket, "User doesn't exit\n", 19);
        return 0;
    }
    read_file(&inode_list[usr_file_id], READ_BUFFER);
    if (strncmp(READ_BUFFER, password, MAX_PASSWORD_LEN) != 0) {
        write(FC_socket, "Password incorrect\n", 20);
        return 0;
    }
    int16_t home_dir_id = search_in_dir(&inode_list[0], "home", 1);
    crt_usr_inode_id = crt_dir_inode_id = search_in_dir(&inode_list[home_dir_id], username, 1);
    strcpy(crt_path, "/home/");
    strcat(crt_path, username);
    sprintf(WRITE_BUFFER, "Switch to user \033[1m\033[34m%s\033[0m successfully\n", username);
    write(FC_socket, WRITE_BUFFER, strlen(WRITE_BUFFER));
    return 0;
}

typedef struct command {
    const char* cmd_name;
    int (*cmd_handler)(char* args, int FC_socket);
} Command;

Command cmd_list[] = {
    {"f", f_handler},
    {"mk", mk_handler},
    {"mkdir", mkdir_handler},
    {"rm", rm_handler},
    {"cd", cd_handler},
    {"rmdir", rmdir_handler},
    {"ls", ls_handler},
    {"cat", cat_handler},
    {"w", w_handler},
    {"i", i_handler},
    {"d", d_handler},
    {"e", e_handler},
    {"useradd", useradd_handler},
    {"userdel", userdel_handler},
    {"su", su_handler}};

const int cmd_list_len = sizeof(cmd_list) / sizeof(Command);

void serve_FC(int FC_socket) {
    char args[MAX_BUFFER_SIZE];
    char cmd_name[10];
    int cmd_ret;
    int cmd_miss;

    memset(crt_path, 0, MAX_PATH_LEN);
    format_flag = 0;

    if (load_superblock() == 0) {
        load_inodes();
        crt_usr_inode_id = crt_dir_inode_id = search_in_dir(&inode_list[0], "public", 1);
        strcpy(crt_path, "/public");
        format_flag = 1;
    }
    while (1) {
        write(FC_socket, crt_path, sizeof(crt_path));
        memset(READ_BUFFER, 0, MAX_BUFFER_SIZE);
        read(FC_socket, READ_BUFFER, MAX_BUFFER_SIZE);
        printf("Receive command: %s\n", READ_BUFFER);
        memset(args, 0, MAX_BUFFER_SIZE);
        sscanf(READ_BUFFER, "%s %[^\n]", cmd_name, args);
        cmd_miss = 1;
        for (int i = 0; i < cmd_list_len; ++i) {
            if (strcmp(cmd_name, cmd_list[i].cmd_name) == 0) {
                cmd_ret = cmd_list[i].cmd_handler(args, FC_socket);
                cmd_miss = 0;
                break;
            }
        }
        if (cmd_miss) {
            write(FC_socket, "Command not found\n", 19);
        }
        if (cmd_ret == -1) {
            break;
        }
        memset(READ_BUFFER, 0, MAX_BUFFER_SIZE);
        read(FC_socket, READ_BUFFER, 3);
        if (strcmp(READ_BUFFER, "OK") != 0) {
            printf("Fail to receive confirm from FC\n");
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: ./FS <DiskServerAddress> <BDSPort> <FSPort>\n");
        exit(-1);
    }
    connect_to_BDS(argv);

    int FSPort = atoi(argv[3]);
    int FS_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (FS_sockfd < 0) {
        perror("Fail to open socket\n");
        exit(-1);
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr));
    server_addr.sin_port = htons(FSPort);
    bind(FS_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(FS_sockfd, 5);
    printf("Listen on port %d\nWaiting for connections ...\n", FSPort);
    while (1) {
        int FC_socket;
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        FC_socket = accept(FS_sockfd, (struct sockaddr*)&client_addr, &addr_len);
        printf("Connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        if (fork() == 0) {
            serve_FC(FC_socket);
            close(FC_socket);
            exit(0);
        }
        close(FC_socket);
        wait(NULL);
    }
}