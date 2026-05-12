#ifndef HANDLERS_H
#define HANDLERS_H

/* Maximum sizes */
#define HANDLER_BUF_SIZE 8192
#define HANDLER_MAX_MSG 4096

/* return codes:
 *  1 - delivered immediately
 *  0 - stored (receiver offline)
 * -1 - error
 * -2 - message too long
 */
int send_message_handler(const char *sender, const char *receiver, const char *message);

/* Send all undelivered messages to user on login */
void push_undelivered_for_user(const char *username, int sockfd);

#endif // HANDLERS_H
