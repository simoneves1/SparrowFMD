# shared

Code/definitions used by more than one firmware target, to avoid the
UART message format between main-esp and touch-ui/ams-esp drifting out
of sync across separately-built firmware.

## Contents
`shared-protocol/` (an ESP-IDF component, see its own source for
details) implements this: frame envelope + version tagging
(`slp_frame`), status_update/control_command message types
(`slp_messages`), a transport-agnostic receive/dispatch/send layer
(`slp_session`, `slp_transport`), and a concrete ESP-IDF UART transport
(`slp_uart_transport`, unverified against real hardware). Both main-esp
and touch-ui already depend on it via `EXTRA_COMPONENT_DIRS` pointing
here from their own `CMakeLists.txt`.
