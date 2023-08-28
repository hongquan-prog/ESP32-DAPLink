/**
 * @file    hex_parser.h
 * @brief   Parser for the intel hex format
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

#pragma once

/** \ingroup hexfile_parser
 *  @{
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** Type of states that the parser can return
     *  @enum hex_parse_status_t
     */
    typedef enum
    {
        HEX_PARSE_OK = 0,       /*!< The input buffer was complete parsed and converted into the output buffer */
        HEX_PARSE_EOF,          /*!< EOF line found in the hex file */
        HEX_PARSE_UNALIGNED,    /*!< The address of decoded data isnt consecutive. Need to program what was returned and continue to parse the input buffer */
        HEX_PARSE_LINE_OVERRUN, /*!< Error state when the record length is longer than the record structure */
        HEX_PARSE_CKSUM_FAIL,   /*!< Error state when the record checksum doesnt properly compute */
        HEX_PARSE_UNINIT,       /*!< Default state. Return of this type is unrecoverable logic error */
        HEX_PARSE_FAILURE       /*!< Amount of hex data to decode didnt match the parsing logics count of decoded bytes */
    } hex_parse_status_t;

#pragma GCC push_options
    typedef union __attribute__((packed, aligned(1)))
    {
        uint8_t buf[0x25];
        struct __attribute__((packed, aligned(1)))
        {
            uint8_t byte_count;
            uint16_t address;
            uint8_t record_type;
            uint8_t data[0x25 - 0x5];
            uint8_t checksum;
        };
    } hex_line_t;
#pragma GCC pop_options

    typedef struct
    {
        hex_line_t line;
        uint32_t next_address_to_write;
        uint8_t low_nibble;
        uint8_t idx;
        uint8_t record_processed;
        uint8_t load_unaligned_record;
        uint8_t skip_until_aligned;
        uint16_t binary_version;
        uint16_t board_id_hex;
        uint16_t board_id_hex_default;
    } hex_parser_t;

    /** Prepare any state that is maintained for the start of a file
     *  @param parser hexfile parser object
     *  @return none
     */
    void reset_hex_parser(hex_parser_t *parser);

    /** Convert a blob of hex data into its binary equivelant
     *  @param parser hexfile parser object
     *  @param hex_blob A block of ascii encoded hexfile data
     *  @param hex_blob_size The amount of valid data in the hex_blob
     *  @param hex_parse_cnt The amount of hex_blob data from the call that was parsed
     *  @param bin_buf Buffer the decoded hex file contents goes into
     *  @param bin_buf_size max size of the buffer
     *  @param bin_buf_address The start address for data in the bin_buf as decoded from the hex file
     *  @param bin_buf_cnt The amount of data in the bin_buf
     *  @return A member of hex_parse_status_t that describes the state of decoding
     */
    hex_parse_status_t parse_hex_blob(hex_parser_t *parser, const uint8_t *hex_blob, const uint32_t hex_blob_size, uint32_t *hex_parse_cnt, uint8_t *bin_buf, const uint32_t bin_buf_size, uint32_t *bin_buf_address, uint32_t *bin_buf_cnt);

#ifdef __cplusplus
}
#endif

/** @} */
