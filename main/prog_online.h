#pragma once

#include "prog_offline.h"
#include "stream_programmer.h"

class ProgOnline : public ProgOffline
{
private:
    bool _recved_new_packet;
    uint32_t _start_time;
    uint32_t _writed_offset;
    uint32_t _total_size;
    StreamProgrammer _stream_program;

public:
    ProgOnline();
    virtual void program_start_handle(ProgData &obj) override;
    virtual void program_timeout_handle(ProgData &obj) override;
    virtual void program_data_handle(ProgData &obj) override;
    virtual const char *name() override;
};