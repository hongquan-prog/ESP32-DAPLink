#include "prog_idle.h"

#define TAG "prog_idle"

void ProgIdle::request_handle(ProgData &obj)
{
    int ret = PROG_ERR_NONE;
    prog_req_t &request = obj.get_request();
    prog_request_swap_t *swap = reinterpret_cast<prog_request_swap_t *>(obj.get_swap());

    /* Parses the request and checks the legitimacy of the parameters */
    ret = obj.request_decode(request, swap->data, swap->len);
    /* Pass the result to the http thread via swap */
    obj.set_swap(reinterpret_cast<void *>(ret));

    /* If parsing is successful, it will switch to the corresponding mode */
    if (ret == PROG_ERR_NONE)
    {
        Prog::switch_mode(request.mode);

        /* Programming is triggered by sending the PROG_EVT_PROGRAM_START event. */
        obj.send_event(PROG_EVT_PROGRAM_START);
        obj.set_progress(0);
        obj.set_busy_state(true);
    }

    /* After a successful mode switch, send a synchronisation signal to keep the http server running */
    obj.send_sync();
}

const char *ProgIdle::name()
{
    return TAG;
};