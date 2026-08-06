# frontend

The config wizard + browser-based flashing UI. Runs entirely client-side
except for calls to the backend build service.

## Flow
1. Board selection (`src/pages`) — pick Main ESP variant, mainboard,
   toolhead board, touch UI, optional AMS.
2. Connection selection — pick transport per link (USB/UART/CAN), mirroring
   Klipper's own `make menuconfig` communication-protocol step.
3. Review/generate — sends selections to the backend, gets back:
   - a printer config file for the Main ESP (runtime config, no compile needed)
   - compiled `.bin` firmware for any third-party MCU boards that need one
4. Flash — branches per board type, see `src/pages` notes below.

No user account / no server-side storage of printer configs planned for v1 —
config is generated and downloaded/flashed in one session. Revisit if the
farm-management side ever wants to remember configs across sessions.
