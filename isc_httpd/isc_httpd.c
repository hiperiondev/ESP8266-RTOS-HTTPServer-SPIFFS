/*
 * This file is part of the esp-iot-secure-core distribution (https://github.com/hiperiondev/esp-iot-secure-core).
 * Copyright (c) 2020 Emiliano Augusto Gonzalez (comercial@hiperion.com.ar)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "isc_common.h"
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include "esp_base64.h"
#include "tcpip_adapter.h"
#include "isc_spiffs.h"
#include "httpd.h"
#include "http_output.h"
static const char *TAG = "isc_httpd";
#define HTTP_SERVER_PORT 80

char filename[64];

static int http_send(int s, const char *buf, int len, int flags) {
    return send(s, buf, len, flags);
}

void* httpd_open(char *name, char *mode) {
    return fopen(name, mode);
}

int httpd_read(char *buf, unsigned size1, unsigned size2, void *file) {
    return fread(buf, size1, size2, file);
}

int httpd_close(void *file) {
    return fclose(file);
}

/*
static int cgi_request_handler(struct http_session *p, char *path, char *args) {
    ESP_LOGI(TAG, "REQUEST: PATH='%s' ARGS='%s'\n", path, args);

    // Else - try and open path as file
    ESP_LOGI(TAG, "exit cgi_handler. trying file ...");
    return 0;
}
*/

static void isc_httpd_task(void *pvParameters) {
    struct http_session httpd;
    struct sockaddr_in addr;
    int server_socket;
    int optval = 1;
    esp_err_t err;
    tcpip_adapter_ip_info_t ip_info;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(HTTP_SERVER_PORT);

    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const void*) &optval, sizeof(int));

    ESP_LOGI(TAG, "Listening on port %d\n", HTTP_SERVER_PORT);
    if (bind(server_socket, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        ESP_LOGI(TAG, "Listen failed\n");
    }

    if (listen(server_socket, 5) < 0) {
        ESP_LOGI(TAG, "Listen failed\n");
    }

    http_init(httpd_open, httpd_read, httpd_close, http_send);
    //http_attach_cgi_handler(cgi_request_handler);

    sprintf(httpd.http_username, "admin");
    sprintf(httpd.http_password, "admin");

    memset(&addr, 0x00, sizeof(addr));

    while (1) {
        char buffer[512];
        err = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
        ESP_LOGI(TAG, "open server socket. tcpip_adapter_get_ip_info: %d / %s", err, ip4addr_ntoa(&(ip_info.ip)));

        int s = accept(server_socket, NULL, NULL);
        if (s >= 0) {
            int res;
            ESP_LOGI(TAG, "http_new_connection");
            http_new_connection(&httpd, (int) s);
            //httpd.http_use_auth = 1;
            ESP_LOGI(TAG, "new_connection");
            while ((res = recv(s, buffer, sizeof(buffer) - 1, 0)) > 0) {
                ESP_LOGI(TAG, "recv");
                buffer[res] = '\0';
                if (http_process_data(&httpd, buffer, res))
                    continue;
                else
                    break;
            }
            ESP_LOGI(TAG, "close server socket");
            close(s);
        }
    }
    ESP_LOGI(TAG, "delete server task");
    vTaskDelete(NULL);
}

void isc_httpd_start(void) {
    xTaskCreate(
            isc_httpd_task,
            "isc_httpd_task",
            8192,
            NULL,
            12,
            NULL);
}
