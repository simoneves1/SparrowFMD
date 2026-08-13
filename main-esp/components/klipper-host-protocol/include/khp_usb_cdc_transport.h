// A khp_transport backed by ESP-IDF's USB Host CDC-ACM class driver, for
// talking to third-party Klipper MCU boards (Octopus, toolhead boards)
// over real USB. This is the transport main-esp/README.md and
// main-esp/src/README.md describe as still missing -- khp_uart_transport
// is a plain UART peripheral and serves a different link entirely (see
// that header's doc comment for why the two aren't interchangeable).
//
// *** UNVERIFIED AGAINST REAL HARDWARE ***
// Same caveat as khp_uart_transport.h: this cross-compiles for esp32p4
// and esp32s3 and follows Espressif's own usb_host_cdc_acm class driver
// API (github.com/espressif/esp-usb), but nothing here has enumerated a
// real USB device, sent a real byte, or been checked against a real
// Klipper MCU board's CDC-ACM endpoint. Treat it as a skeleton to build
// on and test against real hardware, not as verified working code.
//
// Depends on the managed component espressif/usb_host_cdc_acm (see this
// component's idf_component.yml) layered on top of ESP-IDF's built-in
// `usb` (USB Host Library) component.
#ifndef KHP_USB_CDC_TRANSPORT_H
#define KHP_USB_CDC_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "khp_transport.h"

struct khp_usb_cdc_transport_config {
    // CDC_HOST_ANY_VID/CDC_HOST_ANY_PID (0/0, from usb/cdc_acm_host.h)
    // to open the first CDC-ACM device found -- fine for a single
    // point-to-point USB link with no hub, which is the expected
    // topology here (one Main ESP USB host port per third-party board).
    uint16_t vid;
    uint16_t pid;
    uint8_t interface_idx;
    uint32_t connection_timeout_ms;

    // Size of the internal byte-stream buffer that bridges the class
    // driver's async data-received callback to khp_transport's
    // synchronous, timeout-based read(). Must be large enough to
    // absorb bursts between successive khp_session_poll calls.
    size_t rx_buffer_size;
};

// Installs the USB Host Library (if not already installed by this
// process) and the CDC-ACM class driver, spawns the background task
// that pumps USB host events, opens the first device matching
// cfg->vid/cfg->pid, and fills *out with a khp_transport backed by it.
// Returns false if any step fails, including no matching device being
// present within cfg->connection_timeout_ms.
//
// Like khp_uart_transport_init, nothing installed here is ever torn
// down -- matches main-esp's "flash once" runtime-config model where a
// transport is expected to live for the process's lifetime. Only one
// USB CDC-ACM link is supported per process (a single static context
// backs every khp_usb_cdc_transport_init call's out->ctx).
bool khp_usb_cdc_transport_init(struct khp_transport *out
                                , const struct khp_usb_cdc_transport_config *cfg);

#endif // khp_usb_cdc_transport.h
