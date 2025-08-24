/**
 * @file queue.h
 * @brief Thread-safe bounded ring buffer queue implementation
 * 
 * This queue provides a thread-safe FIFO data structure with the following features:
 * - Bounded capacity with blocking on full/empty conditions
 * - Producer-consumer pattern with mutex and condition variables
 * - Clean shutdown mechanism that unblocks all waiting threads
 * - No busy-waiting - all blocking is done with condition variables
 * - Immediate string copying to avoid TOCTOU issues
 * 
 * Thread Safety: All functions are thread-safe and can be called concurrently
 * Memory Management: queue_push copies strings, queue_pop allocates strings (caller must free)
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stddef.h>

/* Return codes */
#define QUEUE_SUCCESS    0
#define QUEUE_ERROR     -1
#define QUEUE_SHUTDOWN  -2

/* Queue structure - opaque to users */
typedef struct queue {
    char** buffer;           /* Ring buffer of string pointers */
    size_t capacity;         /* Maximum number of items */
    size_t size;            /* Current number of items */
    size_t head;            /* Index of next item to remove */
    size_t tail;            /* Index of next item to insert */
    
    pthread_mutex_t mutex;   /* Protects all queue state */
    pthread_cond_t not_full; /* Signaled when queue is not full */
    pthread_cond_t not_empty;/* Signaled when queue is not empty */
    
    int shutdown;           /* Flag indicating queue is shutting down */
} queue_t;

/**
 * @brief Initialize a queue with specified capacity
 * 
 * @param queue Pointer to queue structure to initialize
 * @param capacity Maximum number of items the queue can hold
 * @return 0 on success, -1 on error (sets errno)
 * 
 * @note Queue must be destroyed with queue_destroy when no longer needed
 */
int queue_init(queue_t* queue, size_t capacity);

/**
 * @brief Destroy a queue and free all resources
 * 
 * @param queue Pointer to queue to destroy
 * 
 * @note This function will free any remaining strings in the queue
 * @note Behavior is undefined if called while threads are still using the queue
 */
void queue_destroy(queue_t* queue);

/**
 * @brief Push a string onto the queue (blocking if full)
 * 
 * @param queue Pointer to the queue
 * @param str String to push (will be copied)
 * @return 0 on success, QUEUE_SHUTDOWN if queue is shutting down, -1 on error
 * 
 * @note This function blocks if the queue is full until space becomes available
 * @note The string is copied immediately, caller retains ownership of input string
 * @note Thread-safe: can be called concurrently by multiple producers
 */
int queue_push(queue_t* queue, const char* str);

/**
 * @brief Pop a string from the queue (blocking if empty)
 * 
 * @param queue Pointer to the queue
 * @param out_str Pointer to store allocated string (caller must free)
 * @return 0 on success, QUEUE_SHUTDOWN if queue is shutdown and empty, -1 on error
 * 
 * @note This function blocks if the queue is empty until an item becomes available
 * @note Caller is responsible for freeing the returned string
 * @note After shutdown, this will drain remaining items then return QUEUE_SHUTDOWN
 * @note Thread-safe: can be called concurrently by multiple consumers
 */
int queue_pop(queue_t* queue, char** out_str);

/**
 * @brief Initiate queue shutdown
 * 
 * @param queue Pointer to the queue
 * @return 0 on success, -1 on error
 * 
 * @note After shutdown:
 *   - All blocked push operations return QUEUE_SHUTDOWN
 *   - New push operations return QUEUE_SHUTDOWN immediately
 *   - Pop operations drain existing items, then return QUEUE_SHUTDOWN
 *   - All blocked threads are woken up
 * @note This operation is idempotent - calling multiple times is safe
 */
int queue_shutdown(queue_t* queue);

/**
 * @brief Check if queue is full
 * 
 * @param queue Pointer to the queue
 * @return 1 if full, 0 if not full or shutdown
 * 
 * @note This is a snapshot - state may change immediately after return
 * @note Primarily useful for testing and diagnostics
 */
int queue_is_full(queue_t* queue);

/**
 * @brief Check if queue is empty
 * 
 * @param queue Pointer to the queue
 * @return 1 if empty, 0 if not empty
 * 
 * @note This is a snapshot - state may change immediately after return
 * @note Primarily useful for testing and diagnostics
 */
int queue_is_empty(queue_t* queue);

/**
 * @brief Get current size of queue
 * 
 * @param queue Pointer to the queue
 * @return Current number of items in queue
 * 
 * @note This is a snapshot - state may change immediately after return
 * @note Primarily useful for testing and diagnostics
 */
size_t queue_size(queue_t* queue);

#endif /* QUEUE_H */