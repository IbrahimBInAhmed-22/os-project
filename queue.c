#include "server.h"
#include <stdlib.h>
#include <stdio.h>

// ============================================================================
// CLIENT QUEUE IMPLEMENTATION (Producer-Consumer Queue for Socket Descriptors)
// ============================================================================

/**
 * Initialize the client queue with mutex and condition variable
 * This queue holds incoming client socket descriptors
 */
void init_client_queue(ClientQueue *queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

/**
 * Enqueue a client socket (Producer operation)
 * Called by main thread when accepting new connections
 */
void enqueue_client(ClientQueue *queue, int socket_fd) {
    // Allocate new node
    ClientNode *new_node = malloc(sizeof(ClientNode));
    if (!new_node) {
        perror("Failed to allocate client node");
        return;
    }
    
    new_node->socket_fd = socket_fd;
    new_node->next = NULL;
    
    // Critical section - modify queue
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->tail == NULL) {
        // Queue is empty
        queue->head = new_node;
        queue->tail = new_node;
    } else {
        // Add to end of queue
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
    
    queue->count++;
    
    // Signal waiting consumer threads that new work is available
    pthread_cond_signal(&queue->cond);
    
    pthread_mutex_unlock(&queue->mutex);
}

/**
 * Dequeue a client socket (Consumer operation)
 * Called by client threads - blocks if queue is empty
 * Returns -1 if server is shutting down
 */
int dequeue_client(ClientQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    
    // Wait while queue is empty AND server is still running
    while (queue->count == 0 && server_running) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    
    // Check if we're shutting down
    if (!server_running && queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    // Dequeue the first element
    ClientNode *node = queue->head;
    int socket_fd = node->socket_fd;
    
    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;  // Queue is now empty
    }
    
    queue->count--;
    
    pthread_mutex_unlock(&queue->mutex);
    
    // Free the node
    free(node);
    
    return socket_fd;
}

/**
 * Cleanup client queue - frees all remaining nodes
 */
void destroy_client_queue(ClientQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    
    ClientNode *current = queue->head;
    while (current) {
        ClientNode *next = current->next;
        free(current);
        current = next;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
}

// ============================================================================
// TASK QUEUE IMPLEMENTATION (Producer-Consumer Queue for Work Tasks)
// ============================================================================

/**
 * Initialize the task queue with mutex and condition variable
 * This queue holds file operation tasks (upload, download, delete, list)
 */
void init_task_queue(TaskQueue *queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

/**
 * Enqueue a task (Producer operation)
 * Called by client threads when they receive commands
 */
void enqueue_task(TaskQueue *queue, Task *task) {
    // Allocate new node
    TaskNode *new_node = malloc(sizeof(TaskNode));
    if (!new_node) {
        perror("Failed to allocate task node");
        return;
    }
    
    new_node->task = task;
    new_node->next = NULL;
    
    // Critical section - modify queue
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->tail == NULL) {
        // Queue is empty
        queue->head = new_node;
        queue->tail = new_node;
    } else {
        // Add to end of queue
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
    
    queue->count++;
    
    // Signal waiting worker threads that new work is available
    pthread_cond_signal(&queue->cond);
    
    pthread_mutex_unlock(&queue->mutex);
}

/**
 * Dequeue a task (Consumer operation)
 * Called by worker threads - blocks if queue is empty
 * Returns NULL if server is shutting down
 */
Task* dequeue_task(TaskQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    
    // Wait while queue is empty AND server is still running
    while (queue->count == 0 && server_running) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    
    // Check if we're shutting down
    if (!server_running && queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    // Dequeue the first element
    TaskNode *node = queue->head;
    Task *task = node->task;
    
    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;  // Queue is now empty
    }
    
    queue->count--;
    
    pthread_mutex_unlock(&queue->mutex);
    
    // Free the node (but not the task - that's managed by client thread)
    free(node);
    
    return task;
}

/**
 * Cleanup task queue - frees all remaining nodes and tasks
 */
void destroy_task_queue(TaskQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    
    TaskNode *current = queue->head;
    while (current) {
        TaskNode *next = current->next;
        
        // Free task data if it exists
        if (current->task) {
            if (current->task->file_data) {
                free(current->task->file_data);
            }
            if (current->task->task_mutex) {
                pthread_mutex_destroy(current->task->task_mutex);
                free(current->task->task_mutex);
            }
            if (current->task->task_cond) {
                pthread_cond_destroy(current->task->task_cond);
                free(current->task->task_cond);
            }
            free(current->task);
        }
        
        free(current);
        current = next;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
}