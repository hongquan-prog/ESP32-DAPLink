#pragma once

#include "prog_data.h"

void programmer_init(void);
prog_err_def programmer_request_handle(char *buf, int len);
void programmer_get_status(char *buf, int size, int &encode_len);
prog_err_def programmer_write_data(uint8_t *data, int len);
