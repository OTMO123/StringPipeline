/**
 * @file plugin_common.h
 * @brief Common interface for pipeline plugins
 * 
 * Defines the standard interface that all plugins must implement.
 * Plugins are loaded dynamically via dlopen/dlsym and run in separate threads.
 * Each plugin processes strings from an input queue and writes to an output queue.
 * 
 * Thread Safety: Plugins must be thread-safe if accessed from multiple threads
 * Memory Management: Plugins own their context memory, queues are owned by pipeline
 */

#ifndef PLUGIN_COMMON_H
#define PLUGIN_COMMON_H

#include "queue.h"

/* Forward declaration of plugin context (opaque to users) */
typedef struct plugin_ctx plugin_ctx_t;

/* Plugin function signatures */

/**
 * @brief Create and initialize a plugin instance
 * 
 * @param ctx Output parameter for plugin context pointer
 * @param config Configuration string for the plugin (may be NULL)
 * @param input Input queue for receiving strings
 * @param output Output queue for sending processed strings
 * @return 0 on success, -1 on error
 * 
 * @note This function should start the plugin's processing thread
 * @note The plugin does not own the queues, just references them
 * @note Must be exported with exact name "plugin_create" for dlsym
 */
typedef int (*plugin_create_fn)(plugin_ctx_t** ctx, const char* config,
                                queue_t* input, queue_t* output);

/**
 * @brief Request the plugin to stop processing
 * 
 * @param ctx Plugin context
 * 
 * @note This should signal the plugin to stop but not block
 * @note The plugin thread should exit cleanly after this call
 * @note Must be exported with exact name "plugin_request_stop" for dlsym
 */
typedef void (*plugin_request_stop_fn)(plugin_ctx_t* ctx);

/**
 * @brief Destroy a plugin instance and free resources
 * 
 * @param ctx Plugin context to destroy
 * 
 * @note This should block until the plugin thread has exited
 * @note All plugin resources should be freed
 * @note Must be exported with exact name "plugin_destroy" for dlsym
 */
typedef void (*plugin_destroy_fn)(plugin_ctx_t* ctx);

/**
 * @brief Get the plugin's name
 * 
 * @param ctx Plugin context
 * @return Static string with plugin name, or NULL on error
 * 
 * @note The returned string should not be freed by the caller
 * @note Must be exported with exact name "plugin_name" for dlsym
 */
typedef const char* (*plugin_name_fn)(plugin_ctx_t* ctx);

/**
 * @brief Get plugin version
 * 
 * @return Static version string
 * 
 * @note Optional: plugins may export "plugin_version"
 */
typedef const char* (*plugin_version_fn)(void);

/**
 * @brief Get plugin description
 * 
 * @return Static description string
 * 
 * @note Optional: plugins may export "plugin_description"
 */
typedef const char* (*plugin_description_fn)(void);

/* Plugin interface structure for convenient access */
typedef struct {
    plugin_create_fn create;
    plugin_destroy_fn destroy;
    plugin_request_stop_fn request_stop;
    plugin_name_fn name;
    plugin_version_fn version;         /* Optional */
    plugin_description_fn description; /* Optional */
} plugin_interface_t;

/* Standard plugin export macros for visibility */
#ifdef __GNUC__
#define PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define PLUGIN_EXPORT
#endif

/* Error codes */
#define PLUGIN_SUCCESS       0
#define PLUGIN_ERROR        -1
#define PLUGIN_INVALID_ARG  -2
#define PLUGIN_NO_MEMORY    -3

/* Helper macro for plugin implementation */
#define PLUGIN_IMPL_STANDARD_EXPORTS(name_str, version_str, desc_str) \
    PLUGIN_EXPORT const char* plugin_version(void) { \
        return version_str; \
    } \
    PLUGIN_EXPORT const char* plugin_description(void) { \
        return desc_str; \
    }

#endif /* PLUGIN_COMMON_H */