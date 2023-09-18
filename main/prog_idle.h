#pragma once

#include "prog.h"

class ProgIdle : public Prog
{
public:
    virtual void request_handle(ProgData &obj) override;
    virtual const char *name() override;
};
