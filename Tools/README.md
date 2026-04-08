# Tools

## Controller mapping capture (new gamepad support)

See **[controller_capture/README.md](controller_capture/README.md)**. Run `controller_capture.py` on your PC to record button/axis mappings and VID/PID for maintainers when requesting support for a new controller.

## Dumping Xbox DVD dongle firmware

The firmware for the DVD Playback Kit is not included here, but you can dump your own or place a `.BIN` dump in this directory. Whichever you do, you'll have to run `dump-xremote-firmware.py` to have it included with the firmware when you compile it.