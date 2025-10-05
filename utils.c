#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Create a directory for a user to store their files
 */
void create_user_directory(const char *username) {
    char dir_path[512];
    
    // Create base storage directory if it doesn't exist
    mkdir("./storage", 0755);
    
    // Create user-specific directory
    snprintf(dir_path, sizeof(dir_path), "./storage/%s", username);
    mkdir(dir_path, 0755);
    
    printf("[Utils] Created directory for user: %s\n", username);
}

/**
 * Get the full file path for a user's file
 * Returns allocated string that must be freed by caller
 * Returns NULL on error
 */
char* get_user_file_path(const char *username, const char *filename) {
    // Basic validation - no path traversal
    if (strstr(filename, "..") || strchr(filename, '/')) {
        fprintf(stderr, "[Utils] Invalid filename detected: %s\n", filename);
        return NULL;
    }
    
    char *filepath = malloc(768);
    if (!filepath) {
        return NULL;
    }
    
    snprintf(filepath, 768, "./storage/%s/%s", username, filename);
    return filepath;
}

/**
 * Send a text response to a client socket
 * Thread-safe for sending responses
 */
void send_response(int socket, const char *message) {
    size_t len = strlen(message);
    size_t sent = 0;
    
    while (sent < len) {
        ssize_t bytes = send(socket, message + sent, len - sent, 0);
        if (bytes <= 0) {
            perror("[Utils] Failed to send response");
            break;
        }
        sent += bytes;
    }
}

/**
 * Send a file to a client socket
 * Reads file and sends its contents
 */
void send_file(int socket, const char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("[Utils] Failed to open file for sending");
        return;
    }
    
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        size_t sent = 0;
        while (sent < bytes_read) {
            ssize_t bytes = send(socket, buffer + sent, bytes_read - sent, 0);
            if (bytes <= 0) {
                perror("[Utils] Failed to send file data");
                fclose(file);
                return;
            }
            sent += bytes;
        }
    }
    
    fclose(file);
}