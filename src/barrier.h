/**
 * @file barrier.h
 * @brief Portable barrier implementation for systems without pthread_barrier
 * 
 * Provides a cross-platform barrier synchronization primitive.
 * On Linux with glibc, uses native pthread_barrier.
 * On macOS/BSD, provides custom implementation using mutex and condition variables.
 */

#ifndef BARRIER_H
#define BARRIER_H

#include <pthread.h>

/* Check if native pthread_barrier is available */
#if defined(__linux__) && defined(_GNU_SOURCE)
    #define HAS_PTHREAD_BARRIER 1
#else
    #define HAS_PTHREAD_BARRIER 0
#endif

#if HAS_PTHREAD_BARRIER
    /* Use native pthread_barrier */
    typedef pthread_barrier_t barrier_t;
    
    static inline int barrier_init(barrier_t* barrier, unsigned int count) {
        return pthread_barrier_init(barrier, NULL, count);
    }
    
    static inline int barrier_wait(barrier_t* barrier) {
        return pthread_barrier_wait(barrier);
    }
    
    static inline int barrier_destroy(barrier_t* barrier) {
        return pthread_barrier_destroy(barrier);
    }
#else
    /* Custom barrier implementation */
    typedef struct {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        unsigned int count;
        unsigned int waiting;
        unsigned int generation;
    } barrier_t;
    
    static inline int barrier_init(barrier_t* barrier, unsigned int count) {
        if (count == 0) return -1;
        
        barrier->count = count;
        barrier->waiting = 0;
        barrier->generation = 0;
        
        if (pthread_mutex_init(&barrier->mutex, NULL) != 0) {
            return -1;
        }
        
        if (pthread_cond_init(&barrier->cond, NULL) != 0) {
            pthread_mutex_destroy(&barrier->mutex);
            return -1;
        }
        
        return 0;
    }
    
    static inline int barrier_wait(barrier_t* barrier) {
        pthread_mutex_lock(&barrier->mutex);
        
        unsigned int gen = barrier->generation;
        
        if (++barrier->waiting == barrier->count) {
            /* Last thread to arrive */
            barrier->generation++;
            barrier->waiting = 0;
            pthread_cond_broadcast(&barrier->cond);
            pthread_mutex_unlock(&barrier->mutex);
            return 1; /* PTHREAD_BARRIER_SERIAL_THREAD */
        } else {
            /* Wait for other threads */
            while (gen == barrier->generation) {
                pthread_cond_wait(&barrier->cond, &barrier->mutex);
            }
            pthread_mutex_unlock(&barrier->mutex);
            return 0;
        }
    }
    
    static inline int barrier_destroy(barrier_t* barrier) {
        pthread_cond_destroy(&barrier->cond);
        pthread_mutex_destroy(&barrier->mutex);
        return 0;
    }
#endif

/* Compatibility macros */
#ifndef HAS_PTHREAD_BARRIER
    #define pthread_barrier_t barrier_t
    #define pthread_barrier_init(b, a, c) barrier_init(b, c)
    #define pthread_barrier_wait(b) barrier_wait(b)
    #define pthread_barrier_destroy(b) barrier_destroy(b)
#endif

#endif /* BARRIER_H */