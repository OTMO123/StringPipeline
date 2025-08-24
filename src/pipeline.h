/**
 * @file pipeline.h
 * @brief Pipeline management for string processing
 * 
 * Manages the complete pipeline including:
 * - Loading plugins dynamically
 * - Creating and connecting queues between stages
 * - Starting and stopping plugin threads
 * - Handling shutdown and cleanup
 * 
 * Thread Safety: Pipeline operations are thread-safe
 * Memory Management: Pipeline owns all resources and ensures cleanup
 */

#ifndef PIPELINE_H
#define PIPELINE_H

#include "queue.h"
#include "plugin_common.h"
#include <pthread.h>

/* Maximum number of pipeline stages */
#define MAX_PIPELINE_STAGES 16

/* Pipeline stage structure */
typedef struct {
    char* plugin_path;              /* Path to .so file */
    void* handle;                   /* dlopen handle */
    plugin_interface_t interface;   /* Plugin function pointers */
    plugin_ctx_t* context;          /* Plugin instance context */
    queue_t* input_queue;           /* Input queue (not owned) */
    queue_t* output_queue;          /* Output queue (not owned) */
} pipeline_stage_t;

/* Pipeline structure */
typedef struct {
    pipeline_stage_t* stages;       /* Array of pipeline stages */
    int stage_count;               /* Number of stages */
    queue_t* queues;               /* Array of queues (stage_count + 1) */
    int running;                   /* Pipeline is running */
    pthread_t input_thread;        /* Thread reading input */
    pthread_t output_thread;       /* Thread writing output */
} pipeline_t;

/**
 * @brief Initialize a pipeline
 * 
 * @param pipeline Pointer to pipeline structure
 * @param plugin_paths Array of paths to plugin .so files
 * @param plugin_count Number of plugins
 * @param queue_capacity Capacity for each queue
 * @return 0 on success, -1 on error
 * 
 * @note Creates plugin_count + 1 queues to connect stages
 */
int pipeline_init(pipeline_t* pipeline, const char** plugin_paths, 
                  int plugin_count, size_t queue_capacity);

/**
 * @brief Start the pipeline
 * 
 * @param pipeline Pointer to initialized pipeline
 * @return 0 on success, -1 on error
 * 
 * @note Starts all plugin threads and I/O threads
 * @note Pipeline processes until "<END>" is received
 */
int pipeline_start(pipeline_t* pipeline);

/**
 * @brief Stop the pipeline
 * 
 * @param pipeline Pointer to running pipeline
 * @return 0 on success, -1 on error
 * 
 * @note Initiates graceful shutdown of all stages
 * @note Waits for all threads to complete
 */
int pipeline_stop(pipeline_t* pipeline);

/**
 * @brief Destroy a pipeline and free resources
 * 
 * @param pipeline Pointer to pipeline
 * 
 * @note Stops pipeline if running
 * @note Unloads all plugins and frees all resources
 */
void pipeline_destroy(pipeline_t* pipeline);

/**
 * @brief Send a string into the pipeline
 * 
 * @param pipeline Pointer to running pipeline
 * @param input String to process
 * @return 0 on success, -1 on error
 * 
 * @note Pushes string to first queue
 * @note Returns immediately (non-blocking if queue has space)
 */
int pipeline_send(pipeline_t* pipeline, const char* input);

/**
 * @brief Receive a processed string from the pipeline
 * 
 * @param pipeline Pointer to running pipeline
 * @param output Pointer to store allocated string (caller must free)
 * @return 0 on success, QUEUE_SHUTDOWN on shutdown, -1 on error
 * 
 * @note Pops from last queue
 * @note Blocks if no output available
 */
int pipeline_receive(pipeline_t* pipeline, char** output);

/**
 * @brief Load a plugin from a .so file
 * 
 * @param stage Pointer to stage structure to populate
 * @param plugin_path Path to plugin .so file
 * @return 0 on success, -1 on error
 * 
 * @note Uses dlopen/dlsym to load plugin functions
 * @note Validates that required functions are present
 */
int pipeline_load_plugin(pipeline_stage_t* stage, const char* plugin_path);

/**
 * @brief Unload a plugin
 * 
 * @param stage Pointer to stage with loaded plugin
 * 
 * @note Destroys plugin instance if created
 * @note Calls dlclose on plugin handle
 */
void pipeline_unload_plugin(pipeline_stage_t* stage);

#endif /* PIPELINE_H */