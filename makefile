CC = gcc
CFLAGS = -Wall -Wextra -pthread -O2
LDFLAGS = -pthread

# Source files (in current directory)
SERVER_SRC = server.c queue.c threadpool.c utils.c
CLIENT_SRC = client.c

# Object files
SERVER_OBJ = server.o queue.o threadpool.o utils.o
CLIENT_OBJ = client.o

# Executables
SERVER_BIN = server
CLIENT_BIN = client

.PHONY: all clean test valgrind tsan

all: $(SERVER_BIN) $(CLIENT_BIN)

# Server executable
$(SERVER_BIN): $(SERVER_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Server built successfully!"

# Client executable
$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Client built successfully!"

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies
server.o: server.c queue.h threadpool.h utils.h
queue.o: queue.c queue.h
threadpool.o: threadpool.c threadpool.h queue.h utils.h
utils.o: utils.c utils.h
client.o: client.c

# Clean build artifacts
clean:
	rm -f $(SERVER_OBJ) $(CLIENT_OBJ) $(SERVER_BIN) $(CLIENT_BIN)
	rm -rf users users.txt
	@echo "Cleaned build artifacts"

# Run server
run: $(SERVER_BIN)
	./$(SERVER_BIN)

# Test with Valgrind (memory leak detection)
valgrind: $(SERVER_BIN)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(SERVER_BIN)

# Test with ThreadSanitizer (data race detection)
tsan:
	$(CC) $(CFLAGS) -fsanitize=thread -o $(SERVER_BIN)_tsan $(SERVER_SRC)
	./$(SERVER_BIN)_tsan