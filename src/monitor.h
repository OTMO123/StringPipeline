/**
 * @file monitor.h
 * @brief Monitor synchronization primitive (mutex + condition variable)
 * 
 * Provides a high-level abstraction over pthread mutex and condition variables
 * for implementing monitor-style synchronization patterns. Features include:
 * - Mutual exclusion for critical sections
 * - Condition waiting with automatic mutex handling
 * - Signal and broadcast operations
 * - Timeout support for condition waits
 * - Predicate-based waiting to handle spurious wakeups
 * 
 * Thread Safety: All operations are thread-safe
 * Memory Management: User is responsible for monitor lifetime management
 */

#ifndef MONITOR_H
#define MONITOR_H

#include <pthread.h>
#include <time.h>

/* Monitor structure - opaque to users */
typedef struct monitor {
    pthread_mutex_t mutex;      /* Mutex for mutual exclusion */
    pthread_cond_t condition;   /* Condition variable for waiting */
    int initialized;            /* Flag to track initialization */
} monitor_t;

/* Predicate function type for condition checking */
typedef int (*monitor_predicate_t)(void* arg);

/**
 * @brief Initialize a monitor
 * 
 * @param monitor Pointer to monitor structure to initialize
 * @return 0 on success, -1 on error (sets errno)
 * 
 * @note Monitor must be destroyed with monitor_destroy when no longer needed
 */
int monitor_init(monitor_t* monitor);

/**
 * @brief Destroy a monitor and release resources
 * 
 * @param monitor Pointer to monitor to destroy
 * 
 * @note Behavior is undefined if called while threads are using the monitor
 */
void monitor_destroy(monitor_t* monitor);

/**
 * @brief Enter the monitor (acquire mutex)
 * 
 * @param monitor Pointer to the monitor
 * @return 0 on success, -1 on error
 * 
 * @note This function blocks until the mutex can be acquired
 * @note Must be paired with monitor_exit
 */
int monitor_enter(monitor_t* monitor);

/**
 * @brief Exit the monitor (release mutex)
 * 
 * @param monitor Pointer to the monitor
 * @return 0 on success, -1 on error
 * 
 * @note Must be called after monitor_enter
 * @note Behavior is undefined if called without prior monitor_enter
 */
int monitor_exit(monitor_t* monitor);

/**
 * @brief Wait on the monitor's condition variable
 * 
 * @param monitor Pointer to the monitor
 * @return 0 on success, -1 on error
 * 
 * @note Releases mutex while waiting, reacquires before returning
 * @note Must be called between monitor_enter and monitor_exit
 * @note Should be called in a loop to handle spurious wakeups
 */
int monitor_wait(monitor_t* monitor);

/**
 * @brief Wait on the monitor's condition with timeout
 * 
 * @param monitor Pointer to the monitor
 * @param abstime Absolute time to wait until
 * @return 0 if signaled, ETIMEDOUT if timed out, -1 on error
 * 
 * @note Uses CLOCK_REALTIME for timeout
 * @note Must be called between monitor_enter and monitor_exit
 */
int monitor_wait_timeout(monitor_t* monitor, const struct timespec* abstime);

/**
 * @brief Wait for a predicate to become true
 * 
 * @param monitor Pointer to the monitor
 * @param predicate Function to check condition
 * @param arg Argument to pass to predicate function
 * @return 0 when predicate is true, -1 on error
 * 
 * @note Handles spurious wakeups automatically
 * @note Must be called between monitor_enter and monitor_exit
 */
int monitor_wait_for(monitor_t* monitor, monitor_predicate_t predicate, void* arg);

/**
 * @brief Signal one waiting thread
 * 
 * @param monitor Pointer to the monitor
 * @return 0 on success, -1 on error
 * 
 * @note Wakes at most one thread waiting on the condition
 * @note Can be called inside or outside the monitor
 * @note If no threads are waiting, this is a no-op
 */
int monitor_signal(monitor_t* monitor);

/**
 * @brief Broadcast to all waiting threads
 * 
 * @param monitor Pointer to the monitor
 * @return 0 on success, -1 on error
 * 
 * @note Wakes all threads waiting on the condition
 * @note Can be called inside or outside the monitor
 * @note If no threads are waiting, this is a no-op
 */
int monitor_broadcast(monitor_t* monitor);

/**
 * @brief Try to enter the monitor without blocking
 * 
 * @param monitor Pointer to the monitor
 * @return 0 if acquired, EBUSY if already locked, -1 on error
 * 
 * @note Non-blocking variant of monitor_enter
 * @note If successful, must be paired with monitor_exit
 */
int monitor_try_enter(monitor_t* monitor);

#endif /* MONITOR_H */