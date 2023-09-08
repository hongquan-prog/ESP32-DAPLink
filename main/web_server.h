/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-9-8      lihongquan   add license declaration
 */
#pragma once

#include <stdbool.h>
#include "esp_http_server.h"

typedef enum
{
    WEB_URI_ROOT,
    WEB_URI_FAVICON,
    WEB_URI_WEBSERIAL,
    WEB_URI_UPLOAD,
    WEB_URI_DOWNLOAD,
    WEB_URI_DELETE,
    WEB_URI_BURN_LOCAL_PROGEAM,
    WEB_URI_BURN_ONLINE_PROGEAM,
    WEB_URI_UPGRADE_DEBUGGER,
    WEB_URI_UPDATA_WEB_RESOURCE,
    WEB_URI_NUM
} web_uri_def;

#ifdef __cplusplus
extern "C"
{
#endif

    bool web_server_init(httpd_handle_t *server);
    bool web_server_stop(httpd_handle_t *server);

#ifdef __cplusplus
}
#endif