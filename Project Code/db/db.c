#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sqlite3.h>

static sqlite3 *g_db = NULL;
static pthread_mutex_t g_db_mutex = PTHREAD_MUTEX_INITIALIZER;

int init_db(const char *filename) {
    pthread_mutex_lock(&g_db_mutex);
    if (g_db) { pthread_mutex_unlock(&g_db_mutex); return 0; }
    int rc = sqlite3_open(filename, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite3_open: %s\n", sqlite3_errmsg(g_db));
        sqlite3_close(g_db);
        g_db = NULL;
        pthread_mutex_unlock(&g_db_mutex);
        return -1;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "sender TEXT NOT NULL,"
        "receiver TEXT NOT NULL,"
        "message TEXT NOT NULL,"
        "delivered INTEGER DEFAULT 0,"
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";
    char *errmsg = NULL;
    rc = sqlite3_exec(g_db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite exec create: %s\n", errmsg);
        sqlite3_free(errmsg);
        pthread_mutex_unlock(&g_db_mutex);
        return -1;
    }
    pthread_mutex_unlock(&g_db_mutex);
    return 0;
}

void close_db(void) {
    pthread_mutex_lock(&g_db_mutex);
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    pthread_mutex_unlock(&g_db_mutex);
}

int store_message_db(const char *sender, const char *receiver, const char *message, int delivered_flag) {
    if (!g_db || !sender || !receiver || !message) return -1;
    pthread_mutex_lock(&g_db_mutex);
    const char *sql = "INSERT INTO messages (sender, receiver, message, delivered) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_mutex);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, sender, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, receiver, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, message, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, delivered_flag ? 1 : 0);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int mark_messages_delivered_db(const char *receiver) {
    if (!g_db || !receiver) return -1;
    pthread_mutex_lock(&g_db_mutex);
    const char *sql = "UPDATE messages SET delivered = 1 WHERE receiver = ? AND delivered = 0;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_mutex); return -1; }
    sqlite3_bind_text(stmt, 1, receiver, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int get_undelivered_messages_db(const char *receiver, undelivered_cb_t cb, void *userdata) {
    if (!g_db || !receiver || !cb) return -1;
    pthread_mutex_lock(&g_db_mutex);
    const char *sql = "SELECT sender, receiver, message FROM messages WHERE receiver = ? AND delivered = 0 ORDER BY id ASC;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_mutex); return -1; }
    sqlite3_bind_text(stmt, 1, receiver, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char *sender = sqlite3_column_text(stmt, 0);
        const unsigned char *recv = sqlite3_column_text(stmt, 1);
        const unsigned char *msg = sqlite3_column_text(stmt, 2);
        cb((const char*)sender, (const char*)recv, (const char*)msg, userdata);
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    return 0;
}

int get_message_history_db(const char *user1, const char *user2, history_cb_t cb, void *userdata) {
    if (!g_db || !user1 || !user2 || !cb) return -1;
    pthread_mutex_lock(&g_db_mutex);
    const char *sql =
        "SELECT sender, receiver, message, timestamp FROM messages "
        "WHERE (sender = ? AND receiver = ?) OR (sender = ? AND receiver = ?) "
        "ORDER BY id ASC;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_mutex); return -1; }
    sqlite3_bind_text(stmt, 1, user1, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user2, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user2, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, user1, -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char *sender = sqlite3_column_text(stmt, 0);
        const unsigned char *recv = sqlite3_column_text(stmt, 1);
        const unsigned char *msg = sqlite3_column_text(stmt, 2);
        const unsigned char *ts = sqlite3_column_text(stmt, 3);
        cb((const char*)sender, (const char*)recv, (const char*)msg, (const char*)ts, userdata);
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    return 0;
}

int delete_message_history_db(const char *user1, const char *user2) {
    if (!g_db || !user1 || !user2) return -1;
    pthread_mutex_lock(&g_db_mutex);
    const char *sql = "DELETE FROM messages WHERE (sender = ? AND receiver = ?) OR (sender = ? AND receiver = ?);";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&g_db_mutex); return -1; }
    sqlite3_bind_text(stmt, 1, user1, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user2, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user2, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, user1, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_mutex);
    return (rc == SQLITE_DONE) ? 0 : -1;
}
