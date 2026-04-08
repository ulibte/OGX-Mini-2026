#include <cstring>

#include "host/usbh.h"
#include "class/hid/hid_host.h"

#include "USBHost/HostDriver/PS3/PS3.h"

#if defined(CONFIG_EN_BLUETOOTH)
extern "C" {
/* Do not include bt/uni_bt.h here: it pulls BTstack, whose btstack_hid.h redefines
 * HID_REPORT_TYPE_* / hid_report_type_t and breaks compilation with TinyUSB hid.h. */
typedef uint8_t uni_bt_bd_addr_t[6];
void uni_bt_get_local_bd_addr_safe(uni_bt_bd_addr_t addr);
}
#endif

const tusb_control_request_t PS3Host::RUMBLE_REQUEST = 
{
    .bmRequestType = 0x21,
    .bRequest = 0x09, // SET_REPORT
    .wValue = 0x0201,
    .wIndex = 0x0000, 
    .wLength = sizeof(PS3::OutReport)
};

#if defined(CONFIG_EN_BLUETOOTH)
/* Same as bluepad32/tools/sixaxispairer — DualShock 3 stores host BD_ADDR for Bluetooth. */
const tusb_control_request_t PS3Host::BT_MAC_FEATURE_REQUEST = {
    .bmRequestType = 0x21,
    .bRequest = 0x09, // SET_REPORT
    .wValue = static_cast<uint16_t>((HID_REPORT_TYPE_FEATURE << 8) | 0xF5),
    .wIndex = 0x0000,
    .wLength = 8,
};

bool PS3Host::local_bt_addr_is_nonzero(const uint8_t addr[6]) {
    for (int i = 0; i < 6; ++i) {
        if (addr[i] != 0) {
            return true;
        }
    }
    return false;
}

void PS3Host::bt_pair_complete_cb(tuh_xfer_s* xfer) {
    auto* init_state = reinterpret_cast<InitState*>(xfer->user_data);
    if (init_state == nullptr) {
        return;
    }
    init_state->stage = InitStage::DONE;
    init_state->reports_enabled = true;
    send_control_xfer(init_state->dev_addr, &PS3Host::RUMBLE_REQUEST, reinterpret_cast<uint8_t*>(init_state->out_report), nullptr, 0);
}

void PS3Host::try_deferred_ds3_bt_pair(uint8_t address) {
    if (!ds3_bt_pair_deferred_) {
        return;
    }
    uni_bt_bd_addr_t addr{};
    uni_bt_get_local_bd_addr_safe(addr);
    if (!local_bt_addr_is_nonzero(addr)) {
        return;
    }
    ds3_bt_pair_deferred_ = false;
    bt_pair_buf_[0] = 0xF5;
    bt_pair_buf_[1] = 0;
    std::memcpy(bt_pair_buf_.data() + 2, addr, 6);
    (void)send_control_xfer(address, &BT_MAC_FEATURE_REQUEST, bt_pair_buf_.data(), nullptr, 0);
}
#endif

void PS3Host::initialize(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report_desc, uint16_t desc_len) 
{
    gamepad.set_analog_host(true);
    
    std::memcpy(reinterpret_cast<uint8_t*>(&out_report_), 
                PS3::DEFAULT_OUT_REPORT, 
                std::min(sizeof(PS3::OutReport), sizeof(PS3::DEFAULT_OUT_REPORT)));

    out_report_.leds_bitmap = 0x1 << (idx_ + 1);
    out_report_.leds[idx_].time_enabled = 0xFF;

    init_state_.out_report = &out_report_;
    init_state_.dev_addr = address;
    init_state_.init_buffer.fill(0);
    init_state_.bt_pair_report.fill(0);
#if defined(CONFIG_EN_BLUETOOTH)
    init_state_.defer_bt_pair = &ds3_bt_pair_deferred_;
    ds3_bt_pair_deferred_ = false;
#endif

    tusb_control_request_t init_request =
    {
        .bmRequestType = 0xA1,
        .bRequest = 0x01, // GET_REPORT
        .wValue = (HID_REPORT_TYPE_FEATURE << 8) | 0xF2,
        .wIndex = 0x0000,
        .wLength = 17
    };

    send_control_xfer(address, &init_request, init_state_.init_buffer.data(), get_report_complete_cb, reinterpret_cast<uintptr_t>(&init_state_));

    tuh_hid_receive_report(address, instance);
}

bool PS3Host::send_control_xfer(uint8_t dev_addr, const tusb_control_request_t* request, uint8_t* buffer, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
    tuh_xfer_s transfer = 
    {
        .daddr = dev_addr,
        .ep_addr = 0x00,
        .setup = request, 
        .buffer = buffer,
        .complete_cb = complete_cb, 
        .user_data = user_data
    };
    return tuh_control_xfer(&transfer);
}

void PS3Host::get_report_complete_cb(tuh_xfer_s *xfer)
{
    InitState* init_state = reinterpret_cast<InitState*>(xfer->user_data);
    if (init_state == nullptr || init_state->stage == InitStage::DONE)
    {
        return;
    }

    tusb_control_request_t init_request =
    {
        .bmRequestType = 0xA1,
        .bRequest = 0x01, // GET_REPORT
        .wValue = (HID_REPORT_TYPE_FEATURE << 8) | 0xF2,
        .wIndex = 0x0000,
        .wLength = 17
    };

    switch (init_state->stage)
    {
        case InitStage::RESP1:
            init_state->stage = InitStage::RESP2;
            send_control_xfer(init_state->dev_addr, &init_request, init_state->init_buffer.data(), get_report_complete_cb, xfer->user_data);
            break;
        case InitStage::RESP2:
            init_state->stage = InitStage::RESP3;
            init_request.wLength = 8;
            send_control_xfer(init_state->dev_addr, &init_request, init_state->init_buffer.data(), get_report_complete_cb, xfer->user_data);
            break;
        case InitStage::RESP3:
#if defined(CONFIG_EN_BLUETOOTH)
        {
            uni_bt_bd_addr_t addr{};
            uni_bt_get_local_bd_addr_safe(addr);
            if (local_bt_addr_is_nonzero(addr)) {
                init_state->bt_pair_report[0] = 0xF5;
                init_state->bt_pair_report[1] = 0;
                std::memcpy(init_state->bt_pair_report.data() + 2, addr, 6);
                send_control_xfer(init_state->dev_addr, &BT_MAC_FEATURE_REQUEST, init_state->bt_pair_report.data(), bt_pair_complete_cb, xfer->user_data);
                break;
            }
            if (init_state->defer_bt_pair != nullptr) {
                *init_state->defer_bt_pair = true;
            }
        }
#endif
            init_state->stage = InitStage::DONE;
            init_state->reports_enabled = true;
            send_control_xfer(init_state->dev_addr, &PS3Host::RUMBLE_REQUEST, reinterpret_cast<uint8_t*>(init_state->out_report), nullptr, 0);
            break;
        default:
            break;
    }
}

void PS3Host::process_report(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report, uint16_t len)
{
#if defined(CONFIG_EN_BLUETOOTH)
    try_deferred_ds3_bt_pair(address);
#endif
    const PS3::InReport* in_report = reinterpret_cast<const PS3::InReport*>(report);
    if (std::memcmp(&prev_in_report_, in_report, std::min(static_cast<size_t>(len), static_cast<size_t>(26))) == 0)
    {
        tuh_hid_receive_report(address, instance);
        return;
    }

    Gamepad::PadIn gp_in;   

    if (in_report->buttons[0] & PS3::Buttons0::DPAD_UP)    gp_in.dpad |= gamepad.MAP_DPAD_UP;
    if (in_report->buttons[0] & PS3::Buttons0::DPAD_DOWN)  gp_in.dpad |= gamepad.MAP_DPAD_DOWN;
    if (in_report->buttons[0] & PS3::Buttons0::DPAD_LEFT)  gp_in.dpad |= gamepad.MAP_DPAD_LEFT;
    if (in_report->buttons[0] & PS3::Buttons0::DPAD_RIGHT) gp_in.dpad |= gamepad.MAP_DPAD_RIGHT;

    if (in_report->buttons[0] & PS3::Buttons0::SELECT)   gp_in.buttons |= gamepad.MAP_BUTTON_BACK;
    if (in_report->buttons[0] & PS3::Buttons0::START)    gp_in.buttons |= gamepad.MAP_BUTTON_START;
    if (in_report->buttons[0] & PS3::Buttons0::L3)       gp_in.buttons |= gamepad.MAP_BUTTON_L3;
    if (in_report->buttons[0] & PS3::Buttons0::R3)       gp_in.buttons |= gamepad.MAP_BUTTON_R3;
    if (in_report->buttons[1] & PS3::Buttons1::L1)       gp_in.buttons |= gamepad.MAP_BUTTON_LB;
    if (in_report->buttons[1] & PS3::Buttons1::R1)       gp_in.buttons |= gamepad.MAP_BUTTON_RB;
    if (in_report->buttons[1] & PS3::Buttons1::TRIANGLE) gp_in.buttons |= gamepad.MAP_BUTTON_Y;
    if (in_report->buttons[1] & PS3::Buttons1::CIRCLE)   gp_in.buttons |= gamepad.MAP_BUTTON_B;
    if (in_report->buttons[1] & PS3::Buttons1::CROSS)    gp_in.buttons |= gamepad.MAP_BUTTON_A;
    if (in_report->buttons[1] & PS3::Buttons1::SQUARE)   gp_in.buttons |= gamepad.MAP_BUTTON_X;
    if (in_report->buttons[2] & PS3::Buttons2::SYS)      gp_in.buttons |= gamepad.MAP_BUTTON_SYS;

    if (gamepad.analog_enabled())
    {
        gp_in.analog[gamepad.MAP_ANALOG_OFF_UP]    = in_report->up_axis;
        gp_in.analog[gamepad.MAP_ANALOG_OFF_DOWN]  = in_report->down_axis;
        gp_in.analog[gamepad.MAP_ANALOG_OFF_LEFT]  = in_report->left_axis;
        gp_in.analog[gamepad.MAP_ANALOG_OFF_RIGHT] = in_report->right_axis;
        gp_in.analog[gamepad.MAP_ANALOG_OFF_A]  = in_report->cross_axis;
        gp_in.analog[gamepad.MAP_ANALOG_OFF_B]  = in_report->circle_axis;
        gp_in.analog[gamepad.MAP_ANALOG_OFF_X]  = in_report->square_axis;
        gp_in.analog[gamepad.MAP_ANALOG_OFF_Y]  = in_report->triangle_axis;
        gp_in.analog[gamepad.MAP_ANALOG_OFF_LB] = in_report->l1_axis;
        gp_in.analog[gamepad.MAP_ANALOG_OFF_RB] = in_report->r1_axis;
    }

    gp_in.trigger_l = gamepad.scale_trigger_l(in_report->l2_axis);
    gp_in.trigger_r = gamepad.scale_trigger_r(in_report->r2_axis);

    std::tie(gp_in.joystick_lx, gp_in.joystick_ly) = gamepad.scale_joystick_l(in_report->joystick_lx, in_report->joystick_ly);
    std::tie(gp_in.joystick_rx, gp_in.joystick_ry) = gamepad.scale_joystick_r(in_report->joystick_rx, in_report->joystick_ry);

    gamepad.set_pad_in(gp_in);

    tuh_hid_receive_report(address, instance);
    std::memcpy(&prev_in_report_, in_report, sizeof(PS3::InReport));
}

bool PS3Host::send_feedback(Gamepad& gamepad, uint8_t address, uint8_t instance)
{
    static uint32_t last_rumble_ms = 0;

    uint32_t current_ms = time_us_32() / 1000;
    
    //Spamming set_report doesn't work, limit the rate
    if (init_state_.reports_enabled &&
        current_ms - last_rumble_ms >= 300)
    {
        Gamepad::PadOut gp_out = gamepad.get_pad_out();

        out_report_.rumble.right_duration    = (gp_out.rumble_r > 0) ? 20 : 0;
        out_report_.rumble.right_motor_on    = (gp_out.rumble_r > 0) ? 1  : 0;

        out_report_.rumble.left_duration     = (gp_out.rumble_l > 0) ? 20 : 0;
        out_report_.rumble.left_motor_force  = gp_out.rumble_l;

        last_rumble_ms = current_ms;

        return send_control_xfer(address, &PS3Host::RUMBLE_REQUEST, reinterpret_cast<uint8_t*>(&out_report_), nullptr, 0);
    }
    return true;
}