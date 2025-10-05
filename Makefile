# Makefile for Dropbox Clone Server (Phase 1)

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
LDFLAGS = -pthread

# Target executables
SERVER = server
CLIENT = client

# Server source files
# Note: utilities are implemented in 'utils.c'
SERVER_SOURCES = server.c queue.c user.c worker.c client_thread.c utils.c
SERVER_OBJECTS = $(SERVER_SOURCES:.c=.o)

# Client source files
CLIENT_SOURCES = client.c
CLIENT_OBJECTS = $(CLIENT_SOURCES:.c=.o)

# Default target - build everything
all: $(SERVER) $(CLIENT)

# Build server
$(SERVER): $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Server built successfully!"

# Build client
$(CLIENT): $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Client built successfully!"

# Compile .c files to .o files
%.o: %.c server.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(SERVER) $(CLIENT) *.o
	rm -rf ./storage
	@echo "Cleaned build artifacts"

# Run Valgrind memory leak check on server
valgrind: $(SERVER)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./$(SERVER)

# Run ThreadSanitizer race condition check
# Note: Requires recompilation with -fsanitize=thread
tsan: clean
	$(CC) $(CFLAGS) -fsanitize=thread -o $(SERVER) $(SERVER_SOURCES) $(LDFLAGS)
	$(CC) $(CFLAGS) -o $(CLIENT) $(CLIENT_SOURCES) $(LDFLAGS)
	@echo "Built with ThreadSanitizer"

# Help target
help:
	@echo "Available targets:"
	@echo "  make          - Build server and client"
	@echo "  make clean    - Remove build artifacts and storage"
	@echo "  make valgrind - Run server with Valgrind"
	@echo "  make tsan     - Build with ThreadSanitizer"
	@echo "  make help     - Show this help message"

.PHONY: all clean valgrind tsan help