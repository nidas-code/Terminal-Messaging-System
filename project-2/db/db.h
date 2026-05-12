#ifndef DB_H
#define DB_H

/* Store a message
   delivered_flag = 1 if delivered immediately, 0 if stored offline
   Returns 0 on success, -1 on failure
*/
int store_message_db(const char *sender,
                     const char *receiver,
                     const char *message,
                     int delivered_flag);

/* Fetch undelivered messages */
typedef void (*undelivered_cb_t)(const char *sender,
                                 const char *receiver,
                                 const char *message,
                                 void *userdata);

int get_undelivered_messages_db(const char *username,
                                undelivered_cb_t cb,
                                void *userdata);

/* Mark undelivered as delivered */
int mark_messages_delivered_db(const char *username);

/* Message history callback */
typedef void (*history_cb_t)(const char *sender,
                             const char *receiver,
                             const char *message,
                             const char *timestamp,
                             void *userdata);

/* Retrieve conversation between A and B */
int get_message_history_db(const char *user1,
                           const char *user2,
                           history_cb_t cb,
                           void *userdata);

/* Delete chat history */
int delete_message_history_db(const char *user1,
                              const char *user2);

/* Initialize / close database */
int init_db(const char *filename);
void close_db(void);

#endif
