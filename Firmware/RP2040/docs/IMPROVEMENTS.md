# Firmware Improvements

Improvements and fixes applied to the OGX-Mini RP2040 firmware in this project.

**Version:** From **v1.0.0a3** the version was bumped to **v1.0.0a4** to reflect Wii U controller fixes, Gamecube USB mode, PS3 driver fixes, latency improvements, and Xbox 360 (XInput) support (see below). **v1.0.0.8a+** documents **Pico W / Pico 2 W** work on **DualShock 4 (Classic Bluetooth)** vs **BLE advertising**, **BR inquiry**, and related BT stability (see *Pico W / Pico 2 W — DualShock 4 and Classic Bluetooth* below).

---

## Xbox 360 (XInput) support

**Goal:** Use the adapter in XInput mode on Xbox 360 with Bluetooth controllers (e.g. PS5, Xbox One). The 360 requires XSM3 authentication and specific USB descriptors.

**References:** [joypad-os](https://github.com/joypad-ai/joypad-os) XInput implementation; [libxsm3](https://github.com/InvoxiPlayGames/libxsm3).

### Changes

1. **Descriptors**
   - Device and configuration descriptors aligned with joypad-os (153-byte config, 4 interfaces, bConfigurationValue 1, bMaxPower 0xFA).
   - Configuration callback returns `nullptr` for `index != 0` (single config).
   - String descriptor index 4 (XSM3 security) uses a 96-character buffer and the full string (no 31-char truncation).

2. **XSM3 authentication**
   - XSM3 state is initialized at driver init (not in the 0x81 callback).
   - Challenge init (0x82) and verify (0x87) data are stored when received; crypto (`xsm3_do_challenge_init` / `xsm3_do_challenge_verify`) runs in the main loop (`process()`), not in the USB callback.
   - 0x83 responses: 46 bytes for init, 22 bytes for verify.
   - 0x86 state: 1 = processing, 2 = response ready.

3. **USB on Pico 2 W**
   - USB is initialized before Core1 (Bluetooth) so the 360 can enumerate and run XSM3 even if BT firmware is still loading.

4. **Control handling**
   - Vendor and class control requests are forwarded to the active driver so XSM3 traffic reaches the XInput handler.

5. **Wake console from standby (remote wakeup)**
   - When the Xbox 360 has been turned off via **Guide → Turn off console** (not the front power button), the console may keep USB power and put the bus in suspend. The adapter can then wake the console by signaling USB remote wakeup when you:
     - **Press Guide (Home)** on the controller, or
     - **Hold Start for 3 seconds** (avoids holding Guide on Xbox One/PS5 pads, which can turn the controller off).
   - Remote wakeup is advertised in the configuration descriptor; the stack defaults it enabled when supported so wake works even if the host did not send SET_FEATURE before standby.
   - **Disclaimer:** Wake only works when the console was previously powered on with the adapter connected and then turned off via the controller (soft shutdown). It **cannot** power on the console from a cold start—e.g. if the console was just plugged in, lost power completely, or was turned off with the front power button. In those cases use the console’s power button to turn it on first.

Descriptors and XSM3 flow are aligned with [joypad-os](https://github.com/joypad-ai/joypad-os); a local comparison doc may be kept out of the repo for reference.

---

## PS3 mode — input delays, stuck inputs, Home button, and analog stick emulation

**Source:** Fixes from [OGX-Mini-Plus](https://github.com/guimaraf/OGX-Mini-Plus) (v1.1.1) — *“PS3 Driver Fixes - Fixed input delays and stuck inputs.”* Plus subsequent improvements for Home (PS) button and DS3-accurate analog sticks.

**Files:** `src/USBDevice/DeviceDriver/PS3/PS3.cpp`, `src/Descriptors/PS3.h`

### Changes

1. **L2/R2 axis values**
   - `report_in_.l2_axis` and `report_in_.r2_axis` are now set from `gp_in.trigger_l` and `gp_in.trigger_r`.
   - Previously left at zero, which could cause stuck or incorrect trigger behaviour on PS3. Filling these is required for many PS3 games.

2. **DualShock 3–accurate analog sticks**
   - Sticks now match the real DS3/Sixaxis HID spec:
     - **Range:** 0–255 (full 8-bit; was 0–254).
     - **Center:** **0x80 (128)** at rest and in deadzone (was 0x7F). Matches Linux gamepad spec and HID logical max 255.
     - **Deadzone:** ~1.5% (512 on ±32768) so small movements register without drift; in deadzone the report sends 0x80.
     - **Scaling:** Linear map from signed 16-bit input to 0–255 with correct rounding so center (0) → 128.
   - Reduces stick drift and matches console expectations for DS3-compatible games.

3. **D-pad in analog mode**
   - When `gamepad.analog_enabled()` is true, D-pad axes (`up_axis`, `down_axis`, `left_axis`, `right_axis`) are now derived from the **digital** D-pad bits instead of `gp_in.analog[ANALOG_OFF_*]`.
   - Prevents noisy analog D-pad values from causing stuck or wrong D-pad input on PS3.

4. **Face button axes (digital / non-analog branch)**
   - Corrected mapping so that:
     - `circle_axis` = BUTTON_B (was BUTTON_X)
     - `cross_axis` = BUTTON_A (was BUTTON_B)
     - `square_axis` = BUTTON_X (was BUTTON_A)
   - Circle / Cross / Square now match the intended face buttons.

5. **Home (PS) button**
   - Report is built only when `tud_hid_ready()` (right before send), using `gamepad.get_pad_in()` so the console gets the latest state (helps with DS4/DS5 over Bluetooth).
   - **PS button latch:** When `BUTTON_SYS` is set, the driver latches the PS bit for 8 consecutive report frames so short taps are not missed by timing. `buttons[2]` bit 0 (PS) and bit 1 (Touchpad) are set from `BUTTON_SYS` and `BUTTON_MISC`.
   - If Home does not work over Bluetooth, try a wired controller (some consoles only react to the first controller's Home).

6. **Wake console from standby (remote wakeup)**
   - The configuration descriptor now advertises **remote wakeup** (`bmAttributes` 0xA0) so the PS3 can suspend the USB bus in standby and the adapter can signal wake. When the console is in standby (turned off via **PS button → Turn off system** or similar, not full power loss), you can wake it by **pressing PS (Home)** or **holding Start for 3 seconds**—**only if the console keeps USB power when off**.
   - **Many PS3s cut power to the USB ports** when shut down, so the adapter and controller disconnect and wake is not possible. If your controller stays powered (e.g. charging LED) when the PS3 is off, that model may keep USB in standby and wake may work. Same disclaimer as 360: no wake from cold start; use the console power button first if needed.
   - **Recovery Mode:** Even when wake from standby is not possible (e.g. console cuts USB when off), the adapter is **confirmed working in PS3 Recovery Mode** — you can use it to navigate and select options in Recovery.

---

## Latency reduction

**Goal:** Reduce input-to-output latency in the device (Core0) main loop, especially for XInput with Bluetooth controllers (PS5, Xbox One).

### Main loop delay

- **Before:** The device loop used `sleep_ms(1)` every iteration, then a configurable `MAIN_LOOP_DELAY_US` (default 250 µs).
- **After:**
  - **Default: 0 µs** — no added delay; loop runs as fast as possible for minimum latency (low-latency default).
  - **250+ µs** — set via CMake (e.g. `-DMAIN_LOOP_DELAY_US=250`) to reduce CPU use if desired.

**Files changed:**

- `src/Board/Config.h` — `MAIN_LOOP_DELAY_US` default **0**; override via CMake.
- `src/OGXMini/Board/Standard.cpp`, `PicoW.cpp`, `Four_Channel_I2C.cpp` — use `sleep_us(MAIN_LOOP_DELAY_US)` when `> 0`.
- `CMakeLists.txt` — `MAIN_LOOP_DELAY_US` cache variable (default 0).

### XInput (360): report always fresh; send when ready (minimal latency)

Same goal as Switch Pro and PS3: the only added latency when using a wireless controller is the Bluetooth radio.

- **Every** `process()` call: read `get_pad_in()` and build `in_report_` (buttons, triggers, sticks). Then, if suspended, wake; call `tud_xinput::send_report(&in_report_)`. `send_report()` only actually transmits when the IN endpoint is free (`send_report_ready()`); otherwise we keep the latest `in_report_` so that (1) the host’s `get_report_cb` returns current state if it polls, and (2) the next time the endpoint is free we send that report. No `new_pad_in()` gate.
- **File:** `src/USBDevice/DeviceDriver/XInput/XInput.cpp` — `process()` always builds `in_report_` every loop; `tud_xinput::send_report()` sends only when `send_report_ready()` (see `tud_xinput.cpp`).

### Switch Pro and PS3: report always fresh; send when ready (minimal latency)

**Goal:** For Switch Pro and PS3 output modes, the only added latency should be wireless Bluetooth (radio) when using a BT controller. The adapter does not batch, throttle, or delay reports.

- **Switch Pro:** Every `process()` call reads `get_pad_in()`, builds `switch_report_`, and builds the standard or subcommand report into `report_` **before** any USB decisions. So (1) the host’s `get_report` (poll) always gets the latest `report_`, and (2) when `tud_hid_n_ready(0)` we push that same report. Init reply (0x81) is sent first when pending, then the standard report. No “only build when ready” — report is always current.
- **PS3:** Every `process()` call reads `get_pad_in()` and builds `report_in_` (full DS3 report). Then, when `tud_hid_ready()`, we send it. So (1) the host’s `get_report_cb` always returns the latest `report_in_`, and (2) we push that report whenever the IN endpoint is free. No “only build when ready” — report is always current.

Both modes: no `new_pad_in()` gate; main loop runs with `MAIN_LOOP_DELAY_US=0` by default; `tud_task()` runs before `process()` so the endpoint is ready when we try to send.

### Xbox OG (Duke) gamepad: send only on new input (match Team-Resurgent)

Report build/send timing matches [Team-Resurgent/OGX-Mini](https://github.com/Team-Resurgent/OGX-Mini): the HID report is built and sent **only** when `gamepad.new_pad_in()` is true. Sending every poll caused random disconnects on some OG Xbox setups; the “send when new input” rule avoids that. Guide (SYS) combos are kept: **Guide only** = IGR (LT+RT+Start+Back), **Guide+Start** = shutdown (LT+RT+Back+White). Rumble handling is unchanged.

- **File:** `src/USBDevice/DeviceDriver/XboxOG/XboxOG_GP.cpp` — `process()` builds `in_report_` and calls `tud_xid::send_report()` only when `new_pad_in()` and `send_report_ready(0)`.

**DualSense (PS5) input when outputting to OG Xbox:** The PS5 USB host no longer skips reports that are byte-identical to the previous one. Previously, “unchanged” reports did not call `set_pad_in()`, so with a polled output (OG Xbox) some transitions or sustained input could be dropped when the host loop was slower than the DualSense report rate. Every DualSense report is now pushed into the gamepad queue so the OG Xbox device always has the latest state. **File:** `src/USBHost/HostDriver/PS5/PS5.cpp` — removed the unchanged-report early return; every report is parsed and passed to `gamepad.set_pad_in()`.

### Main loop order: tud_task() before process()

The main loop now calls **`tud_task()` before** `device_driver->process()`. That way the USB stack updates completion status of the previous IN transfer first; then `process()` sees the endpoint as ready and can send the next report immediately with the latest gamepad state. Reduces latency by up to one main-loop iteration (avoids sending only every other loop when the host polls frequently).

**Files:** `src/OGXMini/Board/PicoW.cpp`, `Standard.cpp`, `Four_Channel_I2C.cpp` — order is `process_tasks()` → `tud_task()` → `process()` for each gamepad.

---

## PS2 (GPIO) / Open PS2 Loader stability

**Issue:** With a “primed” first response byte (0xFF) before the mode byte, Open PS2 Loader could hang at startup (black screen) when the adapter was connected.

**Fix:** The PS2 controller (device) response was reverted so the **first response byte is the mode byte** (no leading 0xFF). Protocol and escape-mode response lengths otherwise follow PicoGamepadConverter and DS4toPS2. The main loop drains all pending PS2 transactions each tick so rapid pad init (e.g. OPL at boot) does not desync. Core1 runs Bluetooth when used; Core0 runs the main loop and `psx_device_poll()` so the console sees input correctly.

**File:** `src/USBDevice/DeviceDriver/PS1PS2/controller_simulator.c` — first response byte is the mode byte; no `prime_first_byte()` / leading 0xFF.

---

## PS2 (GPIO) and OG Xbox — Home/Guide IGR and shutdown

**Goal:** Use the Home (PS2) or Guide (OG Xbox) button for in-game reset (IGR) and console shutdown with the same interaction pattern on both platforms: **Home only** = restart (IGR), **Home+Start** = shutdown.

### PS2 (GPIO) mode

**Files:** `src/USBDevice/DeviceDriver/PS1PS2/PS1PS2.cpp`, `PS1PS2.h`

- **Home only** — Sends the OPL in-game reset combo: **L1+L2+R1+R2+Start+Select** (triggers full). The console restarts the game / returns to OPL.
- **Home+Start** — Sends shutdown combo: **L1+L2+R1+R2+L3+R3** (triggers full). The console shuts down.

### OG Xbox mode

**Files:** `src/USBDevice/DeviceDriver/XboxOG/XboxOG_GP.cpp`, `XboxOG_GP.h`

- **Guide only** — Sends IGR (restart) combo: **LT+RT+Start+Back** (triggers full). The console performs a soft reset / returns to dashboard (or IGR handler).
- **Guide+Start** — Sends shutdown combo: **LT+RT+Back+White** (triggers full). The console shuts down.

### How to use

| Goal | Setting |
|------|--------|
| Low latency (default) | Use default `MAIN_LOOP_DELAY_US=0`. |
| Lower CPU use | Configure with e.g. `-DMAIN_LOOP_DELAY_US=250`. |

### Notes

- Core1 (Bluetooth / gamepad) runs with no sleep in its loop.
- USB full-speed poll interval (e.g. 4 ms for XInput) still applies; the improvement is that each report carries the **latest** input and the main loop adds no extra delay by default.
- Bluetooth adds latency; the changes above minimize the adapter’s contribution.

---

## Additional board support (RP2350_ZERO, RP2040_XIAO, RP2354)

**Source:** [Sakura-Research-Lab/OGX-Mini-2026-Testing](https://github.com/Sakura-Research-Lab/OGX-Mini-2026-Testing).

Three additional boards use the same Standard (PIO-USB host) code path as PI_PICO, RP2040_ZERO, ADAFRUIT_FEATHER, and RP2350_USB_A:

| Board | CMake option | Notes |
|-------|--------------|--------|
| Waveshare RP2350-Zero | `OGXM_BOARD=RP2350_ZERO` | PIO USB D+ = GP10, RGB = GP16; RP2350. |
| Seeed Studio XIAO RP2040 | `OGXM_BOARD=RP2040_XIAO` | PIO USB D+ = GP0, RGB = GP12, LED = GP17. |
| RP2354 | `OGXM_BOARD=RP2354` | PIO USB D+ = GP0, LED = GP25; RP2350. |

**Files:** `src/Board/Config.h` (board IDs and pin defines), `src/OGXMini/OGXMini.cpp` (init/run/host_mounted tables), `src/OGXMini/Board/Standard.cpp` (extended `#if`), `CMakeLists.txt` (board branches).

---

## 8BitDo XInput (Xbox 360) host fix

**Source:** [Sakura-Research-Lab/OGX-Mini-2026-Testing](https://github.com/Sakura-Research-Lab/OGX-Mini-2026-Testing).

Some 8BitDo wired XInput controllers (VID 0x2DC8, PID 0x3016 or 0x3106) can disconnect or behave oddly if the host leaves the controller LED on. The Xbox 360 host driver now detects these devices by VID/PID and schedules a repeating delayed task (every 1 s) that sends “LED off” to the controller. Other controllers are unchanged (LED stays on as before).

**File:** `src/USBHost/HostDriver/XInput/Xbox360.cpp` — `initialize()` calls `tuh_vid_pid_get()`, and if 8BitDo, queues `TaskQueue::Core1::queue_delayed_task(..., 1000, true, set_led(..., false))`.

---

## PS5 (DualSense) over Bluetooth — reducing perceived delay vs Xbox One

**Why PS5 can feel slower than Xbox One over BT:** DualSense sends larger reports (78 bytes, with gyro/accel), and Bluepad32’s DS5 parser does more work per report (calibration, etc.). Xbox One reports are smaller and the parser is lighter. Bluetooth poll/report rate and radio latency dominate; the adapter’s job is to not add extra delay.

**Change in this firmware:** The PS5 adaptive-trigger toggle (touchpad/mute button) now runs **after** `set_pad_in(gp_in)`. Previously it ran at the start of the callback; when you pressed the touchpad it could send two output reports (left/right trigger effect) before updating gamepad state, which could delay the next main-loop read. Now the gamepad state is always written first, then the trigger effect is sent, so input is not held up by the trigger command.

**If you need minimum latency with a DualSense:** Use the controller **wired** on the PIO USB host port when possible; wired PS5 uses the same low-latency path as other USB host controllers and avoids BT report size and rate limits.

---

## Pico W / Pico 2 W — DualShock 4 and Classic Bluetooth (BR/EDR) stability

**Context:** On **CYW43439** (Pico W / Pico 2 W), **BLE** and **Classic Bluetooth (BR/EDR)** share one radio. **DualShock 4** uses **Classic ACL** only. **DualSense**, **Xbox Series (BLE)**, and **Switch Pro** (typical pairing) use **LE** — so DS4 was uniquely sensitive to how the stack used the radio at the same time as other activity.

### 1. BLE advertising vs Classic ACL (main DS4 drop fix)

**OGX-Mini** starts **BLE advertising** via **BLEServer** so the **phone / web app** can discover the adapter. If that advertising stays on while a **Classic** gamepad (DS4, DualShock 3 BT, Xbox 360 wireless) holds an **ACL** link, the connection often **dies within seconds** (e.g. blue LED then disconnect).

- **`gap_advertisements_enable(0)`** at the **start of DS4 HID setup** (before lightbar / calibration traffic), and again whenever **any** ready gamepad uses **`GAP_CONNECTION_ACL`**.
- **`gap_advertisements_enable(1)`** when the **last** Classic pad disconnects (BLE-only controllers, e.g. DualSense still connected, are unaffected by this rule).

**Files:** `Firmware/external/bluepad32/.../parser/uni_hid_parser_ds4.c` (OGXM Pico path), `Firmware/RP2040/src/Bluepad32/Bluepad32.cpp`.

**User impact:** While a **Classic Bluetooth** controller is connected, the adapter **does not advertise** for the BLE web app. Disconnect that controller (or use **USB** for setup) to use **Bluetooth** in the web app again.

### 2. BR/EDR inquiry while Classic is connected

**Periodic inquiry** (scanning for new gamepads) **plus** an active **Classic ACL** also contends on the same radio and contributed to **short-lived DS4** links.

- **`uni_bt_bredr_scan_stop()`** at the beginning of DS4 setup and when any **ACL** pad becomes **ready**.
- **`uni_bt_bredr_scan_start()`** when the **last** Classic pad disconnects (if scanning is still enabled globally).

**File:** `Bluepad32.cpp` (and DS4 setup in `uni_hid_parser_ds4.c`).

### 3. DS4 virtual “touchpad mouse” disabled on Pico

Bluepad32 can register a **second virtual HID device** (mouse) on the same DS4 link. On Pico W that path was linked to **unstable links**; the OGX build (**`OGXM_BLUEPAD32_PICO_W`**) **does not create** that virtual device. **DualSense** still uses its normal setup (virtual child rejected in `device_ready` where applicable).

**File:** `uni_hid_parser_ds4.c` (CMake defines `OGXM_BLUEPAD32_PICO_W=1` for the Bluepad32 library in `Firmware/RP2040/CMakeLists.txt`).

### 4. PS4 rumble / FF grace period

For **`CONTROLLER_TYPE_PS4Controller`**, **rumble output** is **delayed ~6 seconds** after connect so the host cannot push force-feedback during the fragile init window.

**File:** `Bluepad32.cpp` — `send_feedback_cb` / `device_ready_cb`.

### 5. Related Pico W Bluetooth improvements (see also Summary table)

- **Core0 `sleep_ms(1)`** in the main loop so Core1 (BT stack) gets CPU time ([Team-Resurgent/OGX-Mini](https://github.com/Team-Resurgent/OGX-Mini) pattern).
- **Lock-free Bluetooth input path** (`set_pad_in_from_bluetooth`) so HID callbacks never block on Core0’s mutex.
- **Reconnect:** last device disconnect calls **`uni_bt_enable_new_connections_unsafe(true)`** so pairing can resume without power-cycling.
- **8 s input-stall disconnect** (non-virtual, non-BLE-Xbox) to clear zombie links; **BLE Xbox** uses keepalive instead of stall disconnect.

---

## Switch Pro — analog stick sensitivity

A configurable sensitivity gain is applied to the analog sticks in Switch Pro emulation. Raw stick values (outside the deadzone) are scaled by **STICK_GAIN_NUM / STICK_GAIN_DEN** (default 120/100 = 1.2×) before mapping to the 12-bit Switch report. The same physical deflection produces slightly larger output for a more responsive feel.

**File:** `src/USBDevice/DeviceDriver/Switch/Switch.cpp` — `gamepad_to_switch_report()`; tune via `STICK_GAIN_NUM` and `STICK_GAIN_DEN`.

---

## Build scripts (new users)

To build firmware without memorizing CMake options, use the interactive build scripts from the **project root**:

| Platform | Command |
|----------|---------|
| **Linux / macOS** | `./scripts/build.sh` |
| **Windows (PowerShell)** | `.\scripts\build.ps1` |

The script checks for required tools (git, python3, cmake, ninja, arm-none-eabi-gcc) and prints install hints if something is missing. It then prompts for: (1) board (Pi Pico, Pico W, Pico 2 W, RP2040-Zero, XIAO, Feather, 4CH I2C, ESP32 hybrid, etc.); (2) default (all modes via combos) or fixed output mode (e.g. Wii, GameCube, N64); (3) Release or Debug. Build output (`.uf2`, `.elf`) is written to **`scripts/build/`**; on failure you can save a log to `scripts/build_log.txt`. See the main [README](../../../README.md) Build section for the full description.

---

## Summary

| Area | Improvement |
|------|-------------|
| **XInput (360)** | XSM3 authentication and descriptors aligned with joypad-os; adapter works on Xbox 360 with BT controllers (PS5, Xbox One). 8BitDo wired fix: LED keepalive for VID 0x2DC8 / PID 0x3016 or 0x3106. Report built every loop, send when endpoint ready — same minimal-latency pattern as Switch/PS3. |
| **PS3** | Stuck inputs and delays addressed via L2/R2 axes; DS3-accurate sticks (0–255, center 0x80, ~1.5% deadzone); D-pad and face button mapping; Home (PS) button with 8-frame latch for BT controllers. |
| **PS2 (GPIO)** | Home only = IGR (L1+L2+R1+R2+Start+Select); Home+Start = shutdown (L1+L2+R1+R2+L3+R3). OPL and protocol stability (first response byte = mode byte). |
| **OG Xbox** | Guide only = IGR. Shutdown = LT+RT+Back+White via **Guide+Start** or **Guide+View (Back)**; Xbox BT often omits Start while Guide is held. Shutdown report strips Start so the chord matches BIOS/softmod expectations. |
| **Switch Pro** | Analog stick sensitivity gain (default 1.2×) for more responsive sticks; configurable in `Switch.cpp`. |
| **Boards** | RP2350_ZERO, RP2040_XIAO, RP2354 supported (Standard/PIO-USB host path). |
| **Latency** | Main loop delay default **0 µs**; `tud_task()` before `process()` so reports send every loop when ready; XInput/Switch/PS3 send latest state when USB ready (no `new_pad_in()` gate). Switch Pro and PS3 always build report every loop so host poll (`get_report`) and IN push both see current state — only remaining delay is BT radio when wireless. |
| **Build** | Interactive scripts `scripts/build.sh` (Linux/macOS) and `scripts/build.ps1` (Windows) for board selection, fixed/default mode, and Release/Debug; output in `scripts/build/`. See [README](../../../README.md) Build section. |
| **Bluetooth (Pico W)** | **DS4 / Classic ACL:** BLE advertising **paused** while Classic pad connected; **BR inquiry stopped** during ACL; **no DS4 virtual mouse**; **6 s PS4 rumble** grace. **Xbox Series (BLE):** no stall disconnect when idle; keepalive 12 s; stale-slot delete on reconnect. **General:** `sleep_ms(1)` main loop; lock-free BT pad-in; re-enable scan on last disconnect. |
