#include "swd_iface.h"
#include "debug_cm.h"
#include <cstring>

#define NVIC_Addr (0xe000e000)
#define DBG_Addr (0xe000edf0)

// AP CSW register, base value
#define CSW_VALUE (CSW_RESERVED | CSW_MSTRDBG | CSW_HPROT | CSW_DBGSTAT | CSW_SADDRINC)

// SWD register access
#define SWD_REG_AP (1)
#define SWD_REG_DP (0)
#define SWD_REG_R (1 << 1)
#define SWD_REG_W (0 << 1)
#define SWD_REG_ADR(a) (a & 0x0c)

#define DCRDR 0xE000EDF8
#define DCRSR 0xE000EDF4
#define DHCSR 0xE000EDF0
#define REGWnR (1 << 16)

#define MAX_SWD_RETRY 100
#define MAX_TIMEOUT 100000

//! This can vary from target to target and should be in the structure or flash blob
#define TARGET_AUTO_INCREMENT_PAGE_SIZE (1024)

// DAP ID
#define DAP_ID_VENDOR 1U
#define DAP_ID_PRODUCT 2U
#define DAP_ID_SER_NUM 3U
#define DAP_ID_DAP_FW_VER 4U
#define DAP_ID_DEVICE_VENDOR 5U
#define DAP_ID_DEVICE_NAME 6U
#define DAP_ID_BOARD_VENDOR 7U
#define DAP_ID_BOARD_NAME 8U
#define DAP_ID_PRODUCT_FW_VER 9U
#define DAP_ID_CAPABILITIES 0xF0U
#define DAP_ID_TIMESTAMP_CLOCK 0xF1U
#define DAP_ID_UART_RX_BUFFER_SIZE 0xFBU
#define DAP_ID_UART_TX_BUFFER_SIZE 0xFCU
#define DAP_ID_SWO_BUFFER_SIZE 0xFDU
#define DAP_ID_PACKET_COUNT 0xFEU
#define DAP_ID_PACKET_SIZE 0xFFU

// Debug Port Register Addresses
#define DP_IDCODE 0x00U    // IDCODE Register (SW Read only)
#define DP_ABORT 0x00U     // Abort Register (SW Write only)
#define DP_CTRL_STAT 0x04U // Control & Status
#define DP_WCR 0x04U       // Wire Control Register (SW Only)
#define DP_SELECT 0x08U    // Select Register (JTAG R/W & SW W)
#define DP_RESEND 0x08U    // Resend (SW Read Only)
#define DP_RDBUFF 0x0CU    // Read Buffer (Read Only)

SWDIface::transfer_err_def SWDIface::transfer_retry(uint32_t req, uint32_t *data)
{
    transfer_err_def ack = TRANSFER_OK;

    for (uint8_t i = 0; i < MAX_SWD_RETRY; i++)
    {
        ack = transer(req, data);

        if (ack != TRANSFER_WAIT)
        {
            return ack;
        }
    }

    return ack;
}

bool SWDIface::read_dp(uint8_t adr, uint32_t *val)
{
    uint32_t req = 0;

    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(adr);

    return (transfer_retry(req, val) == TRANSFER_OK);
}

bool SWDIface::write_dp(uint8_t adr, uint32_t val)
{
    uint32_t req = 0;

    switch (adr)
    {
    case DP_SELECT:
        if (_dap_state.select == val)
        {
            return true;
        }

        _dap_state.select = val;
        break;

    default:
        break;
    }

    req = SWD_REG_DP | SWD_REG_W | SWD_REG_ADR(adr);

    return (transfer_retry(req, &val) == TRANSFER_OK);
}

bool SWDIface::read_ap(uint32_t adr, uint32_t *val)
{
    uint32_t req = 0;
    uint32_t apsel = adr & 0xff000000;
    uint32_t bank_sel = adr & APBANKSEL;

    if (!write_dp(DP_SELECT, apsel | bank_sel))
    {
        return false;
    }

    req = SWD_REG_AP | SWD_REG_R | SWD_REG_ADR(adr);
    // first dummy read
    transfer_retry(req, nullptr);


    return (transfer_retry(req, val) == TRANSFER_OK);
}

bool SWDIface::write_ap(uint32_t adr, uint32_t val)
{
    uint32_t req = 0;
    uint32_t apsel = adr & 0xff000000;
    uint32_t bank_sel = adr & APBANKSEL;

    if (!write_dp(DP_SELECT, apsel | bank_sel))
    {
        return false;
    }

    switch (adr)
    {
    case AP_CSW:
        if (_dap_state.csw == val)
        {
            return true;
        }

        _dap_state.csw = val;
        break;

    default:
        break;
    }

    req = SWD_REG_AP | SWD_REG_W | SWD_REG_ADR(adr);

    if (transfer_retry(req, &val) != TRANSFER_OK)
    {
        return false;
    }

    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);

    return (transfer_retry(req, nullptr) == TRANSFER_OK);
}

bool SWDIface::write_block(uint32_t address, uint8_t *data, uint32_t size)
{
    uint32_t req = 0;
    uint32_t size_in_words = size / sizeof(uint32_t);

    if (size == 0)
    {
        return false;
    }

    // CSW register
    if (!write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32))
    {
        return false;
    }

    // TAR write
    req = SWD_REG_AP | SWD_REG_W | (1 << 2);
    if (transfer_retry(req, &address) != TRANSFER_OK)
    {
        return false;
    }

    // DRW write
    req = SWD_REG_AP | SWD_REG_W | (3 << 2);
    for (int i = 0; i < size_in_words; i++)
    {
        if (transfer_retry(req, reinterpret_cast<uint32_t *>(data)) != TRANSFER_OK)
        {
            return false;
        }

        data += sizeof(uint32_t);
    }

    // dummy read
    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);

    return (transfer_retry(req, nullptr) == TRANSFER_OK);
}

bool SWDIface::read_block(uint32_t address, uint8_t *data, uint32_t size)
{
    uint32_t req = 0;
    uint32_t size_in_words = size / sizeof(uint32_t);

    if (size == 0)
    {
        return false;
    }

    if (!write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32))
    {
        return false;
    }

    // TAR write
    req = SWD_REG_AP | SWD_REG_W | AP_TAR;
    if (transfer_retry(req, &address) != TRANSFER_OK)
    {
        return false;
    }

    // read data
    req = SWD_REG_AP | SWD_REG_R | AP_DRW;
    // initiate first read, data comes back in next read
    if (transfer_retry(req, nullptr) != TRANSFER_OK)
    {
        return false;
    }

    for (uint32_t i = 0; i < (size_in_words - 1); i++)
    {
        if (transfer_retry(req, reinterpret_cast<uint32_t *>(data)) != TRANSFER_OK)
        {
            return false;
        }

        data += sizeof(uint32_t);
    }

    // read last word
    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);

    return (transfer_retry(req, reinterpret_cast<uint32_t *>(data)) == TRANSFER_OK);
}

bool SWDIface::read_data(uint32_t addr, uint32_t *val)
{
    uint32_t req = 0;

    // put addr in TAR register
    req = SWD_REG_AP | SWD_REG_W | (1 << 2);
    if (transfer_retry(req, &addr) != TRANSFER_OK)
    {
        return false;
    }

    // read data
    req = SWD_REG_AP | SWD_REG_R | (3 << 2);
    if (transfer_retry(req, nullptr) != TRANSFER_OK)
    {
        return false;
    }

    // dummy read
    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);

    return (transfer_retry(req, val) == TRANSFER_OK);
}

bool SWDIface::write_data(uint32_t address, uint32_t data)
{
    uint32_t req = 0;

    // put addr in TAR register
    req = SWD_REG_AP | SWD_REG_W | (1 << 2);
    if (transfer_retry(req, &address) != TRANSFER_OK)
    {
        return false;
    }

    // write data
    req = SWD_REG_AP | SWD_REG_W | (3 << 2);
    if (transfer_retry(req, &data) != TRANSFER_OK)
    {
        return false;
    }

    // dummy read
    req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);

    return (transfer_retry(req, nullptr) == TRANSFER_OK);
}

bool SWDIface::read_word(uint32_t addr, uint32_t *val)
{
    if (!write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32))
    {
        return false;
    }

    if (!read_data(addr, val))
    {
        return false;
    }

    return true;
}

bool SWDIface::write_word(uint32_t addr, uint32_t val)
{
    if (!write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32))
    {
        return false;
    }

    if (!write_data(addr, val))
    {
        return false;
    }

    return true;
}

bool SWDIface::read_byte(uint32_t addr, uint8_t *val)
{
    uint32_t tmp = 0;

    if (!write_ap(AP_CSW, CSW_VALUE | CSW_SIZE8))
    {
        return false;
    }

    if (!read_data(addr, &tmp))
    {
        return false;
    }

    *val = (uint8_t)(tmp >> ((addr & 0x03) << 3));
    return true;
}

bool SWDIface::write_byte(uint32_t addr, uint8_t val)
{
    uint32_t tmp = 0;

    if (!write_ap(AP_CSW, CSW_VALUE | CSW_SIZE8))
    {
        return false;
    }

    tmp = val << ((addr & 0x03) << 3);
    if (!write_data(addr, tmp))
    {
        return false;
    }

    return true;
}

bool SWDIface::read_memory(uint32_t address, uint8_t *data, uint32_t size)
{
    uint32_t n = 0;

    // Read bytes until word aligned
    while ((size > 0) && (address & 0x3))
    {
        if (!read_byte(address, data))
        {
            return false;
        }

        address++;
        data++;
        size--;
    }

    // Read word aligned blocks
    while (size > 3)
    {
        // Limit to auto increment page size
        n = TARGET_AUTO_INCREMENT_PAGE_SIZE - (address & (TARGET_AUTO_INCREMENT_PAGE_SIZE - 1));

        if (size < n)
        {
            n = size & 0xFFFFFFFC; // Only count complete words remaining
        }

        if (!read_block(address, data, n))
        {
            return false;
        }

        address += n;
        data += n;
        size -= n;
    }

    // Read remaining bytes
    while (size > 0)
    {
        if (!read_byte(address, data))
        {
            return false;
        }

        address++;
        data++;
        size--;
    }

    return true;
}

bool SWDIface::write_memory(uint32_t address, uint8_t *data, uint32_t size)
{
    uint32_t n = 0;

    // Write bytes until word aligned
    while ((size > 0) && (address & 0x3))
    {
        if (!write_byte(address, *data))
        {
            return false;
        }

        address++;
        data++;
        size--;
    }

    // Write word aligned blocks
    while (size > 3)
    {
        // Limit to auto increment page size
        n = TARGET_AUTO_INCREMENT_PAGE_SIZE - (address & (TARGET_AUTO_INCREMENT_PAGE_SIZE - 1));

        if (size < n)
        {
            n = size & 0xFFFFFFFC; // Only count complete words remaining
        }

        if (!write_block(address, data, n))
        {
            return false;
        }

        address += n;
        data += n;
        size -= n;
    }

    // Write remaining bytes
    while (size > 0)
    {
        if (!write_byte(address, *data))
        {
            return false;
        }

        address++;
        data++;
        size--;
    }

    return true;
}

bool SWDIface::read_core_register(uint32_t n, uint32_t *val)
{
    int i = 0;
    int timeout = 100;

    if (!write_word(DCRSR, n))
    {
        return false;
    }

    // wait for S_REGRDY
    for (i = 0; i < timeout; i++)
    {
        if (!read_word(DHCSR, val))
        {
            return false;
        }

        if (*val & S_REGRDY)
        {
            break;
        }
    }

    if (i == timeout)
    {
        return false;
    }

    if (!read_word(DCRDR, val))
    {
        return false;
    }

    return true;
}

bool SWDIface::write_core_register(uint32_t n, uint32_t val)
{
    int i = 0;
    int timeout = 100;

    if (!write_word(DCRDR, val))
    {
        return false;
    }

    if (!write_word(DCRSR, n | REGWnR))
    {
        return false;
    }

    // wait for S_REGRDY
    for (i = 0; i < timeout; i++)
    {
        if (!read_word(DHCSR, &val))
        {
            return false;
        }

        if (val & S_REGRDY)
        {
            return true;
        }
    }

    return false;
}

bool SWDIface::write_debug_state(debug_state_t *state)
{
    uint32_t i = 0;
    uint32_t status = 0;

    if (!write_dp(DP_SELECT, 0))
    {
        return false;
    }

    // R0, R1, R2, R3
    for (i = 0; i < 4; i++)
    {
        if (!write_core_register(i, state->r[i]))
        {
            return false;
        }
    }

    // R9
    if (!write_core_register(9, state->r[9]))
    {
        return false;
    }

    // R13, R14, R15
    for (i = 13; i < 16; i++)
    {
        if (!write_core_register(i, state->r[i]))
        {
            return false;
        }
    }

    // xPSR
    if (!write_core_register(16, state->xpsr))
    {
        return false;
    }

    if (!write_word(DBG_HCSR, DBGKEY | C_DEBUGEN))
    {
        return false;
    }

    // check status
    if (!read_dp(DP_CTRL_STAT, &status))
    {
        return false;
    }

    if (status & (STICKYERR | WDATAERR))
    {
        return false;
    }

    return true;
}

bool SWDIface::wait_until_halted(void)
{
    // Wait for target to stop
    uint32_t val = 0;
    uint32_t timeout = MAX_TIMEOUT;

    for (uint32_t i = 0; i < timeout; i++)
    {
        if (!read_word(DBG_HCSR, &val))
        {
            return false;
        }

        if (val & S_HALT)
        {
            return true;
        }
    }

    return false;
}

bool SWDIface::flash_syscall_exec(const syscall_t *sysCallParam, uint32_t entry, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    debug_state_t state = {{0}, 0};

    // Call flash algorithm function on target and wait for result.
    state.r[0] = arg1;                         // R0: Argument 1
    state.r[1] = arg2;                         // R1: Argument 2
    state.r[2] = arg3;                         // R2: Argument 3
    state.r[3] = arg4;                         // R3: Argument 4
    state.r[9] = sysCallParam->static_base;    // SB: Static Base
    state.r[13] = sysCallParam->stack_pointer; // SP: Stack Pointer
    state.r[14] = sysCallParam->breakpoint;    // LR: Exit Point
    state.r[15] = entry;                       // PC: Entry Point
    state.xpsr = 0x01000000;                   // xPSR: T = 1, ISR = 0

    if (!write_debug_state(&state))
    {
        return false;
    }

    if (!wait_until_halted())
    {
        return false;
    }

    if (!read_core_register(0, &state.r[0]))
    {
        return false;
    }

    // Flash functions return false if successful.
    if (state.r[0] != 0)
    {
        return false;
    }

    return true;
}

bool SWDIface::swd_reset(void)
{
    uint8_t tmp_in[8] = {0};

    for (int i = 0; i < sizeof(tmp_in); i++)
    {
        tmp_in[i] = 0xff;
    }

    swj_sequence(51, tmp_in);

    return true;
}

bool SWDIface::swd_switch(uint16_t val)
{
    uint8_t tmp_in[2] = {0};

    tmp_in[0] = val & 0xff;
    tmp_in[1] = (val >> 8) & 0xff;
    swj_sequence(16, tmp_in);

    return true;
}

bool SWDIface::read_idcode(uint32_t *id)
{
    uint8_t req = 0x00;

    swj_sequence(8, &req);

    return (read_dp(0, id) == TRANSFER_OK);
}

bool SWDIface::jtag_to_swd()
{
    uint32_t tmp = 0;

    if (!swd_reset())
    {
        return false;
    }

    if (!swd_switch(0xE79E))
    {
        return false;
    }

    if (!swd_reset())
    {
        return false;
    }

    if (!read_idcode(&tmp))
    {
        return false;
    }

    return true;
}

bool SWDIface::init_debug(void)
{
    int i = 0;
    uint32_t tmp = 0;
    int timeout = 100;

    // init dap state with fake values
    _dap_state.select = 0xffffffff;
    _dap_state.csw = 0xffffffff;

    init();

    // call a target dependant function
    // this function can do several stuff before really initing the debug
    // target_before_init_debug();

    if (!jtag_to_swd())
    {
        return false;
    }

    if (!write_dp(DP_ABORT, STKCMPCLR | STKERRCLR | WDERRCLR | ORUNERRCLR))
    {
        return false;
    }

    // Ensure CTRL/STAT register selected in DPBANKSEL
    if (!write_dp(DP_SELECT, 0))
    {
        return false;
    }

    // Power up
    if (!write_dp(DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ))
    {
        return false;
    }

    for (i = 0; i < timeout; i++)
    {
        if (!read_dp(DP_CTRL_STAT, &tmp))
        {
            return false;
        }

        if ((tmp & (CDBGPWRUPACK | CSYSPWRUPACK)) == (CDBGPWRUPACK | CSYSPWRUPACK))
        {
            // Break from loop if powerup is complete
            break;
        }
    }

    if (i == timeout)
    {
        // Unable to powerup DP
        return false;
    }

    if (!write_dp(DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ | TRNNORMAL | MASKLANE))
    {
        return false;
    }

    // call a target dependant function:
    // some target can enter in a lock state, this function can unlock these targets
    // target_unlock_sequence();

    if (!write_dp(DP_SELECT, 0))
    {
        return false;
    }

    return true;
}

bool SWDIface::set_target_state(target_state_t state)
{
    uint32_t val = 0;

    /* Calling swd_init prior to enterring RUN state causes operations to fail. */
    if (state != TARGET_RUN)
    {
        init();
    }

    switch (state)
    {
    case TARGET_RESET_HOLD:
        set_target_reset(1);
        break;

    case TARGET_RESET_RUN:
        // Enable debug and halt the core (DHCSR <- 0xA05F0003)
        if (!write_word(DBG_HCSR, DBGKEY | C_DEBUGEN | C_HALT))
        {
            return false;
        }

        // Wait until core is halted
        do
        {
            if (!read_word(DBG_HCSR, &val))
            {
                return false;
            }
        } while ((val & S_HALT) == 0);

        // Perform a soft reset
        if (!read_word(NVIC_AIRCR, &val))
        {
            return false;
        }

        if (!write_word(NVIC_AIRCR, VECTKEY | (val & SCB_AIRCR_PRIGROUP_Msk) | SYSRESETREQ))
        {
            return false;
        }

        msleep(20);
        off();
        break;

    case TARGET_RESET_PROGRAM:
        if (!init_debug())
        {
            return false;
        }

        // Enable debug and halt the core (DHCSR <- 0xA05F0003)
        if (!write_word(DBG_HCSR, DBGKEY | C_DEBUGEN | C_HALT))
        {
            return false;
        }

        // Wait until core is halted
        do
        {
            if (!read_word(DBG_HCSR, &val))
            {
                return false;
            }
        } while ((val & S_HALT) == 0);

        // Enable halt on reset
        if (!write_word(DBG_EMCR, VC_CORERESET))
        {
            return false;
        }

        // Perform a soft reset
        if (!read_word(NVIC_AIRCR, &val))
        {
            return false;
        }

        msleep(1);

        if (!write_word(NVIC_AIRCR, VECTKEY | (val & SCB_AIRCR_PRIGROUP_Msk) | SYSRESETREQ))
        {
            return false;
        }

        msleep(20);

        do
        {
            if (!read_word(DBG_HCSR, &val))
            {
                return false;
            }
        } while ((val & S_HALT) == 0);

        // Disable halt on reset
        if (!write_word(DBG_EMCR, 0))
        {
            return false;
        }
        break;

    case TARGET_NO_DEBUG:
        if (!write_word(DBG_HCSR, DBGKEY))
        {
            return false;
        }

        break;

    case TARGET_DEBUG:
        if (!jtag_to_swd())
        {
            return false;
        }

        if (!write_dp(DP_ABORT, STKCMPCLR | STKERRCLR | WDERRCLR | ORUNERRCLR))
        {
            return false;
        }

        // Ensure CTRL/STAT register selected in DPBANKSEL
        if (!write_dp(DP_SELECT, 0))
        {
            return false;
        }

        // Power up
        if (!write_dp(DP_CTRL_STAT, CSYSPWRUPREQ | CDBGPWRUPREQ))
        {
            return false;
        }

        // Enable debug
        if (!write_word(DBG_HCSR, DBGKEY | C_DEBUGEN))
        {
            return false;
        }

        break;

    case TARGET_HALT:
        if (!init_debug())
        {
            return false;
        }

        // Enable debug and halt the core (DHCSR <- 0xA05F0003)
        if (!write_word(DBG_HCSR, DBGKEY | C_DEBUGEN | C_HALT))
        {
            return false;
        }

        // Wait until core is halted
        do
        {
            if (!read_word(DBG_HCSR, &val))
            {
                return false;
            }
        } while ((val & S_HALT) == 0);

        break;

    case TARGET_RUN:
        if (!write_word(DBG_HCSR, DBGKEY))
        {
            return false;
        }

        off();
        break;
    case TARGET_POST_FLASH_RESET:
        // This state should be handled in target_reset.c, nothing needs to be done here.
        break;
    default:
        return false;
    }

    return true;
}
