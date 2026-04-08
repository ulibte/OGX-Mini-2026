# Wired controllers (USB input)

This document lists controllers supported when connected to the OGX-Mini adapter **via a wired (USB) connection**. These work as input when the adapter is outputting to any supported platform (Original Xbox, Xbox 360, Switch, PS3, DInput, PS1/PS2 GPIO, Dreamcast, GameCube, N64, etc.).

**Note:** Some third‑party controllers change their VID/PID depending on mode; they may need to be set to the correct mode (e.g. XInput, DInput, or Switch) to be recognized.

---

## By driver type

### XInput (Xbox 360, Xbox One, Xbox Series)

- **Microsoft:** Xbox 360 (wired and wireless with PC receiver), Xbox One, Xbox Series, Xbox Elite
- **Third‑party:** Controllers that identify as XInput over USB (e.g. many 8BitDo, PowerA, PDP, Afterglow when in XInput mode)

*Matched by USB class; no fixed VID/PID list. UsbdSecPatch is not required on Xbox 360.*

---

### PlayStation 3 (DualShock 3 and compatible)

- Sony DualShock 3 (Dualshock 3 / Batoh)
- Sony DualShock 3 (alternate/clone VID)
- Razer Panthera (PS3)
- Afterglow PS3 (multiple models)
- Bigben, Gioteck, Hori (Fighting Commander, Horipad, Fightstick), Mad Catz (Fightstick, Fightpad), PDP, PowerA, Nyko, and other PS3‑protocol controllers

*See `PS3_IDS` in `HardwareIDs.h` for the full VID/PID list.*

---

### PlayStation 4 (DualShock 4 and compatible)

- Sony DualShock 4 (all revisions)
- Sony DS4 wireless adapter (USB dongle)
- Hori Fighting Commander 4, Hori PS4 Mini
- Brook, Nacon, PDP, PowerA, Razer (Raiju), Scuf, Victrix, and other DS4‑protocol controllers

*See `PS4_IDS` in `HardwareIDs.h` for the full VID/PID list.*

---

### PlayStation 5 (DualSense and compatible)

- Sony DualSense (PS5)
- Sony DualSense Edge (PS5)
- Sony PS5 Access Controller

*See `PS5_IDS` in `HardwareIDs.h` for the full VID/PID list.*

---

### Nintendo Switch (Pro and wired)

- **Switch Pro / Joy‑Con / Wii U Pro:**  
  Nintendo Switch Pro Controller, Wii U Pro Controller, Joy‑Con (L/R), Nintendo Switch 2 Pro Controller
- **Wired Switch:**  
  PowerA (wired, Enhanced, Fusion, Spectra, Arcade Stick, Fusion Pro), Hori (Pokken, Horipad for Switch / Switch 2), Afterglow Deluxe, Faceoff Deluxe, and other wired Switch controllers

*See `SWITCH_PRO_IDS` and `SWITCH_WIRED_IDS` in `HardwareIDs.h` for the full VID/PID list.*

**Switch 2–family USB (Pro Controller 2, Joy‑Con 2, NSO GameCube, etc.):**  
The firmware runs the same vendor **bulk OUT** bring‑up on **USB configuration 1, interface 1** as the PC capture tool (`Tools/controller_capture/switch2_usb_init.py` / HandHeldLegend ProCon 2 enabler), then continues with the normal Switch Pro **HID** init (handshake, full report mode, etc.). Wired input reports may use report ID **0x09** (with a 1‑byte counter after the ID).

**Switch 2 Pro (PID 0x2069), wired USB only:** After the same **bulk OUT** bring‑up on **config 1 / interface 1**, **Switch2ProHost** parses the standard **64‑byte** HID report (ID **0x09**, 1‑byte counter, then **10‑byte** `SwitchPro::InReport` payload). Digitals use **Switch‑2‑specific** bit positions (not classic **Buttons0/1/2**): face **B/A/Y/X**, **RB**, **LB** (Home bit), **Minus** → Back, **Plus** / d‑pad directions, **L3**, **Start** (classic R), **R3** (classic ZR bit), **SYS** (classic `bl` d‑pad up), **ZL/ZR** → **LT/RT** (digital full press from captured bits), and **Capture** is still not mapped to **MISC**. **GL**, **GR**, and **Chat** are **documented in source** (`Switch2ProHost.cpp`) but **left unmapped**. Extended report bytes are not used for decoding (avoids IMU coupling / flicker). **Bluetooth** for this model is **not** covered here—use a **wired** connection to the adapter’s USB host port. Other Switch 2–family PIDs keep **SwitchProHost**.

---

### PlayStation Classic

- PlayStation Classic controller (official)

*See `PSCLASSIC_IDS` in `HardwareIDs.h`.*

---

### Nintendo 64 (USB)

- Retrolink N64 USB gamepad
- Hyperkin Admiral N64, Hyperkin N64 Controller Adapter
- Mayflash N64 Adapter

*See `N64_IDS` in `HardwareIDs.h` for the full VID/PID list.*

---

### Original Xbox (USB host)

- Original Xbox Duke and S (via USB adapter/converter)

*Used when the adapter is in OG Xbox output mode with a compatible USB host path.*

---

### DInput / Generic HID

Controllers that use the standard HID gamepad (DInput) protocol, including:

- **Logitech:** F310, F510, F710, RumblePad 2, Dual Action, WingMan, ChillStream, etc.
- **8BitDo:** Many models in DInput/HID mode (e.g. Pro 2, Pro 3, Ultimate, Lite, FC30 Pro, F30 Arcade, GameCube, Dogbone)
- **Hori:** Fightsticks, Fighting Commander, Horipad (various), Taiko Controller, etc.
- **Mad Catz:** Fightsticks, Fightpad, CTRLR, etc.
- **Qanba, Mayflash, Brook:** Arcade sticks and adapters
- **PowerA, PDP, Afterglow:** Various Xbox/PC controllers in HID mode
- **GameSir:** G3, G4, T3, T4, G7 Pro, etc.
- **Razer:** Kishi, Hydra, Serval, Raiju (PC/HID)
- **Steam:** Steam Controller, Steam Deck (when presenting as gamepad)
- **Sony:** DualShock 2 (via USB adapter), Steam Virtual Gamepad
- **Flydigi:** Vader 4 Pro (DInput mode)
- **Scuf:** Envision (Linux/HID)
- **Elecom:** Various gamepads and adapters
- **Other:** ThrustMaster, BigBen, Capcom Home Arcade, Cthulhu, XinMo, Zenaim, Datel, and other generic DInput/HID gamepads and arcade sticks

*See `DINPUT_IDS` in `HardwareIDs.h` for the full VID/PID list. This list is large and includes many fightsticks and third‑party pads.*

---

### Generic HID (unspecified mapping)

- Other HID gamepad‑like devices may work; button and axis mappings might need to be adjusted in the [web app](https://megacadedev.github.io/OGX-Mini-2026-WebApp/).

---

## Reference

- **VID/PID lists:** `Firmware/RP2040/src/USBHost/HardwareIDs.h`
- **Host drivers:** `Firmware/RP2040/src/USBHost/HostDriver/`
- **Platform selection:** See the main [README](../../../README.md) for button combos to change output platform.
