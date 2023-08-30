#include "hex_prog.h"
#include "esp_log.h"

#define TAG "hex_prog"

HexProg::HexProg()
    : BinProg()
{
}

HexProg::HexProg(const std::string &file)
    : BinProg(file)
{
}

bool HexProg::programing_hex(const target_cfg_t &cfg, const std::string &file)
{
    dap_err_t err = ERROR_SUCCESS;
    FRESULT ret = FR_OK;
    FILE *fp = nullptr;
    size_t rd_size = 0;
    uint32_t wr_addr = 0;
    uint32_t decode_size = 0;
    uint32_t total_size = 0;

    err = _flash_manager.init(&cfg);
    if (err != ERROR_SUCCESS)
    {
        return false;
    }

    if (file.empty())
    {
        ESP_LOGE(TAG, "No file specified");
        return false;
    }

    _file_path = file;
    fp = fopen(file.c_str(), "r");
    if (ret != FR_OK)
    {
        ESP_LOGE(TAG, "Failed to open %s", file.c_str());
        return false;
    }

    _start_address = 0;
    reset_hex_parser(&_parser);

    while (feof(fp) == 0)
    {
        rd_size = fread(_hex_buffer, 1, sizeof(_hex_buffer), fp);

        if (rd_size > 0)
        {
            if (!write_hex(_hex_buffer, rd_size, decode_size))
            {
                ESP_LOGE(TAG, "Failed to write hex at:%lx", _start_address + wr_addr);
                _flash_manager.uninit();
                return false;
            }

            wr_addr += decode_size;
            total_size += decode_size;
        }
    }

    fclose(fp);
    _flash_manager.uninit();
    ESP_LOGI(TAG, "DAPLink write %ld bytes to %s at 0x%08lx successfully", total_size, cfg.target_part_number, _start_address);

    return true;
}

bool HexProg::programing_hex(const target_cfg_t &cfg)
{
    return programing_hex(cfg, _file_path);
}

bool HexProg::write_hex(const uint8_t *hex_data, uint32_t size, uint32_t &decode_size)
{
    hex_parse_status_t parse_status = HEX_PARSE_UNINIT;
    uint32_t bin_start_address = 0; // Decoded from the hex file, the binary buffer data starts at this address
    uint32_t bin_buf_written = 0;   // The amount of data in the binary buffer starting at address above
    uint32_t block_amt_parsed = 0;  // amount of data parsed in the block on the last call

    decode_size = 0;

    while (1)
    {
        // try to decode a block of hex data into bin data
        parse_status = parse_hex_blob(&_parser, hex_data, size, &block_amt_parsed, _bin_buffer, sizeof(_bin_buffer), &bin_start_address, &bin_buf_written);

        // Get the start address from the first block
        if (_start_address == 0 && bin_buf_written)
        {
            _start_address = bin_start_address;
            ESP_LOGI(TAG, "Starting to program hex at 0x%lx", _start_address);
        }

        // the entire block of hex was decoded. This is a simple state
        if (HEX_PARSE_OK == parse_status)
        {
            if (bin_buf_written > 0)
            {
                decode_size += bin_buf_written;
                if (ERROR_SUCCESS != _flash_manager.write(bin_start_address, _bin_buffer, bin_buf_written))
                {
                    return false;
                }
            }

            break;
        }
        else if (HEX_PARSE_UNALIGNED == parse_status)
        {
            if (bin_buf_written > 0)
            {
                decode_size += bin_buf_written;
                if (ERROR_SUCCESS != _flash_manager.write(bin_start_address, _bin_buffer, bin_buf_written))
                {
                    return false;
                }
            }

            // incrememntal offset to finish the block
            size -= block_amt_parsed;
            hex_data += block_amt_parsed;
        }
        else if (HEX_PARSE_EOF == parse_status)
        {
            if (bin_buf_written > 0)
            {
                decode_size += bin_buf_written;
                if (ERROR_SUCCESS != _flash_manager.write(bin_start_address, _bin_buffer, bin_buf_written))
                {
                    return false;
                }
            }

            break;
        }
        else if (HEX_PARSE_CKSUM_FAIL == parse_status)
        {
            ESP_LOGE(TAG, "Checksum failed");
            return false;
        }
        else if ((HEX_PARSE_UNINIT == parse_status) || (HEX_PARSE_FAILURE == parse_status))
        {
            ESP_LOGE(TAG, "Failed to parse hex: %d", parse_status);
            return false;
        }
    }

    return true;
}
