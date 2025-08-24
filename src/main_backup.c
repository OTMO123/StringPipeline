/**
 * Main program for string processing pipeline
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include "queue.h"
#include "plugin_common.h"

#define MAX_LINE_LENGTH 1024
#define QUEUE_CAPACITY 100

typedef struct {
    void* handle;
    plugin_interface_t interface;
    plugin_ctx_t* context;
} plugin_t;

static void* input_thread(void* arg) {
    queue_t* input_queue = (queue_t*)arg;
    char line[MAX_LINE_LENGTH];
    
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        if (strcmp(line, "<END>") == 0) {
            queue_shutdown(input_queue);
            break;
        }
        
        queue_push(input_queue, line);
    }
    
    return NULL;
}

static void* output_thread(void* arg) {
    queue_t* output_queue = (queue_t*)arg;
    char* str;
    
    while (queue_pop(output_queue, &str) == 0) {
        printf("%s\n", str);
        fflush(stdout);
        free(str);
    }
    
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s plugin1.so [plugin2.so ...]\n", argv[0]);
        return 1;
    }
    
    int plugin_count = argc - 1;
    plugin_t* plugins = calloc(plugin_count, sizeof(plugin_t));
    queue_t* queues = calloc(plugin_count + 1, sizeof(queue_t));
    
    // Initialize queues
    for (int i = 0; i <= plugin_count; i++) {
        if (queue_init(&queues[i], QUEUE_CAPACITY) != 0) {
            fprintf(stderr, "Failed to initialize queue %d\n", i);
            return 1;
        }
    }
    
    // Load plugins
    for (int i = 0; i < plugin_count; i++) {
        plugins[i].handle = dlopen(argv[i + 1], RTLD_LAZY);
        if (!plugins[i].handle) {
            fprintf(stderr, "Failed to load plugin %s: %s\n", argv[i + 1], dlerror());
            return 1;
        }
        
        plugins[i].interface.create = dlsym(plugins[i].handle, "plugin_create");
        plugins[i].interface.destroy = dlsym(plugins[i].handle, "plugin_destroy");
        plugins[i].interface.request_stop = dlsym(plugins[i].handle, "plugin_request_stop");
        plugins[i].interface.name = dlsym(plugins[i].handle, "plugin_name");
        
        if (!plugins[i].interface.create || !plugins[i].interface.destroy) {
            fprintf(stderr, "Plugin %s missing required functions\n", argv[i + 1]);
            return 1;
        }
        
        if (plugins[i].interface.create(&plugins[i].context, NULL, 
                                        &queues[i], &queues[i + 1]) != 0) {
            fprintf(stderr, "Failed to create plugin %s\n", argv[i + 1]);
            return 1;
        }
        
        if (plugins[i].interface.name) {
            printf("Loaded plugin: %s\n", plugins[i].interface.name(plugins[i].context));
        }
    }
    
    // Start I/O threads
    pthread_t input_tid, output_tid;
    pthread_create(&input_tid, NULL, input_thread, &queues[0]);
    pthread_create(&output_tid, NULL, output_thread, &queues[plugin_count]);
    
    // Wait for input thread to finish
    pthread_join(input_tid, NULL);
    
    // Shutdown pipeline
    for (int i = 0; i <= plugin_count; i++) {
        queue_shutdown(&queues[i]);
    }
    
    // Stop plugins
    for (int i = 0; i < plugin_count; i++) {
        if (plugins[i].interface.request_stop) {
            plugins[i].interface.request_stop(plugins[i].context);
        }
    }
    
    // Destroy plugins
    for (int i = 0; i < plugin_count; i++) {
        if (plugins[i].interface.destroy) {
            plugins[i].interface.destroy(plugins[i].context);
        }
        if (plugins[i].handle) {
            dlclose(plugins[i].handle);
        }
    }
    
    // Wait for output thread
    pthread_join(output_tid, NULL);
    
    // Cleanup
    for (int i = 0; i <= plugin_count; i++) {
        queue_destroy(&queues[i]);
    }
    
    free(plugins);
    free(queues);
    
    return 0;
}
