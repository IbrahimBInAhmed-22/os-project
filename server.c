#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

ClientQueue client_queue;
TaskQueue task_queue;
UserDB user_db;
volatile bool server_running = true;

// ============================================================================
// SIGNAL HANDLER
// ============================================================================

/**
 * Signal handler for graceful shutdown (Ctrl+C)
 */
void signal_handler(int signum) {
    printf("\n[Server] Received signal %d, shutting down gracefully...\n", signum);
    server_running = false;
    
    // Wake up all waiting threads
    pthread_cond_broadcast(&client_queue.cond);
    pthread_cond_broadcast(&task_queue.cond);
}

// ============================================================================
// MAIN SERVER
// ============================================================================

int main() {
    printf("=== Dropbox Clone Server (Phase 1) ===\n");
    
    // Install signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize data structures
    init_client_queue(&client_queue);
    init_task_queue(&task_queue);
    init_user_db(&user_db);
    
    // Create storage directory
    mkdir("./storage", 0755);
    
    // Create client thread pool
    pthread_t client_threads[CLIENT_THREADPOOL_SIZE];
    for (int i = 0; i < CLIENT_THREADPOOL_SIZE; i++) {
        if (pthread_create(&client_threads[i], NULL, client_thread_function, NULL) != 0) {
            perror("Failed to create client thread");
            exit(EXIT_FAILURE);
        }
    }
    printf("[Server] Created %d client threads\n", CLIENT_THREADPOOL_SIZE);
    
    // Create worker thread pool
    pthread_t worker_threads[WORKER_THREADPOOL_SIZE];
    for (int i = 0; i < WORKER_THREADPOOL_SIZE; i++) {
        if (pthread_create(&worker_threads[i], NULL, worker_thread_function, NULL) != 0) {
            perror("Failed to create worker thread");
            exit(EXIT_FAILURE);
        }
    }
    printf("[Server] Created %d worker threads\n", WORKER_THREADPOOL_SIZE);
    
    // Create server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options to reuse address (helps with rapid restarts)
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Failed to set socket options");
        exit(EXIT_FAILURE);
    }
    
    // Bind socket to port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Failed to listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("[Server] Listening on port %d\n", PORT);
    printf("[Server] Ready to accept connections. Press Ctrl+C to stop.\n\n");
    
    // Main accept loop
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Accept new connection (with timeout to check server_running)
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            if (server_running) {
                perror("Failed to accept connection");
            }
            continue;
        }
        
        printf("[Server] Accepted connection from %s:%d (socket %d)\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_socket);
        
        // Enqueue the client socket to client queue
        enqueue_client(&client_queue, client_socket);
    }
    
    printf("\n[Server] Shutdown initiated...\n");
    
    // Close server socket
    close(server_socket);
    
    // Wait for all client threads to finish
    for (int i = 0; i < CLIENT_THREADPOOL_SIZE; i++) {
        pthread_join(client_threads[i], NULL);
    }
    printf("[Server] All client threads terminated\n");
    
    // Wait for all worker threads to finish
    for (int i = 0; i < WORKER_THREADPOOL_SIZE; i++) {
        pthread_join(worker_threads[i], NULL);
    }
    printf("[Server] All worker threads terminated\n");
    
    // Cleanup
    destroy_client_queue(&client_queue);
    destroy_task_queue(&task_queue);
    destroy_user_db(&user_db);
    
    printf("[Server] Cleanup complete. Goodbye!\n");
    
    return 0;
}