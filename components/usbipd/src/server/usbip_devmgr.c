/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-24      hongquan.li   add license declaration
 */

/*****************************************************************************
 * USBIP Device Manager
 *
 * Device management at USBIP protocol layer: driver registration, device import/export state
 *****************************************************************************/

#include "usbip_devmgr.h"
#include <stdio.h>
#include <string.h>
#include "hal/usbip_log.h"

LOG_MODULE_REGISTER(devmgr, CONFIG_USBIP_LOG_LEVEL);

/*****************************************************************************
 * Driver Registry
 *****************************************************************************/

#define MAX_DRIVERS 16

static struct usbip_device_driver* driver_registry[MAX_DRIVERS];
static int driver_count = 0;

/*****************************************************************************
 * Device busy status table (USBIP import/export protocol)
 *****************************************************************************/

#define MAX_BUSY_DEVICES 32

static struct
{
    char busid[SYSFS_BUS_ID_SIZE];
    int busy;
} busy_devices[MAX_BUSY_DEVICES];

/*****************************************************************************
 * Static Function Declarations
 *****************************************************************************/
/* No static functions in this module, all functions are public interfaces */

/*****************************************************************************
 * Driver Registration Interface
 *****************************************************************************/

int usbip_register_driver(struct usbip_device_driver* driver)
{
    if (driver_count >= MAX_DRIVERS)
    {
        LOG_ERR("Driver registry full");
        return -1;
    }

    /* Check if already registered */
    for (int i = 0; i < driver_count; i++)
    {
        if (driver_registry[i] == driver)
        {
            LOG_WRN("Driver already registered: %s", driver->name);
            return -1;
        }
    }

    driver_registry[driver_count++] = driver;

    /* Call driver initialization */
    if (driver->init && driver->init(driver) < 0)
    {
        LOG_ERR("Driver init failed: %s", driver->name);
        driver_count--;
        return -1;
    }

    LOG_DBG("Registered driver: %s", driver->name);

    return 0;
}

void usbip_unregister_driver(struct usbip_device_driver* driver)
{
    for (int i = 0; i < driver_count; i++)
    {
        if (driver_registry[i] == driver)
        {
            /* Call driver cleanup */
            if (driver->cleanup)
            {
                driver->cleanup(driver);
            }
            /* Move subsequent drivers */
            for (int j = i; j < driver_count - 1; j++)
            {
                driver_registry[j] = driver_registry[j + 1];
            }
            driver_count--;
            LOG_DBG("Unregistered driver: %s", driver->name);
            return;
        }
    }
}

/*****************************************************************************
 * Driver Iteration Interface
 *****************************************************************************/

struct usbip_device_driver* usbip_get_first_driver(void)
{
    if (driver_count == 0)
    {
        return NULL;
    }
    return driver_registry[0];
}

struct usbip_device_driver* usbip_get_next_driver(struct usbip_device_driver* current)
{
    for (int i = 0; i < driver_count - 1; i++)
    {
        if (driver_registry[i] == current)
        {
            return driver_registry[i + 1];
        }
    }
    return NULL;
}

/*****************************************************************************
 * Device Busy Status Management
 *****************************************************************************/

void usbip_set_device_busy(const char* busid)
{
    /* Find existing entry */
    for (int i = 0; i < MAX_BUSY_DEVICES; i++)
    {
        if (strcmp(busy_devices[i].busid, busid) == 0)
        {
            busy_devices[i].busy = 1;
            return;
        }
    }
    /* Create new entry */
    for (int i = 0; i < MAX_BUSY_DEVICES; i++)
    {
        if (busy_devices[i].busid[0] == '\0')
        {
            strncpy(busy_devices[i].busid, busid, SYSFS_BUS_ID_SIZE - 1);
            busy_devices[i].busy = 1;
            return;
        }
    }
}

void usbip_set_device_available(const char* busid)
{
    for (int i = 0; i < MAX_BUSY_DEVICES; i++)
    {
        if (strcmp(busy_devices[i].busid, busid) == 0)
        {
            busy_devices[i].busy = 0;
            return;
        }
    }
}

int usbip_is_device_busy(const char* busid)
{
    for (int i = 0; i < MAX_BUSY_DEVICES; i++)
    {
        if (strcmp(busy_devices[i].busid, busid) == 0)
        {
            return busy_devices[i].busy;
        }
    }
    return 0;
}