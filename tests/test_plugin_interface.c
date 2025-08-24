/**
 * Unit tests for the plugin interface
 * Tests plugin lifecycle, string processing, and thread safety
 */

#include "minunit.h"
#include "../src/plugin_common.h"
#include "../src/queue.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Test counters */
int tests_run = 0;
int tests_failed = 0;
int assertions_run = 0;
int assertions_failed = 0;

/* Test plugin implementation for testing */
typedef struct {
    const char* name;
    queue_t* input_queue;
    queue_t* output_queue;
    int stop_requested;
    pthread_t thread;
} test_plugin_ctx_t;

/* Test plugin process function */
void* test_plugin_process(void* arg) {
    test_plugin_ctx_t* ctx = (test_plugin_ctx_t*)arg;
    
    while (!ctx->stop_requested) {
        char* input = NULL;
        int ret = queue_pop(ctx->input_queue, &input);
        
        if (ret == QUEUE_SHUTDOWN || ctx->stop_requested) {
            if (input) free(input);
            break;
        }
        
        if (ret == 0 && input) {
            /* Simple transformation: add prefix */
            char transformed[256];
            snprintf(transformed, sizeof(transformed), "TEST:%s", input);
            
            queue_push(ctx->output_queue, transformed);
            free(input);
        }
    }
    
    return NULL;
}

/* Test plugin factory function */
int test_plugin_create(plugin_ctx_t** ctx, const char* config, 
                      queue_t* input, queue_t* output) {
    test_plugin_ctx_t* plugin = calloc(1, sizeof(test_plugin_ctx_t));
    if (!plugin) return -1;
    
    plugin->name = "test_plugin";
    plugin->input_queue = input;
    plugin->output_queue = output;
    plugin->stop_requested = 0;
    
    /* Start processing thread */
    if (pthread_create(&plugin->thread, NULL, test_plugin_process, plugin) != 0) {
        free(plugin);
        return -1;
    }
    
    *ctx = (plugin_ctx_t*)plugin;
    return 0;
}

/* Test plugin destroy function */
void test_plugin_destroy(plugin_ctx_t* ctx) {
    if (!ctx) return;
    
    test_plugin_ctx_t* plugin = (test_plugin_ctx_t*)ctx;
    plugin->stop_requested = 1;
    
    /* Signal queues to unblock thread */
    if (plugin->input_queue) {
        queue_shutdown(plugin->input_queue);
    }
    
    pthread_join(plugin->thread, NULL);
    free(plugin);
}

/* Test plugin name function */
const char* test_plugin_name(plugin_ctx_t* ctx) {
    if (!ctx) return NULL;
    test_plugin_ctx_t* plugin = (test_plugin_ctx_t*)ctx;
    return plugin->name;
}

/* Test plugin stop function */
void test_plugin_request_stop(plugin_ctx_t* ctx) {
    if (!ctx) return;
    test_plugin_ctx_t* plugin = (test_plugin_ctx_t*)ctx;
    plugin->stop_requested = 1;
}

/* Test: Plugin create and destroy */
test_result_t test_plugin_create_destroy(void) {
    queue_t input, output;
    queue_init(&input, 10);
    queue_init(&output, 10);
    
    plugin_ctx_t* plugin = NULL;
    int ret = test_plugin_create(&plugin, NULL, &input, &output);
    mu_assert_int_eq(0, ret);
    mu_assert_ptr_not_null(plugin);
    
    test_plugin_destroy(plugin);
    
    queue_destroy(&input);
    queue_destroy(&output);
    
    return MU_PASS;
}

/* Test: Plugin name retrieval */
test_result_t test_plugin_name_retrieval(void) {
    queue_t input, output;
    queue_init(&input, 10);
    queue_init(&output, 10);
    
    plugin_ctx_t* plugin = NULL;
    test_plugin_create(&plugin, NULL, &input, &output);
    
    const char* name = test_plugin_name(plugin);
    mu_assert_ptr_not_null(name);
    mu_assert_str_eq("test_plugin", name);
    
    test_plugin_destroy(plugin);
    queue_destroy(&input);
    queue_destroy(&output);
    
    return MU_PASS;
}

/* Test: Plugin processes strings */
test_result_t test_plugin_process_string(void) {
    queue_t input, output;
    queue_init(&input, 10);
    queue_init(&output, 10);
    
    plugin_ctx_t* plugin = NULL;
    test_plugin_create(&plugin, NULL, &input, &output);
    
    /* Push test string */
    queue_push(&input, "hello");
    
    /* Wait for processing */
    usleep(100000); /* 100ms */
    
    /* Get result */
    char* result = NULL;
    int ret = queue_pop(&output, &result);
    mu_assert_int_eq(0, ret);
    mu_assert_ptr_not_null(result);
    mu_assert_str_eq("TEST:hello", result);
    
    free(result);
    test_plugin_destroy(plugin);
    queue_destroy(&input);
    queue_destroy(&output);
    
    return MU_PASS;
}

/* Test: Plugin handles multiple strings */
test_result_t test_plugin_process_multiple(void) {
    queue_t input, output;
    queue_init(&input, 10);
    queue_init(&output, 10);
    
    plugin_ctx_t* plugin = NULL;
    test_plugin_create(&plugin, NULL, &input, &output);
    
    /* Push multiple strings */
    const char* inputs[] = {"one", "two", "three", "four", "five"};
    int count = sizeof(inputs) / sizeof(inputs[0]);
    
    for (int i = 0; i < count; i++) {
        queue_push(&input, inputs[i]);
    }
    
    /* Wait for processing */
    usleep(200000); /* 200ms */
    
    /* Verify all processed */
    for (int i = 0; i < count; i++) {
        char* result = NULL;
        int ret = queue_pop(&output, &result);
        mu_assert_int_eq(0, ret);
        mu_assert_ptr_not_null(result);
        
        char expected[256];
        snprintf(expected, sizeof(expected), "TEST:%s", inputs[i]);
        mu_assert_str_eq(expected, result);
        
        free(result);
    }
    
    test_plugin_destroy(plugin);
    queue_destroy(&input);
    queue_destroy(&output);
    
    return MU_PASS;
}

/* Test: Plugin stops on request */
test_result_t test_plugin_request_stop_behavior(void) {
    queue_t input, output;
    queue_init(&input, 10);
    queue_init(&output, 10);
    
    plugin_ctx_t* plugin = NULL;
    test_plugin_create(&plugin, NULL, &input, &output);
    
    /* Request stop */
    test_plugin_request_stop(plugin);
    
    /* Push a string after stop request */
    queue_push(&input, "should_not_process");
    
    /* Give time for any processing */
    usleep(100000);
    
    /* Output should be empty since plugin stopped */
    char* result = NULL;
    queue_shutdown(&output);
    int ret = queue_pop(&output, &result);
    mu_assert_int_eq(QUEUE_SHUTDOWN, ret);
    
    test_plugin_destroy(plugin);
    queue_destroy(&input);
    queue_destroy(&output);
    
    return MU_PASS;
}

/* Test: Plugin handles queue shutdown */
test_result_t test_plugin_queue_shutdown(void) {
    queue_t input, output;
    queue_init(&input, 10);
    queue_init(&output, 10);
    
    plugin_ctx_t* plugin = NULL;
    test_plugin_create(&plugin, NULL, &input, &output);
    
    /* Add some items */
    queue_push(&input, "item1");
    queue_push(&input, "item2");
    
    /* Shutdown input queue */
    queue_shutdown(&input);
    
    /* Wait for plugin to process and stop */
    usleep(200000);
    
    /* Should have processed existing items */
    char* result = NULL;
    mu_assert_int_eq(0, queue_pop(&output, &result));
    mu_assert_str_eq("TEST:item1", result);
    free(result);
    
    mu_assert_int_eq(0, queue_pop(&output, &result));
    mu_assert_str_eq("TEST:item2", result);
    free(result);
    
    test_plugin_destroy(plugin);
    queue_destroy(&input);
    queue_destroy(&output);
    
    return MU_PASS;
}

/* Thread data for concurrent plugin test */
typedef struct {
    plugin_ctx_t* plugin;
    queue_t* input;
    queue_t* output;
    int thread_id;
    int count;
} plugin_thread_data_t;

/* Producer thread for plugin test */
void* plugin_producer_thread(void* arg) {
    plugin_thread_data_t* data = (plugin_thread_data_t*)arg;
    
    for (int i = 0; i < data->count; i++) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "thread%d_msg%d", data->thread_id, i);
        queue_push(data->input, buffer);
    }
    
    return NULL;
}

/* Test: Plugin handles concurrent input */
test_result_t test_plugin_concurrent_processing(void) {
    queue_t input, output;
    queue_init(&input, 20);
    queue_init(&output, 20);
    
    plugin_ctx_t* plugin = NULL;
    test_plugin_create(&plugin, NULL, &input, &output);
    
    /* Create multiple producer threads */
    pthread_t producers[3];
    plugin_thread_data_t thread_data[3];
    
    for (int i = 0; i < 3; i++) {
        thread_data[i] = (plugin_thread_data_t){
            plugin, &input, &output, i, 5
        };
        pthread_create(&producers[i], NULL, plugin_producer_thread, &thread_data[i]);
    }
    
    /* Wait for producers */
    for (int i = 0; i < 3; i++) {
        pthread_join(producers[i], NULL);
    }
    
    /* Wait for processing */
    usleep(300000);
    
    /* Verify all items processed */
    int processed = 0;
    char* result = NULL;
    while (queue_pop(&output, &result) == 0) {
        mu_assert_ptr_not_null(result);
        /* Should have TEST: prefix */
        mu_assert("Should have TEST: prefix", strncmp(result, "TEST:", 5) == 0);
        free(result);
        processed++;
    }
    
    mu_assert_int_eq(15, processed); /* 3 threads * 5 messages */
    
    test_plugin_destroy(plugin);
    queue_destroy(&input);
    queue_destroy(&output);
    
    return MU_PASS;
}

/* Test: Multiple plugins in pipeline */
test_result_t test_plugin_pipeline(void) {
    /* Create queues for pipeline */
    queue_t queue1, queue2, queue3;
    queue_init(&queue1, 10);
    queue_init(&queue2, 10);
    queue_init(&queue3, 10);
    
    /* Create two plugins in sequence */
    plugin_ctx_t *plugin1 = NULL, *plugin2 = NULL;
    test_plugin_create(&plugin1, NULL, &queue1, &queue2);
    test_plugin_create(&plugin2, NULL, &queue2, &queue3);
    
    /* Push input */
    queue_push(&queue1, "input");
    
    /* Wait for pipeline processing */
    usleep(200000);
    
    /* Get final result */
    char* result = NULL;
    int ret = queue_pop(&queue3, &result);
    mu_assert_int_eq(0, ret);
    mu_assert_ptr_not_null(result);
    /* Should be transformed twice: "TEST:TEST:input" */
    mu_assert_str_eq("TEST:TEST:input", result);
    
    free(result);
    test_plugin_destroy(plugin1);
    test_plugin_destroy(plugin2);
    queue_destroy(&queue1);
    queue_destroy(&queue2);
    queue_destroy(&queue3);
    
    return MU_PASS;
}

/* Main test runner */
int main(void) {
    printf("Running Plugin Interface Unit Tests\n");
    printf("====================================\n\n");
    
    /* Basic lifecycle tests */
    mu_run_test(test_plugin_create_destroy);
    mu_run_test(test_plugin_name_retrieval);
    
    /* Processing tests */
    mu_run_test(test_plugin_process_string);
    mu_run_test(test_plugin_process_multiple);
    
    /* Shutdown tests */
    mu_run_test(test_plugin_request_stop_behavior);
    mu_run_test(test_plugin_queue_shutdown);
    
    /* Concurrency tests */
    mu_run_test(test_plugin_concurrent_processing);
    mu_run_test(test_plugin_pipeline);
    
    mu_print_summary();
    
    return tests_failed > 0 ? 1 : 0;
}