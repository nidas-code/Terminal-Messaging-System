#include "users.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>

typedef struct active_user {
    int sockfd;
    char username[64];
    struct active_user *next;
} active_user_t;

static active_user_t *g_user_list = NULL;
static pthread_mutex_t g_user_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_users(void) {
    pthread_mutex_lock(&g_user_mutex);
    g_user_list = NULL;
    pthread_mutex_unlock(&g_user_mutex);
}

int add_active_user(int sockfd, const char *username) {
    if (!username) return -1;
    pthread_mutex_lock(&g_user_mutex);
    active_user_t *cur = g_user_list;
    while (cur) {
        if (strcmp(cur->username, username) == 0) {
            pthread_mutex_unlock(&g_user_mutex);
            return -1; // duplicate
        }
        cur = cur->next;
    }
    active_user_t *u = malloc(sizeof(active_user_t));
    if (!u) { pthread_mutex_unlock(&g_user_mutex); return -1; }
    u->sockfd = sockfd;
    strncpy(u->username, username, sizeof(u->username)-1);
    u->username[sizeof(u->username)-1] = '\0';
    u->next = g_user_list;
    g_user_list = u;
    pthread_mutex_unlock(&g_user_mutex);
    return 0;
}

void remove_active_user_by_fd(int sockfd) {
    pthread_mutex_lock(&g_user_mutex);
    active_user_t *cur = g_user_list, *prev = NULL;
    while (cur) {
        if (cur->sockfd == sockfd) {
            if (prev) prev->next = cur->next; else g_user_list = cur->next;
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    pthread_mutex_unlock(&g_user_mutex);
}

int find_active_user_sock(const char *username) {
    if (!username) return -1;
    pthread_mutex_lock(&g_user_mutex);
    active_user_t *cur = g_user_list;
    while (cur) {
        if (strcmp(cur->username, username) == 0) {
            int fd = cur->sockfd;
            pthread_mutex_unlock(&g_user_mutex);
            return fd;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&g_user_mutex);
    return -1;
}

char *get_user_list_string(void) {
    pthread_mutex_lock(&g_user_mutex);
    size_t bufsize = 1024;
    char *out = malloc(bufsize);
    if (!out) { pthread_mutex_unlock(&g_user_mutex); return NULL; }
    out[0] = '\0';
    active_user_t *cur = g_user_list;
    int first = 1;
    while (cur) {
        if (!first) strncat(out, ",", bufsize - strlen(out) - 1);
        strncat(out, cur->username, bufsize - strlen(out) - 1);
        first = 0;
        cur = cur->next;
    }
    pthread_mutex_unlock(&g_user_mutex);
    return out;
}

void broadcast_message_all(const char *msg) {
    if (!msg) return;
    pthread_mutex_lock(&g_user_mutex);
    active_user_t *cur = g_user_list;
    while (cur) {
        send(cur->sockfd, msg, strlen(msg), 0);
        cur = cur->next;
    }
    pthread_mutex_unlock(&g_user_mutex);
}

void shutdown_users(void) {
    pthread_mutex_lock(&g_user_mutex);
    active_user_t *cur = g_user_list;
    while (cur) {
        close(cur->sockfd);
        active_user_t *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    g_user_list = NULL;
    pthread_mutex_unlock(&g_user_mutex);
}
