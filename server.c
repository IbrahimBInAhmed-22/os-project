#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "queue.h"
#include "threadpool.h"
#include "utils.h"

#define PORT 8080
#define CLIENT_THREADS 8
#define WORKER_THREADS 4
#define CLIENT_QUEUE_SIZE 100
#define TASK_QUEUE_SIZE 200

/* Global resources for signal handler */
static volatile int server_running = 1;
static int server_socket = -1;
static ClientQueue *client_queue = NULL;
static TaskQueue *task_queue = NULL;
static ClientThreadPool *client_pool = NULL;
static WorkerThreadPool *worker_pool = NULL;
static UserManager *user_mgr = NULL;

/* Signal handler for graceful shutdown */
void handle_shutdown(int sig) {
    sig++;
    printf("\n[Server] Shutdown signal received, cleaning up...\n");
    server_running = 0;
    
    /* Close server socket to unblock accept() */
    if (server_socket != -1) {
        close(server_socket);
    }
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int port = PORT;
    
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    printf("=== Dropbox-Like File Server ===\n");
    printf("Starting server on port %d...\n", port);
    
    /* Install signal handler for SIGINT (Ctrl+C) */
    signal(SIGINT, handle_shutdown);
    signal(SIGTERM, handle_shutdown);
    
    /* Initialize user management */
    user_mgr = user_manager_create();
    if (!user_mgr) {
        fprintf(stderr, "Failed to create user manager\n");
        return 1;
    }
    printf("[Server] User manager initialized (%d users loaded)\n", 
           user_mgr->user_count);
    
    /* Create thread-safe queues */
    client_queue = client_queue_create(CLIENT_QUEUE_SIZE);
    task_queue = task_queue_create(TASK_QUEUE_SIZE);
    
    if (!client_queue || !task_queue) {
        fprintf(stderr, "Failed to create queues\n");
        return 1;
    }
    printf("[Server] Queues created (client: %d, task: %d)\n", 
           CLIENT_QUEUE_SIZE, TASK_QUEUE_SIZE);
    
    /* Create thread pools */
    client_pool = client_pool_create(CLIENT_THREADS, client_queue, 
                                      task_queue, user_mgr);
    worker_pool = worker_pool_create(WORKER_THREADS, task_queue, user_mgr);
    
    if (!client_pool || !worker_pool) {
        fprintf(stderr, "Failed to create thread pools\n");
        return 1;
    }
    printf("[Server] Thread pools created (client: %d, worker: %d)\n",
           CLIENT_THREADS, WORKER_THREADS);
    
    /* Create TCP socket */
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return 1;
    }
    
    /* Set socket options to reuse address */
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    /* Bind to port */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, 
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_socket);
        return 1;
    }
    
    /* Listen for connections */
    if (listen(server_socket, 10) < 0) {
        perror("listen");
        close(server_socket);
        return 1;
    }
    
    printf("[Server] Listening on port %d\n", port);
    printf("[Server] Press Ctrl+C to shutdown\n\n");
    
    /* Main accept loop */
    while (server_running) {
        int client_sock = accept(server_socket, 
                                  (struct sockaddr*)&client_addr, 
                                  &client_len);
        
        if (client_sock < 0) {
            if (server_running) {
                perror("accept");
            }
            break;
        }
        
        printf("[Server] Accepted connection from %s:%d (socket %d)\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port),
               client_sock);
        
        /* Push to client queue for processing */
        ClientConnection conn;
        conn.client_socket = client_sock;
        conn.addr = client_addr;
        
        if (client_queue_push(client_queue, conn) == -1) {
            fprintf(stderr, "[Server] Client queue full, rejecting connection\n");
            close(client_sock);
        }
    }
    
    /* Cleanup */
    printf("\n[Server] Shutting down gracefully...\n");
    
    /* Signal thread pools to shutdown */
    if (client_pool) {
        client_pool_shutdown(client_pool);
    }
    if (worker_pool) {
        worker_pool_shutdown(worker_pool);
    }
    
    /* Destroy thread pools (waits for threads to finish) */
    if (client_pool) {
        printf("[Server] Waiting for client threads...\n");
        client_pool_destroy(client_pool);
    }
    if (worker_pool) {
        printf("[Server] Waiting for worker threads...\n");
        worker_pool_destroy(worker_pool);
    }
    
    /* Destroy queues */
    if (client_queue) {
        client_queue_destroy(client_queue);
    }
    if (task_queue) {
        task_queue_destroy(task_queue);
    }
    
    /* Destroy user manager (saves users) */
    if (user_mgr) {
        user_manager_destroy(user_mgr);
    }
    
    /* Close server socket */
    if (server_socket != -1) {
        close(server_socket);
    }
    
    printf("[Server] Shutdown complete\n");
    return 0;
}