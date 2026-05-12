#ifndef USERS_H
#define USERS_H

/* user list management functions */

/* Initialize user list */
void init_users(void);

/* Add active user: returns 0 on success, -1 if username duplicate or error */
int add_active_user(int sockfd, const char *username);

/* Remove active user by socket fd */
void remove_active_user_by_fd(int sockfd);

/* Find socket fd for username, returns fd or -1 if not found */
int find_active_user_sock(const char *username);

/* Returns newly allocated string of comma-separated usernames. Caller must free() */
char *get_user_list_string(void);

/* Send a single message string to all active users */
void broadcast_message_all(const char *msg);

/* Close sockets and free user list (shutdown) */
void shutdown_users(void);

#endif // USERS_H
