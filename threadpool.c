#include "threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

/* Forward declarations */
static void* client_thread_func(void *arg);
static void* worker_thread_func(void *arg);
static int handle_client_session(int socket, UserManager *user_mgr, 
                                   TaskQueue *task_queue);
static void execute_task(Task *task, UserManager *user_mgr);

/* ===== CLIENT THREAD POOL ===== */

ClientThreadPool* client_pool_create(int num_threads, ClientQueue *cq,
                                      TaskQueue *tq, UserManager *um) {
    ClientThreadPool *pool = malloc(sizeof(ClientThreadPool));
    if (!pool) return NULL;
    
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    if (!pool->threads) {
        free(pool);
        return NULL;
    }
    
    pool->num_threads = num_threads;
    pool->client_queue = cq;
    pool->task_queue = tq;
    pool->user_mgr = um;
    pool->shutdown = 0;
    pthread_mutex_init(&pool->shutdown_mutex, NULL);
    
    /* Create client handler threads */
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->threads[i], NULL, client_thread_func, pool);
    }
    
    return pool;
}

void client_pool_shutdown(ClientThreadPool *pool) {
    if (!pool) return;
    pthread_mutex_lock(&pool->shutdown_mutex);
    pool->shutdown = 1;
    pthread_mutex_unlock(&pool->shutdown_mutex);
    client_queue_shutdown(pool->client_queue);
}

void client_pool_destroy(ClientThreadPool *pool) {
    if (!pool) return;
    
    /* Wait for all threads to finish */
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    pthread_mutex_destroy(&pool->shutdown_mutex);
    free(pool->threads);
    free(pool);
}

/* Client thread: handles authentication and command dispatch */
static void* client_thread_func(void *arg) {
    ClientThreadPool *pool = (ClientThreadPool*)arg;
    
    int should_shutdown = 0;
    while (!should_shutdown) {
        ClientConnection conn;
        
        /* Pop client from queue (blocks until available) */
        if (client_queue_pop(pool->client_queue, &conn) == -1) {
            break; // Shutdown signal
        }
        
        printf("[ClientThread] Handling client on socket %d\n", conn.client_socket);
        
        /* Handle the client session (authentication + commands) */
        handle_client_session(conn.client_socket, pool->user_mgr, pool->task_queue);
        
        /* Close socket when done */
        close(conn.client_socket);
        printf("[ClientThread] Closed connection %d\n", conn.client_socket);
        
        /* Check shutdown flag safely */
        pthread_mutex_lock(&pool->shutdown_mutex);
        should_shutdown = pool->shutdown;
        pthread_mutex_unlock(&pool->shutdown_mutex);
    }
    
    return NULL;
}

/* Handle authentication and command loop for one client */
static int handle_client_session(int socket, UserManager *user_mgr, 
                                   TaskQueue *task_queue) {
    char buffer[1024];
    int user_id = -1;
    
    /* Send welcome message */
    const char *welcome = "Welcome! Commands: REGISTER <user> <pass>, LOGIN <user> <pass>\n";
    send(socket, welcome, strlen(welcome), 0);
    
    /* Authentication loop */
    while (user_id == -1) {
        memset(buffer, 0, sizeof(buffer));
        int n = recv(socket, buffer, sizeof(buffer) - 1, 0);
        
        printf("[ClientThread] Received %d bytes from socket %d\n", n, socket);
        
        if (n <= 0) {
            printf("[ClientThread] Client disconnected during auth (socket %d)\n", socket);
            return -1;
        }
        
        /* Remove newline */
        buffer[strcspn(buffer, "\r\n")] = 0;
        
        printf("[ClientThread] Processing command: '%s'\n", buffer);
        
        char cmd[16], username[MAX_USERNAME], password[MAX_PASSWORD];
        memset(cmd, 0, sizeof(cmd));
        memset(username, 0, sizeof(username));
        memset(password, 0, sizeof(password));
        
        int parsed = sscanf(buffer, "%15s %63s %63s", cmd, username, password);
        
        printf("[ClientThread] Parsed %d fields: cmd='%s' user='%s' pass='%s'\n", 
               parsed, cmd, username, password);
        
        if (parsed != 3) {
            const char *err = "ERROR: Invalid format. Use: REGISTER <username> <password>\n";
            send(socket, err, strlen(err), 0);
            continue;
        }
        
        if (strcmp(cmd, "REGISTER") == 0) {
            printf("[ClientThread] Attempting to register user '%s'\n", username);
            user_id = user_register(user_mgr, username, password);
            if (user_id == -1) {
                printf("[ClientThread] Registration failed - username exists\n");
                const char *err = "ERROR: Username already exists\n";
                send(socket, err, strlen(err), 0);
            } else {
                printf("[ClientThread] Registration successful, user_id=%d\n", user_id);
                const char *ok = "OK: Registered successfully. Please LOGIN.\n";
                send(socket, ok, strlen(ok), 0);
                user_id = -1; // Require login after registration
            }
        } else if (strcmp(cmd, "LOGIN") == 0) {
            printf("[ClientThread] Attempting to login user '%s'\n", username);
            user_id = user_login(user_mgr, username, password);
            if (user_id == -1) {
                printf("[ClientThread] Login failed - invalid credentials\n");
                const char *err = "ERROR: Invalid credentials\n";
                send(socket, err, strlen(err), 0);
            } else {
                printf("[ClientThread] Login successful, user_id=%d\n", user_id);
                const char *ok = "OK: Logged in. Commands: UPLOAD <file>, DOWNLOAD <file>, DELETE <file>, LIST, QUIT\n";
                send(socket, ok, strlen(ok), 0);
            }
        } else {
            printf("[ClientThread] Unknown command: '%s'\n", cmd);
            const char *err = "ERROR: Use REGISTER or LOGIN\n";
            send(socket, err, strlen(err), 0);
        }
    }
    
    /* Command loop */
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int n = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) break;
        
        buffer[strcspn(buffer, "\r\n")] = 0;
        
        if (strcmp(buffer, "QUIT") == 0) {
            const char *bye = "Goodbye!\n";
            send(socket, bye, strlen(bye), 0);
            break;
        }
        
        /* Parse command */
        char cmd[16], filename[256];
        memset(filename, 0, sizeof(filename));
        
        int fields = sscanf(buffer, "%s %s", cmd, filename);
        if (fields < 1) continue;
        
        /* Create task for worker */
        Task *task = malloc(sizeof(Task));
        memset(task, 0, sizeof(Task));
        task->client_id = socket; // Use socket as unique ID
        task->user_id = user_id;
        strncpy(task->command, cmd, sizeof(task->command) - 1);
        task->command[sizeof(task->command) - 1] = '\0';
        if (fields >= 2) {
            strncpy(task->filename, filename, sizeof(task->filename) - 1);
            task->filename[sizeof(task->filename) - 1] = '\0';
        }
        task->result_ready = 0;
        pthread_mutex_init(&task->result_mutex, NULL);
        pthread_cond_init(&task->result_cond, NULL);
        
        /* Submit to task queue */
        if (task_queue_push(task_queue, task) == -1) {
            const char *err = "ERROR: Server overloaded\n";
            send(socket, err, strlen(err), 0);
            pthread_mutex_destroy(&task->result_mutex);
            pthread_cond_destroy(&task->result_cond);
            free(task);
            continue;
        }
        
        /* Wait for worker to complete task */
        pthread_mutex_lock(&task->result_mutex);
        while (!task->result_ready) {
            pthread_cond_wait(&task->result_cond, &task->result_mutex);
        }
        pthread_mutex_unlock(&task->result_mutex);
        
        printf("[ClientThread] Task completed: %s (code=%d)\n", 
               task->command, task->result_code);
        printf("[ClientThread] Result message: %s\n", task->result_message);
        
        /* Send result to client */
        send(socket, task->result_message, strlen(task->result_message), 0);
        
        /* Handle UPLOAD: receive file data after READY response */
        if (strcmp(task->command, "UPLOAD") == 0 && task->result_code == 0) {
            /* Expect: SIZE <bytes> */
            memset(buffer, 0, sizeof(buffer));
            n = recv(socket, buffer, sizeof(buffer) - 1, 0);
            if (n > 0) {
                buffer[strcspn(buffer, "\r\n")] = 0;
                
                long file_size;
                if (sscanf(buffer, "SIZE %ld", &file_size) == 1) {
                    printf("[ClientThread] Attempting to upload %ld bytes for user %d\n", 
                           file_size, user_id);
                    
                    User *user = user_get_by_id(user_mgr, user_id);
                    if (!user) {
                        const char *err = "ERROR: Invalid user\n";
                        send(socket, err, strlen(err), 0);
                    } else {
                        /* Check quota BEFORE accepting upload */
                        pthread_mutex_lock(&user->user_mutex);
                        long current_quota = user->quota_used;
                        long available = USER_QUOTA_BYTES - current_quota;
                        pthread_mutex_unlock(&user->user_mutex);
                        
                        printf("[ClientThread] Current quota: %ld bytes, Available: %ld bytes, Requested: %ld bytes\n",
                               current_quota, available, file_size);
                        
                        if (file_size > available) {
                            char err[256];
                            snprintf(err, sizeof(err), 
                                    "ERROR: Quota exceeded. Available: %ld MB, Requested: %ld MB\n",
                                    available / (1024*1024), file_size / (1024*1024));
                            send(socket, err, strlen(err), 0);
                            printf("[ClientThread] Upload rejected - quota exceeded\n");
                        } else {
                            /* Send confirmation */
                            const char *ok = "OK: Send file data\n";
                            send(socket, ok, strlen(ok), 0);
                            
                            /* Receive file data */
                            char filepath[512];
                            snprintf(filepath, sizeof(filepath), "users/%s/%s",
                                    user->username, task->filename);
                            
                            FILE *fp = fopen(filepath, "wb");
                            if (fp) {
                                long received = 0;
                                char chunk[4096];
                                
                                printf("[ClientThread] Receiving file data...\n");
                                
                                while (received < file_size) {
                                    long to_recv = file_size - received;
                                    if (to_recv > (long)sizeof(chunk)) to_recv = sizeof(chunk);
                                    
                                    int bytes = recv(socket, chunk, to_recv, 0);
                                    if (bytes <= 0) break;
                                    
                                    fwrite(chunk, 1, bytes, fp);
                                    received += bytes;
                                }
                                
                                fclose(fp);
                                
                                if (received == file_size) {
                                    /* Add to quota */
                                    pthread_mutex_lock(&user->user_mutex);
                                    user->quota_used += file_size;
                                    long new_quota = user->quota_used;
                                    pthread_mutex_unlock(&user->user_mutex);
                                    
                                    printf("[ClientThread] Upload complete. New quota: %ld bytes (%.2f MB)\n", 
                                           new_quota, new_quota / (1024.0*1024.0));
                                    
                                    /* Save user data to persist quota - do this OUTSIDE the user mutex */
                                    user_manager_save(user_mgr);
                                    
                                    char success[256];
                                    snprintf(success, sizeof(success),
                                            "SUCCESS: File uploaded (%ld bytes). Quota: %.2f / %d MB\n",
                                            file_size, new_quota / (1024.0*1024.0), USER_QUOTA_MB);
                                    send(socket, success, strlen(success), 0);
                                } else {
                                    const char *err = "ERROR: Incomplete upload\n";
                                    send(socket, err, strlen(err), 0);
                                    remove(filepath);
                                    printf("[ClientThread] Upload failed - incomplete\n");
                                }
                            } else {
                                const char *err = "ERROR: Cannot create file\n";
                                send(socket, err, strlen(err), 0);
                                printf("[ClientThread] Upload failed - cannot create file\n");
                            }
                        }
                    }
                } else {
                    const char *err = "ERROR: Invalid SIZE format\n";
                    send(socket, err, strlen(err), 0);
                }
            }
        }
        
        /* Handle DOWNLOAD: send file data after SIZE response */
        if (strcmp(task->command, "DOWNLOAD") == 0 && task->result_code == 0) {
            char filepath[512];
            User *user = user_get_by_id(user_mgr, user_id);
            snprintf(filepath, sizeof(filepath), "users/%s/%s",
                    user->username, task->filename);
            
            FILE *fp = fopen(filepath, "rb");
            if (fp) {
                char chunk[4096];
                size_t bytes;
                
                while ((bytes = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
                    send(socket, chunk, bytes, 0);
                }
                
                fclose(fp);
            }
        }
        
        /* Cleanup task */
        pthread_mutex_destroy(&task->result_mutex);
        pthread_cond_destroy(&task->result_cond);
        free(task);
    }
    
    return 0;
}

/* ===== WORKER THREAD POOL ===== */

WorkerThreadPool* worker_pool_create(int num_threads, TaskQueue *tq,
                                      UserManager *um) {
    WorkerThreadPool *pool = malloc(sizeof(WorkerThreadPool));
    if (!pool) return NULL;
    
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    if (!pool->threads) {
        free(pool);
        return NULL;
    }
    
    pool->num_threads = num_threads;
    pool->task_queue = tq;
    pool->user_mgr = um;
    pool->shutdown = 0;
    pthread_mutex_init(&pool->shutdown_mutex, NULL);
    
    /* Create worker threads */
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread_func, pool);
    }
    
    return pool;
}

void worker_pool_shutdown(WorkerThreadPool *pool) {
    if (!pool) return;
    pthread_mutex_lock(&pool->shutdown_mutex);
    pool->shutdown = 1;
    pthread_mutex_unlock(&pool->shutdown_mutex);
    task_queue_shutdown(pool->task_queue);
}

void worker_pool_destroy(WorkerThreadPool *pool) {
    if (!pool) return;
    
    /* Wait for all threads to finish */
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    pthread_mutex_destroy(&pool->shutdown_mutex);
    free(pool->threads);
    free(pool);
}

/* Worker thread: executes file operations */
static void* worker_thread_func(void *arg) {
    WorkerThreadPool *pool = (WorkerThreadPool*)arg;
    
    int should_shutdown = 0;
    while (!should_shutdown) {
        /* Pop task from queue (blocks until available) */
        Task *task = task_queue_pop(pool->task_queue);
        if (!task) break; // Shutdown signal
        
        printf("[WorkerThread] Processing %s for user %d\n", 
               task->command, task->user_id);
        
        /* Execute the task */
        execute_task(task, pool->user_mgr);
        
        /* Signal client thread that result is ready */
        pthread_mutex_lock(&task->result_mutex);
        task->result_ready = 1;
        pthread_cond_signal(&task->result_cond);
        pthread_mutex_unlock(&task->result_mutex);
        
        /* Check shutdown flag safely */
        pthread_mutex_lock(&pool->shutdown_mutex);
        should_shutdown = pool->shutdown;
        pthread_mutex_unlock(&pool->shutdown_mutex);
    }
    
    return NULL;
}


/* Execute file operation (UPLOAD, DOWNLOAD, DELETE, LIST) */
static void execute_task(Task *task, UserManager *user_mgr) {
    User *user = user_get_by_id(user_mgr, task->user_id);
    if (!user) {
        snprintf(task->result_message, sizeof(task->result_message),
                 "ERROR: Invalid user\n");
        task->result_code = -1;
        return;
    }
    
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "users/%s/%s", 
             user->username, task->filename);
    
    if (strcmp(task->command, "UPLOAD") == 0) {
        /* Lock for quota check */
        pthread_mutex_lock(&user->user_mutex);
        
        /* Check if filename is provided */
        if (strlen(task->filename) == 0) {
            pthread_mutex_unlock(&user->user_mutex);
            snprintf(task->result_message, sizeof(task->result_message),
                     "ERROR: No filename specified\n");
            task->result_code = -1;
        } else {
            pthread_mutex_unlock(&user->user_mutex);
            
            /* Check if file already exists */
            struct stat st;
            if (stat(filepath, &st) == 0) {
                snprintf(task->result_message, sizeof(task->result_message),
                         "ERROR: File already exists. Delete it first.\n");
                task->result_code = -1;
            } else {
                snprintf(task->result_message, sizeof(task->result_message),
                         "READY: Send file size as: SIZE <bytes>\\n\n");
                task->result_code = 0;
            }
        }
        } else if (strcmp(task->command, "DOWNLOAD") == 0) {
        FILE *fp = fopen(filepath, "rb");
        if (!fp) {
            snprintf(task->result_message, sizeof(task->result_message),
                     "ERROR: File not found\n");
            task->result_code = -1;
        } else {
            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            fclose(fp);
            snprintf(task->result_message, sizeof(task->result_message),
                     "SIZE: %ld\n", size);
            task->result_code = 0;
        }
    }
     else if (strcmp(task->command, "DELETE") == 0) {
        struct stat st;
        if (stat(filepath, &st) == 0) {
            long file_size = st.st_size;
            if (remove(filepath) == 0) {
                /* Update quota */
                pthread_mutex_lock(&user->user_mutex);
                user->quota_used -= file_size;
                if (user->quota_used < 0) user->quota_used = 0;
                long new_quota = user->quota_used;
                pthread_mutex_unlock(&user->user_mutex);
                
                printf("[WorkerThread] File deleted. New quota: %ld bytes (%.2f MB)\n",
                       new_quota, new_quota / (1024.0*1024.0));
                
                /* Save user data to persist quota */
                user_manager_save(user_mgr);
                
                snprintf(task->result_message, sizeof(task->result_message),
                         "OK: File deleted (%ld bytes freed). Quota: %.2f / %d MB\n", 
                         file_size, new_quota / (1024.0*1024.0), USER_QUOTA_MB);
                task->result_code = 0;
            } else {
                snprintf(task->result_message, sizeof(task->result_message),
                         "ERROR: Could not delete file\n");
                task->result_code = -1;
            }
        } else {
            snprintf(task->result_message, sizeof(task->result_message),
                     "ERROR: File not found\n");
            task->result_code = -1;
        }
        
    } else if (strcmp(task->command, "LIST") == 0) {
        printf("[WorkerThread] Processing LIST for user %d (%s)\n", 
               task->user_id, user->username);
        
        char user_dir[256];
        snprintf(user_dir, sizeof(user_dir), "users/%s", user->username);
        
        char result[4096];
        memset(result, 0, sizeof(result));
        
        snprintf(result, sizeof(result), "Files for %s:\n", user->username);
        snprintf(result + strlen(result), sizeof(result) - strlen(result),
                 "%-40s %15s\n", "Filename", "Size");
        snprintf(result + strlen(result), sizeof(result) - strlen(result),
                 "------------------------------------------------------------\n");
        
        printf("[WorkerThread] Opening directory: %s\n", user_dir);
        
        /* Read directory */
        DIR *dir = opendir(user_dir);
        if (dir) {
            struct dirent *entry;
            int file_count = 0;
            long total_size = 0;
            
            printf("[WorkerThread] Directory opened successfully\n");
            
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue; // Skip hidden files
                
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", user_dir, entry->d_name);
                
                struct stat st;
                if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
                    /* Format file size nicely */
                    char size_str[32];
                    if (st.st_size < 1024) {
                        snprintf(size_str, sizeof(size_str), "%ld B", st.st_size);
                    } else if (st.st_size < 1024*1024) {
                        snprintf(size_str, sizeof(size_str), "%.2f KB", st.st_size / 1024.0);
                    } else {
                        snprintf(size_str, sizeof(size_str), "%.2f MB", st.st_size / (1024.0*1024.0));
                    }
                    
                    snprintf(result + strlen(result), sizeof(result) - strlen(result),
                             "%-40s %15s\n", entry->d_name, size_str);
                    file_count++;
                    total_size += st.st_size;
                    
                    printf("[WorkerThread] Found file: %s (%ld bytes)\n", 
                           entry->d_name, st.st_size);
                }
            }
            closedir(dir);
            
            printf("[WorkerThread] Found %d files\n", file_count);
            
            if (file_count == 0) {
                snprintf(result + strlen(result), sizeof(result) - strlen(result),
                         "(no files)\n");
            }
            
            snprintf(result + strlen(result), sizeof(result) - strlen(result),
                     "------------------------------------------------------------\n");
            snprintf(result + strlen(result), sizeof(result) - strlen(result),
                     "Total files: %d\n", file_count);
        } else {
            printf("[WorkerThread] Failed to open directory\n");
            snprintf(result + strlen(result), sizeof(result) - strlen(result),
                     "(directory error)\n");
        }
        
        printf("[WorkerThread] Getting quota information\n");
        
        /* Get current quota from user structure - just read, no lock needed for display */
        long quota_used = user->quota_used;
        
        printf("[WorkerThread] Quota used: %ld bytes\n", quota_used);
        
        snprintf(result + strlen(result), sizeof(result) - strlen(result),
                 "Quota used: %.2f / %d MB (%.1f%%)\n",
                 quota_used / (1024.0*1024.0), USER_QUOTA_MB,
                 (quota_used * 100.0) / USER_QUOTA_BYTES);
        snprintf(result + strlen(result), sizeof(result) - strlen(result),
                 "Available: %.2f MB\n",
                 (USER_QUOTA_BYTES - quota_used) / (1024.0*1024.0));
        
        printf("[WorkerThread] Preparing result message (length: %zu)\n", strlen(result));
        
        snprintf(task->result_message, sizeof(task->result_message), "%s", result);
        task->result_code = 0;
        
        printf("[WorkerThread] LIST command completed\n");
        
    } else {
        snprintf(task->result_message, sizeof(task->result_message),
                 "ERROR: Unknown command\n");
        task->result_code = -1;
    }
}
