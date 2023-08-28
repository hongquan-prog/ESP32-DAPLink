#pragma once

#include "programmer.h"

class BinProgrammer : public Programmer
{
protected:
    std::string _file;

public:
    BinProgrammer();
    BinProgrammer(const std::string &file);
    virtual ~BinProgrammer();
    bool bin_write(uint32_t address, uint8_t *data, uint32_t size);
    virtual bool start(const std::string &file) override;
    virtual bool start() override;
};