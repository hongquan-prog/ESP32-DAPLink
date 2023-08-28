#pragma once

#include <iostream>

class Programmer
{
public:
    virtual ~Programmer();
    virtual bool start(const std::string &file) = 0;
    virtual bool start() = 0;
};