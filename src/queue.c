/**
 * @file queue.c
 * @brief Implementation of thread-safe bounded ring buffer queue
 */

#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/**
 * Initialize a queue with specified capacity
 */
int queue_init(queue_t* queue, size_t capacity) {
    if (!queue || capacity == 0) {
        errno = EINVAL;
        return -1;
    }
    
    /* Initialize buffer */
    queue->buffer = calloc(capacity, sizeof(char*));
    if (!queue->buffer) {
        errno = ENOMEM;
        return -1;
    }
    
    queue->capacity = capacity;
    queue->size = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->shutdown = 0;
    
    /* Initialize synchronization primitives */
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        free(queue->buffer);
        return -1;
    }
    
    if (pthread_cond_init(&queue->not_full, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        free(queue->buffer);
        return -1;
    }
    
    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        pthread_cond_destroy(&queue->not_full);
        pthread_mutex_destroy(&queue->mutex);
        free(queue->buffer);
        return -1;
    }
    
    return 0;
}

/**
 * Destroy a queue and free all resources
 */
void queue_destroy(queue_t* queue) {
    if (!queue) return;
    
    pthread_mutex_lock(&queue->mutex);
    
    /* Free any remaining strings in the queue */
    while (queue->size > 0) {
        free(queue->buffer[queue->head]);
        queue->buffer[queue->head] = NULL;
        queue->head = (queue->head + 1) % queue->capacity;
        queue->size--;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    
    /* Destroy synchronization primitives */
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    pthread_mutex_destroy(&queue->mutex);
    
    /* Free buffer */
    free(queue->buffer);
    queue->buffer = NULL;
}

/**
 * Push a string onto the queue (blocking if full)
 */
int queue_push(queue_t* queue, const char* str) {
    if (!queue || !str) {
        errno = EINVAL;
        return -1;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    /* Check for shutdown */
    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->mutex);
        return QUEUE_SHUTDOWN;
    }
    
    /* Wait while queue is full */
    while (queue->size >= queue->capacity && !queue->shutdown) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }
    
    /* Check for shutdown again after wait */
    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->mutex);
        return QUEUE_SHUTDOWN;
    }
    
    /* Copy string immediately to avoid TOCTOU */
    char* str_copy = strdup(str);
    if (!str_copy) {
        pthread_mutex_unlock(&queue->mutex);
        errno = ENOMEM;
        return -1;
    }
    
    /* Add to queue */
    queue->buffer[queue->tail] = str_copy;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->size++;
    
    /* Signal that queue is not empty */
    pthread_cond_signal(&queue->not_empty);
    
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

/**
 * Pop a string from the queue (blocking if empty)
 */
int queue_pop(queue_t* queue, char** out_str) {
    if (!queue || !out_str) {
        errno = EINVAL;
        return -1;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    /* Wait while queue is empty */
    while (queue->size == 0 && !queue->shutdown) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    
    /* If shutdown and empty, return shutdown status */
    if (queue->shutdown && queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        *out_str = NULL;
        return QUEUE_SHUTDOWN;
    }
    
    /* Remove from queue */
    *out_str = queue->buffer[queue->head];
    queue->buffer[queue->head] = NULL;
    queue->head = (queue->head + 1) % queue->capacity;
    queue->size--;
    
    /* Signal that queue is not full */
    pthread_cond_signal(&queue->not_full);
    
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

/**
 * Initiate queue shutdown
 */
int queue_shutdown(queue_t* queue) {
    if (!queue) {
        errno = EINVAL;
        return -1;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    queue->shutdown = 1;
    
    /* Wake up all waiting threads */
    pthread_cond_broadcast(&queue->not_full);
    pthread_cond_broadcast(&queue->not_empty);
    
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

/**
 * Check if queue is full
 */
int queue_is_full(queue_t* queue) {
    if (!queue) return 0;
    
    pthread_mutex_lock(&queue->mutex);
    int full = (queue->size >= queue->capacity) && !queue->shutdown;
    pthread_mutex_unlock(&queue->mutex);
    
    return full;
}

/**
 * Check if queue is empty
 */
int queue_is_empty(queue_t* queue) {
    if (!queue) return 1;
    
    pthread_mutex_lock(&queue->mutex);
    int empty = (queue->size == 0);
    pthread_mutex_unlock(&queue->mutex);
    
    return empty;
}

/**
 * Get current size of queue
 */
size_t queue_size(queue_t* queue) {
    if (!queue) return 0;
    
    pthread_mutex_lock(&queue->mutex);
    size_t size = queue->size;
    pthread_mutex_unlock(&queue->mutex);
    
    return size;
}