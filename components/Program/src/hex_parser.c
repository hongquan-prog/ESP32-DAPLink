/**
 * @file    hex_parser.c
 * @brief   Implementation of hex_parser.h
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Change Logs:
 * Date           Author                    Notes
 * 2023-7-23      hongquan.prog@gmail.com   port to esp32s3
 */

#include <string.h>
#include "hex_parser.h"

typedef enum
{
    HEX_DATA_RECORD = 0,
    HEX_EOF_RECORD = 1,
    HEX_EXT_SEG_ADDR_RECORD = 2,
    HEX_START_SEG_ADDR_RECORD = 3,
    HEX_EXT_LINEAR_ADDR_RECORD = 4,
    HEX_START_LINEAR_ADDR_RECORD = 5,
    HEX_CUSTOM_METADATA_RECORD = 0x0A,
    HEX_CUSTOM_DATA_RECORD = 0x0D,
} hex_record_t;

/** Swap 16bit value - let compiler figure out the best way
 *  @param val a variable of size uint16_t to be swapped
 *  @return the swapped value
 */
static uint16_t swap16(uint16_t a)
{
    return ((a & 0x00ff) << 8) | ((a & 0xff00) >> 8);
}

/** Converts a character representation of a hex to real value.
 *   @param c is the hex value in char format
 *   @return the value of the hex
 */
static uint8_t ctoh(char c)
{
    return (c & 0x10) ? /*0-9*/ c & 0xf : /*A-F, a-f*/ (c & 0xf) + 9;
}

/** Calculate checksum on a hex record
 *   @param data is the line of hex record
 *   @param size is the length of the data array
 *   @return 1 if the data provided is a valid hex record otherwise 0
 */
static uint8_t validate_checksum(hex_line_t *record)
{
    uint8_t result = 0, i = 0;

    for (; i < (record->byte_count + 5); i++)
    {
        result += record->buf[i];
    }

    return (result == 0);
}

void reset_hex_parser(hex_parser_t *parser)
{
    memset(parser, 0, sizeof(hex_parser_t));
}

hex_parse_status_t parse_hex_blob(hex_parser_t *parser, const uint8_t *hex_blob, const uint32_t hex_blob_size, uint32_t *hex_parse_cnt, uint8_t *bin_buf, const uint32_t bin_buf_size, uint32_t *bin_buf_address, uint32_t *bin_buf_cnt)
{
    uint8_t *end = (uint8_t *)hex_blob + hex_blob_size;
    hex_parse_status_t status = HEX_PARSE_UNINIT;
    // reset the amount of data that is being return'd
    *bin_buf_cnt = (uint32_t)0;

    if (parser->skip_until_aligned)
    {
        if (hex_blob[0] == ':')
        {
            // This is block is aligned we can stop skipping
            parser->skip_until_aligned = 0;
        }
        else
        {
            // This is block is not aligned we can skip it
            status = HEX_PARSE_OK;
            goto hex_parser_exit;
        }
    }

    // we had an exit state where the address was unaligned to the previous record and data count.
    //  Need to pop the last record into the buffer before decoding anthing else since it was
    //  already decoded.
    if (parser->load_unaligned_record)
    {
        // need some help...
        parser->load_unaligned_record = 0;
        // move from line buffer back to input buffer
        memcpy((uint8_t *)bin_buf, (uint8_t *)parser->line.data, parser->line.byte_count);
        bin_buf += parser->line.byte_count;
        *bin_buf_cnt = (uint32_t)(*bin_buf_cnt) + parser->line.byte_count;
        // Store next address to write
        parser->next_address_to_write = ((parser->next_address_to_write & 0xffff0000) | parser->line.address) + parser->line.byte_count;
    }

    while (hex_blob != end)
    {
        switch ((uint8_t)(*hex_blob))
        {
        // we've hit the end of an ascii line
        // junk we dont care about could also just run the validate_checksum on &line
        case '\r':
        case '\n':
            // ignore new lines
            break;

        // found start of a new record. reset state variables
        case ':':
            memset(parser->line.buf, 0, sizeof(hex_line_t));
            parser->low_nibble = 0;
            parser->idx = 0;
            parser->record_processed = 0;
            break;

        // decoding lines
        default:
            if (parser->low_nibble)
            {
                parser->line.buf[parser->idx] |= ctoh((uint8_t)(*hex_blob)) & 0xf;
                if (++(parser->idx) >= (parser->line.byte_count + 5))
                {
                    // all data in
                    if (0 == validate_checksum(&parser->line))
                    {
                        status = HEX_PARSE_CKSUM_FAIL;
                        goto hex_parser_exit;
                    }
                    else
                    {
                        if (!parser->record_processed)
                        {
                            parser->record_processed = 1;
                            // address byteswap...
                            parser->line.address = swap16(parser->line.address);

                            switch (parser->line.record_type)
                            {
                            case HEX_CUSTOM_METADATA_RECORD:
                                parser->binary_version = (uint16_t)parser->line.data[0] << 8 | parser->line.data[1];
                                break;

                            case HEX_DATA_RECORD:
                            case HEX_CUSTOM_DATA_RECORD:
                                if (parser->binary_version == 0)
                                {
                                    // Only save data from the correct binary
                                    // verify this is a continous block of memory or need to exit and dump
                                    if (((parser->next_address_to_write & 0xffff0000) | parser->line.address) != parser->next_address_to_write)
                                    {
                                        parser->load_unaligned_record = 1;
                                        status = HEX_PARSE_UNALIGNED;
                                        // Function will be executed again and will start by finishing to process this record by
                                        // adding the this line into bin_buf, so the 1st loop iteration should be the next blob byte
                                        hex_blob++;
                                        goto hex_parser_exit;
                                    }
                                    else
                                    {
                                        // This should be superfluous but it is necessary for GCC
                                        parser->load_unaligned_record = 0;
                                    }

                                    // move from line buffer back to input buffer
                                    memcpy(bin_buf, parser->line.data, parser->line.byte_count);
                                    bin_buf += parser->line.byte_count;
                                    *bin_buf_cnt = (uint32_t)(*bin_buf_cnt) + parser->line.byte_count;
                                    // Save next address to write
                                    parser->next_address_to_write = ((parser->next_address_to_write & 0xffff0000) | parser->line.address) + parser->line.byte_count;
                                }
                                else
                                {
                                    // This is Universal Hex block that does not match our version.
                                    // We can skip this block and all blocks until we find a
                                    // block aligned on a record boundary.
                                    parser->skip_until_aligned = 1;
                                    status = HEX_PARSE_OK;
                                    goto hex_parser_exit;
                                }
                                break;

                            case HEX_EOF_RECORD:
                                status = HEX_PARSE_EOF;
                                goto hex_parser_exit;

                            case HEX_EXT_SEG_ADDR_RECORD:
                                // Could have had data in the buffer so must exit and try to program
                                //  before updating bin_buf_address with next_address_to_write
                                memset(bin_buf, 0xff, (bin_buf_size - (uint32_t)(*bin_buf_cnt)));
                                // figure the start address for the buffer before returning
                                *bin_buf_address = parser->next_address_to_write - (uint32_t)(*bin_buf_cnt);
                                *hex_parse_cnt = (uint32_t)(hex_blob_size - (end - hex_blob));
                                // update the address msb's
                                parser->next_address_to_write = (parser->next_address_to_write & 0x00000000) | ((parser->line.data[0] << 12) | (parser->line.data[1] << 4));
                                // Need to exit and program if buffer has been filled
                                status = HEX_PARSE_UNALIGNED;
                                return status;

                            case HEX_EXT_LINEAR_ADDR_RECORD:
                                // Could have had data in the buffer so must exit and try to program
                                //  before updating bin_buf_address with next_address_to_write
                                //  Good catch Gaute!!
                                memset(bin_buf, 0xff, (bin_buf_size - (uint32_t)(*bin_buf_cnt)));
                                // figure the start address for the buffer before returning
                                *bin_buf_address = parser->next_address_to_write - (uint32_t)(*bin_buf_cnt);
                                *hex_parse_cnt = (uint32_t)(hex_blob_size - (end - hex_blob));
                                // update the address msb's
                                parser->next_address_to_write = (parser->next_address_to_write & 0x00000000) | ((parser->line.data[0] << 24) | (parser->line.data[1] << 16));
                                // Need to exit and program if buffer has been filled
                                status = HEX_PARSE_UNALIGNED;
                                return status;

                            default:
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                if (parser->idx < sizeof(hex_line_t))
                {
                    parser->line.buf[parser->idx] = ctoh((uint8_t)(*hex_blob)) << 4;
                }
            }

            parser->low_nibble = !parser->low_nibble;
            break;
        }

        hex_blob++;
    }

    // decoded an entire hex block - verify (cant do this hex_parse_cnt is figured below)
    // status = (hex_blob_size == (uint32_t)(*hex_parse_cnt)) ? HEX_PARSE_OK : HEX_PARSE_FAILURE;
    status = HEX_PARSE_OK;

hex_parser_exit:
    memset(bin_buf, 0xff, (bin_buf_size - (uint32_t)(*bin_buf_cnt)));

    // figure the start address for the buffer before returning
    *bin_buf_address = parser->next_address_to_write - (uint32_t)(*bin_buf_cnt);
    *hex_parse_cnt = (uint32_t)(hex_blob_size - (end - hex_blob));
    return status;
}
