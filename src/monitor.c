/**
 * @file monitor.c
 * @brief Implementation of monitor synchronization primitive
 */

#include "monitor.h"
#include <errno.h>
#include <string.h>

/**
 * Initialize a monitor
 */
int monitor_init(monitor_t* monitor) {
    if (!monitor) {
        errno = EINVAL;
        return -1;
    }
    
    /* Clear the structure */
    memset(monitor, 0, sizeof(monitor_t));
    
    /* Initialize mutex */
    if (pthread_mutex_init(&monitor->mutex, NULL) != 0) {
        return -1;
    }
    
    /* Initialize condition variable */
    if (pthread_cond_init(&monitor->condition, NULL) != 0) {
        pthread_mutex_destroy(&monitor->mutex);
        return -1;
    }
    
    monitor->initialized = 1;
    return 0;
}

/**
 * Destroy a monitor and release resources
 */
void monitor_destroy(monitor_t* monitor) {
    if (!monitor || !monitor->initialized) {
        return;
    }
    
    pthread_cond_destroy(&monitor->condition);
    pthread_mutex_destroy(&monitor->mutex);
    monitor->initialized = 0;
}

/**
 * Enter the monitor (acquire mutex)
 */
int monitor_enter(monitor_t* monitor) {
    if (!monitor || !monitor->initialized) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = pthread_mutex_lock(&monitor->mutex);
    if (ret != 0) {
        errno = ret;
        return -1;
    }
    
    return 0;
}

/**
 * Exit the monitor (release mutex)
 */
int monitor_exit(monitor_t* monitor) {
    if (!monitor || !monitor->initialized) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = pthread_mutex_unlock(&monitor->mutex);
    if (ret != 0) {
        errno = ret;
        return -1;
    }
    
    return 0;
}

/**
 * Wait on the monitor's condition variable
 */
int monitor_wait(monitor_t* monitor) {
    if (!monitor || !monitor->initialized) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = pthread_cond_wait(&monitor->condition, &monitor->mutex);
    if (ret != 0) {
        errno = ret;
        return -1;
    }
    
    return 0;
}

/**
 * Wait on the monitor's condition with timeout
 */
int monitor_wait_timeout(monitor_t* monitor, const struct timespec* abstime) {
    if (!monitor || !monitor->initialized || !abstime) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = pthread_cond_timedwait(&monitor->condition, &monitor->mutex, abstime);
    if (ret == ETIMEDOUT) {
        return ETIMEDOUT;
    } else if (ret != 0) {
        errno = ret;
        return -1;
    }
    
    return 0;
}

/**
 * Wait for a predicate to become true
 */
int monitor_wait_for(monitor_t* monitor, monitor_predicate_t predicate, void* arg) {
    if (!monitor || !monitor->initialized || !predicate) {
        errno = EINVAL;
        return -1;
    }
    
    /* Loop to handle spurious wakeups */
    while (!predicate(arg)) {
        int ret = pthread_cond_wait(&monitor->condition, &monitor->mutex);
        if (ret != 0) {
            errno = ret;
            return -1;
        }
    }
    
    return 0;
}

/**
 * Signal one waiting thread
 */
int monitor_signal(monitor_t* monitor) {
    if (!monitor || !monitor->initialized) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = pthread_cond_signal(&monitor->condition);
    if (ret != 0) {
        errno = ret;
        return -1;
    }
    
    return 0;
}

/**
 * Broadcast to all waiting threads
 */
int monitor_broadcast(monitor_t* monitor) {
    if (!monitor || !monitor->initialized) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = pthread_cond_broadcast(&monitor->condition);
    if (ret != 0) {
        errno = ret;
        return -1;
    }
    
    return 0;
}

/**
 * Try to enter the monitor without blocking
 */
int monitor_try_enter(monitor_t* monitor) {
    if (!monitor || !monitor->initialized) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = pthread_mutex_trylock(&monitor->mutex);
    if (ret == EBUSY) {
        return EBUSY;
    } else if (ret != 0) {
        errno = ret;
        return -1;
    }
    
    return 0;
}