#pragma once

#include "prog.h"
#include "bin_program.h"
#include "hex_program.h"
#include "file_programmer.h"

class ProgOffline : public Prog
{
protected:
    static BinaryProgram _bin_program;
    static HexProgram _hex_program;

private:
    FileProgrammer _file_program;

public:
    ProgOffline();
    virtual void program_start_handle(ProgData &obj) override;
    virtual const char *name() override;
};