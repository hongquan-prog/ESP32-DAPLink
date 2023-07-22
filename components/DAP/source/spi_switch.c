/**
 * @file spi_switch.c
 * @author windowsair
 * @brief Switching between SPI mode and IO mode
 * @change: 2021-3-7 Support esp32 SPI
 *          2021-3-20 Fix pure GPIO and problem of mode switching
 * @version 0.2
 * @date 2021-3-20
 *
 * @copyright Copyright (c) 2021
 *
 */
#include <stdbool.h>

#include "cmsis_compiler.h"
#include "spi_switch.h"
#include "DAP_config.h"

// soc register
#include "esp32s3/include/soc/io_mux_reg.h"
#include "include/soc/gpio_periph.h"
#include "esp32s3/include/soc/gpio_struct.h"
#include "hal/gpio_types.h"
#include "driver/spi_master.h"

#include "esp32s3/include/soc/dport_access.h"
#include "esp32s3/include/soc/dport_reg.h"
#include "esp32s3/include/soc/periph_defs.h"
#include "esp32s3/include/soc/spi_struct.h"
#include "esp32s3/include/soc/spi_reg.h"

#define ENTER_CRITICAL() portENTER_CRITICAL()
#define EXIT_CRITICAL() portEXIT_CRITICAL()

spi_device_handle_t spi = {0};

spi_bus_config_t buscfg = {
    .sclk_io_num = PIN_SWDIO_MOSI,
    .mosi_io_num = PIN_SWCLK,
    .miso_io_num = -1,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1};

spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 10 * 1000 * 1000, // Clock out at 10 MHz
    .mode = 3,                          // SPI mode 0
    .spics_io_num = -1,                 // CS pin
    .queue_size = 1,                    // We want to be able to queue 7 transactions at a time
    .command_bits = 0,
    .address_bits = 0,
    .dummy_bits = 0,
    .duty_cycle_pos = 128,
    .flags = SPI_DEVICE_HALFDUPLEX};

/**
 * @brief Initialize on first use
 *
 */
void DAP_SPI_Init()
{
    // In esp32, the driving of GPIO should be stopped,
    // otherwise there will be issue in the spi
    GPIO.out_w1tc = (0x1 << PIN_SWDIO_MOSI);
    GPIO.out_w1tc = (0x1 << PIN_SWCLK);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi));
}

/**
 * @brief Switch to GPIO
 * Note: You may be able to pull the pin high in SPI mode, though you cannot set it to LOW
 */
__FORCEINLINE void DAP_SPI_Deinit()
{
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[PIN_SWCLK], PIN_FUNC_GPIO);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[PIN_SWDIO_MOSI], PIN_FUNC_GPIO); // MOSI

    // enable SWCLK output
    GPIO.enable_w1ts = (0x01 << PIN_SWCLK);

    // disable MISO output connect
    // GPIO.enable_w1tc = (0x1 << 12);

    // enable MOSI output & input
    GPIO.enable_w1ts |= (0x1 << PIN_SWDIO_MOSI);
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[PIN_SWDIO_MOSI]);

    // enable MOSI OD output
    // GPIO.pin[PIN_SWDIO_MOSI].pad_driver = 1;

    // disable MOSI pull up
    // REG_CLR_BIT(GPIO_PIN_MUX_REG[PIN_SWDIO_MOSI], FUN_PU);
}

/**
 * @brief Gain control of SPI
 *
 */
__FORCEINLINE void DAP_SPI_Acquire()
{
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[PIN_SWCLK], FUNC_SPICLK_SPICLK);
}

/**
 * @brief Release control of SPI
 *
 */
__FORCEINLINE void DAP_SPI_Release()
{
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[PIN_SWCLK], PIN_FUNC_GPIO);
    GPIO.enable_w1ts = (0x01 << PIN_SWCLK);
}

/**
 * @brief Use SPI acclerate
 *
 */
void DAP_SPI_Enable()
{
    // may be unuse
    //// FIXME: esp32 nop?
    // PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_HSPID_MOSI); // GPIO13 is SPI MOSI pin (Master Data Out)
}

/**
 * @brief Disable SPI
 * Drive capability not yet known
 */
__FORCEINLINE void DAP_SPI_Disable()
{
    ;
    // CLEAR_PERI_REG_MASK(PERIPHS_IO_MUX_MTCK_U, (PERIPHS_IO_MUX_FUNC << PERIPHS_IO_MUX_FUNC_S));
    //  may be unuse
    //  gpio_pin_reg_t pin_reg;
    //  GPIO.enable_w1ts |= (0x1 << PIN_SWDIO_MOSI);
    //  GPIO.pin[PIN_SWDIO_MOSI].driver = 0; // OD Output
    //  pin_reg.val = READ_PERI_REG(GPIO_PIN_REG(PIN_SWDIO_MOSI));
    //  pin_reg.pullup = 1;
    //  WRITE_PERI_REG(GPIO_PIN_REG(PIN_SWDIO_MOSI), pin_reg.val);
}
