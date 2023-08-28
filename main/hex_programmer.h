#pragma once

#include "bin_programmer.h"
#include "hex_parser.h"
#include "ff.h"

class HexProgrammer : public BinProgrammer
{
private:
    static constexpr int _buf_size = 256;
    uint8_t _hex_buffer[_buf_size];
    uint8_t _bin_buffer[_buf_size];
    hex_parser_t _parser;
    FIL _file_object;

public:
    HexProgrammer();
    HexProgrammer(const std::string &file);
    virtual ~HexProgrammer();
    bool start(const std::string &file) override;
    bool start() override;
    bool write_hex(const uint8_t *data, uint32_t size);
};