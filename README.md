# OGX-Mini 2026

**Support development** — If this firmware helps you, consider **[donating on Ko-fi](https://ko-fi.com/megacadedev)**. Contributions go toward testing and adding support for **more boards and controllers** so the project can grow for everyone.

---

![OGX-Mini Boards](images/OGX-Mini-github.jpg "OGX-Mini Boards")

Firmware for the RP2040, capable of emulating gamepads for several game consoles. The firmware comes in many flavors, supported on the [Adafruit Feather USB Host board](https://www.adafruit.com/product/5723), Pi Pico, Pi Pico 2, Pi Pico W, Pi Pico 2 W, Waveshare RP2040-Zero, Waveshare RP2350-USB-A, Waveshare RP2350-Zero, Seeed Studio XIAO RP2040, RP2354, Pico/ESP32 hybrid, and a 4-Channel RP2040-Zero setup.

[**Visit the web app here**](https://megacadedev.github.io/OGX-Mini-2026-WebApp/) to change your mappings and deadzone settings. To pair the OGX-Mini with the web app via USB, plug your controller in, then connect it to your PC, hold **Start + Left Bumper + Right Bumper** to enter web app mode. Click "Connect via USB" in the web app and select the OGX-Mini. You can also pair via Bluetooth, no extra steps are needed in that case. 

[**Join the discord here!**](https://discord.gg/guaBh9JZQ)

## Supported platforms
- Original Xbox
- Playstation 3
- Nintendo Switch 1 & 2 (docked) — **Switch Pro Controller** emulation over USB
- XInput (Xbox 360, UsbdSecPatch no longer required)
- Playstation Classic
- DInput
- Wii U (GameCube Adapter)
- **Wii (Wiimote)** — Pico W / Pico 2 W only; build with `-DOGXM_FIXED_DRIVER=WII`. See [Wii Mode Guide](Firmware/RP2040/docs/Wii_Mode_Guide.md).
- **PlayStation 1 & 2** — GPIO output to console controller port (DualShock-style).
- **Dreamcast** — GPIO output over Maple Bus to console controller port.
- **GameCube / Wii (GameCube ports)** — GPIO output over single-wire JoyBus. **Build-only (no combo):** use `-DOGXM_FIXED_DRIVER=GAMECUBE`; see below.
- **Nintendo 64** — GPIO output over single-wire to controller port. **Build-only (no combo):** use `-DOGXM_FIXED_DRIVER=N64`; see below.

**RP2040 output modes:** **USB device:** XInput (Xbox 360 + XSM3), DInput, PS3, **Switch Pro** (Nintendo Switch Pro Controller emulation), Wii U, **Wii (Wiimote, build-option only)**, Xbox OG (Gamepad / Steel Battalion / XRemote), PS Classic, Web App. **GPIO (no USB device):** PS1/PS2, Dreamcast, GameCube, N64 — use any wired USB or (on Pico W / Pico 2 W) Bluetooth controller as input. **Wii, GameCube, and N64 are not in the combo list** — use a dedicated build for those modes (see below).

## Changing platforms
By default the OGX-Mini will emulate an OG Xbox controller, you must hold a button combo for 3 seconds to change which platform you want to play on. Your chosen mode will persist after powering off the device. 

Start = Plus (Switch) = Options (Dualsense/DS4)

- XInput (Xbox 360)
    - Start + Dpad Up 
- Original Xbox
    - Start + Dpad Right
- Original Xbox Steel Battalion
    - Start + Dpad Right + Right Bumper
- Original Xbox DVD Remote
    - Start + Dpad Right + Left Bumper
- Switch (Pro Controller emulation)
    - Start + Dpad Down
- PlayStation 3
    - Start + Dpad Left
- PlayStation Classic
    - Start + A (Cross for PlayStation and B for Switch gamepads)
- PlayStation 1 / PlayStation 2 (GPIO output)
    - Start + B
- Dreamcast (GPIO output)
    - Start + Y
- Wii U (GameCube Adapter)
      - Start + Left Bumper + D-Pad Down
- Web Application Mode
    - Start + Left Bumper + Right Bumper

**Wii, GameCube, and N64 are not selectable by combo.** Use a dedicated build for those modes:

- **Wii (Wiimote):** `-DOGXM_FIXED_DRIVER=WII`. See [Wii Mode Guide](Firmware/RP2040/docs/Wii_Mode_Guide.md).
- **GameCube (GPIO):** `-DOGXM_FIXED_DRIVER=GAMECUBE`. Example (Pico 2 W Debug):  
  `cmake -DOGXM_BOARD=PI_PICO2W -DCMAKE_BUILD_TYPE=Debug -DOGXM_FIXED_DRIVER=GAMECUBE ..` then `ninja`. Flash `OGX-Mini-*-GAMECUBE.uf2`. Use `PI_PICOW` for Pico W.
- **N64 (GPIO):** `-DOGXM_FIXED_DRIVER=N64`. Example (Pico 2 W Debug):  
  `cmake -DOGXM_BOARD=PI_PICO2W -DCMAKE_BUILD_TYPE=Debug -DOGXM_FIXED_DRIVER=N64 ..` then `ninja`. Flash `OGX-Mini-*-N64.uf2`.

Input for GPIO modes is Bluetooth or USB; no combo switching in these builds.

After a new mode is stored, the RP2040 will reset itself so you don't need to unplug it.

## Disconnecting Controllers
For most controllers pressing and holding Start+Select (+/-, etc) for the controller will disconnect it and restart pairing mode.
For the OUYA controller there is no Start+Select, the disconnection combo has been set to L3+R3.

## Supported devices
### Wired controllers (USB input)
See [**Wired Controllers**](Firmware/RP2040/docs/Wired_Controllers.md) for a full list by driver type. The following work when the adapter is outputting to any supported platform (Xbox, Switch, XInput, PS3, DInput, **PS1/PS2**, **Dreamcast**, **GameCube**, **N64**, etc.):
- Original Xbox Duke and S
- Xbox 360, One, Series, and Elite
- Dualshock 3 (PS3)
- Dualshock 4 (PS4)
- Dualsense (PS5)
- Nintendo Switch Pro
- Nintendo Switch wired
- Nintendo 64 Generic USB
- Playstation Classic
- Generic DInput
- Generic HID (mappings may need to be editted in the web app)

Note: There are some third party controllers that can change their VID/PID, these might not work correctly.

### Wireless adapters
- Xbox 360 PC adapter (Microsoft or clones)
- 8Bitdo v1 and v2 Bluetooth adapters (set to XInput mode)
- Most wireless adapters that present themselves as Switch/XInput/PlayStation controllers should work

### Wireless Bluetooth controllers (Pico W & ESP32)
**Note:** Bluetooth functionality is in early testing; some controllers may have quirks. BT pads work as input in **any** USB output mode (OG Xbox, XInput, PS3, Switch, etc.) on Pico W / Pico 2 W, and also when outputting over **GPIO** to **PS1/PS2**, **Dreamcast**, **GameCube**, or **N64** (same as wired USB in those modes).

**Pico W / Pico 2 W — important for DualShock 4 (DS4):** DS4 uses **Classic Bluetooth (BR/EDR)**. The same CYW43 radio also runs **BLE advertising** for the **phone / web app**. Running both at once often **drops the DS4 link** seconds after connect (blue LED then off). Firmware therefore **pauses BLE advertising** while a **Classic BT** gamepad is connected (DS4, DS3 over BT, Xbox 360 wireless adapter style links). **DualSense** and **Xbox Series (BLE)** mostly use **LE** and are unaffected by that pause.

| Situation | What to do |
|-----------|------------|
| Use **DS4 / Classic BT** pad | Connect and play; web app BLE discovery is off until that pad disconnects. |
| Use **Bluetooth** in the **web app** | Disconnect **Classic BT** controllers first, **or** plug the adapter in via **USB** and use **Connect via USB** in the app. |

Full technical detail: **[Firmware/RP2040/docs/IMPROVEMENTS.md](Firmware/RP2040/docs/IMPROVEMENTS.md)** → *Pico W / Pico 2 W — DualShock 4 and Classic Bluetooth stability*.

- Xbox Series, One, and Elite 2
- Dualshock 3
- Dualshock 4
- Dualsense
- Switch Pro
- Steam
- Stadia
- Wii U Pro
- Wii Remote
   - Supported Extensions:
      - Gamepad
      - Nunchuck
      - GameCube Controller
- 8BitDo Ultimate Wireless (Switch layout)

Please visit [**this page**](https://bluepad32.readthedocs.io/en/latest/supported_gamepads/) for a more comprehensive list of supported controllers and Bluetooth pairing instructions.

# Features new to this fork

Version history and release notes are in **[CHANGELOG.md](CHANGELOG.md)**. For detailed firmware improvements — **PS3**, **XInput / XSM3**, **OG Xbox**, **PS2/OPL**, **latency**, **Pico W Bluetooth (DS4 Classic BT, BLE coexistence, Xbox Series BLE)**, etc. — see **[Firmware/RP2040/docs/IMPROVEMENTS.md](Firmware/RP2040/docs/IMPROVEMENTS.md)**.

*Note: Many features were added and tested on Pico W / Pico 2 W; other boards may not have been tested for every change.*

## Planned additions from the original creator
- More accurate report parser for unknown HID controllers
- Hardware design for internal OG Xbox install
- Hardware design for 4 channel RP2040-Zero adapter
- Wired Xbox 360 chatpad support
- Wired Xbox One chatpad support
- Switch (as input) rumble support
- OG Xbox communicator support (in some form)
- Generic bluetooth dongle support
- Button macros
- Rumble settings (intensity, enabled/disable, etc.)

## Planned additions for this fork
- **Web app bindings for OG Xbox mode** — Allow users to rebind the Guide tap (Start) and the IGR/shutdown hold combos (e.g. which button triggers 1 s soft IGR or 3 s shutdown) via the web app.
- Output to the following consoles:
      - ~~PS2~~ (done: PS1/PS2 GPIO mode, see changelog)
      - ~~GameCube~~ (done: GameCube GPIO mode, see changelog)
      - DreamCast
      - NES
      - SNES
      - Genesis
      - Master System

## Hardware
For Pi Pico, RP2040-Zero, 4 channel, and ESP32 configurations, please see the hardware folder for diagrams.

I've designed a PCB for the RP2040-Zero so you can make a small form-factor adapter yourself. The gerber files, schematic, and BOM are in Hardware folder.

<img src="images/OGX-Mini-rpzero-int.jpg" alt="OGX-Mini Boards" width="400">

If you would like a prebuilt unit, you can purchase one, with cable and Xbox adapter included, from the original creators store: [**Etsy store**](https://www.etsy.com/listing/1426992904/ogx-mini-controller-adapter-for-original).

## Adding supported controllers

If your third‑party controller isn’t working but a similar one (e.g. same brand or protocol) is listed in [Wired Controllers](Firmware/RP2040/docs/Wired_Controllers.md), we can add support by registering its **VID** (Vendor ID) and **PID** (Product ID). Send those two values (and the controller name/model if you know it) so they can be added to the right list in `Firmware/RP2040/src/USBHost/HardwareIDs.h`.

### How to get VID and PID

**Windows**

- **Device Manager:** Plug in the controller → open Device Manager → find it under “Sound, video and game controllers” or “Human Interface Devices” → right‑click → **Properties** → **Details** → **Property:** “Hardware Ids”. You’ll see something like `HID\VID_054C&PID_09CC`; **VID** is the hex after `VID_`, **PID** is the hex after `PID_` (e.g. VID `054C`, PID `09CC`).
- **USBDeview** (NirSoft): Lists all USB devices with VID and PID in columns.

**Linux**

- Run `lsusb` with the controller plugged in. Each line is `Bus ... Device ... ID **vid:pid** Manufacturer Product`. Use the `vid` and `pid` from the line that matches your controller (e.g. `054c:09cc` → VID `054C`, PID `09CC`; we use uppercase hex in the code).
- Alternatively: `lsusb -v` and look at `idVendor` and `idProduct` for the gamepad interface.

**macOS**

- **System Information:** Apple menu → **About This Mac** → **System Report** → **USB** (or **Bluetooth** if wireless). Select the controller; **Vendor ID** and **Product ID** are shown (often in decimal — convert to hex for the code, e.g. 1356 decimal → `054C` hex).
- Or use **System Profiler** and open the USB section.

**Web / gamepad API**

- Some browser-based tools (e.g. [gamepad tester](https://gamepad-tester.com/) or similar) show the connected gamepad’s VID/PID in the page or in the browser’s device/Gamepad API info. Check the site’s instructions.

When submitting, include **VID**, **PID**, and controller name (e.g. “8BitDo Pro 2 in DInput mode”). If the controller has multiple modes (XInput / DInput / Switch), note which mode you used when reading the IDs.

## Build
### RP2040
Build with **CMake** from the `Firmware/RP2040` directory (or use the build scripts below). Prerequisites: git, python3, CMake, Ninja, and the GCC ARM toolchain. CMake will patch Bluepad32/BTStack and fetch submodules as needed.

#### Board options (OGXM_BOARD)

Use one of these values for **`OGXM_BOARD`** in a manual build, or pick the same board from the build script’s numbered list:

- ```PI_PICO``` 
- ```PI_PICO2``` 
- ```PI_PICOW``` 
- ```PI_PICO2W``` 
- ```RP2040_ZERO``` (Waveshare RP2040-Zero)
- ```RP2350_USB_A``` (Waveshare RP2350-USB-A)
- ```RP2350_ZERO``` (Waveshare RP2350-Zero)
- ```RP2040_XIAO``` (Seeed Studio XIAO RP2040)
- ```RP2354```
- ```ADAFRUIT_FEATHER``` use the board’s **USB Type-A** port for the controller; 5V enable is GPIO 18, D+/D− are GPIO 16/17. If the controller is not detected, plug it in **after** the board has booted and ensure the green 5V LED next to the USB-A port is on.)
- ```ESP32_BLUEPAD32_I2C```
- ```ESP32_BLUERETRO_I2C``` 
- ```EXTERNAL_4CH_I2C```

You can also set ```MAX_GAMEPADS``` (if &gt; 1, only DInput/PS3 and Switch Pro are supported). **Optional:** ```OGXM_FIXED_DRIVER``` to lock output mode (e.g. ```XINPUT```, ```PS3```); ```OGXM_FIXED_DRIVER_ALLOW_COMBOS=ON``` to keep combos when fixed. ```MAIN_LOOP_DELAY_US``` (default ```0```) sets main-loop delay for lower CPU use (e.g. ```250```).

You'll need git, python3, CMake, Ninja and the GCC ARM toolchain installed. CMake scripts will patch some files in Bluepad32 and BTStack and also make sure all git submodules (plus their submodules and dependencies) are downloaded.

#### Build scripts (recommended for new users)

We provide interactive build scripts so you can build firmware without memorizing CMake options:

| Platform | Command (run from project root) |
|----------|----------------------------------|
| **Linux / macOS** | `./scripts/build.sh` |
| **Windows (PowerShell)** | `.\scripts\build.ps1` |

The script will:

1. **Check prerequisites** — Verifies that `git`, `python3` (or `python`), `cmake`, `ninja`, and `arm-none-eabi-gcc` are installed. If something is missing, it prints install hints (e.g. `apt install` on Debian/Ubuntu, `brew install` on macOS, Chocolatey on Windows).
2. **Ask for your board** — Choose from a numbered list (Pi Pico, Pi Pico W, Pi Pico 2 W, RP2040-Zero, XIAO, Adafruit Feather, 4CH I2C, ESP32 hybrid, etc.).
3. **Ask for build type** — **Default** (all output modes available via button combos) or **Fixed output mode** (e.g. Wii, GameCube, N64 — one mode only, no combo switching).
4. **Ask for configuration** — **Release** (smaller, faster) or **Debug** (UART logging for troubleshooting).

Build output (`.uf2`, `.elf`, etc.) is written to **`scripts/build/`**. To flash the board, copy the `.uf2` file from that folder to the Pico’s USB drive. If the build fails, the script can save a log to `scripts/build_log.txt` for debugging.

#### Manual build (CMake from command line)

If you prefer to run CMake yourself or use an IDE (e.g. VSCode CMake Tools), use the same options the scripts pass. Example (Windows, Pico W, Release):
```
git clone --recursive https://github.com/MegaCadeDev/OGX-Mini-2026.git
cd OGX-Mini-2026/Firmware/RP2040
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOGXM_BOARD=PI_PICOW -DMAX_GAMEPADS=1
cmake --build build
```
Outputs (`.elf`, `.uf2`, etc.) are in the build directory; flash the `.uf2` to the board. Or use the GCC ARM toolchain and CMake Tools extension in VSCode.

**Latency:** The adapter sends the latest gamepad state whenever the USB endpoint is free (XInput and similar), and the main loop has no added delay by default, so the host gets updates at its poll rate with minimal latency. See [Firmware/RP2040/docs/IMPROVEMENTS.md](Firmware/RP2040/docs/IMPROVEMENTS.md#latency-reduction) for details.

### Firmware documentation
| Document | Description |
|---------|-------------|
| [Wii_Mode_Guide.md](Firmware/RP2040/docs/Wii_Mode_Guide.md) | Wii mode (build-option only): No Extension / Nunchuk / Classic, USB host, sync and auto-connect, button mapping. |
| [PICO2W_WII_USB_SETUP.md](Firmware/RP2040/docs/PICO2W_WII_USB_SETUP.md) | Pico 2 W / Pico W: USB host wiring (PIO USB), pins, build, troubleshooting for Wii mode. |
| [IMPROVEMENTS.md](Firmware/RP2040/docs/IMPROVEMENTS.md) | Firmware improvements: PS3 fixes, latency, XInput/360. |

### ESP32
Please see the Hardware directory for a diagram showing how to hookup the ESP32 to your RP2040.

You will need ESP-IDF v5.1, esptool, python3, and git installed. If you use VSCode, you can install the ESP-IDF extension and configure the project for ESP-IDF v5.1, it'll download everything for you and then you just click the build button at the bottom of the window.

When you build with ESP-IDF, Cmake will run a python script that copies the necessary BTStack files into the components directory, this is needed since BTStack isn't configured as an ESP-IDF component when you download it with git. 


# Credit to the original creator [https://wiredopposite.github.io/](https://github.com/wiredopposite/OGX-Mini/tree/master) for the original base of the project!

## Other projects that have helped enhance this fork

- **[Sakura-Research-Lab/OGX-Mini-2026-Testing](https://github.com/Sakura-Research-Lab/OGX-Mini-2026-Testing)** — OG Xbox Guide button (tap/hold IGR/shutdown), board support for RP2350_ZERO, RP2040_XIAO, RP2354, and 8BitDo XInput (Xbox 360) LED keepalive fix.
- **[Joypad OS](https://github.com/joypad-ai/joypad-os)** — Reference for Xbox 360 (XSM3) authentication with retail consoles; XInput mode on 360 follows the same approach. Descriptors and XSM3 flow are aligned with joypad-os; single config, full XSM3 security string, init/verify in main loop; USB is initialized before Core1 so the 360 can enumerate while BT loads.
- **[Bluepad32](https://github.com/ricardoquesada/bluepad32)** — Bluetooth controller support (Pico W / Pico 2 W).
- **[faithvoid](https://github.com/faithvoid)** — Xbox 360 controller vibration/rumble fix (host-side rumble handling, including wireless RUMBLE_ENABLE sequence).
- **[PicoGamepadConverter](https://github.com/wiredopposite/PicoGamepadConverter)** — Wii (Wiimote) output mode: approach of USB gamepad on PIO USB with Bluetooth reserved for the Wiimote link, and button/stick mappings for No Extension, Nunchuk, and Classic Controller report modes.
- **[retro-pico-switch](https://github.com/DavidPagels/retro-pico-switch)** — Reference for Nintendo Switch Pro Controller protocol and report layout (N64/GameCube → Raspberry Pi Pico → Switch via USB or Bluetooth). Used to align OGX-Mini’s Switch mode with correct Pro Controller button byte layout and emulation behavior.

## Licenses and third-party code

- **Xbox 360 console authentication:** XInput builds that work on retail Xbox 360 consoles use [libxsm3](https://github.com/InvoxiPlayGames/libxsm3) (Xbox Security Method 3). libxsm3 is licensed under the GNU Lesser General Public License v2.1 or later; see `Firmware/external/libxsm3/LICENSE.txt`. libxsm3 credits oct0xor, emoose (ExCrypt), and sanjay900; see `Firmware/external/libxsm3/README.md`.
- **Other dependencies:** See the root `LICENSE` file and the `Firmware/external` directory for Bluepad32, BTStack, TinyUSB, Pico SDK, and other third-party licenses.

