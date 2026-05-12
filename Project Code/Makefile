CC = gcc
CFLAGS = -Wall -Wextra -pthread -Iserver -Idb
LDFLAGS = -lsqlite3  # Linker flag for SQLite

SRV_DIR = server
CLI_DIR = client
DB_DIR  = db

SERVER_SRCS = $(SRV_DIR)/server.c $(SRV_DIR)/handlers.c $(SRV_DIR)/users.c $(DB_DIR)/db.c
CLIENT_SRCS = $(CLI_DIR)/client.c

SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

all: server_app client_app

server_app: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o server_app $(SERVER_OBJS) $(LDFLAGS)

client_app: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o client_app $(CLIENT_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS)
	rm -f server_app client_app
