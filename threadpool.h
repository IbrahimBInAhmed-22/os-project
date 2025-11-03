#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include "queue.h"
#include "utils.h"

/* Client thread pool configuration */
typedef struct {
    pthread_t *threads;
    int num_threads;
    ClientQueue *client_queue;
    TaskQueue *task_queue;
    UserManager *user_mgr;
    int shutdown;
    pthread_mutex_t shutdown_mutex;  // Protect shutdown flag
} ClientThreadPool;

/* Worker thread pool configuration */
typedef struct {
    pthread_t *threads;
    int num_threads;
    TaskQueue *task_queue;
    UserManager *user_mgr;
    int shutdown;
    pthread_mutex_t shutdown_mutex;  // Protect shutdown flag
} WorkerThreadPool;

/* Client thread pool operations */
ClientThreadPool* client_pool_create(int num_threads, ClientQueue *cq, 
                                      TaskQueue *tq, UserManager *um);
void client_pool_destroy(ClientThreadPool *pool);
void client_pool_shutdown(ClientThreadPool *pool);

/* Worker thread pool operations */
WorkerThreadPool* worker_pool_create(int num_threads, TaskQueue *tq, 
                                      UserManager *um);
void worker_pool_destroy(WorkerThreadPool *pool);
void worker_pool_shutdown(WorkerThreadPool *pool);

#endif