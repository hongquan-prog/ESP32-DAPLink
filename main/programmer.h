#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void programmer_init();
bool programmer_put_cmd(char *msg, int len);

#ifdef __cplusplus
}
#endif
