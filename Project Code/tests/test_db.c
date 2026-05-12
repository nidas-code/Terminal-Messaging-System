#include <stdio.h>
#include <assert.h>
#include "db.h"

void hist_cb(const char *s, const char *r, const char *m, const char *t, void *ud) {
    (void)ud;
    printf("HIST: %s -> %s : %s @ %s\n", s, r, m, t);
}

int main(void) {
    if (init_db("test_messaging.db") != 0) { fprintf(stderr, "DB init fail\n"); return 1; }
    assert(store_message_db("alice", "bob", "hello bob", 0) == 0);
    assert(store_message_db("bob", "alice", "hi alice", 0) == 0);

    get_message_history_db("alice", "bob", hist_cb, NULL);
    assert(delete_message_history_db("alice", "bob") == 0);

    close_db();
    printf("DB tests done\n");
    return 0;
}
