#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../db/db.h"
#include "users.h"
#include "handlers.h"

#define SERVER_PORT 9000
#define BACKLOG 10
#define BUF_SIZE 8192

static volatile sig_atomic_t g_shutdown = 0;
static int g_listen_fd = -1;

/* =============================================================== */
/* Graceful shutdown handler */
void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
    if (g_listen_fd != -1)
        close(g_listen_fd);
}

/* Utility */
static void send_to_client(int sockfd, const char *msg) {
    if (!msg) return;
    send(sockfd, msg, strlen(msg), 0);
}

/* =============================================================== */
/* HISTORY CALLBACK — must be global (NOT nested) */
static void history_callback(
    const char *sender,
    const char *receiver,
    const char *message,
    const char *timestamp,
    void *userdata
) {
    int fd = *(int*)userdata;
    char out[BUF_SIZE];

    snprintf(out, sizeof(out),
             "HIST|%s|%s|%s|%s\n",
             sender, receiver, timestamp, message);

    send(fd, out, strlen(out), 0);
}

/* =============================================================== */
/* Client handler thread */
static void *client_thread(void *arg) {
    int client_fd = *((int*)arg);
    free(arg);

    char buf[BUF_SIZE];
    char username[64] = {0};

    /* -----------------------------
       Step 1: Receive USER|<name>
       ----------------------------- */
    ssize_t n = recv(client_fd, buf, sizeof(buf)-1, 0);
    if (n <= 0) {
        close(client_fd);
        return NULL;
    }
    buf[n] = '\0';

    if (strncmp(buf, "USER|", 5) != 0) {
        send_to_client(client_fd, "ERROR|Expected USER|<name>\n");
        close(client_fd);
        return NULL;
    }

    char *name = buf + 5;
    char *nl = strchr(name, '\n');
    if (nl) *nl = '\0';

    if (strlen(name) == 0 || strlen(name) >= sizeof(username)) {
        send_to_client(client_fd, "ERROR|Invalid username\n");
        close(client_fd);
        return NULL;
    }

    /* Validate characters */
    for (size_t i = 0; i < strlen(name); i++) {
        char c = name[i];
        if (!(isalnum((unsigned char)c) || c == '_' || c == '-')) {
            send_to_client(client_fd, "ERROR|Invalid username chars\n");
            close(client_fd);
            return NULL;
        }
    }

    strncpy(username, name, sizeof(username)-1);

    /* Check duplicate */
    if (add_active_user(client_fd, username) != 0) {
        send_to_client(client_fd, "ERROR|Username in use\n");
        close(client_fd);
        return NULL;
    }

    printf("User '%s' connected (fd=%d)\n", username, client_fd);
    send_to_client(client_fd, "OK|Registered\n");

    /* Push undelivered messages */
    push_undelivered_for_user(username, client_fd);

    /* ======================================================== */
    /* MAIN LOOP — handle commands */
    /* ======================================================== */
    while ((n = recv(client_fd, buf, sizeof(buf)-1, 0)) > 0) {
        buf[n] = '\0';

        char *saveptr;
        char *line = strtok_r(buf, "\n", &saveptr);

        while (line) {

            /* ---------------------------
               SEND|receiver|message
               --------------------------- */
            if (strncmp(line, "SEND|", 5) == 0) {
                char *p = line + 5;
                char *sep = strchr(p, '|');
                if (!sep) {
                    send_to_client(client_fd, "ERROR|Malformed SEND\n");
                } else {
                    *sep = '\0';
                    char *receiver = p;
                    char *message  = sep + 1;

                    int res = send_message_handler(username, receiver, message);

                    if (res < 0)
                        send_to_client(client_fd, "ERROR|Store failed\n");
                    else if (res == 1)
                        send_to_client(client_fd, "OK|Delivered\n");
                    else
                        send_to_client(client_fd, "OK|Stored\n");
                }
            }

            /* ---------------------------
               GET|peer
               --------------------------- */
            else if (strncmp(line, "GET|", 4) == 0) {
                char *peer = line + 4;

                /* Begin history block */
                char header[128];
                snprintf(header, sizeof(header),
                         "MSGHIST|%s|%s\n", username, peer);
                send_to_client(client_fd, header);

                int fd_for_cb = client_fd;

                get_message_history_db(
                    username,
                    peer,
                    history_callback,   // <--- FIXED callback
                    &fd_for_cb
                );

                send_to_client(client_fd, "ENDHIST\n");
            }

            /* ---------------------------
               DELETE|peer
               --------------------------- */
            else if (strncmp(line, "DELETE|", 7) == 0) {
                char *peer = line + 7;

                if (delete_message_history_db(username, peer) == 0)
                    send_to_client(client_fd, "OK|Deleted\n");
                else
                    send_to_client(client_fd, "ERROR|Delete failed\n");
            }

            /* ---------------------------
               USERS
               --------------------------- */
            else if (strcmp(line, "USERS") == 0) {
                char *list = get_user_list_string();
                if (list) {
                    char out[BUF_SIZE];
                    snprintf(out, sizeof(out),
                             "USERLIST|%s\n", list);
                    send_to_client(client_fd, out);
                    free(list);
                } else {
                    send_to_client(client_fd, "USERLIST|\n");
                }
            }

            /* ---------------------------
               EXIT
               --------------------------- */
            else if (strcmp(line, "EXIT") == 0) {
                send_to_client(client_fd, "OK|Bye\n");
                remove_active_user_by_fd(client_fd);
                close(client_fd);
                printf("User '%s' disconnected\n", username);
                return NULL;
            }

            /* ---------------------------
               UNKNOWN
               --------------------------- */
            else {
                send_to_client(client_fd, "ERROR|Unknown command\n");
            }

            line = strtok_r(NULL, "\n", &saveptr);
        }
    }

    /* Client disconnected */
    remove_active_user_by_fd(client_fd);
    close(client_fd);
    printf("Connection closed (user=%s)\n", username);
    return NULL;
}

/* =============================================================== */
int main(void) {
    signal(SIGINT, handle_sigint);

    if (init_db("messaging.db") != 0) {
        fprintf(stderr, "DB init failed\n");
        return 1;
    }

    init_users();

    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port        = htons(SERVER_PORT);

    if (bind(g_listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(g_listen_fd);
        return 1;
    }

    if (listen(g_listen_fd, BACKLOG) < 0) {
        perror("listen");
        close(g_listen_fd);
        return 1;
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    /* =============================================================== */
    /* ACCEPT LOOP */
    /* =============================================================== */
    while (!g_shutdown) {
        int *pclient = malloc(sizeof(int));
        if (!pclient) continue;

        *pclient = accept(g_listen_fd, (struct sockaddr*)&cli_addr, &cli_len);

        if (*pclient < 0) {
            free(pclient);
            if (g_shutdown) break;
            perror("accept");
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, pclient) != 0) {
            perror("pthread_create");
            close(*pclient);
            free(pclient);
            continue;
        }
        pthread_detach(tid);
    }

    /* Graceful shutdown */
    printf("Shutting down server...\n");
    broadcast_message_all("SERVER|SHUTDOWN\n");
    shutdown_users();
    close_db();
    return 0;
}
