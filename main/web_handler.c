/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */
#include <stdbool.h>
#include "esp_log.h"
#include "web_handler.h"
#include "cdc_uart.h"
#include "programmer.h"
#include "cJSON.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define TAG "web_handler"
#define WEB_RESOURCE_NUM 4
#define IS_FILE_EXT(filename, ext) (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

typedef struct
{
    const char *path;
    const uint8_t *start;
    const uint8_t *end;
} web_resource_map_t;

extern const uint8_t root_html_start[] asm("_binary_root_html_start");
extern const uint8_t root_html_end[] asm("_binary_root_html_end");
extern const uint8_t program_html_start[] asm("_binary_program_html_start");
extern const uint8_t program_html_end[] asm("_binary_program_html_end");
extern const uint8_t webserial_html_start[] asm("_binary_webserial_html_start");
extern const uint8_t webserial_html_end[] asm("_binary_webserial_html_end");
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");

web_resource_map_t s_resource_map[WEB_RESOURCE_NUM] = {
    {"/data/httpd/root.html", root_html_start, root_html_end},
    {"/data/httpd/favicon.ico", favicon_ico_start, favicon_ico_end},
    {"/data/httpd/program.html", program_html_start, program_html_end},
    {"/data/httpd/webserial.html", webserial_html_start, webserial_html_end}};

void web_send_to_clients(void *context, uint8_t *data, size_t size)
{
    httpd_handle_t http_server = *((httpd_handle_t *)context);
    size_t clients = CONFIG_HTTPD_MAX_OPENED_SOCKETS;
    static int client_fds[CONFIG_HTTPD_MAX_OPENED_SOCKETS] = {0};
    httpd_ws_frame_t ws_pkt = {false, false, HTTPD_WS_TYPE_TEXT, data, size};

    if (!http_server)
        return;

    if (httpd_get_client_list(http_server, &clients, client_fds) == ESP_OK)
    {
        for (size_t i = 0; i < clients; ++i)
        {
            if (httpd_ws_get_fd_info(http_server, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET)
            {
                httpd_ws_send_frame_async(http_server, client_fds[i], &ws_pkt);
            }
        }
    }
}

esp_err_t web_send_to_uart(httpd_req_t *req)
{
    esp_err_t ret = ESP_OK;
    web_data_t *data = (web_data_t *)req->user_ctx;
    httpd_ws_frame_t ws_pkt = {false, false, HTTPD_WS_TYPE_TEXT, NULL, 0};

    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }

    /* Set max_len = 0 to get the frame len */
    ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        goto __exit;
    }

    if ((ws_pkt.len > CONFIG_HTTPD_RESP_BUF_SIZE))
    {
        ESP_LOGE(TAG, "Frame length is over the limit(%d)", CONFIG_HTTPD_RESP_BUF_SIZE);
        ret = ESP_ERR_INVALID_SIZE;
        goto __exit;
    }

    ws_pkt.payload = data->buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        goto __exit;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT)
    {
        cdc_uart_write(data->buf, ws_pkt.len);
    }

    ret = ESP_OK;

__exit:

    return ret;
}

static esp_err_t web_set_content_type(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf"))
    {
        return httpd_resp_set_type(req, "application/pdf");
    }
    else if (IS_FILE_EXT(filename, ".html"))
    {
        return httpd_resp_set_type(req, "text/html");
    }
    else if (IS_FILE_EXT(filename, ".jpeg"))
    {
        return httpd_resp_set_type(req, "image/jpeg");
    }
    else if (IS_FILE_EXT(filename, ".ico"))
    {
        return httpd_resp_set_type(req, "image/x-icon");
    }

    return httpd_resp_set_type(req, "text/plain");
}

static bool web_resp_file(httpd_req_t *req, const char *filename, char *buf, size_t size)
{
    int i = 0;
    uint32_t offset = 0;
    uint32_t chunksize = 0;
    uint32_t file_size = 0;

    for (i = 0; i < WEB_RESOURCE_NUM; i++)
    {
        if (!strcmp(filename, s_resource_map[i].path))
        {
            break;
        }
    }

    if (i == WEB_RESOURCE_NUM)
    {
        ESP_LOGE(TAG, "File %s not exist", filename);
        httpd_resp_send_404(req);
        return false;
    }

    offset = 0;
    file_size = s_resource_map[i].end - s_resource_map[i].start;
    web_set_content_type(req, filename);

    while (offset < file_size)
    {
        chunksize = file_size - offset;
        chunksize = (chunksize < size) ? (chunksize) : (size);

        if (ESP_OK != httpd_resp_send_chunk(req, (const char *)s_resource_map[i].start + offset, chunksize))
        {
            ESP_LOGE(TAG, "httpd_resp_send_chunk failed");
            httpd_resp_send_chunk(req, NULL, 0);
            httpd_resp_send_500(req);
            return false;
        }

        offset += chunksize;
    }

    return true;
}

esp_err_t web_serial_handler(httpd_req_t *req)
{
    web_data_t *data = (web_data_t *)req->user_ctx;

    if ((req->method == HTTP_GET) && web_resp_file(req, "/data/httpd/webserial.html", (char *)data->buf, CONFIG_HTTPD_RESP_BUF_SIZE))
    {
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t web_index_handler(httpd_req_t *req)
{
    web_data_t *data = (web_data_t *)req->user_ctx;
    ESP_LOGI(TAG, "%d connected", httpd_req_to_sockfd(req));

    if (req->method == HTTP_GET && web_resp_file(req, "/data/httpd/root.html", (char *)data->buf, CONFIG_HTTPD_RESP_BUF_SIZE))
    {
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t web_favicon_handler(httpd_req_t *req)
{
    web_data_t *data = (web_data_t *)req->user_ctx;

    if (req->method == HTTP_GET && web_resp_file(req, "/data/httpd/favicon.ico", (char *)data->buf, CONFIG_HTTPD_RESP_BUF_SIZE))
    {
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
    }

    return ESP_FAIL;
}

bool web_list_files(const char *root, const char *path, void (*cb)(httpd_req_t *, char *), httpd_req_t *req)
{
#define PATH_MAX_SIZE 260

    DIR *dir = NULL;
    bool ret = false;
    struct dirent *entry = NULL;
    char *file_path = (char *)malloc(PATH_MAX_SIZE);

    if (!file_path)
    {
        ESP_LOGE(TAG, "Memory not enough");
        goto __exit;
    }

    dir = opendir(path);
    if (dir == NULL)
    {
        ESP_LOGE(TAG, "Open directory %s failed", path);
        goto __exit;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        snprintf(file_path, PATH_MAX_SIZE, "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_DIR)
        {
            web_list_files(root, file_path, cb, req);
        }
        else if (cb)
        {
            cb(req, file_path + strlen(root) + 1);
        }
    }

    ret = true;

__exit:

    if (dir)
        closedir(dir);

    if (file_path)
        free(file_path);

    return ret;
}

void web_add_option(httpd_req_t *req, char *path)
{
    httpd_resp_sendstr_chunk(req, "<option value=");
    httpd_resp_sendstr_chunk(req, path);
    httpd_resp_sendstr_chunk(req, ">");
    httpd_resp_sendstr_chunk(req, path);
    httpd_resp_sendstr_chunk(req, "</option>");
}

esp_err_t web_program_handler(httpd_req_t *req)
{
    web_data_t *data = (web_data_t *)req->user_ctx;

    if (req->method != HTTP_GET || !web_resp_file(req, "/data/httpd/program.html", (char *)data->buf, CONFIG_HTTPD_RESP_BUF_SIZE))
    {
        return ESP_FAIL;
    }

    httpd_resp_sendstr_chunk(req, "<body>"
                                  "<div class=\"container\">"
                                  "<div class=\"content\">"
                                  "<h1 style=\"text-align: center;\">Program Device</h1>"
                                  "<div class=\"form-group\">"
                                  "<label for=\"algorithm\" style=\"text-align: left;\">算法:</label>"
                                  "<select id=\"algorithm\">"
                                  "<option value=\"\">请选择算法</option>");
    web_list_files(CONFIG_PROGRAMMER_ALGORITHM_ROOT, CONFIG_PROGRAMMER_ALGORITHM_ROOT, web_add_option, req);
    httpd_resp_sendstr_chunk(req, "</select>"
                                  "</div>"
                                  "<div class=\"form-group\">"
                                  "<label for=\"program\" style=\"text-align: left;\">程序:</label>"
                                  "<select id=\"program\">"
                                  "<option value=\"\">请选择程序</option>");
    web_list_files(CONFIG_PROGRAMMER_PROGRAM_ROOT, CONFIG_PROGRAMMER_PROGRAM_ROOT, web_add_option, req);
    httpd_resp_sendstr_chunk(req, "</select>"
                                  "</div>"
                                  "<div class=\"form-group\">"
                                  "<label for=\"flash-address\" style=\"text-align: left;\">Flash地址:</label>"
                                  "<input type=\"text\" id=\"flash-address\" placeholder=\"请输入Flash地址\">"
                                  "</div>"
                                  "<div class=\"form-group\">"
                                  "<label for=\"ram-address\" style=\"text-align: left;\">RAM地址:</label>"
                                  "<input type=\"text\" id=\"ram-address\" placeholder=\"请输入RAM地址\">"
                                  "</div>"
                                  "<div class=\"form-group\">"
                                  "<button id=\"program-button\">烧录</button>"
                                  "</div>");
    httpd_resp_sendstr_chunk(req, "<div class=\"form-group\">"
                                  "<label for=\"upload-program\" style=\"text-align: left; margin-top: 60px;\">上传程序:</label>"
                                  "<input type=\"file\" id=\"upload-program\" style=\"height: 100%; padding: 4px;\">"
                                  "</div>"
                                  "<div class=\"form-group\">"
                                  "<button id=\"upload-program-button\">上传</button>"
                                  "</div>"
                                  "<div class=\"form-group\">"
                                  "<label for=\"upload-algorithm\" style=\"text-align: left; margin-top: 60px;\">上传算法:</label>"
                                  "<input type=\"file\" id=\"upload-algorithm\" style=\"height: 100%; padding: 4px;\">"
                                  "</div>"
                                  "<div class=\"form-group\">"
                                  "<button id=\"upload-algorithm-button\">上传</button>"
                                  "</div>"
                                  "</div>"
                                  "</div>"
                                  "<div class=\"progress-container\">"
                                  "<progress id=\"progress\" value=\"0\" max=\"100\"></progress>"
                                  "</div>"
                                  "</body>"
                                  "</html>");
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

esp_err_t web_flash_handler(httpd_req_t *req)
{
    size_t offset = 0;
    esp_err_t ret = ESP_FAIL;
    web_data_t *data = (web_data_t *)req->user_ctx;
    char *buf = (char *)data->buf;

    if (req->method != HTTP_POST)
    {
        return ESP_FAIL;
    }

    if (req->content_len >= CONFIG_HTTPD_RESP_BUF_SIZE)
    {
        ESP_LOGE(TAG, "Failed to allocate memory of %d bytes!", req->content_len + 1);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (offset < req->content_len)
    {
        ret = httpd_req_recv(req, buf + offset, req->content_len - offset);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                httpd_resp_send_408(req);
            }

            return ESP_FAIL;
        }

        offset += ret;
    }

    buf[offset] = '\0';

    if (programmer_put_cmd(buf, offset))
    {
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_sendstr(req, "Start to program");
        return ESP_OK;
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Program failed");
        return ESP_FAIL;
    }
}

static const char *web_get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest)
    {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize)
    {
        return NULL;
    }

    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    return dest + base_pathlen;
}

static esp_err_t web_upload_file(httpd_req_t *req, const char *base_path, const char *uri_prefix)
{
#define PROGRAM_MAX_SIZE 0xA00000
#define PROGRAM_MAX_SIZE_STR "10M"

    FILE *fd = NULL;
    int received = 0;
    struct stat file_stat = {0};
    int remaining = req->content_len;
    web_data_t *data = (web_data_t *)req->user_ctx;
    char *buf = (char *)data->buf;
    static char filepath[CONFIG_PROGRAMMER_FILE_MAX_LEN] = {0};

    web_get_path_from_uri(filepath, base_path, req->uri + strlen(uri_prefix), CONFIG_PROGRAMMER_FILE_MAX_LEN);
    ESP_LOGI(TAG, "File name : %s", filepath);

    if (stat(base_path, &file_stat) != 0)
    {
        mkdir(base_path, 0777);
    }

    if (stat(filepath, &file_stat) == 0)
    {
        unlink(filepath);
    }

    if (req->content_len > PROGRAM_MAX_SIZE)
    {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File size must be less than " PROGRAM_MAX_SIZE_STR "!");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd)
    {
        ESP_LOGE(TAG, "Failed to create file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    while (remaining > 0)
    {
        ESP_LOGD(TAG, "Remaining size : %d", remaining);
        received = httpd_req_recv(req, buf, (remaining <= CONFIG_HTTPD_RESP_BUF_SIZE) ? (remaining) : (CONFIG_HTTPD_RESP_BUF_SIZE));

        if (received <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }

            fclose(fd);
            unlink(filepath);
            ESP_LOGE(TAG, "File reception failed!");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        if (received && (received != fwrite(buf, 1, received, fd)))
        {
            /* Couldn't write everything to file! Storage may be full? */
            fclose(fd);
            unlink(filepath);
            ESP_LOGE(TAG, "File write failed!");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        remaining -= received;
    }

    fclose(fd);
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, "File uploaded successfully");
    ESP_LOGI(TAG, "File reception complete");

    return ESP_OK;
}

esp_err_t web_upload_program_handler(httpd_req_t *req)
{
    return web_upload_file(req, CONFIG_PROGRAMMER_PROGRAM_ROOT, "/upload-program");
}

esp_err_t web_upload_algorithm_handler(httpd_req_t *req)
{
    return web_upload_file(req, CONFIG_PROGRAMMER_ALGORITHM_ROOT, "/upload-algorithm");
}

esp_err_t web_program_progress_handler(httpd_req_t *req)
{
    static char json_buf[64] = {0};

    snprintf(json_buf, sizeof(json_buf), "{\"progress\": %d, \"status\": \"%s\"}", programmer_get_progress(), programmer_is_busy() ? ("busy") : ("idle"));
    httpd_resp_send_chunk(req, json_buf, strlen(json_buf));
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}