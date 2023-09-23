#include "prog_offline.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "prog_offline"

BinaryProgram ProgOffline::_bin_program;
HexProgram ProgOffline::_hex_program;

ProgOffline::ProgOffline()
    : _file_program(_bin_program, _hex_program)
{
}

void ProgOffline::program_start_handle(ProgData &obj)
{
    TickType_t start_time = 0;
    prog_req_t &request = obj.get_request();
    FlashIface::program_target_t *target = nullptr;
    FlashIface::target_cfg_t *cfg = nullptr;

    _file_program.register_progress_changed_callback(std::bind(&ProgData::set_progress, &obj, std::placeholders::_1));
    ESP_LOGI(TAG, "file: %s", request.program.c_str());

    if (obj.get_algorithm(request.algorithm, &target, &cfg, request.ram_addr))
    {
        start_time = xTaskGetTickCount();
        if (_file_program.program(request.program, *cfg, request.flash_addr))
            ESP_LOGI(TAG, "Elapsed time %ld ms", pdTICKS_TO_MS(xTaskGetTickCount() - start_time));
        else
            ESP_LOGE(TAG, "Program failed");
        obj.clean_algorithm();
    }

    Prog::switch_mode(PROG_IDLE_MODE);
    obj.set_busy_state(false);
}

const char *ProgOffline::name()
{
    return TAG;
};