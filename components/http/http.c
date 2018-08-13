#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "errno.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "url_parser.h"
#include "http.h"
#include "audio_player.h"


#define TAG "http_client"

#ifdef CONFIG_TITLE_PARSER
void title_icy_parser(char recv_buf, char *head_buf, int *pos, int *s)
{
    const char str[] = "StreamTitle='";
    const int len = strlen(str);

    if (*s == len && recv_buf == '\'')
    {
        (*s)++;
    } else if (*s == len) {
        head_buf[(*pos)++] = recv_buf;
    }

    if (*s >= len) {
        return;
    }

    if (recv_buf == str[*s]) {
        (*s)++;
    } else {
        *s = 0;
    }
}

void str_parser(const char *str, char recv_buf, int *s)
{
    if (*s == strlen(str)) {
        (*s)++;
    }

    if (*s > strlen(str)) {
        return;
    }

    if (recv_buf == str[*s]) {
        (*s)++;
    } else {
        *s = 0;
    }
}
#endif
/**
 * @brief simple http_get
 * see https://github.com/nodejs/http-parser for callback usage
 */
int http_client_get(char *uri, http_parser_settings *callbacks, void *user_data)
{
    player_t *player = user_data;
    url_t *url = url_parse(uri);

    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    char port_str[6]; // stack allocated
    snprintf(port_str, 6, "%d", url->port);

    int err = getaddrinfo(url->host, port_str, &hints, &res);
    if(err != ESP_OK || res == NULL) {
        ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
        return err;
    }

    // print resolved IP
    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

    // allocate socket
    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if(sock < 0) {
        ESP_LOGE(TAG, "... Failed to allocate socket.");
        freeaddrinfo(res);
    }
    ESP_LOGI(TAG, "... allocated socket");
    
    // receiving timeout
    struct timeval receiving_timeout;
    receiving_timeout.tv_sec = 3;
    receiving_timeout.tv_usec = 500;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
            sizeof(receiving_timeout)) < 0) {
        ESP_LOGE(TAG, "... failed to set socket receiving timeout");
        close(sock);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return err;
    }

    // connect, retrying a few times
    char retries = 0;
    while(connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        retries++;
        ESP_LOGE(TAG, "... socket connect attempt %d failed, errno=%d", retries, errno);

        if(retries > 5) {
            ESP_LOGE(TAG, "giving up");
            close(sock);
            freeaddrinfo(res);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "... connected");
    freeaddrinfo(res);

    // write http request
    char *request;
#ifdef CONFIG_TITLE_PARSER
    if(asprintf(&request, "GET %s HTTP/1.0\r\nHost: %s:%d\r\nUser-Agent: ESP32\r\nAccept: */*\r\nIcy-MetaData: 1\r\n\r\n", url->path, url->host, url->port) < 0)
#else
    if(asprintf(&request, "GET %s HTTP/1.0\r\nHost: %s:%d\r\nUser-Agent: ESP32\r\nAccept: */*\r\n\r\n", url->path, url->host, url->port) < 0)
#endif

    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "requesting %s", request);

    if (write(sock, request, strlen(request)) < 0) {
        ESP_LOGE(TAG, "... socket send failed");
        close(sock);
    }

    free(request);
    ESP_LOGI(TAG, "... socket send success");


    /* Read HTTP response */
    char recv_buf[256];
    bzero(recv_buf, sizeof(recv_buf));
    ssize_t recved;

#ifdef CONFIG_TITLE_PARSER
    int metaint = 0, recv_len = 0, body = 0, skip = 0, i, m = 0, n=0, pos, s, t = 0, u = 0;
#endif

    /* intercept on_headers_complete() */

    /* parse response */
    http_parser parser;
#ifndef CONFIG_TITLE_PARSER
    http_parser_init(&parser, HTTP_RESPONSE);
#endif
    parser.data = user_data;

    esp_err_t nparsed = 0;
    do {
        recved = read(sock, recv_buf, sizeof(recv_buf)-1);

#ifdef CONFIG_TITLE_PARSER
        if (body) {
            recv_len += recved;
        } else {
            for (i = 0; i < recved ; i++) {

                str_parser("\r\n\r\n", recv_buf[i], &u);
                if (u == 5) {
                    recv_len = recved - i;
                    body = 1;

                    callbacks->on_headers_complete(&parser);
                    nparsed = callbacks->on_body(&parser, recv_buf + i, recv_len);
                    break;
                }

                str_parser("icy-metaint:", recv_buf[i], &t);
                if (t == 13 && recv_buf[i] == '\r') {
                    t++;
                    ESP_LOGI(TAG,"icy-metaint: %d", metaint);
                } else if (t == 13) {
                    if (recv_buf[i] >= '0' && recv_buf[i] <= '9') {
                        metaint *= 10;
                        metaint += recv_buf[i] - 0x30;
                    }
                }
            }
            continue;
        }

        if (recv_len > metaint) {
            m = (recv_len - metaint);
            skip = recv_buf[recved - m];
            skip *= 16;
            skip++;
            recv_len = m - skip;
            n = (recved - m) + skip;

            if (skip != 1)
                ESP_LOGI(TAG,"header skip: %d bytes", skip);

            nparsed = callbacks->on_body(&parser, recv_buf, recved - m);

            //

            s = 0;
            pos = 0;
            for (i = recved - m; i < n ; i++) {
                if (i >= recved || pos >= TITLE_BUF)
                    break;
                title_icy_parser(recv_buf[i], player->title, &pos, &s);
            }

            while (n >= recved) {
                recved = read(sock, recv_buf, sizeof(recv_buf)-1);
                recv_len += recved;
                n = skip - m;
                m += recved;

                for (i = 0; i < n ; i++) {
                    if (i >= recved || pos >= TITLE_BUF)
                        break;
                    title_icy_parser(recv_buf[i], player->title, &pos, &s);
                }
            }

            //

            if (pos) {
                player->title[pos] = '\0';
                player->update = true;
                ESP_LOGI(TAG,"title: %s", player->title);
            }

            nparsed = callbacks->on_body(&parser, recv_buf + n , recved - n);
            continue;
        }

        // invoke on_body cb directly
        nparsed = callbacks->on_body(&parser, recv_buf, recved);
    } while(recved > 0 && nparsed >= 0);
#else
        // using http parser causes stack overflow somtimes - disable for now
        nparsed = http_parser_execute(&parser, callbacks, recv_buf, recved);
    } while (recved > 0 && nparsed >= 0 && parser.http_errno == HPE_OK);
#endif

    free(url);

    ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d", recved, errno);
    close(sock);
    ESP_LOGI(TAG, "socket closed");

    return 0;
}
