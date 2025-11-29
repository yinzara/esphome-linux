/**
 * @file esphome_plugin.c
 * @brief Plugin system implementation for ESPHome Linux
 */

#include "include/esphome_plugin.h"
#include "include/esphome_api.h"
#include "include/esphome_proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define LOG_PREFIX "[plugin-manager] "

/* Global plugin list (linked list) */
static esphome_plugin_t *plugins_head = NULL;

/**
 * Register a plugin (called by ESPHOME_PLUGIN_REGISTER macro via constructor)
 */
void esphome_plugin_register(esphome_plugin_t *plugin) {
    if (!plugin) {
        return;
    }

    /* Add to linked list */
    plugin->next = plugins_head;
    plugins_head = plugin;

    printf(LOG_PREFIX "Registered plugin: %s v%s\n", plugin->name, plugin->version);
}

/**
 * Get the head of the plugin list
 */
esphome_plugin_t *esphome_plugin_get_head(void) {
    return plugins_head;
}

/**
 * Initialize all plugins
 */
int esphome_plugin_init_all(esphome_api_server_t *server, const esphome_device_config_t *config) {
    int count = 0;
    int failed = 0;

    printf(LOG_PREFIX "Initializing plugins...\n");

    for (esphome_plugin_t *plugin = plugins_head; plugin != NULL; plugin = plugin->next) {
        count++;
        printf(LOG_PREFIX "Initializing %s...\n", plugin->name);

        if (plugin->init) {
            /* Allocate persistent context for this plugin */
            esphome_plugin_context_t *ctx = calloc(1, sizeof(esphome_plugin_context_t));
            if (!ctx) {
                fprintf(stderr, LOG_PREFIX "Failed to allocate context for plugin: %s\n", plugin->name);
                failed++;
                continue;
            }

            ctx->server = server;
            ctx->config = config;
            ctx->plugin_data = NULL;

            if (plugin->init(ctx) < 0) {
                fprintf(stderr, LOG_PREFIX "Failed to initialize plugin: %s\n", plugin->name);
                free(ctx);
                failed++;
            } else {
                /* Store the context in the plugin for later use */
                plugin->ctx = ctx;
            }
        }
    }

    printf(LOG_PREFIX "Initialized %d plugin(s), %d failed\n", count, failed);
    return (failed > 0) ? -1 : 0;
}

/**
 * Cleanup all plugins
 */
void esphome_plugin_cleanup_all(esphome_api_server_t *server, const esphome_device_config_t *config) {
    (void)server;
    (void)config;

    printf(LOG_PREFIX "Cleaning up plugins...\n");

    for (esphome_plugin_t *plugin = plugins_head; plugin != NULL; plugin = plugin->next) {
        if (plugin->cleanup && plugin->ctx) {
            printf(LOG_PREFIX "Cleaning up %s...\n", plugin->name);
            plugin->cleanup(plugin->ctx);

            /* Free the persistent context */
            free(plugin->ctx);
            plugin->ctx = NULL;
        }
    }
}

/**
 * Configure device info with all plugins
 * Calls each plugin's configure_device_info callback if present
 */
int esphome_plugin_configure_device_info_all(
    esphome_api_server_t *server,
    const esphome_device_config_t *config,
    esphome_device_info_response_t *device_info)
{
    (void)server;
    (void)config;

    for (esphome_plugin_t *plugin = plugins_head; plugin != NULL; plugin = plugin->next) {
        if (plugin->configure_device_info && plugin->ctx) {
            printf(LOG_PREFIX "Plugin %s configuring device info...\n", plugin->name);
            if (plugin->configure_device_info(plugin->ctx, device_info) < 0) {
                fprintf(stderr, LOG_PREFIX "Warning: Plugin %s failed to configure device info\n",
                        plugin->name);
            }
        }
    }

    return 0;
}

/**
 * List entities from all plugins
 * Calls each plugin's list_entities callback if present
 */
int esphome_plugin_list_entities_all(
    esphome_api_server_t *server,
    const esphome_device_config_t *config,
    int client_id)
{
    (void)server;
    (void)config;

    for (esphome_plugin_t *plugin = plugins_head; plugin != NULL; plugin = plugin->next) {
        if (plugin->list_entities && plugin->ctx) {
            printf(LOG_PREFIX "Plugin %s listing entities...\n", plugin->name);
            if (plugin->list_entities(plugin->ctx, client_id) < 0) {
                fprintf(stderr, LOG_PREFIX "Warning: Plugin %s failed to list entities\n",
                        plugin->name);
            }
        }
    }

    return 0;
}

/**
 * Subscribe states from all plugins
 * Calls each plugin's subscribe_states callback if present
 */
int esphome_plugin_subscribe_states_all(
    esphome_api_server_t *server,
    const esphome_device_config_t *config,
    int client_id)
{
    (void)server;
    (void)config;

    for (esphome_plugin_t *plugin = plugins_head; plugin != NULL; plugin = plugin->next) {
        if (plugin->subscribe_states && plugin->ctx) {
            printf(LOG_PREFIX "Plugin %s subscribe states...\n", plugin->name);
            if (plugin->subscribe_states(plugin->ctx, client_id) < 0) {
                fprintf(stderr, LOG_PREFIX "Warning: Plugin %s failed to subscribe states\n",
                        plugin->name);
            }
        }
    }

    return 0;
}

/**
 * Dispatch message to plugins
 * Returns 0 if a plugin handled the message, -1 if no plugin handled it
 */
int esphome_plugin_handle_message(
    esphome_api_server_t *server,
    const esphome_device_config_t *config,
    int client_id,
    uint32_t msg_type,
    const uint8_t *data,
    size_t len)
{
    (void)server;
    (void)config;

    for (esphome_plugin_t *plugin = plugins_head; plugin != NULL; plugin = plugin->next) {
        if (plugin->handle_message && plugin->ctx) {
            int result = plugin->handle_message(plugin->ctx, client_id, msg_type, data, len);
            if (result == 0) {
                /* Message was handled by this plugin */
                return 0;
            }
        }
    }

    /* No plugin handled this message */
    return -1;
}

/**
 * Send a message to all connected clients
 */
int esphome_plugin_send_message(
    esphome_plugin_context_t *ctx,
    uint32_t msg_type,
    const uint8_t *data,
    size_t len)
{
    if (!ctx || !ctx->server) {
        return -1;
    }

    int sent = esphome_api_broadcast(ctx->server, (uint16_t)msg_type, data, len);
    return (sent > 0) ? 0 : -1;
}

/**
 * Send a message to a specific client
 */
int esphome_plugin_send_message_to_client(
    esphome_plugin_context_t *ctx,
    int client_id,
    uint32_t msg_type,
    const uint8_t *data,
    size_t len)
{
    if (!ctx || !ctx->server) {
        return -1;
    }

    return esphome_api_send_to_client(ctx->server, client_id, (uint16_t)msg_type, data, len);
}

/**
 * Get the hostname/IP address of a connected client
 */
int esphome_plugin_get_client_host(
    esphome_plugin_context_t *ctx,
    int client_id,
    char *host_buf,
    size_t host_buf_size)
{
    if (!ctx || !ctx->server) {
        return -1;
    }

    return esphome_api_get_client_host(ctx->server, client_id, host_buf, host_buf_size);
}

/**
 * Log a message (plugin convenience function)
 */
void esphome_plugin_log(
    esphome_plugin_context_t *ctx,
    int level,
    const char *format, ...)
{
    (void)ctx;

    const char *level_str[] = {"ERROR", "WARNING", "INFO", "DEBUG"};
    if (level < 0 || level > 3) {
        level = 0;
    }

    va_list args;
    va_start(args, format);
    printf(LOG_PREFIX "[%s] ", level_str[level]);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}
