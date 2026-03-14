/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"

#if CONFIG_DEBUG_PROBE_IFACE_JTAG

#include "string.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_rom_sys.h"
#include "esp_chip_info.h"
#include "debug_probe.h"
#include "debug_gpio.h"

static const char *TAG = "esp_usb_jtag";

/* 4K ringbuffer size is more than enough */
#define JTAG_RCVBUF_SIZE     4096
#define JTAG_SNDBUF_SIZE     4096
static RingbufHandle_t s_jtag_rcvbuf = NULL;
static RingbufHandle_t s_jtag_sndbuf = NULL;

static esp_chip_model_t s_target_model;
static TaskHandle_t s_jtag_task_handle = NULL;
static uint8_t s_tdo_bytes[1024];
static uint16_t s_total_tdo_bits = 0;
static uint16_t s_usb_sent_bits = 0;

#define ROUND_UP_BITS(x)        ((x + 7) & (~7))

/* esp usb serial protocol specific definitions */
#define JTAG_PROTO_MAX_BITS      DEBUG_PROBE_PACKET_SIZE * 8
#define JTAG_PROTO_CAPS_VER     1   /*Version field. */
typedef struct __attribute__((packed))
{
    uint8_t proto_ver;  /*Protocol version. Expects JTAG_PROTO_CAPS_VER for now. */
    uint8_t length; /*of this plus any following descriptors */
} jtag_proto_caps_hdr_t;

#define JTAG_PROTO_CAPS_SPEED_APB_TYPE 1
typedef struct __attribute__((packed))
{
    uint8_t type;
    uint8_t length;
} jtag_gen_hdr_t;

typedef struct __attribute__((packed))
{
    uint8_t type;   /*Type, always JTAG_PROTO_CAPS_SPEED_APB_TYPE */
    uint8_t length; /*Length of this */
    uint16_t apb_speed_10khz;   /*ABP bus speed, in 10KHz increments. Base speed is half
                      * this. */
    uint16_t div_min;   /*minimum divisor (to base speed), inclusive */
    uint16_t div_max;   /*maximum divisor (to base speed), inclusive */
} jtag_proto_caps_speed_apb_t;

typedef struct {
    jtag_proto_caps_hdr_t proto_hdr;
    jtag_proto_caps_speed_apb_t caps_apb;
} jtag_proto_caps_t;

// JTAG vendor request commands enum
typedef enum {
    VEND_JTAG_SETDIV        = 0,
    VEND_JTAG_SETIO         = 1,
    VEND_JTAG_GETTDO        = 2,
    VEND_JTAG_SET_CHIPID    = 3,
} eub_jtag_vendor_cmd_t;

// TCK frequency is around 4800KHZ and we do not support selective clock for now.
#define TCK_FREQ(khz) ((khz * 2) / 10)
static const jtag_proto_caps_t jtag_proto_caps = {
    {.proto_ver = JTAG_PROTO_CAPS_VER, .length = sizeof(jtag_proto_caps_hdr_t) + sizeof(jtag_proto_caps_speed_apb_t)},
    {.type = JTAG_PROTO_CAPS_SPEED_APB_TYPE, .length = sizeof(jtag_proto_caps_speed_apb_t), .apb_speed_10khz = TCK_FREQ(4800), .div_min = 1, .div_max = 1}
};

// Use existing alias pattern but with updated signatures
esp_err_t debug_probe_init(void) __attribute__((alias("esp_usb_jtag_init")));
esp_err_t debug_probe_process_data(const uint8_t *data, size_t len) __attribute__((alias("esp_usb_jtag_process_data")));
int debug_probe_get_proto_caps(void *dest) __attribute__((alias("esp_usb_jtag_get_proto_caps")));
void debug_probe_handle_esp32_tdi_bootstrapping(bool rebooting) __attribute__((alias("esp_usb_jtag_handle_esp32_tdi_bootstrapping")));
uint8_t *debug_probe_get_data_to_send(size_t *len, TickType_t timeout) __attribute__((alias("esp_usb_jtag_get_data_to_send")));
void debug_probe_free_sent_data(uint8_t *data) __attribute__((alias("esp_usb_jtag_free_sent_data")));
debug_probe_cmd_response_t debug_probe_handle_command(uint8_t cmd, uint16_t wValue) __attribute__((alias("esp_usb_jtag_handle_command")));

static int esp_usb_jtag_get_proto_caps(void *dest)
{
    memcpy(dest, (uint16_t *)&jtag_proto_caps, sizeof(jtag_proto_caps));
    return sizeof(jtag_proto_caps);
}

static bool esp_usb_jtag_target_is_esp32(void)
{
    return s_target_model == CHIP_ESP32;
}

static void esp_usb_jtag_task_suspend(void)
{
    if (s_jtag_task_handle) {
        vTaskSuspend(s_jtag_task_handle);
    }
}

static void esp_usb_jtag_task_resume(void)
{
    if (s_jtag_task_handle) {
        vTaskResume(s_jtag_task_handle);
    }
}

static void esp_usb_jtag_handle_esp32_tdi_bootstrapping(bool rebooting)
{
    // Only handle this for ESP32 targets
    if (!esp_usb_jtag_target_is_esp32()) {
        return;
    }

    static bool s_tdi_bootstrapping = false;
    if (rebooting) {
        // Entering reboot sequence: suspend JTAG task and force TDI low (only if not already done)
        if (!s_tdi_bootstrapping) {
            esp_usb_jtag_task_suspend();
            s_tdi_bootstrapping = true;
            debug_probe_tdi_write(0);
            ESP_LOGW(TAG, "JTAG task suspended, TDI forced low");
        }
    } else {
        // Exiting reboot sequence: resume JTAG task (only if the JTAG task was suspended)
        // Pin TDI is handled by the JTAG task, so we don't need to do anything here
        if (s_tdi_bootstrapping) {
            esp_rom_delay_us(1000); /* wait for reset */
            esp_usb_jtag_task_resume();
            s_tdi_bootstrapping = false;
            ESP_LOGW(TAG, "JTAG task resumed");
        }
    }
}

// Process data received from USB vendor driver
static esp_err_t esp_usb_jtag_process_data(const uint8_t *data, size_t len)
{
    BaseType_t res = xRingbufferSend(s_jtag_rcvbuf, data, len, pdMS_TO_TICKS(1000));
    if (res != pdTRUE) {
        ESP_LOGE(TAG, "Cannot write to JTAG receive buffer (free %zu of %d)!",
                 xRingbufferGetCurFreeSize(s_jtag_rcvbuf), JTAG_RCVBUF_SIZE);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static uint8_t *esp_usb_jtag_get_data_to_send(size_t *len, TickType_t timeout)
{
    uint8_t *data = xRingbufferReceive(s_jtag_sndbuf, len, timeout);
    if (data && *len > 0) {
        ESP_LOG_BUFFER_HEXDUMP(TAG, data, *len, ESP_LOG_DEBUG);
    }
    return data;
}

static void esp_usb_jtag_free_sent_data(uint8_t *data)
{
    vRingbufferReturnItem(s_jtag_sndbuf, data);
}

static esp_err_t esp_usb_jtag_send_data(const void *buf, const size_t size)
{
    if (xRingbufferSend(s_jtag_sndbuf, buf, size, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Cannot write to JTAG send buffer (free %zu of %d)!",
                 xRingbufferGetCurFreeSize(s_jtag_sndbuf), JTAG_SNDBUF_SIZE);
        return ESP_FAIL;
    }
    return ESP_OK;
}

debug_probe_cmd_response_t esp_usb_jtag_handle_command(uint8_t cmd, uint16_t wValue)
{
    debug_probe_cmd_response_t response = {0};
    static uint8_t tdo_buf;

    switch ((eub_jtag_vendor_cmd_t)cmd) {
    case VEND_JTAG_SETDIV:
    case VEND_JTAG_SETIO:
        // TODO: process the commands
        response.success = true;
        break;

    case VEND_JTAG_GETTDO:
        tdo_buf = gpio_get_level(GPIO_TDO);
        response.success = true;
        response.data = &tdo_buf;
        response.data_len = 1;
        break;

    case VEND_JTAG_SET_CHIPID:
        s_target_model = wValue;
        response.success = true;
        break;

    default:
        response.success = false;
        break;
    }

    return response;
}

inline static void do_jtag_one(const uint8_t tdo_req, const uint8_t tms_tdi_mask)
{
    debug_probe_write_tmstck(tms_tdi_mask);
    debug_probe_tck_set();

    if (tdo_req) {
        s_tdo_bytes[s_total_tdo_bits / 8] |= (debug_probe_tdo_read() << (s_total_tdo_bits % 8));
        s_total_tdo_bits++;
    }

    debug_probe_tck_clr();
}

static void esp_usb_jtag_task(void *pvParameters)
{
    enum e_cmds {
        CMD_CLK_0 = 0, CMD_CLK_1, CMD_CLK_2, CMD_CLK_3,
        CMD_CLK_4, CMD_CLK_5, CMD_CLK_6, CMD_CLK_7,
        CMD_SRST0, CMD_SRST1, CMD_FLUSH, CMD_RSV,
        CMD_REP0, CMD_REP1, CMD_REP2, CMD_REP3
    };

    const struct {
        uint8_t tdo_req;
        uint8_t tms_tdi_mask;
    } pin_levels[] = {                          // { tdo_req, tms, tdi }
        { 0, 0 },                               // { 0, 0, 0 },  CMD_CLK_0
        { 0, GPIO_TDI_MASK },                   // { 0, 0, 1 },  CMD_CLK_1
        { 0, GPIO_TMS_MASK },                   // { 0, 1, 0 },  CMD_CLK_2
        { 0, GPIO_TMS_TDI_MASK },               // { 0, 1, 1 },  CMD_CLK_3
        { 1, 0 },                               // { 1, 0, 0 },  CMD_CLK_4
        { 1, GPIO_TDI_MASK },                   // { 1, 0, 1 },  CMD_CLK_5
        { 1, GPIO_TMS_MASK },                   // { 1, 1, 0 },  CMD_CLK_6
        { 1, GPIO_TMS_TDI_MASK },               // { 1, 1, 1 },  CMD_CLK_7
        { 0, GPIO_TMS_MASK },                   // { 0, 1, 0 },  CMD_SRST0
        { 0, GPIO_TMS_MASK },                   // { 0, 1, 0 },  CMD_SRST1
    };

    s_jtag_task_handle = xTaskGetCurrentTaskHandle();

    size_t cnt = 0;
    int prev_cmd = CMD_SRST0, rep_cnt = 0;

    while (1) {
        debug_probe_notify_activity(false);
        char *nibbles = (char *)xRingbufferReceive(s_jtag_rcvbuf, &cnt, portMAX_DELAY);
        debug_probe_notify_activity(true);

        ESP_LOG_BUFFER_HEXDUMP(TAG, nibbles, cnt, ESP_LOG_DEBUG);

        for (size_t n = 0; n < cnt * 2 ; n++) {
            const int cmd = (n & 1) ? (nibbles[n / 2] & 0x0F) : (nibbles[n / 2] >> 4);
            int cmd_exec = cmd, cmd_rpt_cnt = 1;

            switch (cmd) {
            case CMD_REP0:
            case CMD_REP1:
            case CMD_REP2:
            case CMD_REP3:
                //(r1*2+r0)<<(2*n)
                cmd_rpt_cnt = (cmd - CMD_REP0) << (2 * rep_cnt++);
                cmd_exec = prev_cmd;
                break;
            case CMD_SRST0:         // JTAG Tap reset command is not expected from host but still we are ready
                cmd_rpt_cnt = 8;    // 8 TMS=1 is more than enough to return the TAP state to RESET
                break;
            case CMD_SRST1:         // FIXME: system reset may cause an issue during openocd examination
                cmd_rpt_cnt = 8;    // for now this is also used for the tap reset
                // gpio_set_level(GPIO_RST, 0);
                // ets_delay_us(100);
                // gpio_set_level(GPIO_RST, 1);
                break;
            default:
                rep_cnt = 0;
                break;
            }

            if (cmd_exec < CMD_FLUSH) {
                for (int i = 0; i < cmd_rpt_cnt; i++) {
                    do_jtag_one(pin_levels[cmd_exec].tdo_req, pin_levels[cmd_exec].tms_tdi_mask);
                }
            } else if (cmd_exec == CMD_FLUSH) {
                s_total_tdo_bits = ROUND_UP_BITS(s_total_tdo_bits);
                if (s_usb_sent_bits < s_total_tdo_bits) {
                    int waiting_to_send_bits = s_total_tdo_bits - s_usb_sent_bits;
                    while (waiting_to_send_bits > 0) {
                        int send_bits = waiting_to_send_bits > JTAG_PROTO_MAX_BITS ? JTAG_PROTO_MAX_BITS : waiting_to_send_bits;
                        esp_err_t send_result = esp_usb_jtag_send_data(s_tdo_bytes + (s_usb_sent_bits / 8), send_bits / 8);
                        if (send_result != ESP_OK) {
                            ESP_LOGE(TAG, "JTAG send buffer full, dropping data.");
                        }
                        s_usb_sent_bits += send_bits;
                        waiting_to_send_bits -= send_bits;
                    }
                    memset(s_tdo_bytes, 0x00, sizeof(s_tdo_bytes));
                    s_total_tdo_bits = s_usb_sent_bits = 0;
                }
            }

            /* As soon as either DEBUG_PROBE_PACKET_SIZE bytes have been collected or a CMD_FLUSH command is executed,
                make the usb buffer available for the host to receive.
            */
            int waiting_to_send_bits = s_total_tdo_bits - s_usb_sent_bits;
            if (waiting_to_send_bits >= JTAG_PROTO_MAX_BITS) {
                int send_bits = ROUND_UP_BITS(waiting_to_send_bits > JTAG_PROTO_MAX_BITS ? JTAG_PROTO_MAX_BITS : waiting_to_send_bits);
                int n_byte = send_bits / 8;
                esp_err_t send_result = esp_usb_jtag_send_data(s_tdo_bytes + (s_usb_sent_bits / 8), n_byte);
                if (send_result != ESP_OK) {
                    ESP_LOGE(TAG, "JTAG send buffer full, dropping data.");
                }
                memset(s_tdo_bytes + (s_usb_sent_bits / 8), 0x00, n_byte);
                s_usb_sent_bits += send_bits;
                waiting_to_send_bits -= send_bits;
                if (waiting_to_send_bits <= 0) {
                    s_total_tdo_bits = s_usb_sent_bits = 0;
                }
            }

            if (cmd < CMD_REP0 && cmd != CMD_FLUSH) {
                prev_cmd = cmd;
            }
        }

        vRingbufferReturnItem(s_jtag_rcvbuf, nibbles);
    }

    vTaskDelete(NULL);
}

static esp_err_t esp_usb_jtag_init(void)
{
    ESP_LOGI(TAG, "Initializing JTAG debug probe");

    s_jtag_rcvbuf = xRingbufferCreate(JTAG_RCVBUF_SIZE, RINGBUF_TYPE_NOSPLIT);
    s_jtag_sndbuf = xRingbufferCreate(JTAG_SNDBUF_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (!s_jtag_rcvbuf || !s_jtag_sndbuf) {
        ESP_LOGE(TAG, "Cannot allocate JTAG buffers!");
        return ESP_ERR_NO_MEM;
    }

    /* dedicated GPIO will be binded to the CPU who invokes this API */
    /* we will create a jtag task pinned to this core */
    debug_probe_init_jtag_pins();

    BaseType_t res = xTaskCreatePinnedToCore(esp_usb_jtag_task,
                     "jtag_task",
                     4 * 1024,
                     NULL,
                     DEBUG_PROBE_TASK_PRI,
                     NULL,
                     esp_cpu_get_core_id());
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Cannot create JTAG task!");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

#endif /* CONFIG_DEBUG_PROBE_IFACE_JTAG */
