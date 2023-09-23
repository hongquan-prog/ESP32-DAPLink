#include "prog_online.h"
#include "esp_log.h"

#define TAG "prog_online"

ProgOnline::ProgOnline()
    : _recved_new_packet(false),
      _start_time(0),
      _writed_offset(0),
      _total_size(0),
      _stream_program(_bin_program, _hex_program)
{
}

void ProgOnline::program_start_handle(ProgData &obj)
{
    obj.enable_timeout_timer(10000);
    prog_req_t &request = obj.get_request();
    FlashIface::program_target_t *target = nullptr;
    FlashIface::target_cfg_t *cfg = nullptr;
    StreamProgrammer::Mode mode = (request.format == PROG_HEX_FORMAT) ? (StreamProgrammer::HEX_MODE) : (StreamProgrammer::BIN_MODE);

    ESP_LOGI(TAG, "format: %s, size: %ld", (request.format == PROG_HEX_FORMAT) ? ("hex") : ("bin"), request.total_size);

    if (obj.get_algorithm(request.algorithm, &target, &cfg, request.ram_addr))
    {
        _recved_new_packet = false;
        _writed_offset = 0;
        _total_size = request.total_size;

        if (!_stream_program.init(mode, *cfg, request.flash_addr))
        {
            obj.clean_algorithm();
            Prog::switch_mode(PROG_IDLE_MODE);
            obj.disable_timeout_timer();
            obj.set_busy_state(false);
            obj.clean_algorithm();
        }

        _start_time = xTaskGetTickCount();
    }
}

void ProgOnline::program_timeout_handle(ProgData &obj)
{
    if (!_recved_new_packet)
    {
        Prog::switch_mode(PROG_IDLE_MODE);
        obj.disable_timeout_timer();
        ESP_LOGE(TAG, "Receive Packet timeout");
    }

    _recved_new_packet = false;
}

void ProgOnline::program_data_handle(ProgData &obj)
{
    prog_data_swap_t *swap = reinterpret_cast<prog_data_swap_t *>(obj.get_swap());

    if (!_stream_program.write(swap->data, swap->len))
    {
        obj.clean_algorithm();
        Prog::switch_mode(PROG_IDLE_MODE);
        obj.disable_timeout_timer();
        obj.set_busy_state(false);
        obj.clean_algorithm();
        obj.set_swap(reinterpret_cast<void *>(PROG_ERR_PROGRAM_FAILED));
        obj.send_sync();
        ESP_LOGE(TAG, "Write flash failed");
        return;
    }
    else
    {
        _writed_offset += swap->len;

        if (_writed_offset != _total_size)
            obj.set_progress(_writed_offset * 100 / _total_size);
        else if (_writed_offset == _total_size)
        {
            obj.clean_algorithm();
            Prog::switch_mode(PROG_IDLE_MODE);
            obj.disable_timeout_timer();
            obj.set_busy_state(false);
            obj.clean_algorithm();
            obj.set_progress(100);
            _stream_program.clean();
            ESP_LOGI(TAG, "Elapsed time %ld ms", pdTICKS_TO_MS((xTaskGetTickCount() - _start_time)));
        }
        else
        {
            obj.clean_algorithm();
            Prog::switch_mode(PROG_IDLE_MODE);
            obj.disable_timeout_timer();
            obj.set_busy_state(false);
            obj.clean_algorithm();
            _stream_program.clean();
            ESP_LOGE(TAG, "Received file is greater than total_size");
        }
    }

    _recved_new_packet = true;
    obj.set_swap(reinterpret_cast<void *>(PROG_ERR_NONE));
    /* After a successful mode switch, send a synchronisation signal to keep the http server running */
    obj.send_sync();
}

const char *ProgOnline::name()
{
    return TAG;
};