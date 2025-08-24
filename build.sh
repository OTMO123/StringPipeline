#!/bin/bash

# Build script for String Processing Pipeline
# Compiles all components with production flags

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building String Processing Pipeline...${NC}"

# Compiler settings
CC=${CC:-gcc}
CFLAGS="-std=gnu11 -Wall -Wextra -Werror -O2 -g -pthread -fPIC"
LDFLAGS="-pthread -ldl"

# Directories
SRC_DIR="src"
TEST_DIR="tests"
PLUGIN_DIR="plugins"
BUILD_DIR="build"
LIB_DIR="$BUILD_DIR/lib"
BIN_DIR="$BUILD_DIR/bin"

# Create build directories
echo "Creating build directories..."
mkdir -p "$BUILD_DIR" "$LIB_DIR" "$BIN_DIR" "$LIB_DIR/plugins"

# Build core library components
echo -e "${YELLOW}Building core library...${NC}"
$CC $CFLAGS -c "$SRC_DIR/queue.c" -o "$BUILD_DIR/queue.o"
$CC $CFLAGS -c "$SRC_DIR/monitor.c" -o "$BUILD_DIR/monitor.o"

# Create static library
ar rcs "$LIB_DIR/libpipeline_core.a" "$BUILD_DIR/queue.o" "$BUILD_DIR/monitor.o"
echo -e "${GREEN}✓ Core library built${NC}"

# Build plugins
echo -e "${YELLOW}Building plugins...${NC}"

# Create plugin source files if they don't exist
for plugin in upper lower reverse trim prefix suffix; do
    cat > "$PLUGIN_DIR/${plugin}.c" << EOF
/**
 * Plugin: ${plugin}
 */

#include "../src/plugin_common.h"
#include "../src/queue.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

struct plugin_ctx {
    const char* name;
    queue_t* input;
    queue_t* output;
    pthread_t thread;
    int stop_requested;
};

static char* transform_${plugin}(const char* input) {
    char* output = strdup(input);
    if (!output) return NULL;
    
EOF

    case $plugin in
        upper)
            cat >> "$PLUGIN_DIR/${plugin}.c" << 'EOF'
    for (char* p = output; *p; p++) {
        *p = toupper((unsigned char)*p);
    }
EOF
            ;;
        lower)
            cat >> "$PLUGIN_DIR/${plugin}.c" << 'EOF'
    for (char* p = output; *p; p++) {
        *p = tolower((unsigned char)*p);
    }
EOF
            ;;
        reverse)
            cat >> "$PLUGIN_DIR/${plugin}.c" << 'EOF'
    size_t len = strlen(output);
    for (size_t i = 0; i < len / 2; i++) {
        char tmp = output[i];
        output[i] = output[len - 1 - i];
        output[len - 1 - i] = tmp;
    }
EOF
            ;;
        trim)
            cat >> "$PLUGIN_DIR/${plugin}.c" << 'EOF'
    char* start = output;
    char* end = output + strlen(output) - 1;
    
    while (*start && isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    
    if (start != output) {
        memmove(output, start, strlen(start) + 1);
    }
EOF
            ;;
        prefix)
            cat >> "$PLUGIN_DIR/${plugin}.c" << 'EOF'
    char* prefixed = malloc(strlen(output) + 10);
    if (prefixed) {
        sprintf(prefixed, "PREFIX:%s", output);
        free(output);
        output = prefixed;
    }
EOF
            ;;
        suffix)
            cat >> "$PLUGIN_DIR/${plugin}.c" << 'EOF'
    char* suffixed = malloc(strlen(output) + 10);
    if (suffixed) {
        sprintf(suffixed, "%s:SUFFIX", output);
        free(output);
        output = suffixed;
    }
EOF
            ;;
    esac

    cat >> "$PLUGIN_DIR/${plugin}.c" << EOF
    return output;
}

static void* process_thread(void* arg) {
    struct plugin_ctx* ctx = (struct plugin_ctx*)arg;
    char* str;
    
    while (!ctx->stop_requested) {
        int ret = queue_pop(ctx->input, &str);
        if (ret == QUEUE_SHUTDOWN || ctx->stop_requested) {
            if (str) free(str);
            // Propagate shutdown to output queue
            queue_shutdown(ctx->output);
            break;
        }
        if (ret == 0 && str) {
            char* transformed = transform_${plugin}(str);
            if (transformed) {
                queue_push(ctx->output, transformed);
                free(transformed);
            }
            free(str);
        }
    }
    return NULL;
}

PLUGIN_EXPORT int plugin_create(plugin_ctx_t** ctx, const char* config,
                                queue_t* input, queue_t* output) {
    (void)config;
    struct plugin_ctx* p = calloc(1, sizeof(struct plugin_ctx));
    if (!p) return -1;
    
    p->name = "${plugin}";
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

PLUGIN_EXPORT const char* plugin_version(void) {
    return "1.0.0";
}

PLUGIN_EXPORT const char* plugin_description(void) {
    return "${plugin} transformation plugin";
}
EOF

    # Compile plugin
    $CC $CFLAGS -shared "$PLUGIN_DIR/${plugin}.c" -o "$LIB_DIR/plugins/${plugin}.so" -L"$LIB_DIR" -lpipeline_core
    echo -e "${GREEN}✓ ${plugin} plugin built${NC}"
done

# Build unit tests
echo -e "${YELLOW}Building unit tests...${NC}"

# Queue tests
$CC $CFLAGS "$TEST_DIR/test_queue.c" "$SRC_DIR/queue.c" \
    -o "$BIN_DIR/test_queue" $LDFLAGS
echo -e "${GREEN}✓ Queue tests built${NC}"

# Monitor tests  
$CC $CFLAGS "$TEST_DIR/test_monitor_fixed.c" "$SRC_DIR/monitor.c" \
    -o "$BIN_DIR/test_monitor" $LDFLAGS
echo -e "${GREEN}✓ Monitor tests built${NC}"

# Build main program
echo -e "${YELLOW}Building main program...${NC}"

cat > "$SRC_DIR/main.c" << 'EOF'
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
    
    // Input thread has already shut down the first queue
    // Plugins will propagate shutdown through the pipeline
    
    // Wait for output thread to finish processing all data
    pthread_join(output_tid, NULL);
    
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
    
    // Cleanup
    for (int i = 0; i <= plugin_count; i++) {
        queue_destroy(&queues[i]);
    }
    
    free(plugins);
    free(queues);
    
    return 0;
}
EOF

$CC $CFLAGS "$SRC_DIR/main.c" "$SRC_DIR/queue.c" -o "$BIN_DIR/pipeline" $LDFLAGS
echo -e "${GREEN}✓ Main program built${NC}"

# Build test runner
echo -e "${YELLOW}Building test runner...${NC}"

cat > "$TEST_DIR/test_runner.c" << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int run_test(const char* name, const char* path) {
    printf("\n========================================\n");
    printf("Running: %s\n", name);
    printf("========================================\n");
    
    int pid = fork();
    if (pid == 0) {
        execl(path, path, NULL);
        perror("execl failed");
        exit(1);
    }
    
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("✅ %s PASSED\n", name);
        return 0;
    } else {
        printf("❌ %s FAILED\n", name);
        return 1;
    }
}

int main(void) {
    int failures = 0;
    
    failures += run_test("Queue Tests", "./build/bin/test_queue");
    failures += run_test("Monitor Tests", "./build/bin/test_monitor");
    
    printf("\n========================================\n");
    printf("TEST SUMMARY\n");
    printf("========================================\n");
    if (failures == 0) {
        printf("✅ ALL TESTS PASSED!\n");
    } else {
        printf("❌ %d TEST SUITE(S) FAILED!\n", failures);
    }
    
    return failures > 0 ? 1 : 0;
}
EOF

$CC $CFLAGS "$TEST_DIR/test_runner.c" -o "$BIN_DIR/test_runner"
echo -e "${GREEN}✓ Test runner built${NC}"

# Success message
echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "Binaries in: $BIN_DIR"
echo -e "Libraries in: $LIB_DIR"
echo -e "Plugins in: $LIB_DIR/plugins"
echo -e "\nRun tests with: ./test.sh"
echo -e "Run pipeline: ./build/bin/pipeline plugin1.so plugin2.so ..."