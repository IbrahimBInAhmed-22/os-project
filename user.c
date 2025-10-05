#include "server.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// USER DATABASE IMPLEMENTATION (Authentication & Quota Management)
// ============================================================================

/**
 * Initialize the user database
 * Simple linked list with mutex protection
 */
void init_user_db(UserDB *db) {
    db->head = NULL;
    pthread_mutex_init(&db->mutex, NULL);
}

/**
 * Sign up a new user
 * Returns true if successful, false if username already exists
 */
bool signup_user(UserDB *db, const char *username, const char *password) {
    pthread_mutex_lock(&db->mutex);
    
    // Check if user already exists
    User *current = db->head;
    while (current) {
        if (strcmp(current->username, username) == 0) {
            pthread_mutex_unlock(&db->mutex);
            return false;  // User already exists
        }
        current = current->next;
    }
    
    // Create new user
    User *new_user = malloc(sizeof(User));
    if (!new_user) {
        pthread_mutex_unlock(&db->mutex);
        return false;
    }
    
    strncpy(new_user->username, username, MAX_USERNAME - 1);
    new_user->username[MAX_USERNAME - 1] = '\0';
    
    strncpy(new_user->password, password, MAX_PASSWORD - 1);
    new_user->password[MAX_PASSWORD - 1] = '\0';
    
    new_user->quota_used = 0;
    
    // Add to front of list
    new_user->next = db->head;
    db->head = new_user;
    
    pthread_mutex_unlock(&db->mutex);
    
    // Create user's storage directory
    create_user_directory(username);
    
    return true;
}

/**
 * Login user - verify credentials
 * Returns true if credentials are valid
 */
bool login_user(UserDB *db, const char *username, const char *password) {
    pthread_mutex_lock(&db->mutex);
    
    User *current = db->head;
    while (current) {
        if (strcmp(current->username, username) == 0) {
            // Found user, check password
            bool valid = (strcmp(current->password, password) == 0);
            pthread_mutex_unlock(&db->mutex);
            return valid;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&db->mutex);
    return false;  // User not found
}

/**
 * Get user's current quota usage
 * Returns -1 if user not found
 */
long get_user_quota(UserDB *db, const char *username) {
    pthread_mutex_lock(&db->mutex);
    
    User *current = db->head;
    while (current) {
        if (strcmp(current->username, username) == 0) {
            long quota = current->quota_used;
            pthread_mutex_unlock(&db->mutex);
            return quota;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&db->mutex);
    return -1;
}

/**
 * Update user's quota usage
 * delta: positive to add, negative to subtract
 * Returns false if quota would be exceeded or user not found
 */
bool update_user_quota(UserDB *db, const char *username, long delta) {
    pthread_mutex_lock(&db->mutex);
    
    User *current = db->head;
    while (current) {
        if (strcmp(current->username, username) == 0) {
            long new_quota = current->quota_used + delta;
            
            // Check if exceeding quota (only for positive delta)
            if (delta > 0 && new_quota > USER_QUOTA) {
                pthread_mutex_unlock(&db->mutex);
                return false;
            }
            
            // Don't allow negative quota
            if (new_quota < 0) {
                new_quota = 0;
            }
            
            current->quota_used = new_quota;
            pthread_mutex_unlock(&db->mutex);
            return true;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&db->mutex);
    return false;  // User not found
}

/**
 * Cleanup user database - free all users
 */
void destroy_user_db(UserDB *db) {
    pthread_mutex_lock(&db->mutex);
    
    User *current = db->head;
    while (current) {
        User *next = current->next;
        free(current);
        current = next;
    }
    
    pthread_mutex_unlock(&db->mutex);
    pthread_mutex_destroy(&db->mutex);
}