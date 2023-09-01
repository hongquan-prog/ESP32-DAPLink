#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "flm_extractor.h"

const std::vector<std::string> FlmExtractor::_function_list = {
    "FlashDevice",
    "Init",
    "UnInit",
    "BlankCheck",
    "EraseChip",
    "EraseSector",
    "ProgramPage",
    "Verify"};

const uint32_t FlmExtractor::_flash_bolb_header[8] = {0xE00ABE00, 0x062D780D, 0x24084068, 0xD3000040, 0x1E644058, 0x1C49D1FA, 0x2A001E52, 0x4770D1F};

FlmExtractor::FlmExtractor()
{
}

bool FlmExtractor::read_elf_hdr(FILE *fp, Elf_Ehdr &elf_hdr)
{
    fseek(fp, 0, SEEK_SET);

    if ((sizeof(Elf_Ehdr) != fread(&elf_hdr, 1, sizeof(Elf_Ehdr), fp)) || !IS_ELF(elf_hdr) || (elf_hdr.e_type != ET_EXEC))
        throw std::runtime_error("Invalid ELF file");

    return true;
}

bool FlmExtractor::find_shdr(FILE *fp, Elf_Ehdr &elf_hdr, Elf_Shdr &shstr_shdr, const std::string &scn_name, Elf_Shdr &shdr)
{
    long cur_pos = 0;
    std::string name;

    fseek(fp, elf_hdr.e_shoff, SEEK_SET);

    for (int i = 0; i < elf_hdr.e_shnum; i++)
    {
        if ((sizeof(Elf_Shdr) != fread(&shdr, 1, sizeof(Elf_Shdr), fp)))
            throw std::runtime_error("could not find section " + scn_name);

        cur_pos = ftell(fp);

        if (read_string(fp, shstr_shdr, shdr.sh_name, name) && !scn_name.compare(name))
            return true;

        fseek(fp, cur_pos, SEEK_SET);
    }

    throw std::runtime_error("could not find section " + scn_name);
}

bool FlmExtractor::find_shdr(FILE *fp, Elf_Ehdr &elf_hdr, Elf_Shdr &shstr_shdr, const std::string &scn_name, uint32_t type, Elf_Shdr &shdr)
{
    long cur_pos = 0;
    std::string name;

    fseek(fp, elf_hdr.e_shoff, SEEK_SET);

    for (int i = 0; i < elf_hdr.e_shnum; i++)
    {
        if ((sizeof(Elf_Shdr) != fread(&shdr, 1, sizeof(Elf_Shdr), fp)))
            throw std::runtime_error("could not find section " + scn_name);

        cur_pos = ftell(fp);

        if ((type == shdr.sh_type) && read_string(fp, shstr_shdr, shdr.sh_name, name) && !scn_name.compare(name))
            return true;

        fseek(fp, cur_pos, SEEK_SET);
    }

    throw std::runtime_error("could not find section " + scn_name);
}

bool FlmExtractor::get_shstr_hdr(FILE *fp, Elf_Ehdr &elf_hdr, Elf_Shdr &shstr_shdr)
{
    fseek(fp, elf_hdr.e_shoff + elf_hdr.e_shstrndx * elf_hdr.e_shentsize, SEEK_SET);

    if (fread(&shstr_shdr, 1, sizeof(Elf_Shdr), fp) != sizeof(Elf_Shdr))
        throw std::runtime_error("could not find section header table");

    return true;
}

bool FlmExtractor::read_string(FILE *fp, Elf_Shdr &str_shdr, uint32_t offset, std::string &str)
{
    int c_tmp = 0;
    // FLM所用函数的名称的长度小于30字符
    char buf[300] = {0};
    int bytes_read = 0;

    fseek(fp, str_shdr.sh_offset + offset, SEEK_SET);
    while ((c_tmp = fgetc(fp)) != EOF && (c_tmp != '\0'))
    {
        if (bytes_read + 1 >= sizeof(buf))
            return false;

        buf[bytes_read++] = (char)c_tmp;
    }

    buf[bytes_read] = '\0';
    str.replace(0, str.length(), buf);

    return true;
}

void FlmExtractor::read_symbol_info(FILE *fp, Elf_Shdr &string_hdr, Elf_Shdr &sym_hdr, Elf_Shdr &str_hdr, std::map<std::string, Elf_Sym> &sym)
{
    std::string name;
    long cur_pos = 0;
    Elf_Sym tmp = {0, 0, 0, 0, 0, 0};
    int sym_section_num = sym_hdr.sh_size / sizeof(Elf_Sym);

    fseek(fp, sym_hdr.sh_offset, SEEK_SET);

    for (int i = 0; i < sym_section_num; i++)
    {
        fread(&tmp, 1, sizeof(Elf_Sym), fp);
        cur_pos = ftell(fp);

        if (read_string(fp, str_hdr, tmp.st_name, name))
        {
            auto it = std::find(_function_list.begin(), _function_list.end(), name);
            if (it != _function_list.end())
            {
                sym[name] = tmp;
            }
        }

        fseek(fp, cur_pos, SEEK_SET);
    }
}

bool FlmExtractor::extract_flash_device(FILE *fp, Elf_Sym &sym, Elf_Shdr &shdr, FlashDevice &dev)
{
    fseek(fp, sym.st_value - shdr.sh_addr + shdr.sh_offset, SEEK_SET);

    if (fread(&dev, 1, sizeof(FlashDevice), fp) != sizeof(FlashDevice))
        throw std::runtime_error("could not read flash device info");

    return true;
}

bool FlmExtractor::extract_flash_algo(FILE *fp, Elf_Shdr &code_scn, program_target_t &target)
{
    fseek(fp, code_scn.sh_offset, SEEK_SET);
    target.algo_start = _ram_start;
    target.algo_size = sizeof(_flash_bolb_header) + code_scn.sh_size;
    target.algo_blob = reinterpret_cast<uint32_t *>(malloc(target.algo_size));
    memcpy(target.algo_blob, _flash_bolb_header, sizeof(_flash_bolb_header));

    if (fread(target.algo_blob + sizeof(_flash_bolb_header) / sizeof(uint32_t), 1, code_scn.sh_size, fp) != code_scn.sh_size)
        throw std::runtime_error("could not read flash algo");

    return true;
}

bool FlmExtractor::extract(const std::string &path, program_target_t &target, std::vector<sector_info_t> &sector, target_cfg_t &cfg)
{
    FILE *fp = nullptr;
    Elf_Ehdr elf_hdr;
    Elf_Shdr sym_shdr;
    Elf_Shdr str_shdr;
    Elf_Shdr shstr_shdr;
    std::map<std::string, Elf_Sym> sym_table;
    std::map<std::string, Elf_Shdr> prog_table;
    FlashDevice *device = reinterpret_cast<FlashDevice *>(malloc(sizeof(FlashDevice)));

    try
    {
        fp = fopen(path.c_str(), "r");
        if (fp == nullptr)
            throw std::ifstream::failure("open file failed");

        memset(&shstr_shdr, 0, sizeof(Elf_Shdr));
        memset(&sym_shdr, 0, sizeof(Elf_Shdr));
        memset(&str_shdr, 0, sizeof(Elf_Shdr));
        memset(device, 0, sizeof(Elf_Shdr));
        memset(&target, 0, sizeof(program_target_t));

        read_elf_hdr(fp, elf_hdr);
        get_shstr_hdr(fp, elf_hdr, shstr_shdr);
        find_shdr(fp, elf_hdr, shstr_shdr, ".strtab", str_shdr);
        find_shdr(fp, elf_hdr, shstr_shdr, ".symtab", sym_shdr);
        find_shdr(fp, elf_hdr, shstr_shdr, "DevDscr", prog_table["DevDscr"]);
        find_shdr(fp, elf_hdr, shstr_shdr, "PrgCode", prog_table["PrgCode"]);
        find_shdr(fp, elf_hdr, shstr_shdr, "PrgData", prog_table["PrgData"]);
        read_symbol_info(fp, str_shdr, sym_shdr, str_shdr, sym_table);
        extract_flash_device(fp, sym_table["FlashDevice"], prog_table["DevDscr"], *device);
        extract_flash_algo(fp, prog_table["PrgCode"], target);

        /* 在 DAPLink 中，static_base 是指程序的静态基地址（Static Base Address）。静态基地址是一个指针，指向程序的全局变量和静态变量的起始位置。 */
        target.sys_call_s.static_base = target.algo_start + prog_table["PrgCode"].sh_size + sizeof(_flash_bolb_header);
        target.sys_call_s.breakpoint = target.algo_start + 1;
        /* 设置栈顶指针,位于数据段之后,栈大小为4K */
        target.sys_call_s.stack_pointer = ((target.algo_start + target.algo_size) + 0x100 - 1) / 0x100 * 0x100 + _stack_size;
        /* 设置烧录数据的首地址，确保首地址为0x100的整数倍 */
        target.program_buffer = target.sys_call_s.stack_pointer;
        target.program_buffer_size = device->szPage;
        /* 设置Flash读写函数的地址 */
        target.init = (sym_table.find("Init") != sym_table.end()) ? (sym_table["Init"].st_value + sizeof(_flash_bolb_header) + target.algo_start) : (0);
        target.uninit = (sym_table.find("UnInit") != sym_table.end()) ? (sym_table["UnInit"].st_value + sizeof(_flash_bolb_header) + target.algo_start) : (0);
        target.erase_chip = (sym_table.find("EraseChip") != sym_table.end()) ? (sym_table["EraseChip"].st_value + sizeof(_flash_bolb_header) + target.algo_start) : (0);
        target.erase_sector = (sym_table.find("EraseSector") != sym_table.end()) ? (sym_table["EraseSector"].st_value + sizeof(_flash_bolb_header) + target.algo_start) : (0);
        target.program_page = (sym_table.find("ProgramPage") != sym_table.end()) ? (sym_table["ProgramPage"].st_value + sizeof(_flash_bolb_header) + target.algo_start) : (0);
        target.verify = (sym_table.find("Verify") != sym_table.end()) ? (sym_table["Verify"].st_value + sizeof(_flash_bolb_header) + target.algo_start) : (0);

        for (int i = 0; i < SECTOR_NUM; i++)
        {
            if (device->sectors[i].adrSector == 0xffffffff && device->sectors[i].szSector == 0xffffffff)
                break;
            sector.push_back(sector_info_t{device->devAdr + device->sectors[i].adrSector, device->sectors[i].szSector});
        }

        cfg.version = kTargetConfigVersion;
        cfg.sectors_info = sector.data();
        cfg.sector_info_length = sector.size();
        cfg.erase_reset = false;
        cfg.flash_regions[0].start = device->devAdr;
        cfg.flash_regions[0].end = device->devAdr + device->szDev;
        cfg.flash_regions[0].flags = kRegionIsDefault;
        cfg.flash_regions[0].flash_algo = &target;
        cfg.ram_regions[0].start = _ram_start;
        cfg.ram_regions[0].end = target.program_buffer + target.program_buffer_size;

        printf("Version Number and Architecture: %d\n", device->vers);
        printf("Device Name and Description: %s\n", device->devName);
        printf("Device Type: %x\n", device->devType);
        printf("Default Device Start Address: %lx\n", device->devAdr);
        printf("Total Size of Device: %lx\n", device->szDev);
        printf("Programming Page Size: %lx\n", device->szPage);
        printf("Reserved for future Extension: %lx\n", device->res);
        printf("Content of Erased Memory: %x\n", device->valEmpty);
        printf("Time Out of Program Page Function: %ld\n", device->toProg);
        printf("Time Out of Erase Sector Function: %ld\n", device->toErase);

        fclose(fp);
        free(device);
    }
    catch (...)
    {
        fclose(fp);
        free(device);
        free(target.algo_blob);

        return false;
    }

    return true;
}