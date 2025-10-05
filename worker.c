#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

// ============================================================================
// WORKER THREAD IMPLEMENTATION (Executes File Operations)
// ============================================================================

/**
 * Worker thread main function
 * Continuously dequeues tasks and processes them
 */
void* worker_thread_function(void *arg) {
    (void)arg;  // Mark as unused to suppress warning
    printf("[Worker %lu] Started\n", pthread_self());
    
    while (server_running) {
        // Dequeue a task (blocks if queue is empty)
        Task *task = dequeue_task(&task_queue);
        
        if (task == NULL) {
            // Server is shutting down
            break;
        }
        
        printf("[Worker %lu] Processing task type %d for user %s\n", 
               pthread_self(), task->type, task->username);
        
        // Execute the appropriate handler based on task type
        switch (task->type) {
            case CMD_UPLOAD:
                handle_upload(task);
                break;
            case CMD_DOWNLOAD:
                handle_download(task);
                break;
            case CMD_DELETE:
                handle_delete(task);
                break;
            case CMD_LIST:
                handle_list(task);
                break;
            default:
                send_response(task->client_socket, "ERROR: Unknown command\n");
        }
        
        // Signal the client thread that task is complete
        pthread_mutex_lock(task->task_mutex);
        task->completed = true;
        pthread_cond_signal(task->task_cond);
        pthread_mutex_unlock(task->task_mutex);
    }
    
    printf("[Worker %lu] Shutting down\n", pthread_self());
    return NULL;
}

// ============================================================================
// FILE OPERATION HANDLERS
// ============================================================================

/**
 * Handle UPLOAD command
 * Receives file data from client and saves to user's directory
 */
void handle_upload(Task *task) {
    // Check quota before uploading
    long current_quota = get_user_quota(&user_db, task->username);
    if (current_quota + task->file_size > USER_QUOTA) {
        send_response(task->client_socket, "ERROR: Quota exceeded\n");
        return;
    }
    
    // Get file path
    char *filepath = get_user_file_path(task->username, task->filename);
    if (!filepath) {
        send_response(task->client_socket, "ERROR: Invalid filename\n");
        return;
    }
    
    // Check if file already exists and get its size
    struct stat st;
    long old_size = 0;
    if (stat(filepath, &st) == 0) {
        old_size = st.st_size;
    }
    
    // Write file to disk
    FILE *file = fopen(filepath, "wb");
    if (!file) {
        send_response(task->client_socket, "ERROR: Failed to create file\n");
        free(filepath);
        return;
    }
    
    fwrite(task->file_data, 1, task->file_size, file);
    fclose(file);
    
    // Update quota (subtract old size if file existed, add new size)
    long quota_delta = task->file_size - old_size;
    update_user_quota(&user_db, task->username, quota_delta);
    
    printf("[Worker] Uploaded %s for user %s (%zu bytes)\n", 
           task->filename, task->username, task->file_size);
    
    send_response(task->client_socket, "OK: File uploaded successfully\n");
    free(filepath);
}

/**
 * Handle DOWNLOAD command
 * Reads file from disk and sends to client
 */
void handle_download(Task *task) {
    char *filepath = get_user_file_path(task->username, task->filename);
    if (!filepath) {
        send_response(task->client_socket, "ERROR: Invalid filename\n");
        return;
    }
    
    // Check if file exists
    struct stat st;
    if (stat(filepath, &st) != 0) {
        send_response(task->client_socket, "ERROR: File not found\n");
        free(filepath);
        return;
    }
    
    // Send file size first
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "OK: %ld\n", st.st_size);
    send_response(task->client_socket, response);
    
    // Send file data
    send_file(task->client_socket, filepath);
    
    printf("[Worker] Downloaded %s for user %s (%ld bytes)\n", 
           task->filename, task->username, st.st_size);
    
    free(filepath);
}

/**
 * Handle DELETE command
 * Removes file from user's directory and updates quota
 */
void handle_delete(Task *task) {
    char *filepath = get_user_file_path(task->username, task->filename);
    if (!filepath) {
        send_response(task->client_socket, "ERROR: Invalid filename\n");
        return;
    }
    
    // Get file size before deleting
    struct stat st;
    if (stat(filepath, &st) != 0) {
        send_response(task->client_socket, "ERROR: File not found\n");
        free(filepath);
        return;
    }
    
    long file_size = st.st_size;
    
    // Delete the file
    if (unlink(filepath) != 0) {
        send_response(task->client_socket, "ERROR: Failed to delete file\n");
        free(filepath);
        return;
    }
    
    // Update quota (subtract file size)
    update_user_quota(&user_db, task->username, -file_size);
    
    printf("[Worker] Deleted %s for user %s (%ld bytes freed)\n", 
           task->filename, task->username, file_size);
    
    send_response(task->client_socket, "OK: File deleted successfully\n");
    free(filepath);
}

/**
 * Handle LIST command
 * Lists all files in user's directory
 */
void handle_list(Task *task) {
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "./storage/%s", task->username);
    
    DIR *dir = opendir(dir_path);
    if (!dir) {
        send_response(task->client_socket, "OK: No files\n");
        return;
    }
    
    // Build list of files
    char file_list[BUFFER_SIZE] = "OK:\n";
    struct dirent *entry;
    int file_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Get file size
        char filepath[768];
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, entry->d_name);
        struct stat st;
        if (stat(filepath, &st) == 0) {
            char line[384];
            snprintf(line, sizeof(line), "%s (%ld bytes)\n", entry->d_name, st.st_size);
            strncat(file_list, line, BUFFER_SIZE - strlen(file_list) - 1);
            file_count++;
        }
    }
    
    closedir(dir);
    
    if (file_count == 0) {
        send_response(task->client_socket, "OK: No files\n");
    } else {
        send_response(task->client_socket, file_list);
    }
    
    printf("[Worker] Listed %d files for user %s\n", file_count, task->username);
}