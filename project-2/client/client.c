#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 8192
#define SERVER_PORT 9000

int sockfd = -1;
volatile int running = 1;
char username[128];

void trim_nl(char *s) { size_t n = strlen(s); if (n && s[n-1] == '\n') s[n-1] = '\0'; }

void *recv_thread(void *arg) {
    (void)arg;
    char buf[BUF_SIZE];
    while (running) {
        ssize_t n = recv(sockfd, buf, sizeof(buf)-1, 0);
        if (n <= 0) { printf("\nDisconnected from server.\n"); running = 0; break; }
        buf[n] = '\0';

        if (strncmp(buf, "MSG|", 4) == 0) {
            char *p = buf + 4;
            char *sep = strchr(p, '|');
            if (sep) {
                *sep = '\0';
                char *sender = p;
                char *message = sep + 1;
                trim_nl(message);
                printf("\nMessage from %s: %s\n", sender, message);
            } else {
                printf("\n[SERVER] %s\n", buf);
            }
        } else if (strncmp(buf, "MSGHIST|", 8) == 0 || strncmp(buf, "HIST|", 5) == 0 || strncmp(buf, "ENDHIST", 7) == 0) {
            char *line = strtok(buf, "\n");
            while (line) {
                if (strncmp(line, "MSGHIST|", 8) == 0) {
                    printf("\n--- Message history: %s ---\n", line + 8);
                } else if (strncmp(line, "HIST|", 5) == 0) {
                    char *p = line + 5;
                    char *s = strtok(p, "|");
                    char *r = strtok(NULL, "|");
                    char *t = strtok(NULL, "|");
                    char *m = strtok(NULL, "");
                    if (s && r && t && m) printf("%s (%s) -> %s\n", t, s, m);
                } else if (strcmp(line, "ENDHIST") == 0) {
                    printf("--- End history ---\n");
                } else {
                    printf("%s\n", line);
                }
                line = strtok(NULL, "\n");
            }
        } else if (strncmp(buf, "USERLIST|", 9) == 0) {
            char *p = buf + 9; trim_nl(p);
            printf("\nOnline users: %s\n", p);
        } else if (strncmp(buf, "SERVER|SHUTDOWN", 15) == 0) {
            printf("\nServer shutting down...\n");
            running = 0; break;
        } else {
            printf("\n[SERVER] %s\n", buf);
        }
        printf("\n> "); fflush(stdout);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2) { printf("Usage: ./client <server_ip>\n"); return 1; }
    const char *server_ip = argv[1];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &serv.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&serv, sizeof(serv)) < 0) { perror("connect"); close(sockfd); return 1; }

    printf("Connected to server %s:%d\n", server_ip, SERVER_PORT);
    printf("Enter username: "); fflush(stdout);
    if (!fgets(username, sizeof(username), stdin)) { close(sockfd); return 1; }
    trim_nl(username);
    if (strlen(username) == 0) { printf("Username cannot be empty\n"); close(sockfd); return 1; }

    char reg[256];
    snprintf(reg, sizeof(reg), "USER|%s\n", username);
    send(sockfd, reg, strlen(reg), 0);

    char reply[256];
    ssize_t r = recv(sockfd, reply, sizeof(reply)-1, 0);
    if (r <= 0) { printf("Server closed connection\n"); close(sockfd); return 1; }
    reply[r] = '\0';
    printf("%s", reply);

    pthread_t tid;
    if (pthread_create(&tid, NULL, recv_thread, NULL) != 0) { perror("pthread_create"); close(sockfd); return 1; }

    char input[BUF_SIZE];
    printf("\nCommands:\n  sendmessage <user> <message>\n  getmessages <user>\n  deletemessages <user>\n  getuserlist\n  exit\n");

    while (running) {
        printf("\n> "); fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) break;
        trim_nl(input);
        if (strlen(input) == 0) continue;

        if (strncmp(input, "sendmessage ", 12) == 0) {
            char *p = input + 12;
            char *space = strchr(p, ' ');
            if (!space) { printf("Usage: sendmessage <user> <message>\n"); continue; }
            *space = '\0';
            char *user = p;
            char *message = space + 1;
            char out[BUF_SIZE];
            snprintf(out, sizeof(out), "SEND|%s|%s\n", user, message);
            send(sockfd, out, strlen(out), 0);
        } else if (strncmp(input, "getmessages ", 12) == 0) {
            char *p = input + 12;
            char out[256]; snprintf(out, sizeof(out), "GET|%s\n", p);
            send(sockfd, out, strlen(out), 0);
        } else if (strncmp(input, "deletemessages ", 15) == 0) {
            char *p = input + 15; char out[256]; snprintf(out, sizeof(out), "DELETE|%s\n", p); send(sockfd, out, strlen(out), 0);
        } else if (strcmp(input, "getuserlist") == 0) {
            send(sockfd, "USERS\n", 6, 0);
        } else if (strcmp(input, "exit") == 0) {
            send(sockfd, "EXIT\n", 5, 0);
            running = 0; break;
        } else {
            printf("Unknown command. Use: sendmessage/getmessages/deletemessages/getuserlist/exit\n");
        }
    }

    printf("Closing client...\n");
    running = 0;
    close(sockfd);
    pthread_join(tid, NULL);
    return 0;
}
