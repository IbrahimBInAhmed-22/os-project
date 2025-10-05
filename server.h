#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <stdbool.h>
#include <sys/socket.h>

// Configuration constants
#define PORT 8080
#define CLIENT_THREADPOOL_SIZE 5
#define WORKER_THREADPOOL_SIZE 3
#define MAX_CLIENTS 10
#define MAX_FILENAME 256
#define BUFFER_SIZE 4096
#define MAX_USERNAME 50
#define MAX_PASSWORD 50
#define USER_QUOTA 10485760  // 10MB per user

// Command types
typedef enum {
    CMD_UPLOAD,
    CMD_DOWNLOAD,
    CMD_DELETE,
    CMD_LIST
} CommandType;

// Task structure - represents work to be done by worker threads
typedef struct {
    CommandType type;
    int client_socket;           // Socket to send response back to
    char username[MAX_USERNAME];
    char filename[MAX_FILENAME];
    char *file_data;             // For upload operations
    size_t file_size;
    bool completed;              // Flag to signal completion
    pthread_mutex_t *task_mutex; // Mutex for this specific task
    pthread_cond_t *task_cond;   // Condition variable for completion signal
} Task;

// Client Queue Node - holds socket descriptors waiting to be processed
typedef struct ClientNode {
    int socket_fd;
    struct ClientNode *next;
} ClientNode;

// Client Queue - thread-safe queue for incoming connections
typedef struct {
    ClientNode *head;
    ClientNode *tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} ClientQueue;

// Task Queue Node
typedef struct TaskNode {
    Task *task;
    struct TaskNode *next;
} TaskNode;

// Task Queue - thread-safe queue for tasks
typedef struct {
    TaskNode *head;
    TaskNode *tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} TaskQueue;

// User structure for authentication and quota tracking
typedef struct User {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    long quota_used;
    struct User *next;
} User;

// User database - simple linked list with synchronization
typedef struct {
    User *head;
    pthread_mutex_t mutex;
} UserDB;

// Global variables (defined in server.c)
extern ClientQueue client_queue;
extern TaskQueue task_queue;
extern UserDB user_db;
extern volatile bool server_running;

// Function prototypes

// Queue operations
void init_client_queue(ClientQueue *queue);
void enqueue_client(ClientQueue *queue, int socket_fd);
int dequeue_client(ClientQueue *queue);
void destroy_client_queue(ClientQueue *queue);

void init_task_queue(TaskQueue *queue);
void enqueue_task(TaskQueue *queue, Task *task);
Task* dequeue_task(TaskQueue *queue);
void destroy_task_queue(TaskQueue *queue);

// User management
void init_user_db(UserDB *db);
bool signup_user(UserDB *db, const char *username, const char *password);
bool login_user(UserDB *db, const char *username, const char *password);
long get_user_quota(UserDB *db, const char *username);
bool update_user_quota(UserDB *db, const char *username, long delta);
void destroy_user_db(UserDB *db);

// Thread pool functions
void* client_thread_function(void *arg);
void* worker_thread_function(void *arg);

// Command handlers (executed by worker threads)
void handle_upload(Task *task);
void handle_download(Task *task);
void handle_delete(Task *task);
void handle_list(Task *task);

// Utility functions
void create_user_directory(const char *username);
char* get_user_file_path(const char *username, const char *filename);
void send_response(int socket, const char *message);
void send_file(int socket, const char *filepath);

// Client thread helpers (defined in client_thread.c)
bool handle_authentication(int socket, char *username_out);
bool process_command(int socket, const char *username, const char *command_str);

#endif // SERVER_H