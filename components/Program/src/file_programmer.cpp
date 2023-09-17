#include "file_programmer.h"
#include "log.h"
#include <sys/stat.h>
#include <cstring>

#define TAG "file_programmer"

FileProgrammer::FileProgrammer(ProgramIface &binary_program, ProgramIface &hex_program)
    : _binary_program(binary_program), _hex_program(hex_program), _program_progress(0)
{
}

bool FileProgrammer::compare_extension(const char *filename, const char *extension)
{
    const char *dot = strrchr(filename, '.');

    if (dot && dot != filename)
    {
        return strcmp(dot, extension) == 0;
    }

    return false;
}

ProgramIface *FileProgrammer::selcet_program_iface(const std::string &path)
{
    if (compare_extension(path.c_str(), ".hex"))
    {
        return &_hex_program;
    }
    else if (compare_extension(path.c_str(), ".bin"))
    {
        return &_binary_program;
    }

    return nullptr;
}

bool FileProgrammer::program(const std::string &path, FlashIface::target_cfg_t &cfg, uint32_t program_addr)
{
    FILE *fp = nullptr;
    size_t rd_size = 0;
    uint32_t file_size = 0;
    ProgramIface *iface = nullptr;

    if (path.empty())
    {
        LOG_ERROR("No file specified");
        return false;
    }

    iface = selcet_program_iface(path);
    if (iface == nullptr)
    {
        return false;
    }

    fp = fopen(path.c_str(), "r");
    if (!fp)
    {
        LOG_ERROR("Failed to open %s", path.c_str());
        return false;
    }

    _program_progress = 0;
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (iface->init(cfg, program_addr) != true)
    {
        fclose(fp);
        return false;
    }

    while (feof(fp) == 0)
    {
        rd_size = fread(_buffer, 1, sizeof(_buffer), fp);

        if (rd_size > 0)
        {
            if (iface->write(_buffer, rd_size) != true)
            {
                fclose(fp);
                iface->clean();
                LOG_ERROR("Failed to write hex at:%x", iface->get_program_address());
                return false;
            }

            _program_progress = ftell(fp) * 100 / file_size;
        }
    }

    _program_progress = 100;
    fclose(fp);
    iface->clean();

    return true;
}

int FileProgrammer::get_program_progress(void)
{
    return _program_progress;
}

bool FileProgrammer::is_exist(const char *path)
{
    struct stat file_stat;
    return (path && (stat(path, &file_stat) == 0));
}

void FileProgrammer::reset(void)
{
    _program_progress = 0;
}