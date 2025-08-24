/**
 * Unit tests for the monitor (mutex + condition variable) implementation
 * Tests synchronization, signaling, broadcasting, and timeout behavior
 */

#include "minunit.h"
#include "../src/monitor.h"
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

/* Test counters */
int tests_run = 0;
int tests_failed = 0;
int assertions_run = 0;
int assertions_failed = 0;

/* Test: Initialize and destroy monitor */
test_result_t test_monitor_init_destroy(void) {
    monitor_t monitor;
    
    int ret = monitor_init(&monitor);
    mu_assert_int_eq(0, ret);
    
    monitor_destroy(&monitor);
    
    return MU_PASS;
}

/* Thread data for synchronization tests */
typedef struct {
    monitor_t* monitor;
    int* shared_value;
    int* wait_count;
    int* wake_count;
    pthread_barrier_t* barrier;
} monitor_thread_data_t;

/* Waiter thread function */
void* waiter_thread(void* arg) {
    monitor_thread_data_t* data = (monitor_thread_data_t*)arg;
    
    if (data->barrier) {
        pthread_barrier_wait(data->barrier);
    }
    
    monitor_enter(data->monitor);
    
    /* Increment wait count */
    (*data->wait_count)++;
    
    /* Wait for condition */
    while (*data->shared_value == 0) {
        monitor_wait(data->monitor);
    }
    
    /* Increment wake count */
    (*data->wake_count)++;
    
    monitor_exit(data->monitor);
    
    return NULL;
}

/* Signaler thread function */
void* signaler_thread(void* arg) {
    monitor_thread_data_t* data = (monitor_thread_data_t*)arg;
    
    if (data->barrier) {
        pthread_barrier_wait(data->barrier);
    }
    
    /* Give waiters time to start waiting */
    usleep(100000); /* 100ms */
    
    monitor_enter(data->monitor);
    *data->shared_value = 1;
    monitor_signal(data->monitor);
    monitor_exit(data->monitor);
    
    return NULL;
}

/* Test: Wait and signal with one waiter */
test_result_t test_monitor_wait_signal_single(void) {
    monitor_t monitor;
    monitor_init(&monitor);
    
    int shared_value = 0;
    int wait_count = 0;
    int wake_count = 0;
    
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, 2);
    
    monitor_thread_data_t data = {
        &monitor, &shared_value, &wait_count, &wake_count, &barrier
    };
    
    pthread_t waiter, signaler;
    pthread_create(&waiter, NULL, waiter_thread, &data);
    pthread_create(&signaler, NULL, signaler_thread, &data);
    
    pthread_join(waiter, NULL);
    pthread_join(signaler, NULL);
    
    mu_assert_int_eq(1, wait_count);
    mu_assert_int_eq(1, wake_count);
    mu_assert_int_eq(1, shared_value);
    
    pthread_barrier_destroy(&barrier);
    monitor_destroy(&monitor);
    
    return MU_PASS;
}

/* Broadcaster thread function */
void* broadcaster_thread(void* arg) {
    monitor_thread_data_t* data = (monitor_thread_data_t*)arg;
    
    if (data->barrier) {
        pthread_barrier_wait(data->barrier);
    }
    
    /* Give waiters time to start waiting */
    usleep(200000); /* 200ms */
    
    monitor_enter(data->monitor);
    *data->shared_value = 1;
    monitor_broadcast(data->monitor);
    monitor_exit(data->monitor);
    
    return NULL;
}

/* Test: Broadcast wakes all waiters */
test_result_t test_monitor_broadcast_multiple(void) {
    monitor_t monitor;
    monitor_init(&monitor);
    
    int shared_value = 0;
    int wait_count = 0;
    int wake_count = 0;
    
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, 4); /* 3 waiters + 1 broadcaster */
    
    monitor_thread_data_t data = {
        &monitor, &shared_value, &wait_count, &wake_count, &barrier
    };
    
    pthread_t waiters[3];
    for (int i = 0; i < 3; i++) {
        pthread_create(&waiters[i], NULL, waiter_thread, &data);
    }
    
    pthread_t broadcaster;
    pthread_create(&broadcaster, NULL, broadcaster_thread, &data);
    
    for (int i = 0; i < 3; i++) {
        pthread_join(waiters[i], NULL);
    }
    pthread_join(broadcaster, NULL);
    
    mu_assert_int_eq(3, wait_count);
    mu_assert_int_eq(3, wake_count);
    
    pthread_barrier_destroy(&barrier);
    monitor_destroy(&monitor);
    
    return MU_PASS;
}

/* Timeout waiter thread */
void* timeout_waiter_thread(void* arg) {
    monitor_thread_data_t* data = (monitor_thread_data_t*)arg;
    
    monitor_enter(data->monitor);
    
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 1; /* 1 second timeout */
    
    int ret = monitor_wait_timeout(data->monitor, &timeout);
    
    /* Should timeout since no one signals */
    if (ret == ETIMEDOUT) {
        (*data->wake_count)++; /* Use wake_count to track timeout */
    }
    
    monitor_exit(data->monitor);
    
    return NULL;
}

/* Test: Wait with timeout */
test_result_t test_monitor_wait_timeout(void) {
    monitor_t monitor;
    monitor_init(&monitor);
    
    int shared_value = 0;
    int wait_count = 0;
    int wake_count = 0; /* Used to track timeout occurred */
    
    monitor_thread_data_t data = {
        &monitor, &shared_value, &wait_count, &wake_count, NULL
    };
    
    pthread_t waiter;
    pthread_create(&waiter, NULL, timeout_waiter_thread, &data);
    
    pthread_join(waiter, NULL);
    
    mu_assert_int_eq(1, wake_count); /* Timeout should have occurred */
    
    monitor_destroy(&monitor);
    
    return MU_PASS;
}

/* Predicate function for testing */
int test_predicate(void* arg) {
    int* value = (int*)arg;
    return *value > 0;
}

/* Test: Wait with predicate */
test_result_t test_monitor_wait_predicate(void) {
    monitor_t monitor;
    monitor_init(&monitor);
    
    int shared_value = 0;
    
    /* Thread that waits with predicate */
    pthread_t waiter;
    pthread_create(&waiter, NULL, (void*(*)(void*))monitor_wait_predicate_helper, 
                   &(struct {monitor_t* m; int* v;}) {&monitor, &shared_value});
    
    usleep(100000);
    
    /* Signal with predicate satisfied */
    monitor_enter(&monitor);
    shared_value = 1;
    monitor_signal(&monitor);
    monitor_exit(&monitor);
    
    pthread_join(waiter, NULL);
    
    monitor_destroy(&monitor);
    
    return MU_PASS;
}

/* Helper for predicate wait */
void* monitor_wait_predicate_helper(void* arg) {
    struct {monitor_t* m; int* v;} *data = arg;
    
    monitor_enter(data->m);
    monitor_wait_for(data->m, test_predicate, data->v);
    monitor_exit(data->m);
    
    return NULL;
}

/* Test: Multiple signals wake one waiter each */
test_result_t test_monitor_signal_fairness(void) {
    monitor_t monitor;
    monitor_init(&monitor);
    
    int shared_value = 0;
    int wait_count = 0;
    int wake_count = 0;
    
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, 3); /* 2 waiters + main thread */
    
    monitor_thread_data_t data = {
        &monitor, &shared_value, &wait_count, &wake_count, &barrier
    };
    
    /* Start two waiters */
    pthread_t waiters[2];
    for (int i = 0; i < 2; i++) {
        pthread_create(&waiters[i], NULL, waiter_thread, &data);
    }
    
    pthread_barrier_wait(&barrier);
    usleep(100000); /* Let waiters block */
    
    /* Signal once - should wake one waiter */
    monitor_enter(&monitor);
    shared_value = 1;
    monitor_signal(&monitor);
    monitor_exit(&monitor);
    
    usleep(100000);
    
    /* One waiter should have woken */
    mu_assert_int_eq(1, wake_count);
    
    /* Signal again for second waiter */
    monitor_enter(&monitor);
    monitor_signal(&monitor);
    monitor_exit(&monitor);
    
    /* Wait for both threads */
    for (int i = 0; i < 2; i++) {
        pthread_join(waiters[i], NULL);
    }
    
    mu_assert_int_eq(2, wake_count);
    
    pthread_barrier_destroy(&barrier);
    monitor_destroy(&monitor);
    
    return MU_PASS;
}

/* Test: Monitor protects critical section */
typedef struct {
    monitor_t* monitor;
    int* counter;
    int iterations;
} counter_thread_data_t;

void* increment_thread(void* arg) {
    counter_thread_data_t* data = (counter_thread_data_t*)arg;
    
    for (int i = 0; i < data->iterations; i++) {
        monitor_enter(data->monitor);
        (*data->counter)++;
        monitor_exit(data->monitor);
    }
    
    return NULL;
}

test_result_t test_monitor_mutual_exclusion(void) {
    monitor_t monitor;
    monitor_init(&monitor);
    
    int counter = 0;
    counter_thread_data_t data = {&monitor, &counter, 1000};
    
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, increment_thread, &data);
    }
    
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Counter should be exactly 4000 (4 threads * 1000 iterations) */
    mu_assert_int_eq(4000, counter);
    
    monitor_destroy(&monitor);
    
    return MU_PASS;
}

/* Test: Spurious wakeup handling */
void* spurious_wakeup_waiter(void* arg) {
    monitor_thread_data_t* data = (monitor_thread_data_t*)arg;
    
    monitor_enter(data->monitor);
    
    /* Loop to handle spurious wakeups */
    while (*data->shared_value == 0) {
        (*data->wait_count)++;
        monitor_wait(data->monitor);
    }
    
    (*data->wake_count)++;
    monitor_exit(data->monitor);
    
    return NULL;
}

test_result_t test_monitor_spurious_wakeup_handling(void) {
    monitor_t monitor;
    monitor_init(&monitor);
    
    int shared_value = 0;
    int wait_count = 0;
    int wake_count = 0;
    
    monitor_thread_data_t data = {
        &monitor, &shared_value, &wait_count, &wake_count, NULL
    };
    
    pthread_t waiter;
    pthread_create(&waiter, NULL, spurious_wakeup_waiter, &data);
    
    usleep(100000);
    
    /* Signal without changing value (spurious wakeup) */
    monitor_enter(&monitor);
    monitor_signal(&monitor);
    monitor_exit(&monitor);
    
    usleep(100000);
    
    /* Now signal with value change */
    monitor_enter(&monitor);
    shared_value = 1;
    monitor_signal(&monitor);
    monitor_exit(&monitor);
    
    pthread_join(waiter, NULL);
    
    /* Should have waited at least twice due to spurious wakeup */
    mu_assert("Should handle spurious wakeup", wait_count >= 2);
    mu_assert_int_eq(1, wake_count);
    
    monitor_destroy(&monitor);
    
    return MU_PASS;
}

/* Main test runner */
int main(void) {
    printf("Running Monitor Unit Tests\n");
    printf("==========================\n\n");
    
    /* Basic functionality */
    mu_run_test(test_monitor_init_destroy);
    mu_run_test(test_monitor_wait_signal_single);
    mu_run_test(test_monitor_broadcast_multiple);
    
    /* Timeout tests */
    mu_run_test(test_monitor_wait_timeout);
    
    /* Fairness and exclusion */
    mu_run_test(test_monitor_signal_fairness);
    mu_run_test(test_monitor_mutual_exclusion);
    
    /* Spurious wakeup handling */
    mu_run_test(test_monitor_spurious_wakeup_handling);
    
    mu_print_summary();
    
    return tests_failed > 0 ? 1 : 0;
}