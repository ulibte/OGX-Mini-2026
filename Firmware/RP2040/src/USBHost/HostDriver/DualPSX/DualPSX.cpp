#include <cstring>

#include "host/usbh.h"
#include "class/hid/hid_host.h"

#include "USBHost/HostDriver/DualPSX/DualPSX.h"

void DualPSXHost::initialize(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report_desc, uint16_t desc_len) 
{
    tuh_hid_receive_report(address, instance);
}

void DualPSXHost::process_report(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report, uint16_t len)
{
    const DualPSX::InReport* in_report = reinterpret_cast<const DualPSX::InReport*>(report);

    if (in_report->report_id != 1)
    {
        tuh_hid_receive_report(address, instance);
        return;
    }

    if (std::memcmp(&prev_report_, in_report, sizeof(DualPSX::InReport)) == 0)
    {
        tuh_hid_receive_report(address, instance);
        return;
    }

    Gamepad::PadIn gp_in;

    // D-Pad (Byte 5 low nibble)
    switch (in_report->buttons1 & DualPSX::DPad::MASK)
    {
        case DualPSX::DPad::UP:
            gp_in.dpad |= gamepad.MAP_DPAD_UP;
            break;
        case DualPSX::DPad::DOWN:
            gp_in.dpad |= gamepad.MAP_DPAD_DOWN;
            break;
        case DualPSX::DPad::LEFT:
            gp_in.dpad |= gamepad.MAP_DPAD_LEFT;
            break;
        case DualPSX::DPad::RIGHT: 
            gp_in.dpad |= gamepad.MAP_DPAD_RIGHT;
            break;
        case DualPSX::DPad::UP_RIGHT:
            gp_in.dpad |= gamepad.MAP_DPAD_UP_RIGHT;
            break;
        case DualPSX::DPad::DOWN_RIGHT:
            gp_in.dpad |= gamepad.MAP_DPAD_DOWN_RIGHT;
            break;
        case DualPSX::DPad::DOWN_LEFT:
            gp_in.dpad |= gamepad.MAP_DPAD_DOWN_LEFT;
            break;
        case DualPSX::DPad::UP_LEFT:
            gp_in.dpad |= gamepad.MAP_DPAD_UP_LEFT;
            break;
        default:
            break;
    }

    // Face buttons
    //if ( 1001 0000 & 1000 0000) true
    //if ( 0001 0000 & 1000 0000) false
    if (in_report->buttons1 & DualPSX::Buttons1::SQUARE) gp_in.buttons |= gamepad.MAP_BUTTON_X;
    if (in_report->buttons1 & DualPSX::Buttons1::CROSS) gp_in.buttons |= gamepad.MAP_BUTTON_A;
    if (in_report->buttons1 & DualPSX::Buttons1::CIRCLE) gp_in.buttons |= gamepad.MAP_BUTTON_B;
    if (in_report->buttons1 & DualPSX::Buttons1::TRIANGLE) gp_in.buttons |= gamepad.MAP_BUTTON_Y;

    // Frontal buttons
    if (in_report->buttons2 & DualPSX::Buttons2::L1) gp_in.buttons |= gamepad.MAP_BUTTON_LB;
    if (in_report->buttons2 & DualPSX::Buttons2::R1) gp_in.buttons |= gamepad.MAP_BUTTON_RB;

    // Start, Select, L3, R3
    if (in_report->buttons2 & DualPSX::Buttons2::SELECT) gp_in.buttons |= gamepad.MAP_BUTTON_BACK;
    if (in_report->buttons2 & DualPSX::Buttons2::START) gp_in.buttons |= gamepad.MAP_BUTTON_START;
    if (in_report->buttons2 & DualPSX::Buttons2::L3) gp_in.buttons |= gamepad.MAP_BUTTON_L3;
    if (in_report->buttons2 & DualPSX::Buttons2::R3) gp_in.buttons |= gamepad.MAP_BUTTON_R3;

    // Analog sticks
    std::tie(gp_in.joystick_lx, gp_in.joystick_ly) = gamepad.scale_joystick_l(in_report->lx, in_report->ly);
    std::tie(gp_in.joystick_rx, gp_in.joystick_ry) = gamepad.scale_joystick_r(in_report->rx, in_report->ry);

    gp_in.trigger_l = (in_report->buttons2 & DualPSX::Buttons2::L2) ? 0xFF : 0x00;
    gp_in.trigger_r = (in_report->buttons2 & DualPSX::Buttons2::R2) ? 0xFF : 0x00;

    gamepad.set_pad_in(gp_in);

    tuh_hid_receive_report(address, instance);
    std::memcpy(&prev_report_, in_report, sizeof(DualPSX::InReport));
}

bool DualPSXHost::send_feedback(Gamepad& gamepad, uint8_t address, uint8_t instance)
{
    return true;
}
