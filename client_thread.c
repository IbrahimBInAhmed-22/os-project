#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

// Forward declarations for functions defined later in this file
bool handle_authentication(int socket, char *username_out);
bool process_command(int socket, const char *username, const char *command_str);

// ============================================================================
// CLIENT THREAD IMPLEMENTATION (Handles Client Communication)
// ============================================================================

/**
 * Client thread main function
 * 1. Dequeues a socket from client queue
 * 2. Handles authentication (signup/login)
 * 3. Receives commands from client
 * 4. Creates tasks and enqueues them to task queue
 * 5. Waits for task completion (busy wait in Phase 1, will optimize in Phase 2)
 */
void* client_thread_function(void *arg) {
    (void)arg;  // Mark as unused to suppress warning
    printf("[Client Thread %lu] Started\n", pthread_self());
    
    while (server_running) {
        // Dequeue a client socket (blocks if queue is empty)
        int client_socket = dequeue_client(&client_queue);
        
        if (client_socket == -1) {
            // Server is shutting down
            break;
        }
        
        printf("[Client Thread %lu] Got client socket %d\n", pthread_self(), client_socket);
        
        // Handle authentication
        char username[MAX_USERNAME];
        if (!handle_authentication(client_socket, username)) {
            close(client_socket);
            continue;
        }
        
        printf("[Client Thread %lu] User '%s' authenticated\n", pthread_self(), username);
        
        // Command loop - receive and process commands from this client
        char buffer[BUFFER_SIZE];
        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            
            if (bytes_read <= 0) {
                // Client disconnected
                printf("[Client Thread %lu] Client disconnected\n", pthread_self());
                break;
            }
            
            buffer[bytes_read] = '\0';
            
            // Remove trailing newline
            char *newline = strchr(buffer, '\n');
            if (newline) *newline = '\0';
            
            // Parse command
            if (strncmp(buffer, "QUIT", 4) == 0) {
                send_response(client_socket, "OK: Goodbye\n");
                break;
            }
            
            // Create and process task
            if (!process_command(client_socket, username, buffer)) {
                break;  // Error occurred
            }
        }
        
        close(client_socket);
        printf("[Client Thread %lu] Closed connection\n", pthread_self());
    }
    
    printf("[Client Thread %lu] Shutting down\n", pthread_self());
    return NULL;
}

/**
 * Handle authentication (signup or login)
 * Returns true if successful, false otherwise
 */
bool handle_authentication(int socket, char *username_out) {
    char buffer[BUFFER_SIZE];
    
    // Send authentication prompt
    send_response(socket, "AUTH: Enter SIGNUP username password or LOGIN username password\n");
    
    // Receive authentication command
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_read = recv(socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_read <= 0) {
        return false;
    }
    
    buffer[bytes_read] = '\0';
    
    // Parse authentication command
    char command[20], username[MAX_USERNAME], password[MAX_PASSWORD];
    int parsed = sscanf(buffer, "%19s %49s %49s", command, username, password);
    
    if (parsed != 3) {
        send_response(socket, "ERROR: Invalid authentication format\n");
        return false;
    }
    
    bool success = false;
    
    if (strcmp(command, "SIGNUP") == 0) {
        success = signup_user(&user_db, username, password);
        if (success) {
            send_response(socket, "OK: Signup successful\n");
            strcpy(username_out, username);
        } else {
            send_response(socket, "ERROR: Username already exists\n");
        }
    } else if (strcmp(command, "LOGIN") == 0) {
        success = login_user(&user_db, username, password);
        if (success) {
            send_response(socket, "OK: Login successful\n");
            strcpy(username_out, username);
        } else {
            send_response(socket, "ERROR: Invalid credentials\n");
        }
    } else {
        send_response(socket, "ERROR: Unknown authentication command\n");
    }
    
    return success;
}

/**
 * Process a command from the client
 * Creates a task and enqueues it to the task queue
 * Waits for task completion (busy wait for Phase 1)
 */
bool process_command(int socket, const char *username, const char *command_str) {
    char cmd[20], filename[MAX_FILENAME];
    
    // Parse command
    int parsed = sscanf(command_str, "%19s %255s", cmd, filename);
    
    if (parsed < 1) {
        send_response(socket, "ERROR: Invalid command\n");
        return true;  // Don't disconnect
    }
    
    // Create task structure
    Task *task = malloc(sizeof(Task));
    if (!task) {
        send_response(socket, "ERROR: Memory allocation failed\n");
        return false;
    }
    
    // Initialize task
    task->client_socket = socket;
    strncpy(task->username, username, MAX_USERNAME - 1);
    task->username[MAX_USERNAME - 1] = '\0';
    task->file_data = NULL;
    task->file_size = 0;
    task->completed = false;
    
    // Allocate mutex and condition variable for this task
    task->task_mutex = malloc(sizeof(pthread_mutex_t));
    task->task_cond = malloc(sizeof(pthread_cond_t));
    pthread_mutex_init(task->task_mutex, NULL);
    pthread_cond_init(task->task_cond, NULL);
    
    // Determine command type and handle accordingly
    if (strcmp(cmd, "UPLOAD") == 0) {
        if (parsed < 2) {
            send_response(socket, "ERROR: UPLOAD requires filename\n");
            free(task->task_mutex);
            free(task->task_cond);
            free(task);
            return true;
        }
        
        task->type = CMD_UPLOAD;
        strncpy(task->filename, filename, MAX_FILENAME - 1);
        task->filename[MAX_FILENAME - 1] = '\0';
        
        // Receive file size
        char size_buffer[64];
        recv(socket, size_buffer, sizeof(size_buffer) - 1, 0);
        task->file_size = atol(size_buffer);
        
        // Receive file data
        task->file_data = malloc(task->file_size);
        if (!task->file_data) {
            send_response(socket, "ERROR: Memory allocation failed\n");
            pthread_mutex_destroy(task->task_mutex);
            pthread_cond_destroy(task->task_cond);
            free(task->task_mutex);
            free(task->task_cond);
            free(task);
            return false;
        }
        
        size_t total_received = 0;
        while (total_received < task->file_size) {
            ssize_t bytes = recv(socket, task->file_data + total_received, 
                                task->file_size - total_received, 0);
            if (bytes <= 0) break;
            total_received += bytes;
        }
        
    } else if (strcmp(cmd, "DOWNLOAD") == 0) {
        if (parsed < 2) {
            send_response(socket, "ERROR: DOWNLOAD requires filename\n");
            free(task->task_mutex);
            free(task->task_cond);
            free(task);
            return true;
        }
        
        task->type = CMD_DOWNLOAD;
        strncpy(task->filename, filename, MAX_FILENAME - 1);
        task->filename[MAX_FILENAME - 1] = '\0';
        
    } else if (strcmp(cmd, "DELETE") == 0) {
        if (parsed < 2) {
            send_response(socket, "ERROR: DELETE requires filename\n");
            free(task->task_mutex);
            free(task->task_cond);
            free(task);
            return true;
        }
        
        task->type = CMD_DELETE;
        strncpy(task->filename, filename, MAX_FILENAME - 1);
        task->filename[MAX_FILENAME - 1] = '\0';
        
    } else if (strcmp(cmd, "LIST") == 0) {
        task->type = CMD_LIST;
        task->filename[0] = '\0';
        
    } else {
        send_response(socket, "ERROR: Unknown command\n");
        pthread_mutex_destroy(task->task_mutex);
        pthread_cond_destroy(task->task_cond);
        free(task->task_mutex);
        free(task->task_cond);
        free(task);
        return true;
    }
    
    // Enqueue task to task queue
    enqueue_task(&task_queue, task);
    
    // Wait for task completion (BUSY WAIT for Phase 1 - will optimize in Phase 2)
    pthread_mutex_lock(task->task_mutex);
    while (!task->completed) {
        pthread_cond_wait(task->task_cond, task->task_mutex);
    }
    pthread_mutex_unlock(task->task_mutex);
    
    // Cleanup task
    if (task->file_data) {
        free(task->file_data);
    }
    pthread_mutex_destroy(task->task_mutex);
    pthread_cond_destroy(task->task_cond);
    free(task->task_mutex);
    free(task->task_cond);
    free(task);
    
    return true;
}