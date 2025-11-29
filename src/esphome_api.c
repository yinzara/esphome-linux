/**
 * @file esphome_api.c
 * @brief ESPHome Native API Server Implementation
 */

#include "include/esphome_api.h"
#include "include/esphome_proto.h"
#include "include/esphome_plugin_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <time.h>

#define RECV_BUFFER_SIZE 4096
#define SEND_BUFFER_SIZE 8192
#define LOG_PREFIX "[esphome-api] "

/**
 * Client connection state
 */
typedef struct {
    int fd;
    bool authenticated;
    uint8_t recv_buffer[RECV_BUFFER_SIZE];
    size_t recv_pos;
    pthread_mutex_t send_mutex;
    pthread_t thread;
    bool thread_running;
    struct esphome_api_server *server;
    struct sockaddr_in addr;  /* Client's IP address */
} client_connection_t;

/**
 * API server instance
 */
struct esphome_api_server {
    esphome_device_config_t config;
    int listen_fd;
    bool running;
    pthread_t listen_thread;

    /* Client connections */
    client_connection_t clients[ESPHOME_MAX_CLIENTS];
    pthread_mutex_t clients_mutex;
};

/* -----------------------------------------------------------------
 * Utility functions
 * ----------------------------------------------------------------- */

/* -----------------------------------------------------------------
 * Client management
 * ----------------------------------------------------------------- */

static void client_init(client_connection_t *client) {
    memset(client, 0, sizeof(*client));
    client->fd = -1;
    pthread_mutex_init(&client->send_mutex, NULL);
}

static void client_close(client_connection_t *client) {
    if (client->fd >= 0) {
        close(client->fd);
        client->fd = -1;
    }
    client->authenticated = false;
    client->recv_pos = 0;
}

static void client_cleanup(client_connection_t *client) {
    client_close(client);
    pthread_mutex_destroy(&client->send_mutex);
}

/* -----------------------------------------------------------------
 * Message type names for logging
 * ----------------------------------------------------------------- */

static const char* message_type_name(uint16_t msg_type) {
    switch (msg_type) {
        case ESPHOME_MSG_HELLO_REQUEST: return "HELLO_REQUEST";
        case ESPHOME_MSG_HELLO_RESPONSE: return "HELLO_RESPONSE";
        case ESPHOME_MSG_CONNECT_REQUEST: return "CONNECT_REQUEST";
        case ESPHOME_MSG_CONNECT_RESPONSE: return "CONNECT_RESPONSE";
        case ESPHOME_MSG_DISCONNECT_REQUEST: return "DISCONNECT_REQUEST";
        case ESPHOME_MSG_DISCONNECT_RESPONSE: return "DISCONNECT_RESPONSE";
        case ESPHOME_MSG_PING_REQUEST: return "PING_REQUEST";
        case ESPHOME_MSG_PING_RESPONSE: return "PING_RESPONSE";
        case ESPHOME_MSG_DEVICE_INFO_REQUEST: return "DEVICE_INFO_REQUEST";
        case ESPHOME_MSG_DEVICE_INFO_RESPONSE: return "DEVICE_INFO_RESPONSE";
        case ESPHOME_MSG_LIST_ENTITIES_REQUEST: return "LIST_ENTITIES_REQUEST";
        case ESPHOME_MSG_LIST_ENTITIES_DONE_RESPONSE: return "LIST_ENTITIES_DONE_RESPONSE";
        case ESPHOME_MSG_SUBSCRIBE_STATES_REQUEST: return "SUBSCRIBE_STATES_REQUEST";
        case ESPHOME_MSG_SUBSCRIBE_HOMEASSISTANT_SERVICES_REQUEST: return "SUBSCRIBE_HOMEASSISTANT_SERVICES";
        case ESPHOME_MSG_SUBSCRIBE_HOMEASSISTANT_STATES_REQUEST: return "SUBSCRIBE_HOMEASSISTANT_STATES";
        case ESPHOME_MSG_SUBSCRIBE_BLUETOOTH_LE_ADVERTISEMENTS_REQUEST: return "SUBSCRIBE_BLE_ADVERTISEMENTS";
        case ESPHOME_MSG_BLUETOOTH_LE_RAW_ADVERTISEMENTS_RESPONSE: return "BLE_RAW_ADVERTISEMENTS_RESPONSE";
        default: return "UNKNOWN";
    }
}

/* -----------------------------------------------------------------
 * Message sending
 * ----------------------------------------------------------------- */

static int send_message(client_connection_t *client, uint16_t msg_type,
                        const uint8_t *payload, size_t payload_len) {
    uint8_t send_buf[SEND_BUFFER_SIZE];

    size_t frame_len = esphome_frame_message(send_buf, sizeof(send_buf),
                                              msg_type, payload, payload_len);
    if (frame_len == 0) {
        fprintf(stderr, LOG_PREFIX "Failed to frame message type %u (%s)\n",
                msg_type, message_type_name(msg_type));
        return -1;
    }

    pthread_mutex_lock(&client->send_mutex);
    ssize_t sent = send(client->fd, send_buf, frame_len, MSG_NOSIGNAL);
    pthread_mutex_unlock(&client->send_mutex);

    if (sent < 0) {
        fprintf(stderr, LOG_PREFIX "Send failed: %s\n", strerror(errno));
        return -1;
    }

    if ((size_t)sent != frame_len) {
        fprintf(stderr, LOG_PREFIX "Partial send: %zd/%zu\n", sent, frame_len);
        return -1;
    }

    printf(LOG_PREFIX ">>> Sent %s (type=%u, payload=%zu bytes, total=%zu bytes)\n",
           message_type_name(msg_type), msg_type, payload_len, frame_len);

    return 0;
}

/* -----------------------------------------------------------------
 * Message handlers
 * ----------------------------------------------------------------- */

static void handle_hello_request(esphome_api_server_t *server,
                                  client_connection_t *client,
                                  const uint8_t *payload, size_t payload_len) {
    /* Log the client's HELLO payload */
    if (payload_len > 0) {
        printf(LOG_PREFIX "Client HELLO payload (%zu bytes): ", payload_len);
        for (size_t i = 0; i < payload_len && i < 32; i++) {
            printf("%02x ", payload[i]);
        }
        printf("\n");
    }

    esphome_hello_response_t response;
    memset(&response, 0, sizeof(response));

    response.api_version_major = 1;
    response.api_version_minor = 12;
    snprintf(response.server_info, sizeof(response.server_info),
             "%s (Thingino BLE Proxy v1.0)", server->config.device_name);
    strncpy(response.name, server->config.device_name, sizeof(response.name) - 1);

    uint8_t encode_buf[512];
    size_t len = esphome_encode_hello_response(encode_buf, sizeof(encode_buf), &response);

    if (len > 0) {
        send_message(client, ESPHOME_MSG_HELLO_RESPONSE, encode_buf, len);
    }
}

static void handle_connect_request(esphome_api_server_t *server,
                                    client_connection_t *client,
                                    const uint8_t *payload, size_t payload_len) {
    (void)server;
    (void)payload;
    (void)payload_len;

    esphome_connect_response_t response;
    memset(&response, 0, sizeof(response));

    response.invalid_password = false;
    client->authenticated = true;

    uint8_t encode_buf[32];
    size_t len = esphome_encode_connect_response(encode_buf, sizeof(encode_buf), &response);

    if (len > 0) {
        send_message(client, ESPHOME_MSG_CONNECT_RESPONSE, encode_buf, len);
        printf(LOG_PREFIX "Client authenticated\n");
    }
}

static void handle_device_info_request(esphome_api_server_t *server,
                                        client_connection_t *client,
                                        const uint8_t *payload, size_t payload_len) {
    (void)payload;
    (void)payload_len;

    esphome_device_info_response_t response;
    memset(&response, 0, sizeof(response));

    /* Core device information */
    response.uses_password = false;
    strncpy(response.name, server->config.device_name, sizeof(response.name) - 1);
    strncpy(response.mac_address, server->config.mac_address, sizeof(response.mac_address) - 1);
    strncpy(response.esphome_version, server->config.esphome_version, sizeof(response.esphome_version) - 1);
    strncpy(response.compilation_time, __DATE__ " " __TIME__, sizeof(response.compilation_time) - 1);
    strncpy(response.model, server->config.model, sizeof(response.model) - 1);
    response.has_deep_sleep = false;

    /* Project info (optional - leave empty if not set) */
    response.project_name[0] = '\0';
    response.project_version[0] = '\0';
    response.webserver_port = 0;  /* No webserver by default */

    /* Device metadata */
    strncpy(response.manufacturer, server->config.manufacturer, sizeof(response.manufacturer) - 1);
    strncpy(response.friendly_name, server->config.friendly_name, sizeof(response.friendly_name) - 1);
    strncpy(response.suggested_area, server->config.suggested_area, sizeof(response.suggested_area) - 1);

    /* Feature flags - initialized to 0, plugins will set them */
    response.bluetooth_proxy_feature_flags = 0;
    response.voice_assistant_feature_flags = 0;
    response.zwave_proxy_feature_flags = 0;

    /* API encryption - not supported yet */
    response.api_encryption_supported = false;

    /* Z-Wave - not configured by default */
    response.zwave_home_id = 0;

    /* Let plugins configure device capabilities (e.g., Bluetooth proxy features, Voice Assistant, Z-Wave) */
    esphome_plugin_configure_device_info_all(server, &server->config, &response);

    uint8_t encode_buf[1024];
    size_t len = esphome_encode_device_info_response(encode_buf, sizeof(encode_buf), &response);

    if (len > 0) {
        send_message(client, ESPHOME_MSG_DEVICE_INFO_RESPONSE, encode_buf, len);

        /* Log hex dump of encoded response for debugging */
        printf(LOG_PREFIX "DeviceInfo payload hex (%zu bytes):\n", len);
        for (size_t i = 0; i < len; i++) {
            if (i % 16 == 0) {
                printf(LOG_PREFIX "%04zx: ", i);
            }
            printf("%02x ", encode_buf[i]);
            if ((i + 1) % 16 == 0 || i == len - 1) {
                printf("\n");
            }
        }
        printf(LOG_PREFIX "Looking for field 15 tag (0x78) and field 18 tag (0x92)...\n");
        for (size_t i = 0; i < len - 1; i++) {
            if (encode_buf[i] == 0x78) {
                printf(LOG_PREFIX "Found field 15 (bluetooth_proxy_feature_flags) at offset %zu, value = 0x%02x\n",
                       i, encode_buf[i+1]);
            }
            if (encode_buf[i] == 0x92) {
                /* Field 18 tag: (18 << 3) | 2 = 146 = 0x92 */
                printf(LOG_PREFIX "Found field 18 (bluetooth_mac_address) at offset %zu\n", i);
            }
        }
    }
}

static void handle_list_entities_request(esphome_api_server_t *server,
                                          client_connection_t *client,
                                          const uint8_t *payload, size_t payload_len) {
    (void)payload;
    (void)payload_len;

    /* Find client ID */
    int client_id = -1;
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < ESPHOME_MAX_CLIENTS; i++) {
        if (&server->clients[i] == client) {
            client_id = i;
            break;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);

    /* Allow plugins to list their entities */
    if (client_id >= 0) {
        esphome_plugin_list_entities_all(server, &server->config, client_id);
    }

    /* Send done message */
    send_message(client, ESPHOME_MSG_LIST_ENTITIES_DONE_RESPONSE, NULL, 0);
}

static void handle_subscribe_states_request(esphome_api_server_t *server,
                                             client_connection_t *client,
                                             const uint8_t *payload, size_t payload_len) {
    (void)payload;
    (void)payload_len;

    /* Find client ID */
    int client_id = -1;
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < ESPHOME_MAX_CLIENTS; i++) {
        if (&server->clients[i] == client) {
            client_id = i;
            break;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);

    /* Allow plugins to list their entities */
    if (client_id >= 0) {
        esphome_plugin_subscribe_states_all(server, &server->config, client_id);
    }
}

static void handle_ping_request(esphome_api_server_t *server,
                                 client_connection_t *client,
                                 const uint8_t *payload, size_t payload_len) {
    (void)server;
    (void)payload;
    (void)payload_len;

    /* Send empty ping response */
    send_message(client, ESPHOME_MSG_PING_RESPONSE, NULL, 0);
}

static void handle_subscribe_homeassistant_services(esphome_api_server_t *server,
                                                     client_connection_t *client,
                                                     const uint8_t *payload, size_t payload_len) {
    (void)server;
    (void)client;
    (void)payload;
    (void)payload_len;

    /* We don't provide any Home Assistant services - just acknowledge */
}

static void handle_subscribe_homeassistant_states(esphome_api_server_t *server,
                                                   client_connection_t *client,
                                                   const uint8_t *payload, size_t payload_len) {
    (void)server;
    (void)client;
    (void)payload;
    (void)payload_len;

    /* We don't provide any Home Assistant states - just acknowledge */
}

static void dispatch_message(esphome_api_server_t *server,
                              client_connection_t *client,
                              int client_id,
                              uint16_t msg_type,
                              const uint8_t *payload, size_t payload_len) {
    printf(LOG_PREFIX "<<< Received %s (type=%u, payload=%zu bytes)\n",
           message_type_name(msg_type), msg_type, payload_len);

    switch (msg_type) {
        case ESPHOME_MSG_HELLO_REQUEST:
            handle_hello_request(server, client, payload, payload_len);
            break;
        case ESPHOME_MSG_CONNECT_REQUEST:
            handle_connect_request(server, client, payload, payload_len);
            break;
        case ESPHOME_MSG_DEVICE_INFO_REQUEST:
            handle_device_info_request(server, client, payload, payload_len);
            break;
        case ESPHOME_MSG_LIST_ENTITIES_REQUEST:
            handle_list_entities_request(server, client, payload, payload_len);
            break;
        case ESPHOME_MSG_SUBSCRIBE_STATES_REQUEST:
            handle_subscribe_states_request(server, client, payload, payload_len);
            break;
        case ESPHOME_MSG_SUBSCRIBE_HOMEASSISTANT_SERVICES_REQUEST:
            handle_subscribe_homeassistant_services(server, client, payload, payload_len);
            break;
        case ESPHOME_MSG_SUBSCRIBE_HOMEASSISTANT_STATES_REQUEST:
            handle_subscribe_homeassistant_states(server, client, payload, payload_len);
            break;
        case ESPHOME_MSG_PING_REQUEST:
            handle_ping_request(server, client, payload, payload_len);
            break;
        case ESPHOME_MSG_DISCONNECT_REQUEST:
            printf(LOG_PREFIX "Client requested disconnect\n");
            break;
        default:
            /* Try delegating to plugins */
            if (esphome_plugin_handle_message(server, &server->config, client_id, msg_type, payload, payload_len) < 0) {
                printf(LOG_PREFIX "!!! Unhandled message type: %u (%s)\n",
                       msg_type, message_type_name(msg_type));
            }
            break;
    }
}

/* -----------------------------------------------------------------
 * Client handling
 * ----------------------------------------------------------------- */

static void handle_client_data(esphome_api_server_t *server,
                                client_connection_t *client,
                                int client_id) {
    while (client->recv_pos > 0) {
        uint32_t msg_len;
        uint16_t msg_type;

        printf(LOG_PREFIX "Parsing frame from buffer (%zu bytes available)\n", client->recv_pos);

        /* Hex dump first 32 bytes for debugging */
        printf(LOG_PREFIX "Buffer hex dump: ");
        size_t dump_len = client->recv_pos < 32 ? client->recv_pos : 32;
        for (size_t i = 0; i < dump_len; i++) {
            printf("%02x ", client->recv_buffer[i]);
        }
        printf("\n");

        size_t header_len = esphome_decode_frame_header(client->recv_buffer,
                                                         client->recv_pos,
                                                         &msg_len, &msg_type);
        if (header_len == 0) {
            printf(LOG_PREFIX "Need more data for header (have %zu bytes)\n", client->recv_pos);
            break;
        }

        printf(LOG_PREFIX "Decoded header: header_len=%zu, msg_len=%u, msg_type=%u (%s)\n",
               header_len, msg_len, msg_type, message_type_name(msg_type));

        /*
         * header_len = position where payload starts (after preamble + length_varint + type_varint)
         * msg_len = payload length ONLY (not including type varint)
         *
         * Total message = header_len + msg_len
         */

        uint32_t total_len = header_len + msg_len;
        printf(LOG_PREFIX "Total message length: %u (header=%zu + payload=%u)\n",
               total_len, header_len, msg_len);

        if (client->recv_pos < total_len) {
            printf(LOG_PREFIX "Need more data for message (have %zu, need %u)\n",
                   client->recv_pos, total_len);
            break;
        }

        /* Dispatch message */
        const uint8_t *payload = client->recv_buffer + header_len;
        size_t payload_len = msg_len;

        dispatch_message(server, client, client_id, msg_type, payload, payload_len);

        /* Remove processed message from buffer */
        memmove(client->recv_buffer, client->recv_buffer + total_len,
                client->recv_pos - total_len);
        client->recv_pos -= total_len;
        printf(LOG_PREFIX "Message processed, %zu bytes remaining in buffer\n", client->recv_pos);
    }
}

static void *client_thread(void *arg) {
    esphome_api_server_t *server = (esphome_api_server_t *)arg;
    client_connection_t *client = NULL;

    /* Find this client in the array (passed via arg is actually server) */
    /* We'll handle this differently - see below */

    return NULL;
}

/* -----------------------------------------------------------------
 * TCP server
 * ----------------------------------------------------------------- */

/**
 * Client handling thread
 */
static void *client_thread_func(void *arg) {
    client_connection_t *client = (client_connection_t *)arg;
    esphome_api_server_t *server = client->server;
    int client_id = (int)(client - server->clients);  /* Calculate client index */

    /* Handle client messages */
    while (server->running && client->fd >= 0) {
        ssize_t received = recv(client->fd,
                               client->recv_buffer + client->recv_pos,
                               sizeof(client->recv_buffer) - client->recv_pos,
                               0);

        if (received <= 0) {
            if (received < 0) {
                fprintf(stderr, LOG_PREFIX "Recv failed: %s\n", strerror(errno));
            }
            printf(LOG_PREFIX "Client disconnected\n");
            break;
        }

        printf(LOG_PREFIX "Received %zd bytes from client (buffer now has %zu bytes)\n",
               received, client->recv_pos + received);

        client->recv_pos += received;
        handle_client_data(server, client, client_id);
    }

    /* Cleanup client */
    pthread_mutex_lock(&server->clients_mutex);
    client_close(client);
    client->thread_running = false;
    pthread_mutex_unlock(&server->clients_mutex);

    return NULL;
}

static void *listen_thread_func(void *arg) {
    esphome_api_server_t *server = (esphome_api_server_t *)arg;

    while (server->running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server->listen_fd,
                              (struct sockaddr *)&client_addr,
                              &client_len);

        if (client_fd < 0) {
            if (server->running) {
                fprintf(stderr, LOG_PREFIX "Accept failed: %s\n", strerror(errno));
            }
            continue;
        }

        /* Set TCP_NODELAY for low latency */
        int flag = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        printf(LOG_PREFIX "Client connected from %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        /* Find free slot */
        pthread_mutex_lock(&server->clients_mutex);
        int slot = -1;
        for (int i = 0; i < ESPHOME_MAX_CLIENTS; i++) {
            if (server->clients[i].fd < 0) {
                server->clients[i].fd = client_fd;
                slot = i;
                break;
            }
        }
        pthread_mutex_unlock(&server->clients_mutex);

        if (slot < 0) {
            fprintf(stderr, LOG_PREFIX "Max clients reached, rejecting connection\n");
            close(client_fd);
            continue;
        }

        /* Start client thread */
        client_connection_t *client = &server->clients[slot];
        client->server = server;
        client->thread_running = true;
        client->addr = client_addr;  /* Store client address */

        if (pthread_create(&client->thread, NULL, client_thread_func, client) != 0) {
            fprintf(stderr, LOG_PREFIX "Failed to create client thread\n");
            pthread_mutex_lock(&server->clients_mutex);
            client_close(client);
            pthread_mutex_unlock(&server->clients_mutex);
        }
    }

    return NULL;
}

/* -----------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------- */

esphome_api_server_t *esphome_api_init(const esphome_device_config_t *config) {
    esphome_api_server_t *server = calloc(1, sizeof(esphome_api_server_t));
    if (!server) {
        return NULL;
    }

    server->config = *config;
    server->listen_fd = -1;
    server->running = false;

    pthread_mutex_init(&server->clients_mutex, NULL);

    for (int i = 0; i < ESPHOME_MAX_CLIENTS; i++) {
        client_init(&server->clients[i]);
    }

    return server;
}

int esphome_api_start(esphome_api_server_t *server) {
    /* Create TCP socket */
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        fprintf(stderr, LOG_PREFIX "Failed to create socket: %s\n", strerror(errno));
        return -1;
    }

    /* Set SO_REUSEADDR */
    int opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Bind */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ESPHOME_API_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, LOG_PREFIX "Failed to bind: %s\n", strerror(errno));
        close(server->listen_fd);
        server->listen_fd = -1;
        return -1;
    }

    /* Listen */
    if (listen(server->listen_fd, 2) < 0) {
        fprintf(stderr, LOG_PREFIX "Failed to listen: %s\n", strerror(errno));
        close(server->listen_fd);
        server->listen_fd = -1;
        return -1;
    }

    printf(LOG_PREFIX "Listening on port %d\n", ESPHOME_API_PORT);

    /* Start listen thread */
    server->running = true;
    pthread_create(&server->listen_thread, NULL, listen_thread_func, server);

    return 0;
}

void esphome_api_stop(esphome_api_server_t *server) {
    if (!server) {
        return;
    }

    server->running = false;

    /* Close listen socket to unblock accept() */
    if (server->listen_fd >= 0) {
        shutdown(server->listen_fd, SHUT_RDWR);
        close(server->listen_fd);
        server->listen_fd = -1;
    }

    /* Wait for listen thread */
    pthread_join(server->listen_thread, NULL);

    /* Close all client sockets FIRST to unblock recv() in client threads */
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < ESPHOME_MAX_CLIENTS; i++) {
        if (server->clients[i].fd >= 0) {
            shutdown(server->clients[i].fd, SHUT_RDWR);
            close(server->clients[i].fd);
            server->clients[i].fd = -1;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);

    /* Now wait for all client threads to finish (they should exit quickly now) */
    for (int i = 0; i < ESPHOME_MAX_CLIENTS; i++) {
        if (server->clients[i].thread_running) {
            pthread_join(server->clients[i].thread, NULL);
        }
    }
}

void esphome_api_free(esphome_api_server_t *server) {
    if (!server) {
        return;
    }

    for (int i = 0; i < ESPHOME_MAX_CLIENTS; i++) {
        client_cleanup(&server->clients[i]);
    }

    pthread_mutex_destroy(&server->clients_mutex);

    free(server);
}

/**
 * Send a message to a specific client (for plugin use)
 */
int esphome_api_send_to_client(esphome_api_server_t *server,
                                int client_id,
                                uint16_t msg_type,
                                const uint8_t *payload,
                                size_t payload_len) {
    if (!server || client_id < 0 || client_id >= ESPHOME_MAX_CLIENTS) {
        return -1;
    }

    pthread_mutex_lock(&server->clients_mutex);

    client_connection_t *client = &server->clients[client_id];
    if (client->fd < 0) {
        pthread_mutex_unlock(&server->clients_mutex);
        return -1;
    }

    int result = send_message(client, msg_type, payload, payload_len);

    pthread_mutex_unlock(&server->clients_mutex);

    return result;
}

/**
 * Broadcast a message to all connected clients (for plugin use)
 */
int esphome_api_broadcast(esphome_api_server_t *server,
                          uint16_t msg_type,
                          const uint8_t *payload,
                          size_t payload_len) {
    if (!server) {
        return 0;
    }

    int sent_count = 0;

    pthread_mutex_lock(&server->clients_mutex);

    for (int i = 0; i < ESPHOME_MAX_CLIENTS; i++) {
        client_connection_t *client = &server->clients[i];
        if (client->fd >= 0) {
            if (send_message(client, msg_type, payload, payload_len) == 0) {
                sent_count++;
            }
        }
    }

    pthread_mutex_unlock(&server->clients_mutex);

    return sent_count;
}

/**
 * Get the hostname/IP address of a connected client
 */
int esphome_api_get_client_host(esphome_api_server_t *server,
                                 int client_id,
                                 char *host_buf,
                                 size_t host_buf_size) {
    if (!server || !host_buf || host_buf_size == 0 ||
        client_id < 0 || client_id >= ESPHOME_MAX_CLIENTS) {
        return -1;
    }

    pthread_mutex_lock(&server->clients_mutex);

    client_connection_t *client = &server->clients[client_id];
    if (client->fd < 0) {
        pthread_mutex_unlock(&server->clients_mutex);
        return -1;
    }

    /* Convert IP address to string */
    const char *ip_str = inet_ntoa(client->addr.sin_addr);
    if (!ip_str) {
        pthread_mutex_unlock(&server->clients_mutex);
        return -1;
    }

    strncpy(host_buf, ip_str, host_buf_size - 1);
    host_buf[host_buf_size - 1] = '\0';

    pthread_mutex_unlock(&server->clients_mutex);

    return 0;
}
