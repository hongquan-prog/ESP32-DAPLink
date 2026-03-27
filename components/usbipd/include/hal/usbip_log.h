/*
 * USBIP Log System
 *
 * Reference Zephyr logging design:
 * - Unified logging macros: LOG_ERR, LOG_WRN, LOG_INF, LOG_DBG
 * - Define module TAG and log level via LOG_MODULE_REGISTER
 * - Support timestamp output
 */

/*
 * Copyright (c) 2026-2026, hongquan.li
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-3-24      hongquan.li   add license declaration
 */

#ifndef USBIP_LOG_H
#define USBIP_LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Log color definitions (ANSI Escape Codes)
 *****************************************************************************/

#define LOG_COLOR_NONE "\033[0m"    /* Default/Reset */
#define LOG_COLOR_RED "\033[31m"    /* Error */
#define LOG_COLOR_YELLOW "\033[33m" /* Warning */
#define LOG_COLOR_GREEN "\033[32m"  /* Info */
#define LOG_COLOR_WHITE "\033[37m"  /* Debug default */

/* Color enable control */
#ifndef LOG_USE_COLOR
#    define LOG_USE_COLOR 1
#endif

/*****************************************************************************
 * Log level definitions
 *****************************************************************************/

#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERR 1
#define LOG_LEVEL_WRN 2
#define LOG_LEVEL_INF 3
#define LOG_LEVEL_DBG 4

/* Default global log level */
#ifndef LOG_GLOBAL_LEVEL
#    define LOG_GLOBAL_LEVEL LOG_LEVEL_INF
#endif

/*****************************************************************************
 * Module Registration Macro
 *
 * Use at the beginning of source file:
 *   LOG_MODULE_REGISTER(my_module, LOG_LEVEL_DBG);
 *
 * This defines the current module's TAG and log level
 *****************************************************************************/

#define LOG_MODULE_REGISTER(name, level)                                                           \
    static const char* const _log_module_name = #name;                                             \
    static const int _log_module_level = level

/* Default module registration (used if not registered) */
#define LOG_MODULE_DEFAULT()                                                                       \
    static const char* const _log_module_name = "default";                                         \
    static const int _log_module_level = LOG_GLOBAL_LEVEL

/* Current module's TAG and level (used by log macros) */
#define LOG_MODULE_NAME _log_module_name
#define LOG_MODULE_LEVEL _log_module_level

/*****************************************************************************
 * Log output function (inline implementation)
 *****************************************************************************/

static inline void usbip_log_printf(int level, const char* tag, const unsigned char* data,
                                    unsigned int len, const char* fmt, ...)
{
    va_list args;
    time_t now;
    struct tm* tm_info;
    struct timespec ts;
    char time_buf[16];
    const char* level_str;
    const char* level_color;
    FILE* fp;

    /* Get time */
    clock_gettime(CLOCK_REALTIME, &ts);
    now = ts.tv_sec;
    tm_info = localtime(&now);

    /* Format timestamp: HH:MM:SS.mmm */
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

    /* Level string and color */
    switch (level)
    {
        case LOG_LEVEL_ERR:
            level_str = "ERR";
            level_color = LOG_COLOR_RED;
            fp = stderr;
            break;
        case LOG_LEVEL_WRN:
            level_str = "WRN";
            level_color = LOG_COLOR_YELLOW;
            fp = stderr;
            break;
        case LOG_LEVEL_INF:
            level_str = "INF";
            level_color = LOG_COLOR_GREEN;
            fp = stdout;
            break;
        case LOG_LEVEL_DBG:
            level_str = "DBG";
            level_color = LOG_COLOR_NONE;
            fp = stdout;
            break;
        default:
            level_str = "???";
            level_color = LOG_COLOR_NONE;
            fp = stdout;
            break;
    }

    /* Output format: [color]HH:MM:SS.mmm [L] [tag] message */
    if (LOG_USE_COLOR)
    {
        fprintf(fp, "%s%s.%03ld %s [%s] ", level_color, time_buf, ts.tv_nsec / 1000000, tag,
                level_str);
    }
    else
    {
        fprintf(fp, "%s.%03ld %s [%s] ", time_buf, ts.tv_nsec / 1000000, tag, level_str);
    }

    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);

    /* Output hex data (if any) */
    if (data && len > 0)
    {
        for (unsigned int i = 0; i < len; i++)
        {
            if ((i % 16) == 0)
            {
                fprintf(fp, "\n");
            }

            fprintf(fp, "%02X ", data[i]);
        }
    }

    fprintf(fp, "\n");
    if (LOG_USE_COLOR && level != LOG_LEVEL_DBG)
    {
        fprintf(fp, LOG_COLOR_NONE);
    }
    fflush(fp);
}

/*****************************************************************************
 * Log macros
 *
 * Usage:
 *   LOG_MODULE_REGISTER(my_module, LOG_LEVEL_DBG);
 *   LOG_ERR("Error occurred: %d", err);
 *   LOG_DBG("Debug info: %s", str);
 *****************************************************************************/

#define LOG_ERR(fmt, ...)                                                                          \
    do                                                                                             \
    {                                                                                              \
        if (LOG_MODULE_LEVEL >= LOG_LEVEL_ERR)                                                     \
            usbip_log_printf(LOG_LEVEL_ERR, LOG_MODULE_NAME, NULL, 0, fmt, ##__VA_ARGS__);         \
    } while (0)

#define LOG_WRN(fmt, ...)                                                                          \
    do                                                                                             \
    {                                                                                              \
        if (LOG_MODULE_LEVEL >= LOG_LEVEL_WRN)                                                     \
            usbip_log_printf(LOG_LEVEL_WRN, LOG_MODULE_NAME, NULL, 0, fmt, ##__VA_ARGS__);         \
    } while (0)

#define LOG_INF(fmt, ...)                                                                          \
    do                                                                                             \
    {                                                                                              \
        if (LOG_MODULE_LEVEL >= LOG_LEVEL_INF)                                                     \
            usbip_log_printf(LOG_LEVEL_INF, LOG_MODULE_NAME, NULL, 0, fmt, ##__VA_ARGS__);         \
    } while (0)

#define LOG_DBG(fmt, ...)                                                                          \
    do                                                                                             \
    {                                                                                              \
        if (LOG_MODULE_LEVEL >= LOG_LEVEL_DBG)                                                     \
            usbip_log_printf(LOG_LEVEL_DBG, LOG_MODULE_NAME, NULL, 0, fmt, ##__VA_ARGS__);         \
    } while (0)

/*****************************************************************************
 * Raw log macros (for special cases, specify TAG directly)
 *****************************************************************************/

#define LOG_RAW_ERR(tag, fmt, ...) usbip_log_printf(LOG_LEVEL_ERR, tag, NULL, 0, fmt, ##__VA_ARGS__)

#define LOG_RAW_WRN(tag, fmt, ...) usbip_log_printf(LOG_LEVEL_WRN, tag, NULL, 0, fmt, ##__VA_ARGS__)

#define LOG_RAW_INF(tag, fmt, ...) usbip_log_printf(LOG_LEVEL_INF, tag, NULL, 0, fmt, ##__VA_ARGS__)

#define LOG_RAW_DBG(tag, fmt, ...) usbip_log_printf(LOG_LEVEL_DBG, tag, NULL, 0, fmt, ##__VA_ARGS__)

/*****************************************************************************
 * Hex dump log macros
 *
 * Usage:
 *   LOG_HEX_DBG("Data: ", data, len);
 *****************************************************************************/

#define LOG_HEX_ERR(fmt, data, len, ...)                                                           \
    do                                                                                             \
    {                                                                                              \
        if (LOG_MODULE_LEVEL >= LOG_LEVEL_ERR)                                                     \
            usbip_log_printf(LOG_LEVEL_ERR, LOG_MODULE_NAME, (const unsigned char*)data, len, fmt, \
                             ##__VA_ARGS__);                                                       \
    } while (0)

#define LOG_HEX_WRN(fmt, data, len, ...)                                                           \
    do                                                                                             \
    {                                                                                              \
        if (LOG_MODULE_LEVEL >= LOG_LEVEL_WRN)                                                     \
            usbip_log_printf(LOG_LEVEL_WRN, LOG_MODULE_NAME, (const unsigned char*)data, len, fmt, \
                             ##__VA_ARGS__);                                                       \
    } while (0)

#define LOG_HEX_INF(fmt, data, len, ...)                                                           \
    do                                                                                             \
    {                                                                                              \
        if (LOG_MODULE_LEVEL >= LOG_LEVEL_INF)                                                     \
            usbip_log_printf(LOG_LEVEL_INF, LOG_MODULE_NAME, (const unsigned char*)data, len, fmt, \
                             ##__VA_ARGS__);                                                       \
    } while (0)

#define LOG_HEX_DBG(fmt, data, len, ...)                                                           \
    do                                                                                             \
    {                                                                                              \
        if (LOG_MODULE_LEVEL >= LOG_LEVEL_DBG)                                                     \
            usbip_log_printf(LOG_LEVEL_DBG, LOG_MODULE_NAME, (const unsigned char*)data, len, fmt, \
                             ##__VA_ARGS__);                                                       \
    } while (0)

#define LOG_RAW_HEX_ERR(tag, fmt, data, len, ...)                                                  \
    usbip_log_printf(LOG_LEVEL_ERR, tag, (const unsigned char*)data, len, fmt, ##__VA_ARGS__)

#define LOG_RAW_HEX_WRN(tag, fmt, data, len, ...)                                                  \
    usbip_log_printf(LOG_LEVEL_WRN, tag, (const unsigned char*)data, len, fmt, ##__VA_ARGS__)

#define LOG_RAW_HEX_INF(tag, fmt, data, len, ...)                                                  \
    usbip_log_printf(LOG_LEVEL_INF, tag, (const unsigned char*)data, len, fmt, ##__VA_ARGS__)

#define LOG_RAW_HEX_DBG(tag, fmt, data, len, ...)                                                  \
    usbip_log_printf(LOG_LEVEL_DBG, tag, (const unsigned char*)data, len, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* USBIP_LOG_H */