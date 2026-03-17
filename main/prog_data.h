/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-16     Refactor     Improved documentation and structure
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/message_buffer.h"
#include "algo_extractor.h"

/**
 * @file prog_data.h
 * @brief Programmer state machine data structures
 * 
 * This file contains data structures and definitions for the programmer
 * state machine that handles online and offline programming operations.
 */

/**
 * @brief Programmer events
 */
typedef enum
{
    PROG_EVT_NONE,              ///< No event
    PROG_EVT_REQUEST,           ///< Programming request received
    PROG_EVT_PROGRAM_START,     ///< Programming started
    PROG_EVT_PROGRAM_TIMEOUT,   ///< Programming timeout
    PROG_EVT_PROGRAM_DATA_RECVED ///< Programming data received
} prog_evt_def;

/**
 * @brief Programmer modes
 */
typedef enum
{
    PROG_UNKNOWN_MODE,          ///< Unknown mode
    PROG_ONLINE_MODE,           ///< Online programming mode (streaming)
    PROG_OFFLINE_MODE,          ///< Offline programming mode (file-based)
    PROG_IDLE_MODE              ///< Idle mode
} prog_mode_def;

/**
 * @brief File format types
 */
typedef enum
{
    PROG_UNKNOWN_FORMAT,        ///< Unknown format
    PROG_BIN_FORMAT,            ///< Binary format
    PROG_HEX_FORMAT             ///< Intel HEX format
} prog_format_def;

/**
 * @brief Programmer error codes
 */
typedef enum
{
    PROG_ERR_NONE,                          ///< No error
    PROG_ERR_BUSY,                          ///< Programmer busy
    PROG_ERR_JSON_FORMAT_INCORRECT,         ///< JSON format error
    PROG_ERR_ALGORITHM_NOT_EXIST,           ///< Algorithm file not found
    PROG_ERR_PROGRAM_NOT_EXIST,             ///< Program file not found
    PROG_ERR_FLASH_ADDR_NOT_EXIST,          ///< Flash address invalid
    PROG_ERR_FORMAT_UNKNOWN,                ///< Unknown format
    PROG_ERR_TOTAL_SIZE_INVALID,            ///< Total size invalid
    PROG_ERR_MODE_INVALID,                  ///< Mode invalid
    PROG_ERR_PROGRAM_FAILED,                ///< Programming failed
    PROG_ERR_INVALID_OPERATION              ///< Invalid operation
} prog_err_def;

/**
 * @brief Programming request parameters
 */
typedef struct
{
    prog_mode_def mode;         ///< Programming mode
    prog_format_def format;     ///< File format
    uint32_t flash_addr;        ///< Flash start address
    uint32_t ram_addr;          ///< RAM start address for algorithm
    uint32_t total_size;        ///< Total data size
    std::string algorithm;      ///< Algorithm file path
    std::string program;        ///< Program file path (offline mode)
} prog_req_t;

/**
 * @brief Request data swap structure
 */
typedef struct
{
    char *data;     ///< Request data buffer
    int len;        ///< Data length
} prog_request_swap_t;

/**
 * @brief Program data swap structure
 */
typedef struct
{
    uint8_t *data;  ///< Data buffer
    int len;        ///< Data length
} prog_data_swap_t;

/**
 * @brief Programmer data manager class
 * 
 * This class manages the state and data for the programmer state machine.
 * It handles event queuing, synchronization, and algorithm extraction.
 */
class ProgData
{
private:
    bool _busy;                         ///< Busy flag
    int _progress;                      ///< Programming progress (0-100)
    void *_swap;                        ///< Swap data pointer
    prog_req_t _request;                ///< Current request
    TimerHandle_t _timer;               ///< Timeout timer
    SemaphoreHandle_t _mutex;           ///< Mutex for thread safety
    QueueHandle_t _event_queue;         ///< Event queue
    SemaphoreHandle_t _sync_sig;        ///< Synchronization signal
    MessageBufferHandle_t _msg_buf;     ///< Message buffer for streaming data

    AlgoExtractor _extractor;           ///< Algorithm extractor
    FlashIface::target_cfg_t _cfg;      ///< Target configuration
    FlashIface::program_target_t _target; ///< Program target

public:
    /**
     * @brief Constructor
     */
    ProgData();
    
    /**
     * @brief Initialize programmer data
     */
    void init(void);
    
    /**
     * @brief Wait for event
     * @param timeout Timeout in ticks
     * @return Event type
     */
    prog_evt_def wait_event(uint32_t timeout = 0xFFFFFFFF);
    
    /**
     * @brief Send event to queue
     * @param event Event type
     * @return true if successful
     */
    bool send_event(prog_evt_def event);
    
    /**
     * @brief Wait for synchronization signal
     * @param timeout Timeout in ticks
     */
    void wait_sync(uint32_t timeout = 0xFFFFFFFF);
    
    /**
     * @brief Send synchronization signal
     */
    void send_sync(void);
    
    /**
     * @brief Read from message buffer
     * @param buf Output buffer
     * @param size Buffer size
     * @param timeout Timeout in ticks
     * @return Bytes read
     */
    size_t read_msg(uint8_t *buf, size_t size, uint32_t timeout = 0xFFFFFFFF);
    
    /**
     * @brief Write to message buffer
     * @param msg Message buffer
     * @param len Message length
     */
    void write_msg(uint8_t *msg, size_t len);
    
    /**
     * @brief Set busy state
     * @param state Busy flag
     */
    void set_busy_state(bool state);
    
    /**
     * @brief Check if busy
     * @return true if busy
     */
    bool is_busy(void);
    
    /**
     * @brief Set progress
     * @param progress Progress percentage (0-100)
     */
    void set_progress(int progress);
    
    /**
     * @brief Get progress
     * @return Progress percentage
     */
    int get_progress(void);
    
    /**
     * @brief Set swap data
     * @param swap Swap data pointer
     */
    void set_swap(void *swap);
    
    /**
     * @brief Get swap data
     * @return Swap data pointer
     */
    void *get_swap(void);
    
    /**
     * @brief Get current request
     * @return Reference to request structure
     */
    prog_req_t &get_request(void);
    
    /**
     * @brief Extract algorithm from file
     * @param path Algorithm file path
     * @param target Output program target
     * @param cfg Output target configuration
     * @param ram_addr RAM start address
     * @return true if successful
     */
    bool get_algorithm(const std::string &path, FlashIface::program_target_t **target, FlashIface::target_cfg_t **cfg, uint32_t ram_addr = 0x20000000);
    
    /**
     * @brief Clean algorithm resources
     */
    void clean_algorithm();
    
    /**
     * @brief Enable timeout timer
     * @param ms Timeout in milliseconds
     */
    void enable_timeout_timer(uint32_t ms);
    
    /**
     * @brief Disable timeout timer
     */
    void disable_timeout_timer();
    
    /**
     * @brief Decode JSON request
     * @param request Output request structure
     * @param buf JSON buffer
     * @param len Buffer length
     * @return Error code
     */
    static prog_err_def request_decode(prog_req_t &request, char *buf, int len);
};
