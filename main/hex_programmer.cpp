#include "hex_programmer.h"
#include "esp_log.h"

#define TAG "HexProgrammer"

HexProgrammer::HexProgrammer()
    : BinProgrammer()
{
}

HexProgrammer::HexProgrammer(const std::string &file)
    : BinProgrammer(file)
{
}

bool HexProgrammer::start(const std::string &file)
{
    UINT line_size = 0;
    FRESULT ret = FR_OK;

    if (file.empty())
    {
        ESP_LOGE(TAG, "No file specified");
        return false;
    }

    ret = f_open(&_file_object, file.c_str(), FA_READ);
    if (ret != FR_OK)
    {
        ESP_LOGE(TAG, "Failed to open %s", file.c_str());
        return false;
    }

    reset_hex_parser(&_parser);

    while (f_eof(&_file_object) == 0)
    {
        f_read(&_file_object, _hex_buffer, sizeof(_hex_buffer), &line_size);

        if (line_size > 0)
        {
            if (false == write_hex(_hex_buffer, line_size))
            {
                ESP_LOGE(TAG, "Failed to write hex");
                return false;
            }
        }
    }

    return true;
}

bool HexProgrammer::write_hex(const uint8_t *data, uint32_t size)
{
    hex_parse_status_t parse_status = HEX_PARSE_UNINIT;
    uint32_t bin_start_address = 0; // Decoded from the hex file, the binary buffer data starts at this address
    uint32_t bin_buf_written = 0;   // The amount of data in the binary buffer starting at address above
    uint32_t block_amt_parsed = 0;  // amount of data parsed in the block on the last call

    while (1)
    {
        // try to decode a block of hex data into bin data
        parse_status = parse_hex_blob(&_parser, data, size, &block_amt_parsed, _bin_buffer, sizeof(_bin_buffer), &bin_start_address, &bin_buf_written);

        // the entire block of hex was decoded. This is a simple state
        if (HEX_PARSE_OK == parse_status)
        {
            if (bin_buf_written > 0)
            {
                // status = flash_decoder_write(bin_start_address, _bin_buffer, bin_buf_written);
            }

            break;
        }
        else if (HEX_PARSE_UNALIGNED == parse_status)
        {
            if (bin_buf_written > 0)
            {
                // status = flash_decoder_write(bin_start_address, _bin_buffer, bin_buf_written);
            }

            // incrememntal offset to finish the block
            size -= block_amt_parsed;
            data += block_amt_parsed;
        }
        else if (HEX_PARSE_EOF == parse_status)
        {
            if (bin_buf_written > 0)
            {
                // status = flash_decoder_write(bin_start_address, _bin_buffer, bin_buf_written);
            }

            break;
        }
        else if (HEX_PARSE_CKSUM_FAIL == parse_status)
        {
            return false;
        }
        else if ((HEX_PARSE_UNINIT == parse_status) || (HEX_PARSE_FAILURE == parse_status))
        {
            return false;
        }
    }

    return true;
}
