#include "handlers.h"
#include "../db/db.h"
#include "users.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>

static void undelivered_cb(const char *sender, const char *receiver, const char *message, void *userdata) {
    int sockfd = *(int*)userdata;
    char out[HANDLER_BUF_SIZE];
    /* Use the required client display format: "Message from <sender>: <message>\n" */
    snprintf(out, sizeof(out), "MSG|%s|%s\n", sender, message); // protocol to client threads
    send(sockfd, out, strlen(out), 0);
}

int send_message_handler(const char *sender, const char *receiver, const char *message) {
    if (!sender || !receiver || !message) return -1;
    if (strlen(message) > HANDLER_MAX_MSG) return -2;

    /* check if receiver online */
    int recv_fd = find_active_user_sock(receiver);
    int delivered_flag = (recv_fd != -1) ? 1 : 0;

    if (store_message_db(sender, receiver, message, delivered_flag) != 0) {
        return -1;
    }

    if (delivered_flag) {
        char out[HANDLER_BUF_SIZE];
        /* Use server->client message format: MSG|sender|message\n */
        snprintf(out, sizeof(out), "MSG|%s|%s\n", sender, message);
        send(recv_fd, out, strlen(out), 0);
        /* Optionally mark as delivered in DB */
        mark_messages_delivered_db(receiver);
        return 1;
    }

    return 0; /* stored */
}

void push_undelivered_for_user(const char *username, int sockfd) {
    if (!username) return;
    int sockfd_copy = sockfd;
    get_undelivered_messages_db(username, undelivered_cb, &sockfd_copy);
    /* mark delivered */
    mark_messages_delivered_db(username);
}
