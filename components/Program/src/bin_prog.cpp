/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */
#include "log.h"
#include "bin_prog.h"
#include "target_swd.h"

#define TAG "bin_prog"

BinProg::BinProg()
    : _file_path(),
      _flash_accessor(FlashAccessor::get_instance())
{
    _flash_accessor.swd_init(TargetSWD::get_instance());
}

BinProg::BinProg(const std::string &file)
    : _file_path(file), _flash_accessor(FlashAccessor::get_instance())
{
    _flash_accessor.swd_init(TargetSWD::get_instance());
}

bool BinProg::programming_bin(const FlashIface::target_cfg_t &cfg, uint32_t addr, const std::string &file)
{
    bool ret = false;
    size_t rd_size = 0;
    uint32_t wr_addr = addr;
    uint32_t total_size = 0;
    FILE *fp = nullptr;
    FlashIface::err_t err = FlashIface::ERR_NONE;

    if (file.empty())
    {
        LOG_ERROR("No file specified");
        goto __exit;
    }

    _file_path = file;
    fp = fopen(file.c_str(), "r");
    if (!fp)
    {
        LOG_ERROR("Failed to open %s", file.c_str());
        goto __exit;
    }

    err = _flash_accessor.init(cfg);
    if (err != FlashIface::ERR_NONE)
    {
        goto __exit;
    }

    LOG_INFO("Starting to program bin at 0x%lx", addr);

    while (feof(fp) == 0)
    {
        rd_size = fread(_bin_buffer, 1, sizeof(_bin_buffer), fp);

        if (rd_size > 0)
        {
            total_size += rd_size;
            if (FlashIface::ERR_NONE != _flash_accessor.write(wr_addr, _bin_buffer, rd_size))
            {
                LOG_ERROR("Failed to write bin at:%lx", wr_addr);
                _flash_accessor.uninit();
                return false;
            }

            wr_addr += rd_size;
        }
    }

    ret = true;
    LOG_INFO("DAPLink write %ld bytes to %s at 0x%08lx successfully", total_size, cfg.device_name.c_str(), addr);

__exit:

    if (fp)
        fclose(fp);

    _flash_accessor.uninit();

    return ret;
}

bool BinProg::programming_bin(const FlashIface::target_cfg_t &cfg, uint32_t addr)
{
    return programming_bin(cfg, addr, _file_path);
}