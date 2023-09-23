/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */
#include "hex_program.h"
#include "log.h"

#define TAG "hex_prog"

HexProgram::HexProgram()
    : BinaryProgram()
{
}

bool HexProgram::init(const FlashIface::target_cfg_t &cfg, uint32_t program_addr)
{
    _program_addr = 0;
    reset_hex_parser(&_hex_parser);
    return (_flash_accessor.init(cfg) == FlashIface::ERR_NONE);
}

bool HexProgram::write(uint8_t *data, size_t len)
{
    uint32_t decode_size = 0;

    if (!write_hex(data, len, decode_size))
    {
        LOG_ERROR("Failed to write data at:%lx", _program_addr);
        return false;
    }

    _program_addr += decode_size;

    return true;
}

bool HexProgram::write_hex(const uint8_t *hex_data, uint32_t size, uint32_t &decode_size)
{
    hex_parse_status_t parse_status = HEX_PARSE_UNINIT;
    uint32_t bin_start_address = 0; // Decoded from the hex file, the binary buffer data starts at this address
    uint32_t bin_buf_written = 0;   // The amount of data in the binary buffer starting at address above
    uint32_t block_amt_parsed = 0;  // amount of data parsed in the block on the last call

    decode_size = 0;

    while (1)
    {
        // try to decode a block of hex data into bin data
        parse_status = parse_hex_blob(&_hex_parser, hex_data, size, &block_amt_parsed, _decode_buffer, sizeof(_decode_buffer), &bin_start_address, &bin_buf_written);

        // Get the start address from the first block
        if (_program_addr == 0 && bin_buf_written)
        {
            _program_addr = bin_start_address;
            LOG_INFO("Starting to program hex at 0x%lx", _program_addr);
        }

        // the entire block of hex was decoded. This is a simple state
        if (HEX_PARSE_OK == parse_status)
        {
            if (bin_buf_written > 0)
            {
                decode_size += bin_buf_written;
                if (FlashIface::ERR_NONE != _flash_accessor.write(bin_start_address, _decode_buffer, bin_buf_written))
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
                if (FlashIface::ERR_NONE != _flash_accessor.write(bin_start_address, _decode_buffer, bin_buf_written))
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
                if (FlashIface::ERR_NONE != _flash_accessor.write(bin_start_address, _decode_buffer, bin_buf_written))
                {
                    return false;
                }
            }

            break;
        }
        else if (HEX_PARSE_CKSUM_FAIL == parse_status)
        {
            LOG_ERROR("Checksum failed");
            return false;
        }
        else if ((HEX_PARSE_UNINIT == parse_status) || (HEX_PARSE_FAILURE == parse_status))
        {
            LOG_ERROR("Failed to parse hex: %d", parse_status);
            return false;
        }
    }

    return true;
}
