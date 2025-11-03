#include "queue.h"
#include <stdlib.h>
#include <string.h>

/* ===== CLIENT QUEUE ===== */

ClientQueue* client_queue_create(int capacity) {
    ClientQueue *queue = malloc(sizeof(ClientQueue));
    if (!queue) return NULL;
    
    queue->connections = malloc(sizeof(ClientConnection) * capacity);
    if (!queue->connections) {
        free(queue);
        return NULL;
    }
    
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    queue->capacity = capacity;
    queue->shutdown = 0;
    
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
    
    return queue;
}

void client_queue_destroy(ClientQueue *queue) {
    if (!queue) return;
    
    free(queue->connections);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    free(queue);
}

/* Producer: push client connection (blocks if full) */
int client_queue_push(ClientQueue *queue, ClientConnection conn) {
    pthread_mutex_lock(&queue->mutex);
    
    /* Wait while queue is full and not shutting down */
    while (queue->count >= queue->capacity && !queue->shutdown) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }
    
    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    queue->connections[queue->rear] = conn;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->count++;
    
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

/* Consumer: pop client connection (blocks if empty) */
int client_queue_pop(ClientQueue *queue, ClientConnection *conn) {
    pthread_mutex_lock(&queue->mutex);
    
    /* Wait while queue is empty and not shutting down */
    while (queue->count == 0 && !queue->shutdown) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    
    if (queue->shutdown && queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    *conn = queue->connections[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->count--;
    
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

void client_queue_shutdown(ClientQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    queue->shutdown = 1;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
}

/* ===== TASK QUEUE ===== */

TaskQueue* task_queue_create(int capacity) {
    TaskQueue *queue = malloc(sizeof(TaskQueue));
    if (!queue) return NULL;
    
    queue->tasks = malloc(sizeof(Task*) * capacity);
    if (!queue->tasks) {
        free(queue);
        return NULL;
    }
    
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    queue->capacity = capacity;
    queue->shutdown = 0;
    
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
    
    return queue;
}

void task_queue_destroy(TaskQueue *queue) {
    if (!queue) return;
    
    free(queue->tasks);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    free(queue);
}

/* Producer: push task pointer (blocks if full) */
int task_queue_push(TaskQueue *queue, Task *task) {
    pthread_mutex_lock(&queue->mutex);
    
    while (queue->count >= queue->capacity && !queue->shutdown) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }
    
    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    queue->tasks[queue->rear] = task;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->count++;
    
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

/* Consumer: pop task pointer (blocks if empty) */
Task* task_queue_pop(TaskQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    
    while (queue->count == 0 && !queue->shutdown) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    
    if (queue->shutdown && queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    Task *task = queue->tasks[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->count--;
    
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    
    return task;
}

void task_queue_shutdown(TaskQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    queue->shutdown = 1;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
}