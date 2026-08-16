# Hardware testing checklist

Everything in this repo currently marked "unverified against real
hardware" (or "cross-compiles, nothing more"), collected in one place so
there's a single list to work through as hardware becomes available,
rather than hunting through each module's own README/header comments.
Grouped by what needs to physically exist before that item is testable
at all. Update this file (check items off, move them, or delete them
once genuinely done) rather than letting it go stale -- it's a working
list, not a permanent record.

## Already have: touch-ui board (4.0inch ESP32-32E "yellow display" clone)
Done, not re-listed here in detail -- see `touch-ui/README.md`'s Status
section for the full story (display orientation, LVGL color byte-swap,
touch calibration with axis-swap detection, all found and fixed against
real hardware). If a *different physical unit* of this same board model
ever gets used, its touch panel will need its own calibration re-run --
see `touch-ui/main/main.c`'s `TOUCH_UI_RUN_TOUCH_CALIBRATION` flag and
`run_touch_calibration()`'s comment.

Still open on this board specifically:
- [ ] `touch-ui`'s Files screen still only shows static demo data.
      `main-esp` now has a real SD listing to ask for
      (`storage_sd_list_gcode_files`, wired into `GET /api/files`, see
      root README) -- but touch-ui talks to main-esp over the UART link
      in "Once main-esp *and* touch-ui can both be powered and wired
      together" below, which doesn't exist yet either, so there's
      nothing to ask it over yet. Revisit once that link exists.
      Macros' "Bed mesh" button is intentionally a no-op (no
      corresponding command exists) -- separate main-esp-side work.

## Once the P4 board is purchased (`main-esp`'s actual target)
Nothing in `main-esp` has run on real hardware at all yet -- everything
below cross-compiles for esp32p4 (and esp32s3 as a portability check)
but that's all that's been confirmed.

- [ ] Basic bring-up: does it boot, do the self-tests in
      `main-esp/main/main.c`'s `app_main()` all report PASS over serial
      (klipper-host-protocol, gcode-parser, motion-planner,
      shared-protocol, safety, storage, web-ui)
- [ ] `kinematics`/`motion-planner`'s real open question: does the
      float32 chelper port actually run fast enough on the P4's hardware
      single-precision FPU to keep up with real G-code at real print
      speeds. This is the one thing in the whole project that
      specifically *needs* real P4 silicon to answer honestly -- the
      available QEMU only supports the esp32c3 machine, which has no
      hardware FPU at all, so it can't stand in for this. See root
      README's Status section for the full history (a first float32
      attempt hit a real correctness bug, fixed with an adaptive
      epsilon; the speed question was deliberately left for real
      hardware rather than an estimate).
- [ ] `storage_sd` (SD card mount, SDMMC+FATFS) -- needs a real SD card
      and the card slot wired per `main-esp/PINOUT.md`. `main.c` already
      attempts `storage_sd_mount("/sdcard")` on every boot and feeds the
      result into `GET /api/files` via `storage_sd_list_gcode_files()`
      (host-tested, pure POSIX opendir/readdir/stat, see
      `main-esp/components/storage/include/storage_sd.h`), so this item
      is specifically about confirming the mount itself succeeds on real
      hardware and the listing then shows real files placed on the card.
- [ ] `khp_uart_transport` (klipper-host-protocol's UART transport) --
      needs the second UART peripheral pin choice finalized first, see
      "touch-ui <-> main-esp UART link" below (same open pin question)
- [ ] A network driver (WiFi or Ethernet) -- doesn't exist at all yet,
      not just unverified. The P4 has no native radio; this needs a
      hardware/module decision (external WiFi coprocessor vs. Ethernet
      PHY) before any code. Until this exists, `web_server` starts and
      binds on boot but is unreachable from anywhere.
- [ ] `web_server` (web-ui's HTTP+WebSocket server) -- once a network
      driver exists, load the page from a real browser on a real device
      and confirm the WebSocket connects, `command_ack` toasts appear,
      `/api/files`/`/api/history`/`/api/diagnostics`/`/api/settings`/
      `/api/camera` all respond, and the Console/Tune panels round-trip
      real commands. Was only ever tested against a hand-rolled local
      mock server (not ESP-IDF) driving the real page -- confirms the
      frontend/API contract works together, not a substitute for this.

## Once a real Klipper MCU board is available (Octopus, toolhead board, etc.)
- [ ] `khp_usb_cdc_transport` -- no real USB device has ever been
      enumerated against it. Needs a real board connected over USB and,
      critically, a real identify handshake: the actual `queue_step`
      message id/format isn't something this repo can hardcode ahead of
      time, Klipper's wire protocol is dictionary-driven (the MCU's
      compiled dictionary defines its own message formats).
- [ ] Confirm the identify/dictionary-decompression path
      (`khp_dictionary`, `khp_msgtable`) against a real MCU's actual
      compiled dictionary, not just synthetic test data.
- [ ] Point `step-encoder` at a real MCU's actual dictionary instead of
      its current mock one, and confirm the real board accepts and
      executes the resulting `queue_step`/`set_next_step_dir` messages.
      `motion-planner`'s step events -> `klipper-host-protocol` message
      bytes pipeline itself already exists and is host-tested (22
      checks, mock dictionary using Klipper's real format strings) --
      see `main-esp/components/step-encoder/README.md` -- this item is
      specifically about swapping the mock dictionary for a real one and
      real per-axis `oid`/MCU clock frequency values, plus whatever the
      real board reveals that the mock didn't catch (e.g. real
      `queue_step` throughput without run-length compression, which
      `step-encoder` doesn't implement yet).

## Once main-esp *and* touch-ui can both be powered and wired together
- [ ] The touch-ui <-> main-esp UART link itself. Blocked on a pin
      choice: touch-ui's board already has its UART0 claimed by the
      onboard USB-serial bridge for flashing/logging, so the real link
      needs a second UART peripheral on different pins -- still an open
      question, see `touch-ui/PINOUT.md`.
- [ ] `slp_uart_transport` (shared-protocol's concrete UART transport,
      used by both sides of that link) -- cross-compiles on both ends,
      never run against a real connection.
- [ ] Once the link exists: does touch-ui's Status screen show real
      temps/progress from main-esp instead of the canned demo push, and
      do touch-ui's button presses (jog, macros, pause/stop) actually
      reach and do something on main-esp instead of just being logged.

## Not yet relevant
`ams-esp` (filament switcher) is still just design notes, no code to
test yet -- explicitly post-v1, see root README.
