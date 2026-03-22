#include "Descriptors/DualPSX.h"
#include "Gamepad/Gamepad.h"

// The DualShock 2 doesn't have analog triggers
// But it can be simulated by the state of the buttons
static constexpr uint8_t TRIGGER_MIN = 0;
static constexpr uint8_t TRIGGER_MAX = 255;

void DualPSX::parse_in_report(const DualPSX::InReport* in_report, Gamepad::PadIn* gp_in)
{
    // Handle dpad
    uint8_t dpad_mask = in_report->buttonsA & DualPSX::DPad::MASK;
    if (dpad_mask != 0x0F) // 0x0F is neutral
    {
        switch (dpad_mask)
        {
            case DualPSX::DPad::UP:         gp_in->dpad = Gamepad::DPAD_UP; break;
            case DualPSX::DPad::UP_RIGHT:   gp_in->dpad = Gamepad::DPAD_UP_RIGHT; break;
            case DualPSX::DPad::RIGHT:      gp_in->dpad = Gamepad::DPAD_RIGHT; break;
            case DualPSX::DPad::DOWN_RIGHT: gp_in->dpad = Gamepad::DPAD_DOWN_RIGHT; break;
            case DualPSX::DPad::DOWN:       gp_in->dpad = Gamepad::DPAD_DOWN; break;
            case DualPSX::DPad::DOWN_LEFT:  gp_in->dpad = Gamepad::DPAD_DOWN_LEFT; break;
            case DualPSX::DPad::LEFT:       gp_in->dpad = Gamepad::DPAD_LEFT; break;
            case DualPSX::DPad::UP_LEFT:    gp_in->dpad = Gamepad::DPAD_UP_LEFT; break;
        }
    }
    else
    {
        gp_in->dpad = Gamepad::DPAD_NONE;
    }

    // Handle buttons
    if (in_report->buttonsA & DualPSX::ButtonsA::SQUARE)   gp_in->buttons |= Gamepad::BUTTON_X;
    if (in_report->buttonsA & DualPSX::ButtonsA::CROSS)    gp_in->buttons |= Gamepad::BUTTON_A;
    if (in_report->buttonsA & DualPSX::ButtonsA::CIRCLE)   gp_in->buttons |= Gamepad::BUTTON_B;
    if (in_report->buttonsA & DualPSX::ButtonsA::TRIANGLE) gp_in->buttons |= Gamepad::BUTTON_Y;

    if (in_report->buttonsB & DualPSX::ButtonsB::L1) gp_in->buttons |= Gamepad::BUTTON_LB;
    if (in_report->buttonsB & DualPSX::ButtonsB::R1) gp_in->buttons |= Gamepad::BUTTON_RB;
    if (in_report->buttonsB & DualPSX::ButtonsB::L2) gp_in->trigger_l = TRIGGER_MAX; else gp_in->trigger_l = TRIGGER_MIN;
    if (in_report->buttonsB & DualPSX::ButtonsB::R2) gp_in->trigger_r = TRIGGER_MAX; else gp_in->trigger_r = TRIGGER_MIN;

    if (in_report->buttonsB & DualPSX::ButtonsB::SELECT) gp_in->buttons |= Gamepad::BUTTON_BACK;
    if (in_report->buttonsB & DualPSX::ButtonsB::START)  gp_in->buttons |= Gamepad::BUTTON_START;
    if (in_report->buttonsB & DualPSX::ButtonsB::L3)     gp_in->buttons |= Gamepad::BUTTON_L3;
    if (in_report->buttonsB & DualPSX::ButtonsB::R3)     gp_in->buttons |= Gamepad::BUTTON_R3;

    // Joysticks
    // The Gamepad class expects values from -32768 to 32767.
    // The DualPSX report gives 0-255.
    // We can scale it by recentering 0-255 to -128 to 127 and then multiplying.
    // A simple way is: (value - 128) * 257
    // Y-axis is inverted on PS2 controllers.
    gp_in->joystick_lx = (in_report->lx - 128) * 257;
    gp_in->joystick_ly = (255 - in_report->ly - 128) * 257;
    gp_in->joystick_rx = (in_report->rx - 128) * 257;
    gp_in->joystick_ry = (255 - in_report->ry - 128) * 257;
}
