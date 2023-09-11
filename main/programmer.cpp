#include "programmer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/message_buffer.h"
#include "hex_prog.h"
#include "algo_extractor.h"
#include "cJSON.h"
#include "esp_log.h"
#include <cstring>

#define TAG "programmer"

typedef struct
{
    HexProg programmer;
    AlgoExtractor extractor;
    FlashIface::program_target_t target;
    FlashIface::target_cfg_t cfg;
    MessageBufferHandle_t cmd_buf;
    SemaphoreHandle_t sync_sig;
    bool cmd_ret;
    bool busy;
} programmer_t;

static programmer_t s_programmer;

bool programmer_suffix_check(const char *filename, const char *suffix)
{
    const char *dot = strrchr(filename, '.');
    if (dot && dot != filename)
    {
        return strcmp(dot, suffix) == 0;
    }

    return false;
}

bool programmer_file_exist(const char *filename)
{
    FILE *fp = fopen(filename, "r");

    if (fp)
    {
        fclose(fp);
    }

    return (fp != nullptr);
}

static void programmer_task(void *pvParameters)
{
    cJSON *root = NULL;
    cJSON *ram_addr_item = NULL;
    cJSON *flash_addr_item = NULL;
    cJSON *algo_item = NULL;
    cJSON *program_item = NULL;
    size_t readBytes = 0;
    static char buf[CONFIG_HTTPD_RESP_BUF_SIZE];
    uint32_t ram_addr = 0;
    uint32_t flash_addr = 0;
    TickType_t start_time = 0;
    FlashIface::target_cfg_t &cfg = s_programmer.cfg;

    for (;;)
    {
        readBytes = xMessageBufferReceive(s_programmer.cmd_buf, buf, CONFIG_HTTPD_RESP_BUF_SIZE, portMAX_DELAY);

        if (readBytes)
        {
            flash_addr = 0;
            ram_addr = 0x20000000;
            root = cJSON_Parse(buf);
            ram_addr_item = cJSON_GetObjectItem(root, "ram_addr");
            flash_addr_item = cJSON_GetObjectItem(root, "flash_addr");
            program_item = cJSON_GetObjectItem(root, "program");
            algo_item = cJSON_GetObjectItem(root, "algorithm");

            if (ram_addr_item && ram_addr_item->type == cJSON_Number)
            {
                ram_addr = ram_addr_item->valueint;
            }

            if (!root || !algo_item || !program_item || (algo_item->type != cJSON_String) || (program_item->type != cJSON_String) ||
                !programmer_file_exist(program_item->valuestring) || !programmer_file_exist(algo_item->valuestring))
            {
                s_programmer.cmd_ret = false;
                cJSON_Delete(root);
                xSemaphoreGive(s_programmer.sync_sig);
                continue;
            }

            // bin 文件必须有flash地址
            if (programmer_suffix_check(program_item->valuestring, ".bin"))
            {
                if (flash_addr_item && (flash_addr_item->type == cJSON_Number))
                {
                    flash_addr = flash_addr_item->valueint;
                }
                else
                {
                    s_programmer.cmd_ret = false;
                    cJSON_Delete(root);
                    xSemaphoreGive(s_programmer.sync_sig);
                    continue;
                }
            }

            s_programmer.cmd_ret = true;
            s_programmer.busy = true;
            xSemaphoreGive(s_programmer.sync_sig);

            if (s_programmer.extractor.extract(algo_item->valuestring, s_programmer.target, s_programmer.cfg, ram_addr))
            {
                start_time = xTaskGetTickCount();
                if (!flash_addr)
                    s_programmer.programmer.programing_hex(cfg, program_item->valuestring);
                else
                    s_programmer.programmer.programming_bin(cfg, flash_addr, program_item->valuestring);
                ESP_LOGI(TAG, "Elapsed time %ld ms", (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS);

                if (s_programmer.target.algo_blob)
                    delete[] s_programmer.target.algo_blob;
            }

            cJSON_Delete(root);
            s_programmer.busy = false;
        }
    }
}

void programmer_init()
{
    s_programmer.busy = false;
    s_programmer.cmd_buf = xMessageBufferCreate(CONFIG_HTTPD_RESP_BUF_SIZE);
    s_programmer.sync_sig = xSemaphoreCreateBinary();
    xTaskCreate(programmer_task, "programmer", 1024 * 4, NULL, 2, NULL);
}

bool programmer_put_cmd(char *msg, int len)
{
    if (s_programmer.busy)
    {
        return false;
    }

    xMessageBufferSend(s_programmer.cmd_buf, msg, len, portMAX_DELAY);
    xSemaphoreTake(s_programmer.sync_sig, portMAX_DELAY);

    return s_programmer.cmd_ret;
}
