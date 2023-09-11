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
#include <dirent.h>

#define TAG "web_handler"
#define IS_FILE_EXT(filename, ext) (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

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
    bool ret = false;
    FILE *fp = NULL;
    size_t chunksize = 0;

    fp = fopen(filename, "r");
    if (!fp)
    {
        ESP_LOGE(TAG, "File %s not exist", filename);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        goto __exit;
    }

    web_set_content_type(req, filename);

    while (feof(fp) == 0)
    {
        chunksize = fread(buf, 1, size, fp);

        if (ESP_OK != httpd_resp_send_chunk(req, buf, chunksize))
        {
            ESP_LOGE(TAG, "httpd_resp_send_chunk failed");
            httpd_resp_send_chunk(req, NULL, 0);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            goto __exit;
        }
    }

    ret = true;

__exit:

    if (fp)
        fclose(fp);

    return ret;
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

    if (req->method == HTTP_GET && web_resp_file(req, "/data/httpd/index.html", (char *)data->buf, CONFIG_HTTPD_RESP_BUF_SIZE))
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

bool web_list_files(const char *path, void (*cb)(httpd_req_t *, char *), httpd_req_t *req)
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
            web_list_files(file_path, cb, req);
        }
        else if (cb)
        {
            cb(req, file_path);
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

    if (req->method == HTTP_GET && web_resp_file(req, "/data/httpd/program.html", (char *)data->buf, CONFIG_HTTPD_RESP_BUF_SIZE))
    {
        httpd_resp_sendstr_chunk(req, "<body>"
                                      "<div class=\"container\">"
                                      "<div class=\"content\">"
                                      "<h1 style=\"text-align: center;\">Program Device</h1>"
                                      "<div class=\"form-group\">"
                                      "<label for=\"algorithm\" style=\"text-align: left;\">算法:</label>"
                                      "<select id=\"algorithm\">"
                                      "<option value=\"\">请选择算法</option>");
        web_list_files("/data/algo", web_add_option, req);
        httpd_resp_sendstr_chunk(req, "</select>"
                                      "</div>"
                                      "<div class=\"form-group\">"
                                      "<label for=\"program\" style=\"text-align: left;\">程序:</label>"
                                      "<select id=\"program\">"
                                      "<option value=\"\">请选择程序</option>");
        web_list_files("/data/program", web_add_option, req);
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
                                      "</div>"
                                      "</div>"
                                      "</div>"
                                      "</body>"
                                      "</html>");
        httpd_resp_send_chunk(req, NULL, 0);

        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t web_flash_handler(httpd_req_t *req)
{
    size_t offset = 0;
    esp_err_t ret = ESP_FAIL;
    web_data_t *data = (web_data_t *)req->user_ctx;
    char *buf = (char *)data->buf;

    if (req->method == HTTP_POST)
    {
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

    return ESP_FAIL;
}