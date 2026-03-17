/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   Initial version
 * 2026-3-16     Refactor     Improved logging macros
 */
#pragma once

#include <cstdio>

/**
 * @file log.h
 * @brief Logging macros for Program module
 * 
 * Provides colored console output for INFO, WARN, and ERROR level logs.
 * Each log message includes the TAG identifier for the source module.
 */

// ANSI color codes for console output
#define LOG_COLOR_RED       "\x1b[31m"
#define LOG_COLOR_GREEN     "\x1b[32m"
#define LOG_COLOR_YELLOW    "\x1b[33m"
#define LOG_COLOR_BLUE      "\x1b[34m"
#define LOG_COLOR_MAGENTA   "\x1b[35m"
#define LOG_COLOR_CYAN      "\x1b[36m"
#define LOG_COLOR_RESET     "\x1b[0m"

/**
 * @brief Log INFO level message (green)
 * @param format printf-style format string
 * @param ... Variable arguments
 */
#define LOG_INFO(format, ...)  printf(LOG_COLOR_GREEN "I %s: " format LOG_COLOR_RESET "\n", TAG, ##__VA_ARGS__)

/**
 * @brief Log WARN level message (yellow)
 * @param format printf-style format string
 * @param ... Variable arguments
 */
#define LOG_WARN(format, ...) printf(LOG_COLOR_YELLOW "W %s: " format LOG_COLOR_RESET "\n", TAG, ##__VA_ARGS__)

/**
 * @brief Log ERROR level message (red)
 * @param format printf-style format string
 * @param ... Variable arguments
 */
#define LOG_ERROR(format, ...) printf(LOG_COLOR_RED "E %s: " format LOG_COLOR_RESET "\n", TAG, ##__VA_ARGS__)
