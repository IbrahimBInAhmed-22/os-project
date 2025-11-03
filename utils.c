#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define USERS_FILE "users.txt"

UserManager* user_manager_create(void) {
    UserManager *mgr = malloc(sizeof(UserManager));
    if (!mgr) return NULL;
    
    mgr->user_count = 0;
    pthread_mutex_init(&mgr->mutex, NULL);
    
    /* Initialize per-user mutexes */
    for (int i = 0; i < MAX_USERS; i++) {
        pthread_mutex_init(&mgr->users[i].user_mutex, NULL);
    }
    
    /* Create users directory if not exists */
    mkdir("users", 0755);
    
    /* Load existing users */
    user_manager_load(mgr);
    
    return mgr;
}

void user_manager_destroy(UserManager *mgr) {
    if (!mgr) return;
    
    /* Save before shutdown */
    user_manager_save(mgr);
    
    /* Destroy per-user mutexes */
    for (int i = 0; i < MAX_USERS; i++) {
        pthread_mutex_destroy(&mgr->users[i].user_mutex);
    }
    
    pthread_mutex_destroy(&mgr->mutex);
    free(mgr);
}

/* Register new user (returns user_id or -1 on error) */
int user_register(UserManager *mgr, const char *username, const char *password) {
    pthread_mutex_lock(&mgr->mutex);
    
    /* Check if username already exists */
    for (int i = 0; i < mgr->user_count; i++) {
        if (strcmp(mgr->users[i].username, username) == 0) {
            pthread_mutex_unlock(&mgr->mutex);
            return -1; // Username taken
        }
    }
    
    if (mgr->user_count >= MAX_USERS) {
        pthread_mutex_unlock(&mgr->mutex);
        return -1;
    }
    
    /* Create new user */
    int user_id = mgr->user_count;
    mgr->users[user_id].id = user_id;
    strncpy(mgr->users[user_id].username, username, MAX_USERNAME - 1);
    mgr->users[user_id].username[MAX_USERNAME - 1] = '\0';
    strncpy(mgr->users[user_id].password, password, MAX_PASSWORD - 1);
    mgr->users[user_id].password[MAX_PASSWORD - 1] = '\0';
    mgr->users[user_id].quota_used = 0;
    mgr->user_count++;
    
    /* Create user directory */
    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), "users/%s", username);
    mkdir(user_dir, 0755);
    
    /* Save to file - must unlock first to avoid deadlock */
    pthread_mutex_unlock(&mgr->mutex);
    user_manager_save(mgr);
    
    return user_id;
}

/* Authenticate user (returns user_id or -1 on failure) */
int user_login(UserManager *mgr, const char *username, const char *password) {
    pthread_mutex_lock(&mgr->mutex);
    
    for (int i = 0; i < mgr->user_count; i++) {
        if (strcmp(mgr->users[i].username, username) == 0 &&
            strcmp(mgr->users[i].password, password) == 0) {
            pthread_mutex_unlock(&mgr->mutex);
            return mgr->users[i].id;
        }
    }
    
    pthread_mutex_unlock(&mgr->mutex);
    return -1;
}

/* Get user by ID (not thread-safe, caller must lock if needed) */
User* user_get_by_id(UserManager *mgr, int user_id) {
    if (user_id < 0 || user_id >= mgr->user_count) {
        return NULL;
    }
    return &mgr->users[user_id];
}

/* Add to user's quota (returns 0 on success, -1 if exceeds quota) */
int user_add_quota(UserManager *mgr, int user_id, long bytes) {
    User *user = user_get_by_id(mgr, user_id);
    if (!user) return -1;
    
    pthread_mutex_lock(&user->user_mutex);
    
    if (user->quota_used + bytes > USER_QUOTA_BYTES) {
        pthread_mutex_unlock(&user->user_mutex);
        return -1;
    }
    
    user->quota_used += bytes;
    pthread_mutex_unlock(&user->user_mutex);
    
    return 0;
}

/* Remove from user's quota */
int user_remove_quota(UserManager *mgr, int user_id, long bytes) {
    User *user = user_get_by_id(mgr, user_id);
    if (!user) return -1;
    
    pthread_mutex_lock(&user->user_mutex);
    user->quota_used -= bytes;
    if (user->quota_used < 0) user->quota_used = 0;
    pthread_mutex_unlock(&user->user_mutex);
    
    return 0;
}

/* Load users from file */
int user_manager_load(UserManager *mgr) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) return 0; // No file yet
    
    pthread_mutex_lock(&mgr->mutex);
    
    mgr->user_count = 0;
    char username[MAX_USERNAME], password[MAX_PASSWORD];
    long quota_used;
    
    while (fscanf(fp, "%s %s %ld\n", username, password, &quota_used) == 3) {
        if (mgr->user_count >= MAX_USERS) break;
        
        int id = mgr->user_count;
        mgr->users[id].id = id;
        strncpy(mgr->users[id].username, username, MAX_USERNAME - 1);
        mgr->users[id].username[MAX_USERNAME - 1] = '\0';
        strncpy(mgr->users[id].password, password, MAX_PASSWORD - 1);
        mgr->users[id].password[MAX_PASSWORD - 1] = '\0';
        mgr->users[id].quota_used = quota_used;
        mgr->user_count++;
    }
    
    pthread_mutex_unlock(&mgr->mutex);
    fclose(fp);
    
    return 0;
}

/* Save users to file */
int user_manager_save(UserManager *mgr) {
    FILE *fp = fopen(USERS_FILE, "w");
    if (!fp) return -1;
    
    pthread_mutex_lock(&mgr->mutex);
    
    for (int i = 0; i < mgr->user_count; i++) {
        /* Lock individual user to safely read quota_used */
        pthread_mutex_lock(&mgr->users[i].user_mutex);
        long quota = mgr->users[i].quota_used;
        pthread_mutex_unlock(&mgr->users[i].user_mutex);
        
        fprintf(fp, "%s %s %ld\n",
                mgr->users[i].username,
                mgr->users[i].password,
                quota);
    }
    
    pthread_mutex_unlock(&mgr->mutex);
    fclose(fp);
    
    return 0;
}