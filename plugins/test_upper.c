/**
 * Test plugin: Convert strings to uppercase
 */

#include "../src/plugin_common.h"
#include "../src/queue.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct plugin_ctx {
    const char* name;
    queue_t* input;
    queue_t* output;
    pthread_t thread;
    int stop_requested;
};

static void* process_thread(void* arg) {
    struct plugin_ctx* ctx = (struct plugin_ctx*)arg;
    char* str;
    
    while (!ctx->stop_requested) {
        int ret = queue_pop(ctx->input, &str);
        if (ret == QUEUE_SHUTDOWN || ctx->stop_requested) {
            if (str) free(str);
            break;
        }
        if (ret == 0 && str) {
            for (char* p = str; *p; p++) {
                *p = toupper((unsigned char)*p);
            }
            queue_push(ctx->output, str);
            free(str);
        }
    }
    return NULL;
}

PLUGIN_EXPORT int plugin_create(plugin_ctx_t** ctx, const char* config,
                                queue_t* input, queue_t* output) {
    (void)config; /* Unused */
    
    struct plugin_ctx* p = calloc(1, sizeof(struct plugin_ctx));
    if (!p) return -1;
    
    p->name = "test_upper";
    p->input = input;
    p->output = output;
    p->stop_requested = 0;
    
    if (pthread_create(&p->thread, NULL, process_thread, p) != 0) {
        free(p);
        return -1;
    }
    
    *ctx = p;
    return 0;
}

PLUGIN_EXPORT void plugin_request_stop(plugin_ctx_t* ctx) {
    if (ctx) {
        struct plugin_ctx* p = (struct plugin_ctx*)ctx;
        p->stop_requested = 1;
    }
}

PLUGIN_EXPORT void plugin_destroy(plugin_ctx_t* ctx) {
    if (!ctx) return;
    struct plugin_ctx* p = (struct plugin_ctx*)ctx;
    p->stop_requested = 1;
    if (p->input) queue_shutdown(p->input);
    pthread_join(p->thread, NULL);
    free(p);
}

PLUGIN_EXPORT const char* plugin_name(plugin_ctx_t* ctx) {
    struct plugin_ctx* p = (struct plugin_ctx*)ctx;
    return p ? p->name : NULL;
}