/**
 * @file esphome_api.h
 * @brief ESPHome Native API Server
 *
 * Implements the ESPHome Native API protocol over TCP.
 * Provides a plugin-based architecture for extending functionality.
 */

#ifndef ESPHOME_API_H
#define ESPHOME_API_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

/* Server configuration */
#define ESPHOME_API_PORT 6053
#define ESPHOME_MAX_CLIENTS 2

/**
 * Device configuration
 */
typedef struct esphome_device_config {
    char device_name[128];
    char mac_address[24];         /* "AA:BB:CC:DD:EE:FF" */
    char esphome_version[32];
    char model[128];
    char manufacturer[128];
    char friendly_name[128];
    char suggested_area[64];
} esphome_device_config_t;

/**
 * API server instance
 */
typedef struct esphome_api_server esphome_api_server_t;

/**
 * Initialize the API server
 *
 * @param config Device configuration
 * @return Server instance, or NULL on error
 */
esphome_api_server_t *esphome_api_init(const esphome_device_config_t *config);

/**
 * Start the API server (non-blocking)
 *
 * Starts a background thread to handle TCP connections.
 *
 * @param server Server instance
 * @return 0 on success, -1 on error
 */
int esphome_api_start(esphome_api_server_t *server);

/**
 * Stop the API server
 *
 * @param server Server instance
 */
void esphome_api_stop(esphome_api_server_t *server);

/**
 * Free the API server
 *
 * @param server Server instance
 */
void esphome_api_free(esphome_api_server_t *server);

/**
 * Send a message to a specific client (for plugin use)
 *
 * @param server API server instance
 * @param client_id Client index (0-based)
 * @param msg_type ESPHome Native API message type
 * @param payload Message payload
 * @param payload_len Length of payload
 * @return 0 on success, -1 on error
 */
int esphome_api_send_to_client(esphome_api_server_t *server,
                                int client_id,
                                uint16_t msg_type,
                                const uint8_t *payload,
                                size_t payload_len);

/**
 * Broadcast a message to all connected clients (for plugin use)
 *
 * @param server API server instance
 * @param msg_type ESPHome Native API message type
 * @param payload Message payload
 * @param payload_len Length of payload
 * @return Number of clients message was sent to
 */
int esphome_api_broadcast(esphome_api_server_t *server,
                          uint16_t msg_type,
                          const uint8_t *payload,
                          size_t payload_len);

/**
 * Get the hostname/IP address of a connected client
 *
 * @param server API server instance
 * @param client_id Client index (0-based)
 * @param host_buf Buffer to store the hostname/IP string
 * @param host_buf_size Size of the buffer
 * @return 0 on success, -1 on error
 */
int esphome_api_get_client_host(esphome_api_server_t *server,
                                 int client_id,
                                 char *host_buf,
                                 size_t host_buf_size);

#endif /* ESPHOME_API_H */
