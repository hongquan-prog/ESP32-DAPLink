#include "components/elaphureLink/elaphureLink_protocol.h"


#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

extern int kSock;
extern int usbip_network_send(int s, const void *dataptr, size_t size, int flags);

extern void malloc_dap_ringbuf();
extern void free_dap_ringbuf();

extern uint32_t DAP_ExecuteCommand(const uint8_t *request, uint8_t *response);

uint8_t* el_process_buffer = NULL;

void el_process_buffer_malloc() {
    if (el_process_buffer != NULL)
        return;

    free_dap_ringbuf();

    el_process_buffer = malloc(1500);
}


void el_process_buffer_free() {
    if (el_process_buffer != NULL) {
        free(el_process_buffer);
        el_process_buffer = NULL;
    }
}


int el_handshake_process(int fd, void *buffer, size_t len) {
    if (len != sizeof(el_request_handshake)) {
        return -1;
    }

    el_request_handshake* req = (el_request_handshake*)buffer;

    if (ntohl(req->el_link_identifier) != EL_LINK_IDENTIFIER) {
        return -1;
    }

    if (ntohl(req->command) != EL_COMMAND_HANDSHAKE) {
        return -1;
    }

    el_response_handshake res;
    res.el_link_identifier = htonl(EL_LINK_IDENTIFIER);
    res.command = htonl(EL_COMMAND_HANDSHAKE);
    res.el_dap_version = htonl(EL_DAP_VERSION);

    usbip_network_send(fd, &res, sizeof(el_response_handshake), 0);

    return 0;
}


void el_dap_data_process(void* buffer, size_t len) {
    int res = DAP_ExecuteCommand(buffer, (uint8_t *)el_process_buffer);
    res &= 0xFFFF;

    usbip_network_send(kSock, el_process_buffer, res, 0);
}
