/**
 * @file esphome_plugin_internal.h
 * @brief Internal plugin system API (for core use only)
 *
 * This header is used by the ESPHome API server to interact with the plugin system.
 * Plugins should NOT include this header - they should use esphome_plugin.h instead.
 */

#ifndef ESPHOME_PLUGIN_INTERNAL_H
#define ESPHOME_PLUGIN_INTERNAL_H

#include "esphome_plugin.h"
#include "esphome_api.h"
#include "esphome_proto.h"

/**
 * Get the head of the plugin list
 *
 * @return Pointer to first plugin in linked list, or NULL if no plugins
 */
esphome_plugin_t *esphome_plugin_get_head(void);

/**
 * Initialize all registered plugins
 *
 * Called during server startup to initialize all plugins.
 *
 * @param server API server instance
 * @param config Device configuration
 * @return 0 on success, -1 if any plugin failed to initialize
 */
int esphome_plugin_init_all(esphome_api_server_t *server,
                             const esphome_device_config_t *config);

/**
 * Cleanup all plugins
 *
 * Called during server shutdown to cleanup all plugin resources.
 *
 * @param server API server instance
 * @param config Device configuration
 */
void esphome_plugin_cleanup_all(esphome_api_server_t *server,
                                const esphome_device_config_t *config);

/**
 * Allow all plugins to configure device info
 *
 * Called when building a device info response. Each plugin can modify
 * the device_info structure to advertise its capabilities.
 *
 * @param server API server instance
 * @param config Device configuration
 * @param device_info Device info response to configure
 * @return 0 on success, -1 on error
 */
int esphome_plugin_configure_device_info_all(
    esphome_api_server_t *server,
    const esphome_device_config_t *config,
    esphome_device_info_response_t *device_info);

/**
 * Allow all plugins to list their entities
 *
 * Called during the list entities phase. Each plugin can send entity
 * responses for any entities it exposes.
 *
 * @param server API server instance
 * @param config Device configuration
 * @param client_id Client requesting entity list
 * @return 0 on success, -1 on error
 */
int esphome_plugin_list_entities_all(
    esphome_api_server_t *server,
    const esphome_device_config_t *config,
    int client_id);

/**
 * Dispatch a message to all plugins
 *
 * Called when a message is received from a client. The message is passed
 * to each plugin's handle_message callback until one returns 0 (handled).
 *
 * @param server API server instance
 * @param config Device configuration
 * @param client_id ID of the client that sent the message
 * @param msg_type ESPHome Native API message type
 * @param data Message payload
 * @param len Length of payload
 * @return 0 if a plugin handled the message, -1 if no plugin handled it
 */
int esphome_plugin_handle_message(
    esphome_api_server_t *server,
    const esphome_device_config_t *config,
    int client_id,
    uint32_t msg_type,
    const uint8_t *data,
    size_t len);

#endif /* ESPHOME_PLUGIN_INTERNAL_H */
