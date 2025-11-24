/**
 * @file main.c
 * @brief ESPHome Linux Service Entry Point
 *
 * This service implements the ESPHome Native API for Linux devices,
 * enabling integration with Home Assistant via a plugin-based architecture.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "include/esphome_api.h"
#include "include/esphome_plugin_internal.h"

#define PROGRAM_NAME "esphome-linux"
#define VERSION "1.0.0"

static volatile sig_atomic_t running = 1;
static esphome_api_server_t *api_server = NULL;

/**
 * Signal handler for graceful shutdown
 */
static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

/**
 * Get the MAC address of the primary network interface
 */
static int get_mac_address(char *mac_str, size_t size) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));

    /* Try common interface names */
    const char *ifaces[] = {"eth0", "wlan0", "ra0", "br-lan", NULL};

    for (int i = 0; ifaces[i] != NULL; i++) {
        strncpy(ifr.ifr_name, ifaces[i], IFNAMSIZ - 1);

        if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
            unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
            snprintf(mac_str, size, "%02X:%02X:%02X:%02X:%02X:%02X",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            close(fd);
            return 0;
        }
    }

    close(fd);
    return -1;
}

/**
 * Get the hostname
 */
static int get_hostname(char *hostname, size_t size) {
    if (gethostname(hostname, size) == 0) {
        return 0;
    }
    return -1;
}

/**
 * Main entry point
 */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("%s v%s - ESPHome Native API for Linux\n",
           PROGRAM_NAME, VERSION);
    printf("Copyright (c) 2025 Thingino Project\n\n");

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    /* Get device information */
    char hostname[128] = "thingino-proxy";
    char mac_address[24] = "00:00:00:00:00:00";

    get_hostname(hostname, sizeof(hostname));
    get_mac_address(mac_address, sizeof(mac_address));

    printf("Device: %s\n", hostname);
    printf("MAC: %s\n\n", mac_address);

    /* Configure device */
    esphome_device_config_t config;
    memset(&config, 0, sizeof(config));

    snprintf(config.device_name, sizeof(config.device_name), "%s", hostname);
    snprintf(config.mac_address, sizeof(config.mac_address), "%s", mac_address);
    snprintf(config.esphome_version, sizeof(config.esphome_version), "2025.12.0");
    snprintf(config.model, sizeof(config.model), "ESPHome Linux");
    snprintf(config.manufacturer, sizeof(config.manufacturer), "Thingino");
    snprintf(config.friendly_name, sizeof(config.friendly_name), "%s", hostname);
    snprintf(config.suggested_area, sizeof(config.suggested_area), "");

    /* Initialize API server */
    api_server = esphome_api_init(&config);
    if (!api_server) {
        fprintf(stderr, "Failed to initialize API server\n");
        return EXIT_FAILURE;
    }

    /* Start API server */
    if (esphome_api_start(api_server) < 0) {
        fprintf(stderr, "Failed to start API server\n");
        esphome_api_free(api_server);
        return EXIT_FAILURE;
    }

    printf("ESPHome API server started successfully\n");
    printf("Listening on port 6053\n");

    /* Initialize all registered plugins */
    if (esphome_plugin_init_all(api_server, &config) < 0) {
        fprintf(stderr, "Warning: Some plugins failed to initialize\n");
    }

    printf("Plugins loaded and ready\n");

    printf("Press Ctrl+C to stop\n\n");

    /* Main loop - wait for signal */
    while (running) {
        sleep(1);
    }

    /* Cleanup */
    printf("\nShutting down...\n");

    /* Cleanup all plugins */
    esphome_plugin_cleanup_all(api_server, &config);

    esphome_api_stop(api_server);
    esphome_api_free(api_server);

    printf("Goodbye!\n");
    return EXIT_SUCCESS;
}
