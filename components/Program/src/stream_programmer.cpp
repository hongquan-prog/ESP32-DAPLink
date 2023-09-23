#include "stream_programmer.h"
#include "log.h"
#include <sys/stat.h>
#include <cstring>

#define TAG "stream_programmer"

StreamProgrammer::StreamProgrammer(ProgramIface &binary_program, ProgramIface &hex_program)
    : _binary_program(binary_program), _hex_program(hex_program),
      _iface(nullptr)
{
}

StreamProgrammer::~StreamProgrammer()
{
    clean();
}

bool StreamProgrammer::init(StreamProgrammer::Mode mode, FlashIface::target_cfg_t &cfg, uint32_t program_addr)
{
    _iface = nullptr;

    if (BIN_MODE == mode)
        _iface = &_binary_program;
    else if (HEX_MODE == mode)
        _iface = &_hex_program;

    if (!_iface)
    {
        LOG_ERROR("Invalid program mode");
        return false;
    }

    if (_iface->init(cfg, program_addr) != true)
    {
        return false;
    }

    return true;
}

bool StreamProgrammer::write(uint8_t *data, size_t len)
{
    if (_iface && _iface->write(data, len))
    {
        return true;
    }

    if (_iface)
        _iface->clean();

    return false;
}

void StreamProgrammer::clean(void)
{
    if (_iface)
        _iface->clean();
}