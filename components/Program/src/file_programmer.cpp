/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-16     Refactor     Fixed typos and improved code style
 */
#include "file_programmer.h"
#include "log.h"
#include <sys/stat.h>
#include <cstring>

#define TAG "file_programmer"

FileProgrammer::FileProgrammer(ProgramIface &binary_program, ProgramIface &hex_program)
    : _binary_program(binary_program), _hex_program(hex_program), _program_progress(0), _progress_changed_cb(nullptr)
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

ProgramIface *FileProgrammer::select_program_iface(const std::string &path)
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

    iface = select_program_iface(path);
    if (iface == nullptr)
    {
        LOG_ERROR("Unsupported file format: %s", path.c_str());
        return false;
    }

    fp = fopen(path.c_str(), "r");
    if (!fp)
    {
        LOG_ERROR("Failed to open %s", path.c_str());
        return false;
    }

    set_program_progress(0);
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
                LOG_ERROR("Failed to write at address: 0x%lx", (unsigned long)iface->get_program_address());
                return false;
            }

            set_program_progress(ftell(fp) * 100 / file_size);
        }
    }

    set_program_progress(100);
    fclose(fp);
    iface->clean();

    return true;
}

int FileProgrammer::get_program_progress(void)
{
    return _program_progress;
}

void FileProgrammer::set_program_progress(int progress)
{
    _program_progress = progress;

    if (_progress_changed_cb)
        _progress_changed_cb(_program_progress);
}

void FileProgrammer::register_progress_changed_callback(const progress_changed_cb_t &func)
{
    _progress_changed_cb = func;
}

bool FileProgrammer::is_exist(const char *path)
{
    struct stat file_stat;
    return (path && (stat(path, &file_stat) == 0));
}