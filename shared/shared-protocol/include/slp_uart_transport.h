// An slp_transport backed by an ESP-IDF UART peripheral -- this is the
// real transport for shared-protocol, unlike klipper-host-protocol's
// khp_uart_transport (which serves touch-ui/ams-esp's uart-links case
// but isn't the actual Octopus/toolhead-board transport, which needs
// USB CDC-ACM instead). shared-protocol's whole point is main-esp<->
// touch-ui/ams-esp over UART, so this one *is* the real thing, not a
// stand-in.
//
// *** UNVERIFIED AGAINST REAL HARDWARE ***
// Same caveat as every other ESP-IDF-glue file in this project so far:
// cross-compiles (for esp32p4, esp32s3, and whatever touch-ui/ams-esp
// end up targeting), and that's the only thing that's actually been
// checked. Nothing here has run against a real UART peripheral or a
// real device on the other end.
#ifndef SLP_UART_TRANSPORT_H
#define SLP_UART_TRANSPORT_H

#include <stdbool.h>
#include "driver/uart.h"
#include "slp_transport.h"

struct slp_uart_transport_config {
    uart_port_t port;
    int tx_pin;
    int rx_pin;
    int baud_rate;
};

// Configures and installs the UART driver for cfg->port and fills *out
// with an slp_transport backed by it. Returns false if any ESP-IDF UART
// driver call fails. Like khp_uart_transport, the underlying driver is
// never torn down (matches main-esp's "flash once" runtime-config model).
bool slp_uart_transport_init(struct slp_transport *out
                             , const struct slp_uart_transport_config *cfg);

#endif // slp_uart_transport.h
