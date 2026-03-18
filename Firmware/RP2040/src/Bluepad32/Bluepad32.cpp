#include <atomic>
#include <cstdio>
#include <cstring>
#include <functional>
#include <pico/mutex.h>
#include <pico/cyw43_arch.h>
#include <pico/time.h>

#include "btstack_run_loop.h"
#include "gap.h"
#include "uni.h"
#include "bt/uni_bt.h"
#include "bt/uni_bt_bredr.h"
#include "parser/uni_hid_parser_ds5.h"
#include "parser/uni_hid_parser_xboxone.h"

#include "sdkconfig.h"
#include "Bluepad32/Bluepad32.h"
#include "Board/board_api.h"
#include "Board/ogxm_log.h"

#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
    #error "Pico W must use BLUEPAD32_PLATFORM_CUSTOM"
#endif

static_assert((CONFIG_BLUEPAD32_MAX_DEVICES == MAX_GAMEPADS), "Mismatch between BP32 and Gamepad max devices");

namespace bluepad32 {

static constexpr uint32_t FEEDBACK_TIME_MS = 250;
static constexpr uint32_t LED_CHECK_TIME_MS = 500;
/** If no HID input report reaches us for this long while "connected", the BT link is zombie
 *  (L2CAP stops delivering; OG Xbox then holds last USB report). Force disconnect so user can reconnect. */
static constexpr uint32_t BT_INPUT_STALL_DISCONNECT_MS = 8000;
/** BLE Xbox: host→pad output while idle (no rumble) or controller sleeps link ~1 min */
static constexpr uint32_t XBOX_BLE_KEEPALIVE_MS = 12000;

static uint32_t s_last_bt_input_ms[MAX_GAMEPADS]{};
static uint32_t s_xbox_ble_ka_last_ms[MAX_GAMEPADS]{};
/** Ignore Start+Select disconnect combo for this long after connect (DS4 can glitch both on first reports). */
static uint32_t s_bt_disconnect_combo_grace_until_ms[MAX_GAMEPADS]{};
/** DS4 BT: delay rumble output (host can request rumble immediately; early FF reports can drop link). */
static uint32_t s_ps4_rumble_ok_ms[MAX_GAMEPADS]{};

struct BTDevice {
    bool connected{false};
    Gamepad* gamepad{nullptr};
};

BTDevice bt_devices_[MAX_GAMEPADS];
btstack_timer_source_t feedback_timer_;
btstack_timer_source_t led_timer_;
bool led_timer_set_{false};
bool feedback_timer_set_{false};

static constexpr uint32_t GPIO_PROCESS_INTERVAL_MS = 4;
static btstack_timer_source_t gpio_process_timer_;
static void (*gpio_process_cb_)(void*) = nullptr;
static void* gpio_process_ctx_ = nullptr;

static void gpio_process_timer_cb(btstack_timer_source_t* ts) {
    if (gpio_process_cb_ != nullptr && gpio_process_ctx_ != nullptr) {
        gpio_process_cb_(gpio_process_ctx_);
    }
    btstack_run_loop_set_timer(ts, GPIO_PROCESS_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

// PS5: touchpad click toggles adaptive triggers (per-controller state)
static bool adaptive_trigger_enabled_[MAX_GAMEPADS]{false};
static bool prev_touchpad_clicked_[MAX_GAMEPADS]{false};
// Defer sending adaptive trigger effect out of BT callback to avoid l2cap_send in callback (reduces input lag)
static bool pending_adaptive_trigger_send_[MAX_GAMEPADS]{false};

bool any_connected()
{
    for (auto& device : bt_devices_)
    {
        if (device.connected)
        {
            return true;
        }
    }
    return false;
}

bool is_wii_controller_connected(uint8_t idx)
{
    if (idx >= MAX_GAMEPADS) {
        return false;
    }
    uni_hid_device_t* bp_device = uni_hid_device_get_instance_for_idx(idx);
    return (bp_device != nullptr && bp_device->controller_type == CONTROLLER_TYPE_WiiController);
}

bool any_wii_controller_connected()
{
    for (uint8_t i = 0; i < MAX_GAMEPADS; ++i)
    {
        if (bt_devices_[i].connected && is_wii_controller_connected(i))
        {
            return true;
        }
    }
    return false;
}

//This solves a function pointer/crash issue with bluepad32
void set_rumble(uni_hid_device_t* bp_device, uint16_t length, uint8_t rumble_l, uint8_t rumble_r)
{
    switch (bp_device->controller_type)
    {
        case CONTROLLER_TYPE_XBoxOneController:
            uni_hid_parser_xboxone_play_dual_rumble(bp_device, 0, length + 10, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_AndroidController:
            if (bp_device->vendor_id == UNI_HID_PARSER_STADIA_VID && bp_device->product_id == UNI_HID_PARSER_STADIA_PID) 
            {
                uni_hid_parser_stadia_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            }
            break;
        case CONTROLLER_TYPE_PSMoveController:
            uni_hid_parser_psmove_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_PS3Controller:
            uni_hid_parser_ds3_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_PS4Controller:
            uni_hid_parser_ds4_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_PS5Controller:
            uni_hid_parser_ds5_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_WiiController:
            uni_hid_parser_wii_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_SwitchProController:
        case CONTROLLER_TYPE_SwitchJoyConRight:
        case CONTROLLER_TYPE_SwitchJoyConLeft:
            uni_hid_parser_switch_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            break;
        default:
            break;
    }
}

static void send_feedback_cb(btstack_timer_source *ts)
{
    uni_hid_device_t* bp_device = nullptr;
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    for (uint8_t i = 0; i < MAX_GAMEPADS; ++i)
    {
        if (!bt_devices_[i].connected ||
            !(bp_device = uni_hid_device_get_instance_for_idx(i)))
        {
            continue;
        }
        /* Virtual slot (e.g. DS4's BT "mouse"): never gets gamepad HID → would always stall-disconnect
         * and drop the real controller. */
        if (uni_hid_device_is_virtual_device(bp_device))
            goto after_stall_check;
        /* BLE Xbox (Series) often sends HID only on change — idle sticks look like "stall" and we
         * would disconnect every few seconds. BR/EDR Xbox (1708) polls constantly; stall OK there. */
        if (!(bp_device->controller_type == CONTROLLER_TYPE_XBoxOneController && bp_device->hids_cid != 0))
        {
        if (s_last_bt_input_ms[i] != 0 && (now_ms - s_last_bt_input_ms[i]) > BT_INPUT_STALL_DISCONNECT_MS)
        {
            printf("[Bluepad32] BT input stalled (%u ms); forcing disconnect (slot %u)\n",
                   static_cast<unsigned>(now_ms - s_last_bt_input_ms[i]), static_cast<unsigned>(i));
            uni_hid_device_disconnect(bp_device);
            continue;
        }
        }
    after_stall_check:

        Gamepad::PadOut gp_out = bt_devices_[i].gamepad->get_pad_out();
        if (bp_device->controller_type == CONTROLLER_TYPE_XBoxOneController && bp_device->hids_cid != 0 &&
            gp_out.rumble_l == 0 && gp_out.rumble_r == 0)
        {
            const uint32_t last_ka = s_xbox_ble_ka_last_ms[i];
            if (last_ka == 0u || (now_ms - last_ka) >= XBOX_BLE_KEEPALIVE_MS)
            {
                uni_hid_parser_xboxone_ble_keepalive(bp_device);
                s_xbox_ble_ka_last_ms[i] = now_ms;
            }
        }
        if (gp_out.rumble_l > 0 || gp_out.rumble_r > 0)
        {
            if (bp_device->controller_type == CONTROLLER_TYPE_PS4Controller &&
                now_ms < s_ps4_rumble_ok_ms[i])
                ;
            else
                set_rumble(bp_device, static_cast<uint16_t>(FEEDBACK_TIME_MS), gp_out.rumble_l, gp_out.rumble_r);
        }
    }

    btstack_run_loop_set_timer(ts, FEEDBACK_TIME_MS);
    btstack_run_loop_add_timer(ts);
}

static void check_led_cb(btstack_timer_source *ts)
{
    static bool led_state = false;

    led_state = !led_state;

    board_api::set_led(any_connected() ? true : led_state);

    btstack_run_loop_set_timer(ts, LED_CHECK_TIME_MS);
    btstack_run_loop_add_timer(ts);
}

//BT Driver

static void init(int argc, const char** arg_V) {
}

static void init_complete_cb(void) {
    // Faster pairing: more aggressive GAP inquiry/periodic (units: 1.28s).
    // Defaults are inquiry=3, max=5, min=4; slightly tighter so we discover and reconnect sooner.
    uni_bt_set_gap_inquiry_length(2);
    uni_bt_set_gap_max_peridic_length(4);
    uni_bt_set_gap_min_peridic_length(3);

    uni_bt_enable_new_connections_unsafe(true);
    // uni_bt_del_keys_unsafe();
    uni_property_dump_all();
}

static uni_error_t device_discovered_cb(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    uint8_t minor = cod & UNI_BT_COD_MINOR_MASK;

    if (!(minor & (UNI_BT_COD_MINOR_GAMEPAD |
                   UNI_BT_COD_MINOR_JOYSTICK |
                   UNI_BT_COD_MINOR_REMOTE_CONTROL))) {
        return UNI_ERROR_IGNORE_DEVICE;
    }

    return UNI_ERROR_SUCCESS;
}

static void device_connected_cb(uni_hid_device_t* device) {
}

/** CYW43: resume OGX BLE advertising when no Classic (BR/EDR) gamepad remains connected. */
static void ogxm_resume_ble_ads_if_no_acl_pad(int disconnected_idx) {
#if defined(CONFIG_TARGET_PICO_W)
    for (uint8_t i = 0; i < MAX_GAMEPADS; ++i) {
        if (i == static_cast<unsigned>(disconnected_idx))
            continue;
        if (!bt_devices_[i].connected)
            continue;
        uni_hid_device_t* d = uni_hid_device_get_instance_for_idx(static_cast<int>(i));
        if (!d || uni_bt_conn_get_state(&d->conn) != UNI_BT_CONN_STATE_DEVICE_READY)
            continue;
        if (gap_get_connection_type(d->conn.handle) == GAP_CONNECTION_ACL)
            return;
    }
    gap_advertisements_enable(1);
#endif
}

/** CYW43: periodic BR/EDR inquiry while a Classic ACL link is up can drop DS4/PS3 in ~1–2 s. */
static void maybe_restart_bredr_inquiry_after_disconnect(int disconnected_idx) {
#if defined(CONFIG_TARGET_PICO_W)
    if (!uni_bt_enable_new_connections_is_enabled())
        return;
    for (uint8_t i = 0; i < MAX_GAMEPADS; ++i) {
        if (i == static_cast<unsigned>(disconnected_idx))
            continue;
        if (!bt_devices_[i].connected)
            continue;
        uni_hid_device_t* d = uni_hid_device_get_instance_for_idx(static_cast<int>(i));
        if (!d || uni_bt_conn_get_state(&d->conn) != UNI_BT_CONN_STATE_DEVICE_READY)
            continue;
        if (gap_get_connection_type(d->conn.handle) == GAP_CONNECTION_ACL)
            return;
    }
    uni_bt_bredr_scan_start();
#endif
}

static void device_disconnected_cb(uni_hid_device_t* device) {
    int idx = uni_hid_device_get_idx_for_instance(device);
    if (idx >= MAX_GAMEPADS || idx < 0) {
        return;
    }

    bt_devices_[idx].connected = false;
    s_last_bt_input_ms[idx] = 0;
    s_xbox_ble_ka_last_ms[idx] = 0;
    s_bt_disconnect_combo_grace_until_ms[idx] = 0;
    s_ps4_rumble_ok_ms[idx] = 0;
    prev_touchpad_clicked_[idx] = false;
    pending_adaptive_trigger_send_[idx] = false;
    bt_devices_[idx].gamepad->reset_pad_in();

    if (!led_timer_set_ && !any_connected()) {
        led_timer_set_ = true;
        led_timer_.process = check_led_cb;
        led_timer_.context = nullptr;
        btstack_run_loop_set_timer(&led_timer_, LED_CHECK_TIME_MS);
        btstack_run_loop_add_timer(&led_timer_);
    }
    if (feedback_timer_set_ && !any_connected()) {
        feedback_timer_set_ = false;
        btstack_run_loop_remove_timer(&feedback_timer_);
    }
    maybe_restart_bredr_inquiry_after_disconnect(idx);
    ogxm_resume_ble_ads_if_no_acl_pad(idx);
    // Re-enable scanning when last device disconnects so the controller can reconnect without
    // power-cycling the dongle (fixes "pairing mode but controller won't reconnect" in OG Xbox / BT mode).
    if (!any_connected()) {
        uni_bt_enable_new_connections_unsafe(true);
    }
}

static uni_error_t device_ready_cb(uni_hid_device_t* device) {
    /* DS4/DS5 BT create a second "virtual mouse" device on the same ACL. OGX-Mini only uses
     * gamepad input; accepting the virtual slot destabilized the link (disconnect ~2 s). */
    if (uni_hid_device_is_virtual_device(device))
        return UNI_ERROR_INVALID_CONTROLLER;

    int idx = uni_hid_device_get_idx_for_instance(device);
    if (idx >= MAX_GAMEPADS || idx < 0) {
        return UNI_ERROR_SUCCESS;
    }

    bt_devices_[idx].connected = true;
#if defined(CONFIG_TARGET_PICO_W)
    if (gap_get_connection_type(device->conn.handle) == GAP_CONNECTION_ACL) {
        uni_bt_bredr_scan_stop();
        gap_advertisements_enable(0);
    }
#endif
    const uint32_t tnow = to_ms_since_boot(get_absolute_time());
    s_last_bt_input_ms[idx] = tnow;
    s_bt_disconnect_combo_grace_until_ms[idx] = tnow + 3500u;
    if (device->controller_type == CONTROLLER_TYPE_PS4Controller)
        s_ps4_rumble_ok_ms[idx] = tnow + 6000u;
    /* Xbox BLE: 0 = send keepalive on next feedback tick (wakes Series/SW2 pad link immediately). */
    if (device->controller_type == CONTROLLER_TYPE_XBoxOneController && device->hids_cid != 0)
        s_xbox_ble_ka_last_ms[idx] = 0;

    // Set controller player LED to match slot (e.g. Wii U: LED 1 = player 1, LED 2 = player 2).
    // Same as Bluepad32 NINA platform: set_player_leds(d, BIT(idx)).
    if (device->report_parser.set_player_leds != nullptr) {
        device->report_parser.set_player_leds(device, static_cast<uint8_t>(1u << idx));
    }

    // PS5: start with adaptive triggers off; touchpad click toggles them.
    if (device->controller_type == CONTROLLER_TYPE_PS5Controller) {
        adaptive_trigger_enabled_[idx] = false;
        ds5_adaptive_trigger_effect_t off = ds5_new_adaptive_trigger_effect_off();
        ds5_set_adaptive_trigger_effect(device, UNI_ADAPTIVE_TRIGGER_TYPE_LEFT, &off);
        ds5_set_adaptive_trigger_effect(device, UNI_ADAPTIVE_TRIGGER_TYPE_RIGHT, &off);
    }

    if (led_timer_set_) {
        led_timer_set_ = false;
        btstack_run_loop_remove_timer(&led_timer_);
        board_api::set_led(true);
    }
    if (!feedback_timer_set_) {
        feedback_timer_set_ = true;
        feedback_timer_.process = send_feedback_cb;
        feedback_timer_.context = nullptr;
        btstack_run_loop_set_timer(&feedback_timer_, FEEDBACK_TIME_MS);
        btstack_run_loop_add_timer(&feedback_timer_);
    }
    return UNI_ERROR_SUCCESS;
}

static void oob_event_cb(uni_platform_oob_event_t event, void* data) {
	return;
}

// Set to 1 to print all Bluepad32 controller inputs to UART (only when state changes)
#ifndef BLUEPAD32_UART_LOG_INPUT
#define BLUEPAD32_UART_LOG_INPUT 0
#endif

static void controller_data_cb(uni_hid_device_t* device, uni_controller_t* controller) {
    static uni_gamepad_t prev_uni_gp[MAX_GAMEPADS] = {};

    if (controller->klass != UNI_CONTROLLER_CLASS_GAMEPAD){
        return;
    }

    uni_gamepad_t *uni_gp = &controller->gamepad;
    int idx = uni_hid_device_get_idx_for_instance(device);
    if (idx >= 0 && idx < static_cast<int>(MAX_GAMEPADS))
        s_last_bt_input_ms[static_cast<unsigned>(idx)] = to_ms_since_boot(get_absolute_time());

#if BLUEPAD32_UART_LOG_INPUT
    {
        bool changed = std::memcmp(uni_gp, &prev_uni_gp[idx], sizeof(uni_gamepad_t)) != 0;
        if (changed) {
            printf("[BP32 idx=%d] dpad=0x%02x btns=0x%04x misc=0x%02x brake=%u throttle=%u "
                   "Lx=%d Ly=%d Rx=%d Ry=%d\n",
                   idx, (unsigned)uni_gp->dpad, (unsigned)uni_gp->buttons, (unsigned)uni_gp->misc_buttons,
                   (unsigned)uni_gp->brake, (unsigned)uni_gp->throttle,
                   (int)uni_gp->axis_x, (int)uni_gp->axis_y, (int)uni_gp->axis_rx, (int)uni_gp->axis_ry);
        }
    }
#endif

    Gamepad* gamepad = bt_devices_[idx].gamepad;
    Gamepad::PadIn gp_in;

    switch (uni_gp->dpad) 
    {
        case DPAD_UP:
            gp_in.dpad = gamepad->MAP_DPAD_UP;
            break;
        case DPAD_DOWN:
            gp_in.dpad = gamepad->MAP_DPAD_DOWN;
            break;
        case DPAD_LEFT:
            gp_in.dpad = gamepad->MAP_DPAD_LEFT;
            break;
        case DPAD_RIGHT:
            gp_in.dpad = gamepad->MAP_DPAD_RIGHT;
            break;
        case DPAD_UP | DPAD_RIGHT:
            gp_in.dpad = gamepad->MAP_DPAD_UP_RIGHT;
            break;
        case DPAD_DOWN | DPAD_RIGHT:
            gp_in.dpad = gamepad->MAP_DPAD_DOWN_RIGHT;
            break;
        case DPAD_DOWN | DPAD_LEFT:
            gp_in.dpad = gamepad->MAP_DPAD_DOWN_LEFT;
            break;
        case DPAD_UP | DPAD_LEFT:
            gp_in.dpad = gamepad->MAP_DPAD_UP_LEFT;
            break;
        default:
            break;
    }

    if (is_wii_controller_connected(idx)) {
        if (uni_gp->buttons & BUTTON_A) gp_in.buttons |= gamepad->MAP_BUTTON_A;
        if (uni_gp->buttons & BUTTON_B) gp_in.buttons |= gamepad->MAP_BUTTON_B;
        if (uni_gp->buttons & BUTTON_X) gp_in.buttons |= gamepad->MAP_BUTTON_X;
        if (uni_gp->buttons & BUTTON_Y) gp_in.buttons |= gamepad->MAP_BUTTON_Y;
        if (uni_gp->buttons & BUTTON_SHOULDER_L) gp_in.buttons |= gamepad->MAP_BUTTON_LB;
        if (uni_gp->buttons & BUTTON_SHOULDER_R) gp_in.buttons |= gamepad->MAP_BUTTON_RB;
        //if (uni_gp->buttons & BUTTON_THUMB_L)    gp_in.buttons |= gamepad->MAP_BUTTON_L3;  
        //if (uni_gp->buttons & BUTTON_THUMB_R)    gp_in.buttons |= gamepad->MAP_BUTTON_R3;
        if (uni_gp->misc_buttons & MISC_BUTTON_BACK)    gp_in.buttons |= gamepad->MAP_BUTTON_BACK;
        if (uni_gp->misc_buttons & MISC_BUTTON_START)   gp_in.buttons |= gamepad->MAP_BUTTON_START;
        if (uni_gp->misc_buttons & MISC_BUTTON_SYSTEM)  gp_in.buttons |= gamepad->MAP_BUTTON_SYS;
    }
    else {
        if (uni_gp->buttons & BUTTON_A) gp_in.buttons |= gamepad->MAP_BUTTON_A;
        if (uni_gp->buttons & BUTTON_B) gp_in.buttons |= gamepad->MAP_BUTTON_B;
        if (uni_gp->buttons & BUTTON_X) gp_in.buttons |= gamepad->MAP_BUTTON_X;
        if (uni_gp->buttons & BUTTON_Y) gp_in.buttons |= gamepad->MAP_BUTTON_Y;
        if (uni_gp->buttons & BUTTON_SHOULDER_L) gp_in.buttons |= gamepad->MAP_BUTTON_LB;
        if (uni_gp->buttons & BUTTON_SHOULDER_R) gp_in.buttons |= gamepad->MAP_BUTTON_RB;
        if (uni_gp->buttons & BUTTON_THUMB_L)    gp_in.buttons |= gamepad->MAP_BUTTON_L3;  
        if (uni_gp->buttons & BUTTON_THUMB_R)    gp_in.buttons |= gamepad->MAP_BUTTON_R3;
        if (uni_gp->misc_buttons & MISC_BUTTON_BACK)    gp_in.buttons |= gamepad->MAP_BUTTON_BACK;
        if (uni_gp->misc_buttons & MISC_BUTTON_START)   gp_in.buttons |= gamepad->MAP_BUTTON_START;
        if (uni_gp->misc_buttons & MISC_BUTTON_SYSTEM)  gp_in.buttons |= gamepad->MAP_BUTTON_SYS; 
    }

    // Check for disconnect combo: Start+Select for most controllers, L3+R3 for OUYA (no Start/Select)
    static uint32_t disconnect_combo_hold_time[MAX_GAMEPADS] = {0};
    const uint32_t now_cb = to_ms_since_boot(get_absolute_time());
    const bool combo_grace =
        (idx >= 0 && idx < MAX_GAMEPADS && now_cb < s_bt_disconnect_combo_grace_until_ms[idx]);
    bool is_ouya = (device->controller_type == CONTROLLER_TYPE_OUYAController);
    bool combo_pressed = is_ouya
        ? ((uni_gp->buttons & BUTTON_THUMB_L) && (uni_gp->buttons & BUTTON_THUMB_R))
        : ((uni_gp->misc_buttons & MISC_BUTTON_START) && (uni_gp->misc_buttons & MISC_BUTTON_BACK));

    if (combo_grace) {
        disconnect_combo_hold_time[idx] = 0;
    } else if (combo_pressed) {
        disconnect_combo_hold_time[idx]++;
        // Require combo to be held for ~500ms (assuming ~60Hz callback rate, ~30 frames)
        if (disconnect_combo_hold_time[idx] >= 30) {
            printf("[BP32] Disconnect combo detected, disconnecting controller %d\n", idx);
            uni_hid_device_disconnect(device);
            disconnect_combo_hold_time[idx] = 0;
            return; // Don't process further input after disconnect
        }
    } else {
        disconnect_combo_hold_time[idx] = 0;
    }

    // Prefer analog triggers (brake / throttle) when present, but fall back to
    // digital trigger buttons (e.g. Wii U LT / RT) when analog value is zero.
    // For Wii controllers: Z button (shoulder) reports brake/throttle, but we only want it to map to LB/RB, not triggers
    // So skip trigger mapping when shoulder buttons are pressed on Wii controllers
    bool wii_shoulder_pressed = is_wii_controller_connected(idx) && 
                                 ((uni_gp->buttons & BUTTON_SHOULDER_L) || (uni_gp->buttons & BUTTON_SHOULDER_R));
    
    if (!wii_shoulder_pressed) {
        gp_in.trigger_l = gamepad->scale_trigger_l<10>(static_cast<uint16_t>(uni_gp->brake));
        gp_in.trigger_r = gamepad->scale_trigger_r<10>(static_cast<uint16_t>(uni_gp->throttle));

        if (gp_in.trigger_l == 0 && (uni_gp->buttons & BUTTON_TRIGGER_L)) {
            gp_in.trigger_l = 0xFF;
        }
        if (gp_in.trigger_r == 0 && (uni_gp->buttons & BUTTON_TRIGGER_R)) {
            gp_in.trigger_r = 0xFF;
        }
    }
    
    std::tie(gp_in.joystick_lx, gp_in.joystick_ly) = gamepad->scale_joystick_l<10>(uni_gp->axis_x, uni_gp->axis_y);
    std::tie(gp_in.joystick_rx, gp_in.joystick_ry) = gamepad->scale_joystick_r<10>(uni_gp->axis_rx, uni_gp->axis_ry);

    gamepad->set_pad_in_from_bluetooth(gp_in);

    // PS5: defer adaptive trigger send to main loop so callback never does l2cap_send (reduces input lag)
    if (device->controller_type == CONTROLLER_TYPE_PS5Controller && idx >= 0 && idx < static_cast<int>(MAX_GAMEPADS)) {
        bool touchpad_clicked = (uni_gp->misc_buttons & MISC_BUTTON_CAPTURE) != 0;
        if (touchpad_clicked && !prev_touchpad_clicked_[idx]) {
            adaptive_trigger_enabled_[idx] = !adaptive_trigger_enabled_[idx];
            pending_adaptive_trigger_send_[idx] = true;
        }
        prev_touchpad_clicked_[idx] = touchpad_clicked;
    }

#if BLUEPAD32_UART_LOG_INPUT
    if (idx >= 0 && idx < static_cast<int>(MAX_GAMEPADS)) {
        std::memcpy(&prev_uni_gp[idx], uni_gp, sizeof(uni_gamepad_t));
    }
#endif
}

const uni_property_t* get_property_cb(uni_property_idx_t idx) 
{
    return nullptr;
}

void process_pending_adaptive_triggers()
{
    for (uint8_t i = 0; i < MAX_GAMEPADS; ++i) {
        if (!pending_adaptive_trigger_send_[i])
            continue;
        pending_adaptive_trigger_send_[i] = false;
        uni_hid_device_t* device = uni_hid_device_get_instance_for_idx(static_cast<int>(i));
        if (!device || device->controller_type != CONTROLLER_TYPE_PS5Controller)
            continue;
        if (adaptive_trigger_enabled_[i]) {
            ds5_adaptive_trigger_effect_t on = ds5_new_adaptive_trigger_effect_feedback(5, 4);
            ds5_set_adaptive_trigger_effect(device, UNI_ADAPTIVE_TRIGGER_TYPE_LEFT, &on);
            ds5_set_adaptive_trigger_effect(device, UNI_ADAPTIVE_TRIGGER_TYPE_RIGHT, &on);
        } else {
            ds5_adaptive_trigger_effect_t off = ds5_new_adaptive_trigger_effect_off();
            ds5_set_adaptive_trigger_effect(device, UNI_ADAPTIVE_TRIGGER_TYPE_LEFT, &off);
            ds5_set_adaptive_trigger_effect(device, UNI_ADAPTIVE_TRIGGER_TYPE_RIGHT, &off);
        }
    }
}

uni_platform* get_driver() 
{
    static uni_platform driver = 
    {
        .name = "OGXMiniW",
        .init = init,
        .on_init_complete = init_complete_cb,
        .on_device_discovered = device_discovered_cb,
        .on_device_connected = device_connected_cb,
        .on_device_disconnected = device_disconnected_cb,
        .on_device_ready = device_ready_cb,
        .on_controller_data = controller_data_cb,
        .get_property = get_property_cb,
        .on_oob_event = oob_event_cb,
    };
    return &driver;
}

//Public API

void set_gpio_device_process_callback(void (*callback)(void* ctx), void* ctx) {
    gpio_process_cb_ = callback;
    gpio_process_ctx_ = ctx;
}

void init(Gamepad(&gamepads)[MAX_GAMEPADS])
{
    for (uint8_t i = 0; i < MAX_GAMEPADS; ++i)
    {
        bt_devices_[i].gamepad = &gamepads[i];
    }

    uni_platform_set_custom(get_driver());
    uni_init(0, nullptr);

    led_timer_set_ = true;
    led_timer_.process = check_led_cb;
    led_timer_.context = nullptr;
    btstack_run_loop_set_timer(&led_timer_, LED_CHECK_TIME_MS);
    btstack_run_loop_add_timer(&led_timer_);

    if (gpio_process_cb_ != nullptr) {
        gpio_process_timer_.process = gpio_process_timer_cb;
        gpio_process_timer_.context = nullptr;
        btstack_run_loop_set_timer(&gpio_process_timer_, GPIO_PROCESS_INTERVAL_MS);
        btstack_run_loop_add_timer(&gpio_process_timer_);
    }
}

void run_task(Gamepad(&gamepads)[MAX_GAMEPADS])
{
    init(gamepads);
    btstack_run_loop_execute();
}

} // namespace bluepad32 