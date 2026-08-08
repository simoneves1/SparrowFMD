// A khp_transport backed by an ESP-IDF UART peripheral.
//
// *** UNVERIFIED AGAINST REAL HARDWARE ***
// Every other file in this component has been either unit-tested on the
// host or (for ESP-IDF-specific code) at minimum cross-compiled -- for
// both esp32p4 (main-esp's actual target) and esp32s3 (a portability
// check, see main-esp/README.md's "designed to not lock into one chip"
// note). This file cross-compiles for both too, but "compiles" is the
// only thing that's actually been checked -- nothing here has run
// against a real UART peripheral or a real device on the other end.
// Treat it as a skeleton to build on and test against real hardware,
// not as verified working code.
//
// Also note: this is a plain UART transport. main-esp/README.md and
// main-esp/src/README.md describe the klipper-host-protocol module
// talking to third-party boards (Octopus, toolhead boards) *over USB*,
// which on real Klipper MCU boards means USB CDC-ACM, not a UART
// peripheral. This file does NOT implement that -- USB Host CDC-ACM
// (ESP-IDF's usb_host + cdc_acm_host components) is a meaningfully
// larger, more complex piece of work (device enumeration, hotplug,
// class driver setup) that's a poor candidate for writing blind without
// hardware to validate against. This UART transport exists to (a) prove
// out the khp_transport interface against a real ESP-IDF peripheral API
// end-to-end, and (b) serve the touch-ui/ams-esp `uart-links` module,
// which genuinely is plain UART. The USB CDC-ACM transport for talking
// to third-party boards is still a separate, unstarted piece of work.
#ifndef KHP_UART_TRANSPORT_H
#define KHP_UART_TRANSPORT_H

#include <stdbool.h>
#include "driver/uart.h"
#include "khp_transport.h"

struct khp_uart_transport_config {
    uart_port_t port;
    int tx_pin;
    int rx_pin;
    int baud_rate;
};

// Configures and installs the UART driver for cfg->port and fills *out
// with a khp_transport backed by it. Returns false if any ESP-IDF UART
// driver call fails. The underlying UART driver is never torn down by
// this component (matches main-esp's "flash once" runtime-config model
// -- a transport is expected to live for the process's lifetime).
bool khp_uart_transport_init(struct khp_transport *out
                             , const struct khp_uart_transport_config *cfg);

#endif // khp_uart_transport.h
