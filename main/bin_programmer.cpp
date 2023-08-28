#include "bin_programmer.h"

BinProgrammer::BinProgrammer()
{
}

BinProgrammer::BinProgrammer(const std::string &file)
{
}

bool BinProgrammer::bin_write(uint32_t address, uint8_t *data, uint32_t size)
{
    return false;
}

bool BinProgrammer::start(const std::string &file)
{
    return false;
}

bool BinProgrammer::start()
{
    return false;
}