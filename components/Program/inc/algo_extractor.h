/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-16     Refactor     Improved interface documentation
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "elf.h"
#include "flash_iface.h"
#include "FlashOS.h"

/**
 * @brief Flash algorithm extractor from ELF files
 * 
 * This class extracts flash programming algorithms from Keil FLM (ELF format) files.
 * It parses the ELF file and extracts function addresses, flash device information,
 * and algorithm blobs for use in flash programming operations.
 */
class AlgoExtractor
{
private:
    /**
     * @brief Function information structure
     */
    typedef struct
    {
        uint32_t offset;  ///< Function offset in section
        uint32_t size;    ///< Function size
    } elf_func_t;

    static const std::vector<std::string> _function_list;   ///< List of required functions
    static const uint32_t _flash_blob_header[8];            ///< Flash blob header pattern
    static constexpr uint32_t _stack_size = 0x800;          ///< Stack size for algorithm execution

    /**
     * @brief Read string from ELF string table
     */
    bool read_string(FILE *fp, Elf_Shdr &str_tab_hdr, uint32_t offset, std::string &str);
    
    /**
     * @brief Find section header by program header
     */
    bool find_scn_hdr_by_phdr(FILE *fp, Elf_Ehdr &elf_hdr, Elf_Phdr &phdr, std::vector<Elf_Shdr> &shdr);
    
    /**
     * @brief Get section header string table header
     */
    bool get_shstr_hdr(FILE *fp, Elf_Ehdr &elf_hdr, Elf_Shdr &str_tab_hdr);
    
    /**
     * @brief Extract flash device information
     */
    bool extract_flash_device(FILE *fp, Elf_Sym &sym, Elf_Shdr &shdr, FlashDevice &dev);
    
    /**
     * @brief Extract flash algorithm code
     */
    bool extract_flash_algo(FILE *fp, Elf_Shdr &code_scn, FlashIface::program_target_t &target);
    
    /**
     * @brief Find section header by name
     */
    bool find_shdr(FILE *fp, Elf_Ehdr &elf_hdr, Elf_Shdr &shstr_shdr, const std::string &scn_name, Elf_Shdr &shdr);
    
    /**
     * @brief Find section header by name and type
     */
    bool find_shdr(FILE *fp, Elf_Ehdr &elf_hdr, Elf_Shdr &shstr_shdr, const std::string &scn_name, uint32_t type, Elf_Shdr &shdr);
    
    /**
     * @brief Read ELF header
     */
    bool read_elf_hdr(FILE *fp, Elf_Ehdr &elf_hdr);
    
    /**
     * @brief Read symbol information from ELF file
     */
    void read_symbol_info(FILE *fp, Elf_Shdr &string_hdr, Elf_Shdr &sym_hdr, Elf_Shdr &str_hdr, std::map<std::string, Elf_Sym> &func);

public:
    /**
     * @brief Constructor
     */
    AlgoExtractor();
    
    /**
     * @brief Extract flash algorithm from ELF file
     * @param path Path to ELF file
     * @param target Output program target structure
     * @param cfg Output target configuration
     * @param ram_begin RAM start address for algorithm loading
     * @return true if extraction successful
     */
    bool extract(const std::string &path, FlashIface::program_target_t &target, FlashIface::target_cfg_t &cfg, uint32_t ram_begin = 0x20000000);
};