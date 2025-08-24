/**
 * Unit tests for the thread-safe bounded queue implementation
 * Tests FIFO ordering, blocking behavior, shutdown semantics, and thread safety
 */

#include "minunit.h"
#include "../src/queue.h"
#include "../src/barrier.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/* Test counters */
int tests_run = 0;
int tests_failed = 0;
int assertions_run = 0;
int assertions_failed = 0;

/* Test: Initialize and destroy queue */
test_result_t test_queue_init_destroy(void) {
    queue_t queue;
    
    int ret = queue_init(&queue, 10);
    mu_assert_int_eq(0, ret);
    
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Test: Push and pop single item */
test_result_t test_queue_push_pop_single(void) {
    queue_t queue;
    queue_init(&queue, 10);
    
    const char* test_str = "hello";
    int ret = queue_push(&queue, test_str);
    mu_assert_int_eq(0, ret);
    
    char* popped_str = NULL;
    ret = queue_pop(&queue, &popped_str);
    mu_assert_int_eq(0, ret);
    mu_assert_ptr_not_null(popped_str);
    mu_assert_str_eq("hello", popped_str);
    
    free(popped_str);
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Test: Push and pop multiple items maintaining FIFO order */
test_result_t test_queue_push_pop_multiple(void) {
    queue_t queue;
    queue_init(&queue, 10);
    
    const char* items[] = {"first", "second", "third", "fourth", "fifth"};
    int count = sizeof(items) / sizeof(items[0]);
    
    /* Push all items */
    for (int i = 0; i < count; i++) {
        int ret = queue_push(&queue, items[i]);
        mu_assert_int_eq(0, ret);
    }
    
    /* Pop all items and verify order */
    for (int i = 0; i < count; i++) {
        char* popped = NULL;
        int ret = queue_pop(&queue, &popped);
        mu_assert_int_eq(0, ret);
        mu_assert_ptr_not_null(popped);
        mu_assert_str_eq(items[i], popped);
        free(popped);
    }
    
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Test: Queue capacity limits */
test_result_t test_queue_capacity_limits(void) {
    queue_t queue;
    queue_init(&queue, 3);
    
    /* Fill queue to capacity */
    mu_assert_int_eq(0, queue_push(&queue, "one"));
    mu_assert_int_eq(0, queue_push(&queue, "two"));
    mu_assert_int_eq(0, queue_push(&queue, "three"));
    
    /* Verify queue is full */
    mu_assert_int_eq(1, queue_is_full(&queue));
    
    /* Pop one item */
    char* item = NULL;
    mu_assert_int_eq(0, queue_pop(&queue, &item));
    free(item);
    
    /* Should be able to push again */
    mu_assert_int_eq(0, queue_push(&queue, "four"));
    
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Test: Shutdown behavior for producers */
test_result_t test_queue_shutdown_producer(void) {
    queue_t queue;
    queue_init(&queue, 10);
    
    /* Shutdown the queue */
    int ret = queue_shutdown(&queue);
    mu_assert_int_eq(0, ret);
    
    /* Push should return QUEUE_SHUTDOWN */
    ret = queue_push(&queue, "test");
    mu_assert_int_eq(QUEUE_SHUTDOWN, ret);
    
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Test: Shutdown behavior for consumers */
test_result_t test_queue_shutdown_consumer(void) {
    queue_t queue;
    queue_init(&queue, 10);
    
    /* Add some items */
    queue_push(&queue, "item1");
    queue_push(&queue, "item2");
    
    /* Shutdown the queue */
    queue_shutdown(&queue);
    
    /* Should be able to drain existing items */
    char* item = NULL;
    mu_assert_int_eq(0, queue_pop(&queue, &item));
    mu_assert_str_eq("item1", item);
    free(item);
    
    mu_assert_int_eq(0, queue_pop(&queue, &item));
    mu_assert_str_eq("item2", item);
    free(item);
    
    /* After draining, should get QUEUE_SHUTDOWN */
    mu_assert_int_eq(QUEUE_SHUTDOWN, queue_pop(&queue, &item));
    
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Thread data for concurrent tests */
typedef struct {
    queue_t* queue;
    int id;
    int count;
    int* results;
    barrier_t* barrier;
} thread_data_t;

/* Producer thread function */
void* producer_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    if (data->barrier) {
        barrier_wait(data->barrier);
    }
    
    for (int i = 0; i < data->count; i++) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "msg_%d_%d", data->id, i);
        data->results[i] = queue_push(data->queue, buffer);
    }
    
    return NULL;
}

/* Consumer thread function */
void* consumer_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    if (data->barrier) {
        barrier_wait(data->barrier);
    }
    
    for (int i = 0; i < data->count; i++) {
        char* item = NULL;
        int ret = queue_pop(data->queue, &item);
        data->results[i] = ret;
        if (ret == 0 && item) {
            free(item);
        }
    }
    
    return NULL;
}

/* Test: Single producer, single consumer */
test_result_t test_queue_concurrent_single(void) {
    queue_t queue;
    queue_init(&queue, 5);
    
    barrier_t barrier;
    barrier_init(&barrier, 2);
    
    thread_data_t producer_data = {&queue, 1, 10, calloc(10, sizeof(int)), &barrier};
    thread_data_t consumer_data = {&queue, 1, 10, calloc(10, sizeof(int)), &barrier};
    
    pthread_t producer, consumer;
    pthread_create(&producer, NULL, producer_thread, &producer_data);
    pthread_create(&consumer, NULL, consumer_thread, &consumer_data);
    
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    
    /* Verify all operations succeeded */
    for (int i = 0; i < 10; i++) {
        mu_assert_int_eq(0, producer_data.results[i]);
        mu_assert_int_eq(0, consumer_data.results[i]);
    }
    
    free(producer_data.results);
    free(consumer_data.results);
    barrier_destroy(&barrier);
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Test: Multiple producers, single consumer */
test_result_t test_queue_concurrent_multiple_producers(void) {
    queue_t queue;
    queue_init(&queue, 10);
    
    barrier_t barrier;
    barrier_init(&barrier, 5); /* 4 producers + 1 consumer */
    
    thread_data_t producers[4];
    pthread_t producer_threads[4];
    
    /* Start producers */
    for (int i = 0; i < 4; i++) {
        producers[i] = (thread_data_t){&queue, i, 5, calloc(5, sizeof(int)), &barrier};
        pthread_create(&producer_threads[i], NULL, producer_thread, &producers[i]);
    }
    
    /* Start consumer */
    thread_data_t consumer_data = {&queue, 0, 20, calloc(20, sizeof(int)), &barrier};
    pthread_t consumer;
    pthread_create(&consumer, NULL, consumer_thread, &consumer_data);
    
    /* Wait for all threads */
    for (int i = 0; i < 4; i++) {
        pthread_join(producer_threads[i], NULL);
    }
    pthread_join(consumer, NULL);
    
    /* Verify all operations succeeded */
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 5; j++) {
            mu_assert_int_eq(0, producers[i].results[j]);
        }
        free(producers[i].results);
    }
    
    for (int i = 0; i < 20; i++) {
        mu_assert_int_eq(0, consumer_data.results[i]);
    }
    
    free(consumer_data.results);
    barrier_destroy(&barrier);
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Test: Blocking behavior when queue is full */
void* blocking_producer_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    /* This should block since queue is full */
    data->results[0] = queue_push(data->queue, "blocked");
    
    return NULL;
}

test_result_t test_queue_blocking_when_full(void) {
    queue_t queue;
    queue_init(&queue, 2);
    
    /* Fill the queue */
    queue_push(&queue, "first");
    queue_push(&queue, "second");
    
    thread_data_t producer_data = {&queue, 0, 1, calloc(1, sizeof(int)), NULL};
    pthread_t producer;
    pthread_create(&producer, NULL, blocking_producer_thread, &producer_data);
    
    /* Give producer time to block */
    usleep(100000); /* 100ms */
    
    /* Pop one item to unblock producer */
    char* item = NULL;
    queue_pop(&queue, &item);
    free(item);
    
    /* Producer should now complete */
    pthread_join(producer, NULL);
    mu_assert_int_eq(0, producer_data.results[0]);
    
    free(producer_data.results);
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Test: Blocking behavior when queue is empty */
void* blocking_consumer_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    char* item = NULL;
    /* This should block since queue is empty */
    data->results[0] = queue_pop(data->queue, &item);
    if (item) free(item);
    
    return NULL;
}

test_result_t test_queue_blocking_when_empty(void) {
    queue_t queue;
    queue_init(&queue, 10);
    
    thread_data_t consumer_data = {&queue, 0, 1, calloc(1, sizeof(int)), NULL};
    pthread_t consumer;
    pthread_create(&consumer, NULL, blocking_consumer_thread, &consumer_data);
    
    /* Give consumer time to block */
    usleep(100000); /* 100ms */
    
    /* Push item to unblock consumer */
    queue_push(&queue, "unblock");
    
    /* Consumer should now complete */
    pthread_join(consumer, NULL);
    mu_assert_int_eq(0, consumer_data.results[0]);
    
    free(consumer_data.results);
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Test: Shutdown unblocks all waiting threads */
void* waiting_producer(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    data->results[0] = queue_push(data->queue, "waiting");
    return NULL;
}

void* waiting_consumer(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    char* item = NULL;
    data->results[0] = queue_pop(data->queue, &item);
    if (item) free(item);
    return NULL;
}

test_result_t test_queue_shutdown_unblocks_threads(void) {
    queue_t queue;
    queue_init(&queue, 1);
    
    /* Fill queue */
    queue_push(&queue, "full");
    
    /* Start producer that will block */
    thread_data_t producer_data = {&queue, 0, 1, calloc(1, sizeof(int)), NULL};
    pthread_t producer;
    pthread_create(&producer, NULL, waiting_producer, &producer_data);
    
    /* Give producer time to block */
    usleep(50000);
    
    /* Now empty the queue */
    char* item = NULL;
    queue_pop(&queue, &item);
    free(item);
    
    /* Start consumer that will block on empty queue */
    thread_data_t consumer_data = {&queue, 0, 1, calloc(1, sizeof(int)), NULL};
    pthread_t consumer;
    pthread_create(&consumer, NULL, waiting_consumer, &consumer_data);
    
    /* Give consumer time to block */
    usleep(50000);
    
    /* Shutdown should unblock both */
    queue_shutdown(&queue);
    
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    
    /* Producer might succeed if space becomes available, or get QUEUE_SHUTDOWN */
    mu_assert("Producer should get valid result", 
              producer_data.results[0] == 0 || producer_data.results[0] == QUEUE_SHUTDOWN);
    /* Consumer might get 0 if producer pushed, or QUEUE_SHUTDOWN */
    mu_assert("Consumer should get valid result", 
              consumer_data.results[0] == 0 || consumer_data.results[0] == QUEUE_SHUTDOWN);
    
    free(producer_data.results);
    free(consumer_data.results);
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Main test runner */
int main(void) {
    printf("Running Queue Unit Tests\n");
    printf("========================\n\n");
    
    /* Basic functionality tests */
    mu_run_test(test_queue_init_destroy);
    mu_run_test(test_queue_push_pop_single);
    mu_run_test(test_queue_push_pop_multiple);
    mu_run_test(test_queue_capacity_limits);
    
    /* Shutdown tests */
    mu_run_test(test_queue_shutdown_producer);
    mu_run_test(test_queue_shutdown_consumer);
    
    /* Concurrency tests */
    mu_run_test(test_queue_concurrent_single);
    mu_run_test(test_queue_concurrent_multiple_producers);
    
    /* Blocking tests */
    mu_run_test(test_queue_blocking_when_full);
    mu_run_test(test_queue_blocking_when_empty);
    mu_run_test(test_queue_shutdown_unblocks_threads);
    
    mu_print_summary();
    
    return tests_failed > 0 ? 1 : 0;
}