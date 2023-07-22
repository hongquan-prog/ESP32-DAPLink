/**
 * @file gpio_op.h
 * @author windowsair
 * @brief esp32 GPIO operation
 * @version 0.1
 * @date 2021-03-03
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef __GPIO_OP_H__
#define __GPIO_OP_H__

#include "cmsis_compiler.h"

// soc register
#include "esp32s3/include/soc/io_mux_reg.h"
#include "esp32s3/include/soc/gpio_struct.h"
#include "include/soc/gpio_periph.h"
#include "hal/gpio_types.h"

__STATIC_INLINE void GPIO_FUNCTION_SET(int io_num)
{
    // function number 2 is GPIO_FUNC for each pin
    // Note that the index starts at 0, so we are using function 3.
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[io_num], PIN_FUNC_GPIO);
}

static void GPIO_SET_DIRECTION_NORMAL_OUT(int io_num)
{
    GPIO.enable_w1ts = (0x1 << io_num);
    // PP out
    GPIO.pin[io_num].pad_driver = 0;
}

__STATIC_INLINE void GPIO_PULL_UP_ONLY_SET(int io_num)
{
    // disable pull down
    REG_CLR_BIT(GPIO_PIN_MUX_REG[io_num], FUN_PD);
    // enable pull up
    REG_SET_BIT(GPIO_PIN_MUX_REG[io_num], FUN_PU);
}




// static void GPIO_SET_DIRECTION_NORMAL_IN(int io_num)
// {
//   GPIO.enable_w1tc |= (0x1 << io_num);
// }

#endif