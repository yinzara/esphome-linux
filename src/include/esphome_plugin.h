/**
 * @file esphome_plugin.h
 * @brief ESPHome Plugin API for extensibility
 *
 * This API allows developers to extend the ESPHome proxy with additional
 * functionality like MediaPlayer, VoiceAssistant, Climate, etc.
 *
 * Plugins can:
 * - Register custom message handlers for ESPHome Native API messages
 * - Send custom messages to connected clients
 * - Access device configuration
 * - Register initialization and cleanup hooks
 *
 * Example plugin structure:
 * @code
 * static int my_plugin_init(esphome_plugin_context_t *ctx) {
 *     // Initialize your plugin
 *     return 0;
 * }
 *
 * static void my_plugin_cleanup(esphome_plugin_context_t *ctx) {
 *     // Cleanup resources
 * }
 *
 * static int my_plugin_handle_message(esphome_plugin_context_t *ctx,
 *                                      uint32_t msg_type,
 *                                      const uint8_t *data,
 *                                      size_t len) {
 *     // Handle custom message types
 *     return 0;
 * }
 *
 * ESPHOME_PLUGIN_REGISTER(my_plugin, "MyPlugin", "1.0.0") = {
 *     .init = my_plugin_init,
 *     .cleanup = my_plugin_cleanup,
 *     .handle_message = my_plugin_handle_message,
 *     .configure_device_info = NULL,  // Optional: configure device capabilities
 *     .list_entities = NULL,          // Optional: register entities
 * };
 * @endcode
 */

#ifndef ESPHOME_PLUGIN_H
#define ESPHOME_PLUGIN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct esphome_api_server esphome_api_server_t;
typedef struct esphome_device_config esphome_device_config_t;
typedef struct esphome_plugin_context esphome_plugin_context_t;
typedef struct esphome_plugin esphome_plugin_t;

/**
 * Plugin context - provided to each plugin
 * Contains access to server, device config, and plugin-specific data
 */
struct esphome_plugin_context {
    esphome_api_server_t *server;           /* API server instance */
    const esphome_device_config_t *config;  /* Device configuration */
    void *plugin_data;                       /* Plugin-specific data */
};

/**
 * Plugin initialization callback
 *
 * Called when the plugin is loaded during server startup.
 * Plugins should allocate resources and set up any required state.
 *
 * @param ctx Plugin context
 * @return 0 on success, -1 on error
 */
typedef int (*esphome_plugin_init_fn)(esphome_plugin_context_t *ctx);

/**
 * Plugin cleanup callback
 *
 * Called when the server is shutting down.
 * Plugins should free all resources.
 *
 * @param ctx Plugin context
 */
typedef void (*esphome_plugin_cleanup_fn)(esphome_plugin_context_t *ctx);

/**
 * Plugin message handler callback
 *
 * Called when a message is received from a client.
 * Return 0 if the message was handled, -1 if not recognized.
 *
 * @param ctx Plugin context
 * @param client_id ID of the client that sent the message (0-indexed)
 * @param msg_type ESPHome Native API message type
 * @param data Message payload
 * @param len Length of payload
 * @return 0 if handled, -1 if not handled
 */
typedef int (*esphome_plugin_msg_handler_fn)(esphome_plugin_context_t *ctx,
                                               int client_id,
                                               uint32_t msg_type,
                                               const uint8_t *data,
                                               size_t len);

/**
 * Forward declaration for device info response type
 */
typedef struct esphome_device_info_response esphome_device_info_response_t;

/**
 * Plugin device info configuration callback
 *
 * Called when building a device info response to allow plugins to
 * configure API features and capabilities they provide.
 *
 * Plugins can modify fields like:
 * - bluetooth_proxy_feature_flags
 * - bluetooth_mac_address
 * - voice_assistant_feature_flags (future)
 * - media_player_feature_flags (future)
 * etc.
 *
 * @param ctx Plugin context
 * @param device_info Device info response to configure
 * @return 0 on success, -1 on error
 */
typedef int (*esphome_plugin_configure_device_info_fn)(
    esphome_plugin_context_t *ctx,
    esphome_device_info_response_t *device_info);

/**
 * Plugin entity list callback
 *
 * Called during the list entities phase to allow plugins to send
 * entity responses for any entities they expose (sensors, switches, etc.).
 *
 * Plugins should use esphome_plugin_send_message_to_client() to send
 * entity responses (e.g., BinarySensorInfo, SwitchInfo, etc.).
 *
 * @param ctx Plugin context
 * @param client_id Client requesting entity list
 * @return 0 on success, -1 on error
 */
typedef int (*esphome_plugin_list_entities_fn)(
    esphome_plugin_context_t *ctx,
    int client_id);

/**
 * Plugin descriptor
 */
struct esphome_plugin {
    const char *name;                        /* Plugin name */
    const char *version;                     /* Plugin version */
    esphome_plugin_init_fn init;             /* Initialization callback */
    esphome_plugin_cleanup_fn cleanup;       /* Cleanup callback */
    esphome_plugin_msg_handler_fn handle_message; /* Message handler */
    esphome_plugin_configure_device_info_fn configure_device_info; /* Device info config (optional) */
    esphome_plugin_list_entities_fn list_entities; /* Entity registration (optional) */
    esphome_plugin_t *next;                  /* Linked list (internal use) */
    esphome_plugin_context_t *ctx;           /* Persistent context (internal use) */
};

/**
 * Register a plugin (used via ESPHOME_PLUGIN_REGISTER macro)
 *
 * @param plugin Plugin descriptor
 */
void esphome_plugin_register(esphome_plugin_t *plugin);

/**
 * Send a message to all connected clients
 *
 * Plugins use this to send custom ESPHome Native API messages.
 *
 * @param ctx Plugin context
 * @param msg_type ESPHome Native API message type
 * @param data Message payload
 * @param len Length of payload
 * @return 0 on success, -1 on error
 */
int esphome_plugin_send_message(esphome_plugin_context_t *ctx,
                                 uint32_t msg_type,
                                 const uint8_t *data,
                                 size_t len);

/**
 * Send a message to a specific client
 *
 * @param ctx Plugin context
 * @param client_id Client ID (0-indexed)
 * @param msg_type ESPHome Native API message type
 * @param data Message payload
 * @param len Length of payload
 * @return 0 on success, -1 on error
 */
int esphome_plugin_send_message_to_client(esphome_plugin_context_t *ctx,
                                           int client_id,
                                           uint32_t msg_type,
                                           const uint8_t *data,
                                           size_t len);

/**
 * Get the hostname/IP address of a connected client
 *
 * @param ctx Plugin context
 * @param client_id Client ID (0-indexed)
 * @param host_buf Buffer to store the hostname/IP string
 * @param host_buf_size Size of the buffer
 * @return 0 on success, -1 on error
 */
int esphome_plugin_get_client_host(esphome_plugin_context_t *ctx,
                                    int client_id,
                                    char *host_buf,
                                    size_t host_buf_size);

/**
 * Log a message (convenience wrapper)
 *
 * @param ctx Plugin context
 * @param level Log level (0=error, 1=warning, 2=info, 3=debug)
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void esphome_plugin_log(esphome_plugin_context_t *ctx,
                        int level,
                        const char *format, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * Plugin registration macro
 *
 * Usage:
 * @code
 * ESPHOME_PLUGIN_REGISTER(my_plugin, "MyPlugin", "1.0.0",
 *     my_plugin_init,
 *     my_plugin_cleanup,
 *     my_plugin_handle_message,
 *     NULL,  // Optional: configure_device_info
 *     NULL   // Optional: list_entities
 * );
 * @endcode
 */
#define ESPHOME_PLUGIN_REGISTER(var_name, plugin_name, plugin_version, \
                                 init_fn, cleanup_fn, handle_msg_fn, \
                                 config_device_info_fn, list_entities_fn) \
    static esphome_plugin_t var_name = { \
        .name = plugin_name, \
        .version = plugin_version, \
        .init = init_fn, \
        .cleanup = cleanup_fn, \
        .handle_message = handle_msg_fn, \
        .configure_device_info = config_device_info_fn, \
        .list_entities = list_entities_fn, \
        .next = NULL \
    }; \
    __attribute__((constructor)) static void __register_##var_name(void) { \
        esphome_plugin_register(&var_name); \
    }

#ifdef __cplusplus
}
#endif

#endif /* ESPHOME_PLUGIN_H */
