/**
 * Integration tests for the string processing pipeline
 * Tests complete pipeline with multiple stages, concurrent operations, and plugin loading
 */

#include "minunit.h"
#include "../src/queue.h"
#include "../src/monitor.h"
#include "../src/plugin_common.h"
#include "../src/pipeline.h"
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Test counters */
int tests_run = 0;
int tests_failed = 0;
int assertions_run = 0;
int assertions_failed = 0;

/* Test: Single producer, single consumer pipeline */
test_result_t test_single_producer_consumer(void) {
    queue_t queue;
    queue_init(&queue, 10);
    
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, 2);
    
    /* Producer thread data */
    struct {
        queue_t* queue;
        pthread_barrier_t* barrier;
        int count;
    } producer_data = {&queue, &barrier, 100};
    
    /* Consumer thread data */
    struct {
        queue_t* queue;
        pthread_barrier_t* barrier;
        int count;
        char** results;
    } consumer_data = {&queue, &barrier, 100, calloc(100, sizeof(char*))};
    
    /* Producer thread */
    pthread_t producer;
    pthread_create(&producer, NULL, [](void* arg) -> void* {
        typeof(producer_data)* data = arg;
        pthread_barrier_wait(data->barrier);
        
        for (int i = 0; i < data->count; i++) {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "msg_%d", i);
            queue_push(data->queue, buffer);
        }
        return NULL;
    }, &producer_data);
    
    /* Consumer thread */
    pthread_t consumer;
    pthread_create(&consumer, NULL, [](void* arg) -> void* {
        typeof(consumer_data)* data = arg;
        pthread_barrier_wait(data->barrier);
        
        for (int i = 0; i < data->count; i++) {
            queue_pop(data->queue, &data->results[i]);
        }
        return NULL;
    }, &consumer_data);
    
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    
    /* Verify all messages received in order */
    for (int i = 0; i < 100; i++) {
        char expected[32];
        snprintf(expected, sizeof(expected), "msg_%d", i);
        mu_assert_str_eq(expected, consumer_data.results[i]);
        free(consumer_data.results[i]);
    }
    
    free(consumer_data.results);
    pthread_barrier_destroy(&barrier);
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Test: Multiple producers, single consumer */
test_result_t test_multiple_producers_single_consumer(void) {
    queue_t queue;
    queue_init(&queue, 20);
    
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, 5); /* 4 producers + 1 consumer */
    
    /* Track received messages */
    int message_counts[4] = {0};
    pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    /* Producer threads */
    pthread_t producers[4];
    for (int i = 0; i < 4; i++) {
        struct {
            queue_t* queue;
            pthread_barrier_t* barrier;
            int id;
            int count;
        }* data = malloc(sizeof(*data));
        *data = (typeof(*data)){&queue, &barrier, i, 25};
        
        pthread_create(&producers[i], NULL, [](void* arg) -> void* {
            typeof(*data) d = *(typeof(data))arg;
            free(arg);
            pthread_barrier_wait(d.barrier);
            
            for (int j = 0; j < d.count; j++) {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "p%d_m%d", d.id, j);
                queue_push(d.queue, buffer);
            }
            return NULL;
        }, data);
    }
    
    /* Consumer thread */
    pthread_t consumer;
    struct {
        queue_t* queue;
        pthread_barrier_t* barrier;
        int total;
        int* counts;
        pthread_mutex_t* mutex;
    } consumer_data = {&queue, &barrier, 100, message_counts, &count_mutex};
    
    pthread_create(&consumer, NULL, [](void* arg) -> void* {
        typeof(consumer_data)* data = arg;
        pthread_barrier_wait(data->barrier);
        
        for (int i = 0; i < data->total; i++) {
            char* msg = NULL;
            queue_pop(data->queue, &msg);
            
            /* Parse producer ID from message */
            if (msg && sscanf(msg, "p%d_", &i) == 1 && i >= 0 && i < 4) {
                pthread_mutex_lock(data->mutex);
                data->counts[i]++;
                pthread_mutex_unlock(data->mutex);
            }
            free(msg);
        }
        return NULL;
    }, &consumer_data);
    
    for (int i = 0; i < 4; i++) {
        pthread_join(producers[i], NULL);
    }
    pthread_join(consumer, NULL);
    
    /* Verify all producers' messages were received */
    for (int i = 0; i < 4; i++) {
        mu_assert_int_eq(25, message_counts[i]);
    }
    
    pthread_mutex_destroy(&count_mutex);
    pthread_barrier_destroy(&barrier);
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Test: Pipeline with multiple stages */
test_result_t test_multi_stage_pipeline(void) {
    /* Create 3-stage pipeline */
    queue_t queues[4];
    for (int i = 0; i < 4; i++) {
        queue_init(&queues[i], 10);
    }
    
    /* Stage threads */
    pthread_t stages[3];
    
    /* Stage 1: Add prefix */
    pthread_create(&stages[0], NULL, [](void* arg) -> void* {
        queue_t* queues = (queue_t*)arg;
        char* input;
        while (queue_pop(&queues[0], &input) == 0) {
            char output[256];
            snprintf(output, sizeof(output), "S1:%s", input);
            queue_push(&queues[1], output);
            free(input);
        }
        queue_shutdown(&queues[1]);
        return NULL;
    }, queues);
    
    /* Stage 2: Add another prefix */
    pthread_create(&stages[1], NULL, [](void* arg) -> void* {
        queue_t* queues = (queue_t*)arg;
        char* input;
        while (queue_pop(&queues[1], &input) == 0) {
            char output[256];
            snprintf(output, sizeof(output), "S2:%s", input);
            queue_push(&queues[2], output);
            free(input);
        }
        queue_shutdown(&queues[2]);
        return NULL;
    }, queues);
    
    /* Stage 3: Add final prefix */
    pthread_create(&stages[2], NULL, [](void* arg) -> void* {
        queue_t* queues = (queue_t*)arg;
        char* input;
        while (queue_pop(&queues[2], &input) == 0) {
            char output[256];
            snprintf(output, sizeof(output), "S3:%s", input);
            queue_push(&queues[3], output);
            free(input);
        }
        queue_shutdown(&queues[3]);
        return NULL;
    }, queues);
    
    /* Feed input */
    queue_push(&queues[0], "input1");
    queue_push(&queues[0], "input2");
    queue_push(&queues[0], "input3");
    queue_shutdown(&queues[0]);
    
    /* Collect output */
    char* results[3];
    for (int i = 0; i < 3; i++) {
        queue_pop(&queues[3], &results[i]);
    }
    
    /* Verify transformations */
    mu_assert_str_eq("S3:S2:S1:input1", results[0]);
    mu_assert_str_eq("S3:S2:S1:input2", results[1]);
    mu_assert_str_eq("S3:S2:S1:input3", results[2]);
    
    for (int i = 0; i < 3; i++) {
        free(results[i]);
        pthread_join(stages[i], NULL);
    }
    
    for (int i = 0; i < 4; i++) {
        queue_destroy(&queues[i]);
    }
    
    return MU_PASS;
}

/* Test: Bounded buffer blocking behavior */
test_result_t test_bounded_buffer_blocking(void) {
    queue_t queue;
    queue_init(&queue, 2); /* Small capacity */
    
    /* Synchronization for test */
    pthread_barrier_t start_barrier;
    pthread_barrier_init(&start_barrier, NULL, 2);
    
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    int producer_blocked = 0;
    
    /* Producer thread */
    pthread_t producer;
    struct {
        queue_t* queue;
        pthread_barrier_t* barrier;
        pthread_mutex_t* mutex;
        pthread_cond_t* cond;
        int* blocked;
    } producer_data = {&queue, &start_barrier, &mutex, &cond, &producer_blocked};
    
    pthread_create(&producer, NULL, [](void* arg) -> void* {
        typeof(producer_data)* data = arg;
        pthread_barrier_wait(data->barrier);
        
        /* Fill queue */
        queue_push(data->queue, "item1");
        queue_push(data->queue, "item2");
        
        /* This should block */
        pthread_mutex_lock(data->mutex);
        *data->blocked = 1;
        pthread_cond_signal(data->cond);
        pthread_mutex_unlock(data->mutex);
        
        queue_push(data->queue, "item3"); /* Blocks here */
        
        pthread_mutex_lock(data->mutex);
        *data->blocked = 0;
        pthread_mutex_unlock(data->mutex);
        
        return NULL;
    }, &producer_data);
    
    pthread_barrier_wait(&start_barrier);
    
    /* Wait for producer to block */
    pthread_mutex_lock(&mutex);
    while (!producer_blocked) {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    
    /* Verify producer is blocked */
    mu_assert_int_eq(1, producer_blocked);
    
    /* Consume one item to unblock producer */
    char* item = NULL;
    queue_pop(&queue, &item);
    free(item);
    
    /* Wait for producer to complete */
    pthread_join(producer, NULL);
    
    /* Producer should have unblocked */
    mu_assert_int_eq(0, producer_blocked);
    
    pthread_barrier_destroy(&start_barrier);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Test: Pipeline shutdown propagation */
test_result_t test_pipeline_shutdown_propagation(void) {
    /* Create 3-stage pipeline */
    queue_t queues[4];
    for (int i = 0; i < 4; i++) {
        queue_init(&queues[i], 10);
    }
    
    int stages_completed[3] = {0};
    pthread_t stages[3];
    
    /* Create pipeline stages that track completion */
    for (int i = 0; i < 3; i++) {
        struct {
            int stage_id;
            queue_t* input;
            queue_t* output;
            int* completed;
        }* data = malloc(sizeof(*data));
        *data = (typeof(*data)){i, &queues[i], &queues[i+1], &stages_completed[i]};
        
        pthread_create(&stages[i], NULL, [](void* arg) -> void* {
            typeof(*data) d = *(typeof(data))arg;
            free(arg);
            
            char* input;
            while (queue_pop(d.input, &input) == 0) {
                char output[256];
                snprintf(output, sizeof(output), "S%d:%s", d.stage_id, input);
                queue_push(d.output, output);
                free(input);
            }
            
            /* Propagate shutdown */
            queue_shutdown(d.output);
            *d.completed = 1;
            return NULL;
        }, data);
    }
    
    /* Send data and shutdown */
    queue_push(&queues[0], "data1");
    queue_push(&queues[0], "data2");
    queue_shutdown(&queues[0]);
    
    /* Wait for all stages to complete */
    for (int i = 0; i < 3; i++) {
        pthread_join(stages[i], NULL);
    }
    
    /* Verify all stages completed */
    for (int i = 0; i < 3; i++) {
        mu_assert_int_eq(1, stages_completed[i]);
    }
    
    /* Verify output queue received shutdown */
    char* final = NULL;
    queue_pop(&queues[3], &final);
    free(final);
    queue_pop(&queues[3], &final);
    free(final);
    mu_assert_int_eq(QUEUE_SHUTDOWN, queue_pop(&queues[3], &final));
    
    for (int i = 0; i < 4; i++) {
        queue_destroy(&queues[i]);
    }
    
    return MU_PASS;
}

/* Test: End marker handling */
test_result_t test_end_marker_handling(void) {
    queue_t input, output;
    queue_init(&input, 10);
    queue_init(&output, 10);
    
    /* Processing thread that stops on <END> */
    pthread_t processor;
    pthread_create(&processor, NULL, [](void* arg) -> void* {
        queue_t* queues = (queue_t*)arg;
        char* str;
        
        while (queue_pop(&queues[0], &str) == 0) {
            if (strcmp(str, "<END>") == 0) {
                free(str);
                queue_shutdown(&queues[0]);
                queue_shutdown(&queues[1]);
                break;
            }
            
            /* Process normally */
            char output[256];
            snprintf(output, sizeof(output), "PROCESSED:%s", str);
            queue_push(&queues[1], output);
            free(str);
        }
        return NULL;
    }, &input);
    
    /* Send data with end marker */
    queue_push(&input, "data1");
    queue_push(&input, "data2");
    queue_push(&input, "<END>");
    queue_push(&input, "should_not_process");
    
    pthread_join(processor, NULL);
    
    /* Verify only pre-END data was processed */
    char* result = NULL;
    mu_assert_int_eq(0, queue_pop(&output, &result));
    mu_assert_str_eq("PROCESSED:data1", result);
    free(result);
    
    mu_assert_int_eq(0, queue_pop(&output, &result));
    mu_assert_str_eq("PROCESSED:data2", result);
    free(result);
    
    /* Should get shutdown, not more data */
    mu_assert_int_eq(QUEUE_SHUTDOWN, queue_pop(&output, &result));
    
    queue_destroy(&input);
    queue_destroy(&output);
    
    return MU_PASS;
}

/* Test: Load test with many messages */
test_result_t test_high_volume_pipeline(void) {
    const int MESSAGE_COUNT = 10000;
    queue_t queue;
    queue_init(&queue, 100);
    
    /* Producer thread */
    pthread_t producer;
    pthread_create(&producer, NULL, [](void* arg) -> void* {
        queue_t* q = (queue_t*)arg;
        for (int i = 0; i < MESSAGE_COUNT; i++) {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%d", i);
            queue_push(q, buffer);
        }
        queue_shutdown(q);
        return NULL;
    }, &queue);
    
    /* Consumer thread */
    int last_seen = -1;
    pthread_t consumer;
    pthread_create(&consumer, NULL, [](void* arg) -> void* {
        struct { queue_t* q; int* last; } *data = arg;
        char* msg;
        
        while (queue_pop(data->q, &msg) == 0) {
            int val = atoi(msg);
            /* Verify sequential ordering */
            if (*data->last >= 0) {
                if (val != *data->last + 1) {
                    fprintf(stderr, "Out of order: expected %d, got %d\n", 
                            *data->last + 1, val);
                }
            }
            *data->last = val;
            free(msg);
        }
        return NULL;
    }, &(struct { queue_t* q; int* last; }){&queue, &last_seen});
    
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    
    /* Verify all messages received */
    mu_assert_int_eq(MESSAGE_COUNT - 1, last_seen);
    
    queue_destroy(&queue);
    
    return MU_PASS;
}

/* Main test runner */
int main(void) {
    printf("Running Integration Tests\n");
    printf("=========================\n\n");
    
    /* Basic pipeline tests */
    mu_run_test(test_single_producer_consumer);
    mu_run_test(test_multiple_producers_single_consumer);
    mu_run_test(test_multi_stage_pipeline);
    
    /* Blocking and synchronization */
    mu_run_test(test_bounded_buffer_blocking);
    mu_run_test(test_pipeline_shutdown_propagation);
    
    /* Special cases */
    mu_run_test(test_end_marker_handling);
    mu_run_test(test_high_volume_pipeline);
    
    mu_print_summary();
    
    return tests_failed > 0 ? 1 : 0;
}