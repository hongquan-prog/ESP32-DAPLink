#pragma once

#include <cstdio>

#define LOG_COLOR_RED "\x1b[31m"
#define LOG_COLOR_GREEN "\x1b[32m"
#define LOG_COLOR_YELLOW "\x1b[33m"
#define LOG_COLOR_BLUE "\x1b[34m"
#define LOG_COLOR_MAGENTA "\x1b[35m"
#define LOG_COLOR_CYAN "\x1b[36m"
#define LOG_COLOR_RESET "\x1b[0m"

#define LOG_INFO(format, ...) printf(LOG_COLOR_GREEN "I %s: " format LOG_COLOR_RESET "\n", TAG, ##__VA_ARGS__)
#define LOG_WARN(format, ...) printf(LOG_COLOR_YELLOW "W %s: " format LOG_COLOR_RESET "\n", TAG, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) printf(LOG_COLOR_RED "E %s: " format LOG_COLOR_RESET "\n", TAG, ##__VA_ARGS__)
