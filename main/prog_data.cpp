#include "prog_data.h"
#include "cJSON.h"
#include "esp_log.h"
#include <cstring>
#include "file_programmer.h"

#define TAG "prog_data"
#define MSG_BUF_SIZE 512

ProgData::ProgData()
    : _busy(false), _progress(0), _event_queue(nullptr)
{
}

void program_timeout(TimerHandle_t timer)
{
    ProgData *data = reinterpret_cast<ProgData *>(pvTimerGetTimerID(timer));

    if (data)
    {
        data->send_event(PROG_EVT_PROGRAM_TIMEOUT);
    }
}

void ProgData::init()
{
    _busy = false;
    _progress = 0;
    _mutex = xSemaphoreCreateMutex();
    _msg_buf = xMessageBufferCreate(MSG_BUF_SIZE);
    _event_queue = xQueueCreate(10, sizeof(prog_evt_def));
    _sync_sig = xSemaphoreCreateBinary();
    _timer = xTimerCreate("prog_timer", pdMS_TO_TICKS(10000), pdTRUE, this, program_timeout);
}

prog_evt_def ProgData::wait_event(uint32_t timeout)
{
    prog_evt_def event = PROG_EVT_NONE;

    xQueueReceive(_event_queue, &event, (timeout != portMAX_DELAY) ? (pdMS_TO_TICKS(timeout)) : (portMAX_DELAY));

    return event;
}

bool ProgData::send_event(prog_evt_def event)
{
    return (pdPASS == xQueueSend(_event_queue, &event, portMAX_DELAY));
}

void ProgData::wait_sync(uint32_t timeout)
{
    xSemaphoreTake(_sync_sig, (timeout != portMAX_DELAY) ? (pdMS_TO_TICKS(timeout)) : (portMAX_DELAY));
}

void ProgData::send_sync(void)
{
    xSemaphoreGive(_sync_sig);
}

size_t ProgData::read_msg(uint8_t *buf, size_t size, uint32_t timeout)
{
    return xMessageBufferReceive(_msg_buf, buf, size, (timeout != portMAX_DELAY) ? (pdMS_TO_TICKS(timeout)) : (portMAX_DELAY));
}

void ProgData::write_msg(uint8_t *msg, size_t len)
{
    xMessageBufferSend(_msg_buf, msg, len, portMAX_DELAY);
}

void ProgData::set_busy_state(bool state)
{
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _busy = state;
    xSemaphoreGive(_mutex);
}

bool ProgData::is_busy(void)
{
    bool ret = false;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    ret = _busy;
    xSemaphoreGive(_mutex);

    return ret;
}

void ProgData::set_progress(int progress)
{
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _progress = progress;
    xSemaphoreGive(_mutex);
}

int ProgData::get_progress(void)
{
    int ret = false;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    ret = _progress;
    xSemaphoreGive(_mutex);

    return ret;
}

void ProgData::set_swap(void *swap)
{
    _swap = swap;
}

void *ProgData::get_swap(void)
{
    return _swap;
}

prog_req_t &ProgData::get_request(void)
{
    return _request;
}

bool ProgData::get_algorithm(const std::string &path, FlashIface::program_target_t **target, FlashIface::target_cfg_t **cfg, uint32_t ram_addr)
{
    if (_extractor.extract(path, _target, _cfg, ram_addr))
    {
        *target = &_target;
        *cfg = &_cfg;
        return true;
    }

    return false;
}

void ProgData::clean_algorithm()
{
    if (_target.algo_blob)
    {
        delete[] _target.algo_blob;
        _target.algo_blob = nullptr;
    }
}

void ProgData::enable_timeout_timer(uint32_t ms)
{
    xTimerChangePeriod(_timer, pdMS_TO_TICKS(ms), portMAX_DELAY);
    xTimerStart(_timer, pdMS_TO_TICKS(ms));
}

void ProgData::disable_timeout_timer()
{
    xTimerStop(_timer, portMAX_DELAY);
}

prog_err_def ProgData::request_decode(prog_req_t &request, char *buf, int len)
{
    cJSON *root = NULL;
    cJSON *ram_addr_item = NULL;
    cJSON *flash_addr_item = NULL;
    cJSON *algorithm_item = NULL;
    cJSON *program_item = NULL;
    cJSON *program_mode_item = NULL;
    cJSON *format_item = NULL;
    cJSON *total_size_item = NULL;

    root = cJSON_Parse(buf);
    if (!root)
    {
        ESP_LOGI(TAG, "JSON format error");
        return PROG_ERR_JSON_FORMAT_INCORRECT;
    }

    request.program.clear();
    request.algorithm.clear();
    request.flash_addr = 0;
    request.total_size = 0;
    request.ram_addr = 0x20000000;
    request.mode = PROG_UNKNOWN_MODE;
    request.format = PROG_UNKNOWN_FORMAT;
    program_mode_item = cJSON_GetObjectItem(root, "program_mode");
    ram_addr_item = cJSON_GetObjectItem(root, "ram_addr");
    flash_addr_item = cJSON_GetObjectItem(root, "flash_addr");
    program_item = cJSON_GetObjectItem(root, "program");
    algorithm_item = cJSON_GetObjectItem(root, "algorithm");
    format_item = cJSON_GetObjectItem(root, "format");
    total_size_item = cJSON_GetObjectItem(root, "total_size");

    if (algorithm_item && algorithm_item->type == cJSON_String)
        request.algorithm = std::string(CONFIG_PROGRAMMER_ALGORITHM_ROOT) + "/" + std::string(algorithm_item->valuestring);

    if (program_item && program_item->type == cJSON_String)
        request.program = std::string(CONFIG_PROGRAMMER_PROGRAM_ROOT) + "/" + std::string(program_item->valuestring);

    if (flash_addr_item && (flash_addr_item->type == cJSON_Number))
        request.flash_addr = flash_addr_item->valueint;

    if (ram_addr_item && (ram_addr_item->type == cJSON_Number))
        request.ram_addr = ram_addr_item->valueint;

    if (total_size_item && (total_size_item->type == cJSON_Number))
        request.total_size = total_size_item->valueint;

    if (program_mode_item && (program_mode_item->type == cJSON_String))
    {
        if (!strcmp("online", program_mode_item->valuestring))
            request.mode = PROG_ONLINE_MODE;
        else if (!strcmp("offline", program_mode_item->valuestring))
            request.mode = PROG_OFFLINE_MODE;
    }

    if (format_item && format_item->type == cJSON_String)
    {
        if (!strcmp("hex", format_item->valuestring))
            request.format = PROG_HEX_FORMAT;
        else if (!strcmp("bin", format_item->valuestring))
            request.format = PROG_BIN_FORMAT;
    }

    if (request.mode == PROG_UNKNOWN_MODE)
    {
        ESP_LOGE(TAG, "Invalid program mode");
        cJSON_Delete(root);
        return PROG_ERR_MODE_INVALID;
    }

    if (request.algorithm.empty() || !FileProgrammer::is_exist(request.algorithm.c_str()))
    {
        ESP_LOGE(TAG, "Algorithm is not exist");
        cJSON_Delete(root);
        return PROG_ERR_ALGORITHM_NOT_EXIST;
    }

    if ((request.mode == PROG_OFFLINE_MODE) && (request.program.empty() || !FileProgrammer::is_exist(request.program.c_str())))
    {
        ESP_LOGE(TAG, "Program is not exist");
        cJSON_Delete(root);
        return PROG_ERR_PROGRAM_NOT_EXIST;
    }

    if (FileProgrammer::compare_extension(request.program.c_str(), ".bin") && (request.flash_addr == 0))
    {
        ESP_LOGE(TAG, "The programming address must be provided for programming with binary files.");
        cJSON_Delete(root);
        return PROG_ERR_FLASH_ADDR_NOT_EXIST;
    }

    if ((request.mode == PROG_ONLINE_MODE) && (request.format == PROG_UNKNOWN_FORMAT))
    {
        ESP_LOGE(TAG, "Format is unsupported");
        cJSON_Delete(root);
        return PROG_ERR_FORMAT_UNKNOWN;
    }

    if ((request.mode == PROG_ONLINE_MODE) && (0 == request.total_size))
    {
        ESP_LOGE(TAG, "Total size is zero");
        cJSON_Delete(root);
        return PROG_ERR_TOTAL_SIZE_INVALID;
    }

    cJSON_Delete(root);
    return PROG_ERR_NONE;
}
