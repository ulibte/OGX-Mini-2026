#pragma once

#include <cstdint>
#include <array>

#include "Gamepad/Gamepad.h"
#include "Board/Config.h"

/*  NOTE: Everything bluepad32/uni needs to be wrapped
    and kept away from tinyusb due to naming conflicts */

namespace bluepad32 {
    void run_task(Gamepad(&gamepads)[MAX_GAMEPADS]);
    void init(Gamepad(&gamepads)[MAX_GAMEPADS]);
    /** Call from main loop to send deferred PS5 adaptive-trigger updates (keeps BT callback fast). */
    void process_pending_adaptive_triggers();
    /** Optional: when set, a timer in the BT run loop calls this every ~4 ms (for GPIO device modes: PS1/PS2, GameCube). Call before run_task(). */
    void set_gpio_device_process_callback(void (*callback)(void* ctx), void* ctx);
} 