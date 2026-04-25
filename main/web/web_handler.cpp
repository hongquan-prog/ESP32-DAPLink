/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 * 2026-3-17     refactor     Integrate SerialManager for web serial
 */
#include <stdbool.h>
#include <string.h>
#include <memory>
#include "esp_log.h"
#include "web/web_handler.h"
#include "serial/cdc_uart.h"
#include "programmer/programmer.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "hex_program.h"
#include "serial/serial_manager.h"
#include "wifi.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <iomanip>

#define TAG "web_handler"
#define WEB_RESOURCE_NUM 5
#define IS_FILE_EXT(filename, ext) (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

typedef struct
{
    const char *path;
    const uint8_t *start;
    const uint8_t *end;
} web_resource_map_t;

extern const uint8_t root_html_start[] asm("_binary_root_html_start");
extern const uint8_t root_html_end[] asm("_binary_root_html_end");
extern const uint8_t webserial_html_start[] asm("_binary_webserial_html_start");
extern const uint8_t webserial_html_end[] asm("_binary_webserial_html_end");
extern const uint8_t upgrade_html_start[] asm("_binary_upgrade_html_start");
extern const uint8_t upgrade_html_end[] asm("_binary_upgrade_html_end");
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");
extern const uint8_t config_html_start[] asm("_binary_settings_html_start");
extern const uint8_t config_html_end[] asm("_binary_settings_html_end");

web_resource_map_t s_resource_map[WEB_RESOURCE_NUM] = {
    {"/data/httpd/root.html", root_html_start, root_html_end},
    {"/data/httpd/favicon.ico", favicon_ico_start, favicon_ico_end},
    {"/data/httpd/webserial.html", webserial_html_start, webserial_html_end},
    {"/data/httpd/upgrade.html", upgrade_html_start, upgrade_html_end},
    {"/data/httpd/settings.html", config_html_start, config_html_end}};

void web_send_to_clients(void *context, uint8_t *data, size_t size)
{
    httpd_handle_t http_server = *((httpd_handle_t *)context);
    size_t clients = CONFIG_HTTPD_MAX_OPENED_SOCKETS;
    static int client_fds[CONFIG_HTTPD_MAX_OPENED_SOCKETS] = {0};
    httpd_ws_frame_t ws_pkt = {false, false, HTTPD_WS_TYPE_TEXT, data, size};

    if (!http_server)
        return;

    /* Only send to web clients when in WEB state */
    if (serial_manager_get_state() != SERIAL_STATE_WEB)
    {
        return;
    }

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

static httpd_handle_t s_http_server = NULL;

void web_notify_serial_state(int state)
{
    size_t clients = CONFIG_HTTPD_MAX_OPENED_SOCKETS;
    static int client_fds[CONFIG_HTTPD_MAX_OPENED_SOCKETS] = {0};
    const char *state_str = "IDLE";
    char json_msg[64] = {0};
    httpd_ws_frame_t ws_pkt = {false, false, HTTPD_WS_TYPE_TEXT, NULL, 0};

    if (!s_http_server)
        return;

    /* Build JSON message: {"type":"state_change","state":"USB"} */
    switch (state)
    {
    case SERIAL_STATE_USB:
        state_str = "USB";
        break;

    case SERIAL_STATE_WEB:
        state_str = "WEB";
        break;

    case SERIAL_STATE_IDLE:
    default:
        state_str = "IDLE";
        break;
    }

    snprintf(json_msg, sizeof(json_msg), "{\"type\":\"state_change\",\"state\":\"%s\"}", state_str);

    ws_pkt.payload = (uint8_t *)json_msg;
    ws_pkt.len = strlen(json_msg);

    if (httpd_get_client_list(s_http_server, &clients, client_fds) == ESP_OK)
    {
        for (size_t i = 0; i < clients; ++i)
        {
            if (httpd_ws_get_fd_info(s_http_server, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET)
            {
                httpd_ws_send_frame_async(s_http_server, client_fds[i], &ws_pkt);
            }
        }
    }
}

void web_set_server_handle(httpd_handle_t server)
{
    s_http_server = server;
}

esp_err_t web_send_to_uart(httpd_req_t *req)
{
    esp_err_t ret = ESP_OK;
    web_data_t *data = (web_data_t *)req->user_ctx;
    httpd_ws_frame_t ws_pkt = {false, false, HTTPD_WS_TYPE_TEXT, NULL, 0};

    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        // Notify SerialManager about new web client
        serial_manager_web_client_connected();
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

    // Handle close frame
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE)
    {
        ESP_LOGI(TAG, "WebSocket close frame received");
        serial_manager_web_client_disconnected();
        goto __exit;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT)
    {
        // Use SerialManager to send data
        serial_manager_send_to_uart(data->buf, ws_pkt.len);
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

esp_err_t web_upgrade_handler(httpd_req_t *req)
{
    web_data_t *data = (web_data_t *)req->user_ctx;
    int received = 0;
    int remaining = req->content_len;
    int total = 0;

    if (req->method == HTTP_GET)
    {
        if (web_resp_file(req, "/data/httpd/upgrade.html", (char *)data->buf, CONFIG_HTTPD_RESP_BUF_SIZE))
        {
            httpd_resp_send_chunk(req, NULL, 0);
            return ESP_OK;
        }

        return ESP_FAIL;
    }

    if (req->method == HTTP_POST)
    {
        // OTA 升级处理
        esp_ota_handle_t update_handle = 0;
        const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

        if (!update_partition)
        {
            ESP_LOGE(TAG, "No update partition found");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No update partition");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Starting OTA update, size: %d", req->content_len);

        esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_begin failed: %d", err);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
            return ESP_FAIL;
        }

        while (remaining > 0)
        {
            received = httpd_req_recv(req, (char *)data->buf,
                                      remaining < CONFIG_HTTPD_RESP_BUF_SIZE ? remaining : CONFIG_HTTPD_RESP_BUF_SIZE);
            if (received <= 0)
            {
                if (received == HTTPD_SOCK_ERR_TIMEOUT)
                    continue;
                esp_ota_abort(update_handle);
                return ESP_FAIL;
            }

            err = esp_ota_write(update_handle, data->buf, received);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_ota_write failed: %d", err);
                esp_ota_abort(update_handle);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
                return ESP_FAIL;
            }

            total += received;
            remaining -= received;
            ESP_LOGD(TAG, "OTA progress: %d/%d", total, req->content_len);
        }

        err = esp_ota_end(update_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_end failed: %d", err);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
            return ESP_FAIL;
        }

        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %d", err);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "OTA update successful, rebooting...");
        httpd_resp_sendstr(req, "OTA success, rebooting...");

        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
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
        mkdir(path, 0777);
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
    char *buf = NULL;
    size_t buf_size = 0;
    bool is_zh = false;

    if (req->method != HTTP_GET)
    {
        return ESP_FAIL;
    }

    /* Check for language preference from query string */
    buf_size = httpd_req_get_url_query_len(req) + 1;
    if (buf_size > 1)
    {
        buf = (char *)malloc(buf_size);
        if (buf)
        {
            if (httpd_req_get_url_query_str(req, buf, buf_size) == ESP_OK)
            {
                char lang[4] = {0};
                if (httpd_query_key_value(buf, "lang", lang, sizeof(lang)) == ESP_OK)
                {
                    is_zh = (strcmp(lang, "zh") == 0);
                }
            }
            free(buf);
        }
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html>"
                                  "<html lang=\"en\">"
                                  "<head>"
                                  "<meta charset=\"utf-8\">"
                                  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
                                  "<title>Program Device</title>"
                                  "<style>"
                                  "*{margin:0;padding:0;box-sizing:border-box}"
                                  "body{font-family:'Segoe UI','Microsoft YaHei',sans-serif;background:linear-gradient(135deg,#0a0a0f 0%,#1a1a2e 50%,#0a0a0f 100%);color:#fff}"
                                  ".container{max-width:800px;margin:0 auto;padding:15px}"
                                  ".header{text-align:center;margin-bottom:20px}"
                                  ".content{background:rgba(18,18,26,.8);backdrop-filter:blur(10px);border:1px solid rgba(0,245,255,.1);border-radius:15px;padding:25px}"
                                  ".form-group{margin-bottom:20px}"
                                  ".form-group label{display:block;margin-bottom:8px;color:rgba(0,245,255,.8);font-weight:600}"
                                  ".form-group input,.form-group select,.form-group button{width:100%;padding:12px;border-radius:8px;font-size:14px}"
                                  ".form-group input,.form-group select{background:rgba(0,0,0,.3);border:1px solid rgba(0,245,255,.2);color:#fff}"
                                  ".form-group button{background:linear-gradient(135deg,rgba(0,245,255,.2),rgba(123,44,191,.2));border:1px solid rgba(0,245,255,.3);color:#fff;font-weight:600;cursor:pointer}"
                                  ".form-group button:hover{border-color:rgba(0,245,255,.5);transform:translateY(-2px)}"
                                  ".progress-container{margin:20px 0}"
                                  ".progress-container progress{width:100%;height:20px;border-radius:10px}"
                                  ".log-section{background:rgba(0,0,0,.3);border-radius:8px;padding:10px;max-height:150px;overflow-y:auto}"
                                  ".log-entry{padding:5px 0;border-bottom:1px solid rgba(255,255,255,.05);font-size:13px}"
                                  ".log-entry.info{color:#00f5ff}"
                                  ".log-entry.success{color:#00ff88}"
                                  ".log-entry.error{color:#ff006e}"
                                  ".log-entry.warning{color:#ffaa00}"
                                  ".drop-zone{border:2px dashed rgba(0,245,255,.3);border-radius:10px;padding:20px;text-align:center;cursor:pointer;transition:all .3s}"
                                  ".drop-zone:hover{border-color:rgba(0,245,255,.6);background:rgba(0,245,255,.02)}"
                                  ".file-info{background:rgba(0,245,255,.05);border-radius:8px;padding:10px;margin-top:10px}"
                                  "</style>"
                                  "</head>"
                                  "<body>"
                                  "<div class=\"container\">"
                                  "<div class=\"header\">"
                                  "<p>");

    /* i18n JavaScript translations */
    const char *i18n_js =
        "var _i18n={en:{},zh:{}},_t=function(k){var l=localStorage.getItem('lang')||'en';return _i18n[l][k]||k;};"
        "_i18n.en={"
        "'program.title':'Offline Programming Tool',"
        "'program.algorithm':'Algorithm File',"
        "'program.select_algorithm':'Select algorithm',"
        "'program.program':'Program File',"
        "'program.select_program':'Select program',"
        "'program.start_addr':'Flash Address',"
        "'program.start_addr_hint':'BIN needs manual input, HEX auto-parses',"
        "'program.ram_addr':'RAM Address',"
        "'program.ram_hint':'e.g.: 0x20000000',"
        "'program.offline_program':'Offline Program',"
        "'program.online_program':'Online Program',"
        "'program.drag_hint':'Drag file here or click to select',"
        "'program.start':'Start Programming',"
        "'program.ready':'[System] Ready',"
        "'program.progress':'Progress: {p}% ({s})',"
        "'program.complete':'Programming complete!',"
        "'program.selected':'Selected: {name}',"
        "'program.parsing_hex':'Parsing HEX file address...',"
        "'program.hex_parsed':'HEX address auto-parsed: {addr}',"
        "'program.hex_not_found':'HEX address not found, please fill manually',"
        "'program.bin_manual':'BIN file needs manual Flash address',"
        "'program.hex_auto':'hex address auto-parsed',"
        "'program.hex_failed':'Failed to parse hex file',"
        "'program.bin_manual2':'BIN file needs manual Flash address',"
        "'program.select_both':'Please select program and algorithm',"
        "'program.start_offline':'Starting offline programming: {name}',"
        "'program.starting':'Programming started',"
        "'program.program_failed':'Failed: {err}',"
        "'program.select_file':'Please select file',"
        "'program.select_algorithm2':'Please select algorithm',"
        "'program.upload_algorithm':'Upload Algorithm',"
        "'program.algorithm_upload_hint':'Drag FLM file here or click to upload',"
        "'program.algorithm_uploaded':'Algorithm uploaded: {name}',"
        "'program.algorithm_upload_failed':'Algorithm upload failed',"
        "'program.start_online':'Starting online programming: {name} ({fmt})',"
        "'program.config_sent':'Config sent, uploading file...',"
        "'program.config_failed':'Config failed: {err}',"
        "'program.upload_success':'File uploaded successfully',"
        "'program.upload_failed':'Upload failed',"
        "'program.parse_failed':'Parse failed: {err}'"
        "};"
        "_i18n.zh={"
        "'program.title':'离线烧录工具',"
        "'program.algorithm':'算法文件',"
        "'program.select_algorithm':'请选择算法',"
        "'program.program':'程序文件',"
        "'program.select_program':'请选择程序',"
        "'program.start_addr':'烧录地址',"
        "'program.start_addr_hint':'bin 文件需要手动填写，hex 文件自动解析',"
        "'program.ram_addr':'RAM 地址',"
        "'program.ram_hint':'例如：0x20000000',"
        "'program.offline_program':'离线烧录',"
        "'program.online_program':'在线烧录',"
        "'program.drag_hint':'拖拽文件到此处或点击选择',"
        "'program.start':'开始烧录',"
        "'program.ready':'[系统] 准备就绪',"
        "'program.progress':'进度：{p}% ({s})',"
        "'program.complete':'烧录完成！',"
        "'program.selected':'已选择：{name}',"
        "'program.parsing_hex':'正在解析 HEX 文件地址...',"
        "'program.hex_parsed':'HEX 文件地址已自动解析：{addr}',"
        "'program.hex_not_found':'未找到 HEX 地址，请手动填写',"
        "'program.bin_manual':'BIN 文件请手动填写 Flash 地址',"
        "'program.hex_auto':'hex 文件地址已自动解析',"
        "'program.hex_failed':'解析 hex 文件失败',"
        "'program.bin_manual2':'bin 文件请手动填写 Flash 地址',"
        "'program.select_both':'请选择程序和算法',"
        "'program.start_offline':'开始离线烧录：{name}',"
        "'program.starting':'烧录已开始',"
        "'program.program_failed':'失败：{err}',"
        "'program.select_file':'请选择文件',"
        "'program.select_algorithm2':'请选择算法',"
        "'program.upload_algorithm':'上传算法',"
        "'program.algorithm_upload_hint':'拖拽 FLM 文件到此处或点击上传',"
        "'program.algorithm_uploaded':'算法已上传：{name}',"
        "'program.algorithm_upload_failed':'算法上传失败',"
        "'program.start_online':'开始在线烧录：{name} ({fmt})',"
        "'program.config_sent':'配置已发送，开始上传文件...',"
        "'program.config_failed':'配置失败：{err}',"
        "'program.upload_success':'文件上传成功',"
        "'program.upload_failed':'上传失败',"
        "'program.parse_failed':'解析失败：{err}'"
        "};";

    const char *title = is_zh ? "离线烧录工具" : "Offline Programming Tool";
    const char *algorithm_label = is_zh ? "算法文件" : "Algorithm File";
    const char *select_algorithm = is_zh ? "请选择算法" : " Select algorithm";
    const char *program_label = is_zh ? "程序文件" : "Program File";
    const char *select_program = is_zh ? "请选择程序" : "Select program";
    const char *start_addr_label = is_zh ? "烧录地址" : "Flash Address";
    const char *start_addr_ph = is_zh ? "bin 文件需要手动填写，hex 文件自动解析" : "BIN needs manual input, HEX auto-parses";
    const char *ram_addr_label = is_zh ? "RAM 地址" : "RAM Address";
    const char *ram_addr_ph = is_zh ? "例如：0x20000000" : "e.g.: 0x20000000";
    const char *offline_program_btn = is_zh ? "离线烧录" : "Offline Program";
    const char *online_program_label = is_zh ? "在线烧录" : "Online Program";
    const char *drag_hint = is_zh ? "拖拽文件到此处或点击选择" : "Drag file here or click to select";
    const char *start_btn = is_zh ? "开始在线烧录" : "Start Programming";
    const char *ready_msg = is_zh ? "[系统] 准备就绪" : "[System] Ready";
    const char *upload_algorithm = is_zh ? "上传算法文件" : "Upload Algorithm File";
    const char *algo_upload_hint = is_zh ? "拖拽 FLM 文件到此处或点击上传" : "Drag FLM file here or click to upload";

    httpd_resp_sendstr_chunk(req, "<div class=\"container\">"
                                  "<div class=\"header\">"
                                  "<h1>🔧 ESP32 DAPLink</h1>"
                                  "<p>");
    httpd_resp_sendstr_chunk(req, title);
    httpd_resp_sendstr_chunk(req, "</p>"
                                  "</div>"
                                  "<div class=\"content\">"
                                  "<div class=\"form-group\">"
                                  "<label for=\"algorithm\">");
    httpd_resp_sendstr_chunk(req, algorithm_label);
    httpd_resp_sendstr_chunk(req, "</label>"
                                  "<select id=\"algorithm\">"
                                  "<option value=\"\">");
    httpd_resp_sendstr_chunk(req, select_algorithm);
    httpd_resp_sendstr_chunk(req, "</option>");
    web_list_files(CONFIG_PROGRAMMER_ALGORITHM_ROOT, CONFIG_PROGRAMMER_ALGORITHM_ROOT, web_add_option, req);
    httpd_resp_sendstr_chunk(req, "</select>"
                                  "</div>"
                                  "<div class=\"form-group\">"
                                  "<label>");
    httpd_resp_sendstr_chunk(req, upload_algorithm);
    httpd_resp_sendstr_chunk(req, "</label>"
                                  "<div id=\"algo-drop-zone\" style=\"border:2px dashed rgba(0,245,255,0.3);border-radius:10px;padding:20px;text-align:center;cursor:pointer;background:rgba(0,245,255,0.02);\" "
                                  "onclick=\"document.getElementById('algo-file').click()\" "
                                  "ondragover=\"event.preventDefault();this.style.borderColor='var(--primary-color);\" "
                                  "ondragleave=\"this.style.borderColor='rgba(0,245,255,0.3)';\" "
                                  "ondrop=\"handleAlgoDrop(event);\">"
                                  "<div style=\"font-size:1.5em;margin-bottom:5px;\">📁</div>"
                                  "<div style=\"font-size:0.85em;color:rgba(255,255,255,0.6);\">");
    httpd_resp_sendstr_chunk(req, algo_upload_hint);
    httpd_resp_sendstr_chunk(req, "</div>"
                                  "<input type=\"file\" id=\"algo-file\" accept=\"*.flm\" style=\"display:none;\" onchange=\"handleAlgoFile(this.files[0]);\">"
                                  "</div>"
                                  "<div id=\"algo-file-info\" style=\"display:none;margin-top:10px;padding:10px;background:rgba(0,245,255,0.05);border-radius:8px;\">"
                                  "<div id=\"algo-file-name\" style=\"color:var(--primary-color);font-weight:600;\"></div>"
                                  "<div id=\"algo-file-size\" style=\"color:var(--text-secondary);font-size:0.9em;\"></div>"
                                  "</div>"
                                  "<div class=\"form-group\">"
                                  "<label for=\"offline-program\">");
    httpd_resp_sendstr_chunk(req, program_label);
    httpd_resp_sendstr_chunk(req, "</label>"
                                  "<select id=\"offline-program\">"
                                  "<option value=\"\">");
    httpd_resp_sendstr_chunk(req, select_program);
    httpd_resp_sendstr_chunk(req, "</option>");
    web_list_files(CONFIG_PROGRAMMER_PROGRAM_ROOT, CONFIG_PROGRAMMER_PROGRAM_ROOT, web_add_option, req);
    httpd_resp_sendstr_chunk(req, "</select>"
                                  "</div>"
                                  "<div class=\"form-group\">"
                                  "<label for=\"start-address\">");
    httpd_resp_sendstr_chunk(req, start_addr_label);
    httpd_resp_sendstr_chunk(req, "</label>"
                                  "<input type=\"text\" id=\"start-address\" placeholder=\"");
    httpd_resp_sendstr_chunk(req, start_addr_ph);
    httpd_resp_sendstr_chunk(req, "\">"
                                  "</div>"
                                  "<div class=\"form-group\">"
                                  "<label for=\"ram-address\">");
    httpd_resp_sendstr_chunk(req, ram_addr_label);
    httpd_resp_sendstr_chunk(req, "</label>"
                                  "<input type=\"text\" id=\"ram-address\" placeholder=\"");
    httpd_resp_sendstr_chunk(req, ram_addr_ph);
    httpd_resp_sendstr_chunk(req, "\" value=\"0x20000000\">"
                                  "</div>"
                                  "<div class=\"form-group\">"
                                  "<button id=\"offline-program-btn\">");
    httpd_resp_sendstr_chunk(req, offline_program_btn);
    httpd_resp_sendstr_chunk(req, "</button>"
                                  "</div>"
                                  "<hr style=\"border:0;border-top:1px solid rgba(0,245,255,0.2);margin:20px 0;\">"
                                  "<div class=\"form-group\">"
                                  "<label>");
    httpd_resp_sendstr_chunk(req, online_program_label);
    httpd_resp_sendstr_chunk(req, "</label>"
                                  "<div id=\"drop-zone\" style=\"border:2px dashed rgba(0,245,255,0.3);border-radius:10px;padding:30px;text-align:center;cursor:pointer;background:rgba(0,245,255,0.02);\" "
                                  "onclick=\"document.getElementById('online-file').click()\" "
                                  "ondragover=\"event.preventDefault();this.style.borderColor='var(--primary-color);\" "
                                  "ondragleave=\"this.style.borderColor='rgba(0,245,255,0.3)';\" "
                                  "ondrop=\"handleDrop(event);\">"
                                  "<div style=\"font-size:2em;margin-bottom:10px;\">📤</div>"
                                  "<div>");
    httpd_resp_sendstr_chunk(req, drag_hint);
    httpd_resp_sendstr_chunk(req, "</div>"
                                  "<input type=\"file\" id=\"online-file\" accept=\"*.bin,*.hex\" style=\"display:none;\" onchange=\"handleFile(this.files[0]);\">"
                                  "</div>"
                                  "<div id=\"file-info\" style=\"display:none;margin-top:10px;padding:10px;background:rgba(0,245,255,0.05);border-radius:8px;\">"
                                  "<div id=\"file-name\" style=\"color:var(--primary-color);font-weight:600;\"></div>"
                                  "<div id=\"file-size\" style=\"color:var(--text-secondary);font-size:0.9em;\"></div>"
                                  "</div>"
                                  "<button id=\"online-program-btn\" style=\"margin-top:10px;display:none;\">");
    httpd_resp_sendstr_chunk(req, start_btn);
    httpd_resp_sendstr_chunk(req, "</button>"
                                  "</div>"
                                  "<div class=\"progress-container\">"
                                  "<progress id=\"progress\" value=\"0\" max=\"100\"></progress>"
                                  "</div>"
                                  "<div class=\"log-section\" id=\"log-section\">"
                                  "<div class=\"log-entry\">");
    httpd_resp_sendstr_chunk(req, ready_msg);
    httpd_resp_sendstr_chunk(req, "</div>"
                                  "</div>"
                                  "</div>"
                                  "</div>"
                                  "<script>");
    httpd_resp_sendstr_chunk(req, i18n_js);
    httpd_resp_sendstr_chunk(req,
                                  "var pollTimer=null,selectedFile=null;"
                                  "function fmt(s,d){for(var k in d)s=s.replace('{'+k+'}',d[k]);return s;}"
                                  "function addLog(m,t){var l=document.getElementById('log-section');if(!l)return;var e=document.createElement('div');e.className='log-entry '+t;e.innerHTML='['+new Date().toLocaleTimeString()+'] '+m;l.appendChild(e);l.scrollTop=l.scrollHeight;}"
                                  "function updateProgress(p){document.getElementById('progress').value=p;}"
                                  "function pollStatus(){fetch('/api/query?type=program-status').then(function(r){return r.json();}).then(function(d){var p=d.progress||0;var s=d.status||'unknown';updateProgress(p);addLog(fmt(_t('program.progress'),{p:p,s:s}),'info');if(p>=100||s=='idle'){clearInterval(pollTimer);addLog(_t('program.complete'),'success');}}).catch(function(e){});}"
                                  "function handleDrop(e){e.preventDefault();if(e.dataTransfer.files.length)handleFile(e.dataTransfer.files[0]);}"
                                  "function handleFile(f){if(!f)return;selectedFile=f;document.getElementById('file-name').textContent=f.name;document.getElementById('file-size').textContent=(f.size/1024).toFixed(1)+' KB';document.getElementById('file-info').style.display='block';document.getElementById('online-program-btn').style.display='inline-block';addLog(fmt(_t('program.selected'),{name:f.name}),'info');if(f.name.toLowerCase().endsWith('.hex')){addLog(_t('program.parsing_hex'),'info');fetch('/api/parse-start-addr',{method:'POST',body:f}).then(function(r){return r.json();}).then(function(d){if(d.start_addr){document.getElementById('start-address').value=d.start_addr;addLog(fmt(_t('program.hex_parsed'),{addr:d.start_addr}),'success');}else{document.getElementById('start-address').value='';addLog(_t('program.hex_not_found'),'warning');}}).catch(function(e){addLog(fmt(_t('program.parse_failed'),{err:e.message}),'warning');});}else{document.getElementById('start-address').value='';addLog(_t('program.bin_manual'),'warning');}}"
                                  "function handleAlgoDrop(e){e.preventDefault();if(e.dataTransfer.files.length)handleAlgoFile(e.dataTransfer.files[0]);}"
                                  "function handleAlgoFile(f){if(!f)return;addLog(fmt(_t('program.selected'),{name:f.name}),'info');var x=new XMLHttpRequest();x.open('POST','/api/upload?location=algorithm&name='+encodeURIComponent(f.name)+'&overwrite=true');x.onload=function(){if(x.status==200){document.getElementById('algo-file-name').textContent=f.name;document.getElementById('algo-file-size').textContent=(f.size/1024).toFixed(1)+' KB';document.getElementById('algo-file-info').style.display='block';addLog(fmt(_t('program.algorithm_uploaded'),{name:f.name}),'success');var opt=document.createElement('option');opt.value=f.name;opt.textContent=f.name;var sel=document.getElementById('algorithm');sel.insertBefore(opt,sel.firstChild);sel.value=f.name;}else{addLog(_t('program.algorithm_upload_failed'),'error');}};x.onerror=function(){addLog(_t('program.algorithm_upload_failed'),'error');};x.send(f);}"
                                  "document.getElementById('offline-program').onchange=function(){var p=this.value;if(!p){document.getElementById('start-address').value='';return;}if(p.toLowerCase().endsWith('.hex')){fetch('/api/query?type=start-addr&file='+encodeURIComponent(p)).then(function(r){return r.json();}).then(function(d){if(d.start_addr){document.getElementById('start-address').value=d.start_addr;}addLog(_t('program.hex_auto'),'success');}).catch(function(e){addLog(_t('program.hex_failed'),'warning');});}else{document.getElementById('start-address').value='';addLog(_t('program.bin_manual2'),'warning');}};"
                                  "document.getElementById('offline-program-btn').onclick=function(){var p=document.getElementById('offline-program').value;var a=document.getElementById('algorithm').value;var fa=document.getElementById('start-address').value;var ra=document.getElementById('ram-address').value;if(!p||!a){addLog(_t('program.select_both'),'error');return;}addLog(fmt(_t('program.start_offline'),{name:p}),'info');var x=new XMLHttpRequest();x.open('POST','/program');x.setRequestHeader('Content-Type','application/json');x.onload=function(){if(x.status==200){addLog(_t('program.starting'),'success');pollTimer=setInterval(pollStatus,1000);}else{addLog(fmt(_t('program.program_failed'),{err:x.responseText}),'error');}};x.send(JSON.stringify({program:p,algorithm:a,start_addr:parseInt(fa),ram_addr:parseInt(ra),program_mode:'offline',format:'bin',total_size:0}));};"
                                  "document.getElementById('online-program-btn').onclick=function(){if(!selectedFile){addLog(_t('program.select_file'),'error');return;}var a=document.getElementById('algorithm').value;var fa=document.getElementById('start-address').value;var ra=document.getElementById('ram-address').value;if(!a){addLog(_t('program.select_algorithm2'),'error');return;}var fileFormat=selectedFile.name.toLowerCase().endsWith('.hex')?'hex':'bin';addLog(fmt(_t('program.start_online'),{name:selectedFile.name,fmt:fileFormat}),'info');var x=new XMLHttpRequest();x.open('POST','/program');x.setRequestHeader('Content-Type','application/json');x.onload=function(){if(x.status==200){addLog(_t('program.config_sent'),'success');uploadFile(selectedFile);}else{addLog(fmt(_t('program.config_failed'),{err:x.responseText}),'error');}};x.send(JSON.stringify({algorithm:a,start_addr:parseInt(fa),ram_addr:parseInt(ra),program_mode:'online',format:fileFormat,total_size:selectedFile.size}));};"
                                  "function uploadFile(file){var x=new XMLHttpRequest();x.open('POST','/api/online-program');x.setRequestHeader('Content-Type','application/octet-stream');x.onload=function(){if(x.status==200){addLog(_t('program.upload_success'),'success');pollTimer=setInterval(pollStatus,1000);}else{addLog(_t('program.upload_failed'),'error');}};x.onerror=function(){addLog(_t('program.upload_failed'),'error');};x.send(file);};"
                                  "</script>"
                                  "</body></html>");
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

esp_err_t web_flash_handler(httpd_req_t *req)
{
    size_t offset = 0;
    esp_err_t ret = ESP_FAIL;
    web_data_t *data = (web_data_t *)req->user_ctx;
    char *buf = (char *)data->buf;

    printf("web_flash_handler\n");
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

    if (programmer_request_handle(buf, offset) == PROG_ERR_NONE)
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

static esp_err_t web_upload_file(httpd_req_t *req, char *path, bool overwrite)
{
#define PROGRAM_MAX_SIZE 0xA00000
#define PROGRAM_MAX_SIZE_STR "10M"

    char *dir_path = NULL;
    char *last_slash = NULL;
    FILE *fd = NULL;
    int received = 0;
    struct stat file_stat;
    int remaining = req->content_len;
    web_data_t *data = (web_data_t *)req->user_ctx;

    /* Ensure parent directory exists */
    dir_path = strdup(path);
    if (dir_path)
    {
        last_slash = strrchr(dir_path, '/');
        if (last_slash)
        {
            *last_slash = '\0';
            if (stat(dir_path, &file_stat) != 0)
            {
                ESP_LOGI(TAG, "Creating directory: %s", dir_path);
                mkdir(dir_path, 0777);
            }
        }
        free(dir_path);
    }

    if (stat(path, &file_stat) == 0)
    {
        if (!overwrite)
        {
            ESP_LOGE(TAG, "File already exists: %s", path);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exist!");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "%s will be overwritten", path);
        unlink(path);
    }

    if (req->content_len > PROGRAM_MAX_SIZE)
    {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File size must be less than " PROGRAM_MAX_SIZE_STR "!");
        return ESP_FAIL;
    }

    fd = fopen(path, "w");
    if (!fd)
    {
        ESP_LOGE(TAG, "Failed to create file : %s", path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    while (remaining > 0)
    {
        ESP_LOGD(TAG, "Remaining size : %d", remaining);
        received = httpd_req_recv(req, (char *)data->buf, (remaining <= CONFIG_HTTPD_RESP_BUF_SIZE) ? (remaining) : (CONFIG_HTTPD_RESP_BUF_SIZE));

        if (received <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }

            fclose(fd);
            unlink(path);
            ESP_LOGE(TAG, "File reception failed!");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        if (received && (received != fwrite((char *)data->buf, 1, received, fd)))
        {
            /* Couldn't write everything to file! Storage may be full? */
            fclose(fd);
            unlink(path);
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

esp_err_t web_upload_file_handler(httpd_req_t *req)
{
    struct stat st;
    char *buf = NULL;
    size_t buf_size = 0;
    char overwrite[5] = {0};
    size_t location_offset = 0;
    static char location[CONFIG_PROGRAMMER_FILE_MAX_LEN] = {0};

    buf_size = httpd_req_get_url_query_len(req) + 1;
    if (buf_size == 1)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }

    buf = (char *)malloc(buf_size);
    if (!buf)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Not enough ram to store query string");
        return ESP_FAIL;
    }

    if (httpd_req_get_url_query_str(req, buf, buf_size) != ESP_OK)
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Query string read failed");
        return ESP_FAIL;
    }

    /* Set disk mount path */
    memcpy(location, "/data/", sizeof("/data/"));
    location_offset = strlen(location);

    if (httpd_query_key_value(buf, "location", location + location_offset, sizeof(location) - location_offset) != ESP_OK)
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Location is unknown");
        return ESP_FAIL;
    }

    /* Check the upload position */
    if (strcmp(location + location_offset, "algorithm") && strcmp(location + location_offset, "program"))
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Upload position is invalid");
        return ESP_FAIL;
    }

    /* Create directory if not exists */
    if (stat(location, &st) != 0)
    {
        ESP_LOGI(TAG, "Creating directory: %s", location);
        if (mkdir(location, 0777) != 0)
        {
            ESP_LOGE(TAG, "Failed to create directory: %s", location);
        }
    }

    /* Add Slash */
    location_offset = strlen(location);
    if ((location[location_offset - 1] != '/') && ((location_offset + 2) < sizeof(location)))
    {
        location[location_offset] = '/';
        location_offset++;
        location[location_offset] = '\0';
    }

    if (httpd_query_key_value(buf, "name", location + location_offset, sizeof(location) - location_offset) != ESP_OK)
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Name is unknown");
        return ESP_FAIL;
    }

    httpd_query_key_value(buf, "overwrite", overwrite, sizeof(overwrite));
    free(buf);

    return web_upload_file(req, location, 0 == memcmp(overwrite, "true", sizeof("true")));
}

esp_err_t web_query_handler(httpd_req_t *req)
{
    char *buf = NULL;
    size_t buf_size = 0;
    char type[32] = {0};
    static char status[128] = {0};
    int encode_len = 0;
    web_data_t *data = (web_data_t *)req->user_ctx;
    const esp_app_desc_t *app_desc = NULL;
    wifi_ap_record_t ap_info;
    int rssi = 0;

    buf_size = httpd_req_get_url_query_len(req) + 1;
    if (buf_size == 1)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }

    buf = (char *)malloc(buf_size);
    if (!buf)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Not enough ram to store query string");
        return ESP_FAIL;
    }

    if (httpd_req_get_url_query_str(req, buf, buf_size) != ESP_OK)
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Query string read failed");
        return ESP_FAIL;
    }

    if (httpd_query_key_value(buf, "type", type, sizeof(type)) != ESP_OK)
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Type is unknown");
        return ESP_FAIL;
    }

    if (!strcmp("program-status", type))
    {
        programmer_get_status((char *)data->buf, CONFIG_HTTPD_RESP_BUF_SIZE, encode_len);
        httpd_resp_send_chunk(req, (char *)data->buf, encode_len);
        httpd_resp_send_chunk(req, NULL, 0);
    }
    else if (!strcmp("device-status", type))
    {
        app_desc = esp_app_get_description();

        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
        {
            rssi = ap_info.rssi;
        }

        snprintf(status, sizeof(status),
                 "{\"version\":\"%s\",\"wifi_rssi\":%d,\"chip_model\":\"ESP32-S3\",\"flash_size\":16}",
                 app_desc->version, rssi);
        httpd_resp_sendstr(req, status);
    }
    else if (!strcmp("start-addr", type))
    {
        uint32_t start_addr = 0xFFFFFFFF;
        auto temp = std::make_unique<char[]>(64);

        memset(temp.get(), 0, 64);
        httpd_query_key_value(buf, "file", temp.get(), 64);
        std::string file_str(temp.get());

        // URL 解码
        for (size_t i = 0; i < file_str.length(); i++)
        {
            if (file_str[i] == '%' && i + 2 < file_str.length())
            {
                int h;
                if (sscanf(file_str.c_str() + i + 1, "%2x", &h) == 1)
                {
                    file_str[i] = (char)h;
                    file_str.erase(i + 1, 2);
                }
            }
        }

        std::string filepath = std::string(CONFIG_PROGRAMMER_PROGRAM_ROOT) + "/" + file_str;
        start_addr = get_hex_start_address(filepath.c_str());

        std::stringstream ss;
        ss << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << start_addr;
        std::string json = "{\"start_addr\":\"" + ss.str() + "\"}";
        httpd_resp_sendstr(req, json.c_str());
    }
    else
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unsupported type");
        return ESP_FAIL;
    }

    free(buf);
    return ESP_OK;
}

esp_err_t web_parse_start_addr_handler(httpd_req_t *req)
{
    int received = 0;
    int total_received = 0;
    const size_t max_parse_size = 1024;
    uint32_t start_addr = 0xFFFFFFFF;
    std::stringstream ss;
    std::string json;

    auto parse_buffer = std::unique_ptr<uint8_t[]>(new uint8_t[max_parse_size]);
    size_t to_read = (req->content_len < max_parse_size) ? req->content_len : max_parse_size;

    while (total_received < (int)to_read)
    {
        received = httpd_req_recv(req, (char *)(parse_buffer.get() + total_received), to_read - total_received);
        if (received <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
                continue;

            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        total_received += received;
    }

    start_addr = parse_hex_addr((const char *)parse_buffer.get(), total_received);
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << start_addr;
    if (start_addr != 0xFFFFFFFF)
    {
        json = "{\"status\":\"success\",\"start_addr\":\"0x" + ss.str() + "\"}";
        ESP_LOGI(TAG, "Parsed HEX start address: 0x%08lX", start_addr);
    }
    else
    {
        json = "{\"status\":\"success\",\"start_addr\":null,\"message\":\"Not HEX or addr not found\"}";
        ESP_LOGI(TAG, "No HEX start address found in the uploaded file");
    }

    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, json.c_str());

    return ESP_OK;
}

esp_err_t web_online_program_handler(httpd_req_t *req)
{
    int received = 0;
    int remaining = req->content_len;
    web_data_t *data = (web_data_t *)req->user_ctx;

    while (remaining > 0)
    {
        ESP_LOGD(TAG, "Remaining size : %d", remaining);
        received = httpd_req_recv(req, (char *)data->buf, (remaining <= CONFIG_HTTPD_RESP_BUF_SIZE) ? (remaining) : (CONFIG_HTTPD_RESP_BUF_SIZE));

        if (received <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }

            ESP_LOGE(TAG, "File reception failed!");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive program");
            return ESP_FAIL;
        }

        if (received)
        {
            prog_err_def ret = programmer_write_data(data->buf, received);

            if (PROG_ERR_NONE != ret)
            {
                ESP_LOGE(TAG, "File flash failed! ret: %d", ret);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to target");
                return ESP_FAIL;
            }
        }

        remaining -= received;
    }

    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, "Target program successfully");

    return ESP_OK;
}

esp_err_t web_set_uart_config_handler(httpd_req_t *req)
{
    web_data_t *data = (web_data_t *)req->user_ctx;
    int received = 0;
    int remaining = req->content_len;
    int total_received = 0;
    cJSON *root = NULL;
    cJSON *baudrate_item = NULL;
    cJSON *data_bits_item = NULL;
    cJSON *parity_item = NULL;
    cJSON *stop_bits_item = NULL;
    int data_bits = 0;
    int parity = 0;
    int stop_bits = 0;
    uint32_t baudrate = 0;

    if (req->method != HTTP_POST)
    {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }

    if (req->content_len >= CONFIG_HTTPD_RESP_BUF_SIZE)
    {
        ESP_LOGE(TAG, "Request too large");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request too large");
        return ESP_FAIL;
    }

    while (remaining > 0)
    {
        received = httpd_req_recv(req, (char *)data->buf + total_received, remaining);
        if (received <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            ESP_LOGE(TAG, "Failed to receive request body");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            return ESP_FAIL;
        }
        total_received += received;
        remaining -= received;
    }

    data->buf[total_received] = '\0';

    /* Parse JSON request */
    root = cJSON_Parse((char *)data->buf);
    if (!root)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    /* Get baudrate (optional) */
    baudrate_item = cJSON_GetObjectItem(root, "baudrate");
    if (baudrate_item && cJSON_IsNumber(baudrate_item))
    {
        baudrate = (uint32_t)baudrate_item->valueint;
        ESP_LOGI(TAG, "Setting baudrate to %" PRIu32, baudrate);
        serial_manager_set_baudrate(baudrate);
    }

    /* Get UART config (optional) */
    data_bits_item = cJSON_GetObjectItem(root, "data_bits");
    parity_item = cJSON_GetObjectItem(root, "parity");
    stop_bits_item = cJSON_GetObjectItem(root, "stop_bits");

    if (data_bits_item && cJSON_IsNumber(data_bits_item) &&
        parity_item && cJSON_IsNumber(parity_item) &&
        stop_bits_item && cJSON_IsNumber(stop_bits_item))
    {
        data_bits = data_bits_item->valueint;
        parity = parity_item->valueint;
        stop_bits = stop_bits_item->valueint;

        ESP_LOGI(TAG, "Setting UART config: data_bits=%d, parity=%d, stop_bits=%d", data_bits, parity, stop_bits);
        serial_manager_set_config(data_bits, parity, stop_bits);
    }

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"success\"}");

    return ESP_OK;
}

esp_err_t web_wifi_config_handler(httpd_req_t *req)
{
    web_data_t *data = (web_data_t *)req->user_ctx;

    if (req->method == HTTP_GET && web_resp_file(req, "/data/httpd/settings.html", (char *)data->buf, CONFIG_HTTPD_RESP_BUF_SIZE))
    {
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t web_wifi_settings_handler(httpd_req_t *req)
{
    web_data_t *data = (web_data_t *)req->user_ctx;
    int received = 0;
    int remaining = req->content_len;
    int total_received = 0;
    cJSON *root = NULL;
    cJSON *ssid_item = NULL;
    cJSON *password_item = NULL;
    nvs_handle_t nvs_handle;
    esp_err_t err;

    if (req->method != HTTP_POST)
    {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }

    if (req->content_len >= CONFIG_HTTPD_RESP_BUF_SIZE)
    {
        ESP_LOGE(TAG, "Request too large");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request too large");
        return ESP_FAIL;
    }

    while (remaining > 0)
    {
        received = httpd_req_recv(req, (char *)data->buf + total_received, remaining);
        if (received <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            ESP_LOGE(TAG, "Failed to receive request body");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            return ESP_FAIL;
        }
        total_received += received;
        remaining -= received;
    }

    data->buf[total_received] = '\0';
    root = cJSON_Parse((char *)data->buf);
    if (!root)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    ssid_item = cJSON_GetObjectItem(root, "ssid");
    password_item = cJSON_GetObjectItem(root, "password");

    if (!ssid_item || !cJSON_IsString(ssid_item))
    {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid ssid");
        return ESP_FAIL;
    }

    err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS open failed");
        return ESP_FAIL;
    }

    err = nvs_set_str(nvs_handle, "WIFI_SSID", ssid_item->valuestring);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save SSID");
        return ESP_FAIL;
    }

    if (password_item && cJSON_IsString(password_item))
    {
        err = nvs_set_str(nvs_handle, "WIFI_PASSWORD", password_item->valuestring);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to save password: %s", esp_err_to_name(err));
            nvs_close(nvs_handle);
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save password");
            return ESP_FAIL;
        }
    }

    nvs_close(nvs_handle);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "WiFi config saved, rebooting...");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}