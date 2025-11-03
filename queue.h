#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <netinet/in.h>

/* Client connection structure pushed to client queue */
typedef struct {
    int client_socket;
    struct sockaddr_in addr;
} ClientConnection;

/* Task structure for worker threads */
typedef struct {
    int client_id;              // Unique client thread ID
    int user_id;                // Authenticated user ID
    char command[16];           // UPLOAD, DOWNLOAD, DELETE, LIST
    char filename[256];         // Target filename
    int result_ready;           // Flag: 0=pending, 1=done
    int result_code;            // 0=success, -1=error
    char result_message[512];   // Error/success message
    pthread_mutex_t result_mutex;
    pthread_cond_t result_cond;
} Task;

/* Thread-safe client queue (circular buffer) */
typedef struct {
    ClientConnection *connections;
    int front, rear, count, capacity;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int shutdown;               // Signal for shutdown
} ClientQueue;

/* Thread-safe task queue (circular buffer) */
typedef struct {
    Task **tasks;               // Array of pointers to tasks
    int front, rear, count, capacity;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int shutdown;
} TaskQueue;

/* Client queue operations */
ClientQueue* client_queue_create(int capacity);
void client_queue_destroy(ClientQueue *queue);
int client_queue_push(ClientQueue *queue, ClientConnection conn);
int client_queue_pop(ClientQueue *queue, ClientConnection *conn);
void client_queue_shutdown(ClientQueue *queue);

/* Task queue operations */
TaskQueue* task_queue_create(int capacity);
void task_queue_destroy(TaskQueue *queue);
int task_queue_push(TaskQueue *queue, Task *task);
Task* task_queue_pop(TaskQueue *queue);
void task_queue_shutdown(TaskQueue *queue);

#endif