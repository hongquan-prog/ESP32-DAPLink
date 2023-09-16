#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void programmer_init(void);
bool programmer_put_cmd(char *msg, int len);
int programmer_get_progress(void);
bool programmer_is_busy();

#ifdef __cplusplus
}
#endif
