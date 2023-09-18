#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/message_buffer.h"
#include "algo_extractor.h"

typedef enum
{
    PROG_EVT_NONE,
    PROG_EVT_REQUEST,
    PROG_EVT_PROGRAM_START,
    PROG_EVT_PROGRAM_TIMEOUT,
    PROG_EVT_PROGRAM_DATA_RECVED
} prog_evt_def;

typedef enum
{
    PROG_UNKNOWN_MODE,
    PROG_ONLINE_MODE,
    PROG_OFFLINE_MODE,
    PROG_IDLE_MODE
} prog_mode_def;

typedef enum
{
    PROG_UNKNOWN_FORMAT,
    PROG_BIN_FORMAT,
    PROG_HEX_FORMAT
} prog_format_def;

typedef enum
{
    PROG_ERR_NONE,
    PROG_ERR_BUSY,
    PROG_ERR_JSON_FORMAT_INCORRECT,
    PROG_ERR_ALGORITHM_NOT_EXIST,
    PROG_ERR_PROGRAM_NOT_EXIST,
    PROG_ERR_FLASH_ADDR_NOT_EXIST,
    PROG_ERR_FORMAT_UNKNOWN,
    PROG_ERR_TOTAL_SIZE_INVALID,
    PROG_ERR_MODE_INVALID,
    PROG_ERR_PROGRAM_FAILED,
    PROG_ERR_INVALID_OPERATION
} prog_err_def;

typedef struct
{
    prog_mode_def mode;
    prog_format_def format;
    uint32_t flash_addr;
    uint32_t ram_addr;
    uint32_t total_size;
    std::string algorithm;
    std::string program;
} prog_req_t;

typedef struct
{
    char *data;
    int len;
} prog_request_swap_t;

typedef struct
{
    uint8_t *data;
    int len;
} prog_data_swap_t;

class ProgData
{
private:
    bool _busy;
    int _progress;
    void *_swap;
    prog_req_t _request;
    TimerHandle_t _timer;
    SemaphoreHandle_t _mutex;
    QueueHandle_t _event_queue;
    SemaphoreHandle_t _sync_sig;
    MessageBufferHandle_t _msg_buf;

    AlgoExtractor _extractor;
    FlashIface::target_cfg_t _cfg;
    FlashIface::program_target_t _target;

public:
    ProgData();
    void init(void);
    prog_evt_def wait_event(uint32_t timeout = 0xFFFFFFFF);
    bool send_event(prog_evt_def event);
    void wait_sync(uint32_t timeout = 0xFFFFFFFF);
    void send_sync(void);
    size_t read_msg(uint8_t *buf, size_t size, uint32_t timeout = 0xFFFFFFFF);
    void write_msg(uint8_t *msg, size_t len);
    void set_busy_state(bool state);
    bool is_busy(void);
    void set_progress(int progress);
    int get_progress(void);
    void set_swap(void *swap);
    void *get_swap(void);
    prog_req_t &get_request(void);
    bool get_algorithm(const std::string &path, FlashIface::program_target_t **target, FlashIface::target_cfg_t **cfg, uint32_t ram_addr = 0x20000000);
    void clean_algorithm();
    void enable_timeout_timer(uint32_t ms);
    void disable_timeout_timer();

    static prog_err_def request_decode(prog_req_t &request, char *buf, int len);
};
