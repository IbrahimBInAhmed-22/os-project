#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>

#define MAX_USERS 1000
#define MAX_USERNAME 64
#define MAX_PASSWORD 64
#define USER_QUOTA_MB 100
#define USER_QUOTA_BYTES (USER_QUOTA_MB * 1024 * 1024)

/* User account structure */
typedef struct {
    int id;
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    long quota_used;            // Bytes used
    pthread_mutex_t user_mutex; // Per-user lock for file operations
} User;

/* User management system */
typedef struct {
    User users[MAX_USERS];
    int user_count;
    pthread_mutex_t mutex;      // Protects user_count and array
} UserManager;

/* Initialize user management */
UserManager* user_manager_create(void);
void user_manager_destroy(UserManager *mgr);

/* User operations */
int user_register(UserManager *mgr, const char *username, const char *password);
int user_login(UserManager *mgr, const char *username, const char *password);
User* user_get_by_id(UserManager *mgr, int user_id);

/* File size tracking */
int user_add_quota(UserManager *mgr, int user_id, long bytes);
int user_remove_quota(UserManager *mgr, int user_id, long bytes);

/* Persistence */
int user_manager_load(UserManager *mgr);
int user_manager_save(UserManager *mgr);

#endif