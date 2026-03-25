#include <cstring>
#include <array>

#include "host/usbh.h"
#include "class/hid/hid_host.h"
#include "common/tusb_types.h"

#include "USBHost/HostDriver/SwitchPro/SwitchPro.h"
#include "USBHost/HostDriver/SwitchPro/Switch2UsbInitPackets.h"

bool SwitchProHost::is_switch2_usb_family(uint16_t vid, uint16_t pid)
{
    if (vid != 0x057Eu)
    {
        return false;
    }
    switch (pid)
    {
    case 0x2066u:
    case 0x2067u:
    case 0x2069u:
    case 0x2073u:
        return true;
    default:
        return false;
    }
}

void SwitchProHost::initialize(Gamepad& gamepad, uint8_t address, uint8_t instance,
                              const uint8_t* report_desc, uint16_t desc_len)
{
    (void)report_desc;
    (void)desc_len;

    std::memset(&out_report_, 0, sizeof(out_report_));
    hid_instance_ = instance;
    switch2_dev_addr_ = address;
    switch2_gamepad_ = &gamepad;
    init_state_ = InitState::HANDSHAKE;

    uint16_t vid = 0;
    uint16_t pid = 0;
    tuh_vid_pid_get(address, &vid, &pid);

    if (is_switch2_usb_family(vid, pid))
    {
        switch2_bringup_active_ = true;
        switch2_pkt_idx_ = 0;
        switch2_ep_out_ = 0;
        switch2_ep_in_ = 0;
        switch2_start_config_read();
    }
    else
    {
        switch2_bringup_active_ = false;
        init_switch_host(gamepad, address, instance);
    }
}

void SwitchProHost::switch2_start_config_read()
{
    if (!tuh_descriptor_get_configuration(
            switch2_dev_addr_,
            0,
            switch2_cfg_buf_.data(),
            static_cast<uint16_t>(switch2_cfg_buf_.size()),
            switch2_cfg_cb,
            reinterpret_cast<uintptr_t>(this)))
    {
        switch2_finish_bringup();
    }
}

bool SwitchProHost::switch2_parse_open_iface1(const uint8_t* desc_cfg, uint16_t buflen)
{
    switch2_ep_out_ = 0;
    switch2_ep_in_ = 0;

    if (buflen < sizeof(tusb_desc_configuration_t))
    {
        return false;
    }

    auto const* cfg = reinterpret_cast<tusb_desc_configuration_t const*>(desc_cfg);
    uint16_t total = tu_le16toh(cfg->wTotalLength);
    uint16_t len = TU_MIN(total, buflen);
    uint8_t const* end = desc_cfg + len;
    uint8_t const* p = tu_desc_next(desc_cfg);

    while (p < end)
    {
        if (tu_desc_type(p) != TUSB_DESC_INTERFACE)
        {
            p = tu_desc_next(p);
            continue;
        }

        auto const* itf = reinterpret_cast<tusb_desc_interface_t const*>(p);
        if (itf->bInterfaceNumber != 1 || itf->bAlternateSetting != 0)
        {
            p = tu_desc_next(p);
            continue;
        }

        p = tu_desc_next(p);
        int endpoint = 0;
        while (endpoint < itf->bNumEndpoints && p < end)
        {
            if (tu_desc_type(p) != TUSB_DESC_ENDPOINT)
            {
                p = tu_desc_next(p);
                continue;
            }

            auto const* ep = reinterpret_cast<tusb_desc_endpoint_t const*>(p);
            if (ep->bmAttributes.xfer == TUSB_XFER_BULK)
            {
                if (!tuh_edpt_open(switch2_dev_addr_, ep))
                {
                    return false;
                }
                if (tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_OUT && switch2_ep_out_ == 0)
                {
                    switch2_ep_out_ = ep->bEndpointAddress;
                }
                else if (tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_IN && switch2_ep_in_ == 0)
                {
                    switch2_ep_in_ = ep->bEndpointAddress;
                }
            }
            endpoint++;
            p = tu_desc_next(p);
        }

        return switch2_ep_out_ != 0;
    }

    return false;
}

void SwitchProHost::switch2_cfg_cb(tuh_xfer_s* xfer)
{
    auto* self = reinterpret_cast<SwitchProHost*>(xfer->user_data);
    if (self == nullptr || !self->switch2_bringup_active_)
    {
        return;
    }

    if (xfer->result != XFER_RESULT_SUCCESS ||
        xfer->actual_len < sizeof(tusb_desc_configuration_t))
    {
        self->switch2_finish_bringup();
        return;
    }

    uint16_t actual = static_cast<uint16_t>(xfer->actual_len);
    if (!self->switch2_parse_open_iface1(self->switch2_cfg_buf_.data(), actual))
    {
        self->switch2_finish_bringup();
        return;
    }

    self->switch2_pkt_idx_ = 0;
    self->switch2_submit_out_packet();
}

void SwitchProHost::switch2_out_cb(tuh_xfer_s* xfer)
{
    auto* self = reinterpret_cast<SwitchProHost*>(xfer->user_data);
    if (self == nullptr || !self->switch2_bringup_active_)
    {
        return;
    }

    if (self->switch2_ep_in_ != 0)
    {
        self->switch2_submit_in_read();
    }
    else
    {
        self->switch2_after_packet_round();
    }
}

void SwitchProHost::switch2_in_cb(tuh_xfer_s* xfer)
{
    auto* self = reinterpret_cast<SwitchProHost*>(xfer->user_data);
    if (self == nullptr || !self->switch2_bringup_active_)
    {
        return;
    }
    (void)xfer;
    self->switch2_after_packet_round();
}

void SwitchProHost::switch2_submit_out_packet()
{
    const Switch2UsbBulkPacket& pkt = SWITCH2_USB_BULK_PACKETS[switch2_pkt_idx_];
    if (pkt.len > switch2_bulk_out_buf_.size())
    {
        switch2_finish_bringup();
        return;
    }

    std::memcpy(switch2_bulk_out_buf_.data(), pkt.data, pkt.len);

    tuh_xfer_t xfer{};
    xfer.daddr = switch2_dev_addr_;
    xfer.ep_addr = switch2_ep_out_;
    xfer.buflen = pkt.len;
    xfer.buffer = switch2_bulk_out_buf_.data();
    xfer.complete_cb = switch2_out_cb;
    xfer.user_data = reinterpret_cast<uintptr_t>(this);

    if (!tuh_edpt_xfer(&xfer))
    {
        switch2_after_packet_round();
    }
}

void SwitchProHost::switch2_submit_in_read()
{
    tuh_xfer_t xfer{};
    xfer.daddr = switch2_dev_addr_;
    xfer.ep_addr = switch2_ep_in_;
    xfer.buflen = 64;
    xfer.buffer = switch2_bulk_in_buf_.data();
    xfer.complete_cb = switch2_in_cb;
    xfer.user_data = reinterpret_cast<uintptr_t>(this);

    if (!tuh_edpt_xfer(&xfer))
    {
        switch2_after_packet_round();
    }
}

void SwitchProHost::switch2_after_packet_round()
{
    switch2_pkt_idx_++;
    if (switch2_pkt_idx_ >= SWITCH2_USB_BULK_PACKET_COUNT)
    {
        switch2_finish_bringup();
        return;
    }
    switch2_submit_out_packet();
}

void SwitchProHost::switch2_finish_bringup()
{
    switch2_bringup_active_ = false;
    if (switch2_gamepad_ != nullptr)
    {
        init_switch_host(*switch2_gamepad_, switch2_dev_addr_, hid_instance_);
    }
}

uint8_t SwitchProHost::get_output_sequence_counter()
{
    uint8_t counter = sequence_counter_;
    sequence_counter_ = (sequence_counter_ + 1) & 0x0F;
    return counter;
}

// The other way is to write a class driver just for switch pro, we'll see if there are issues with this
void SwitchProHost::init_switch_host(Gamepad& gamepad, uint8_t address, uint8_t instance)
{
    // See: https://github.com/Dan611/hid-procon
    //      https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering
    //      https://github.com/HisashiKato/USB_Host_Shield_Library_2.0

    std::memset(&out_report_, 0, sizeof(out_report_));

    uint8_t report_size = 10;

    out_report_.command = SwitchPro::CMD::RUMBLE_ONLY;
    out_report_.sequence_counter = get_output_sequence_counter();

    out_report_.rumble_l[0] = 0x00;
    out_report_.rumble_l[1] = 0x01;
    out_report_.rumble_l[2] = 0x40;
    out_report_.rumble_l[3] = 0x40;

    out_report_.rumble_r[0] = 0x00;
    out_report_.rumble_r[1] = 0x01;
    out_report_.rumble_r[2] = 0x40;
    out_report_.rumble_r[3] = 0x40;

    switch (init_state_)
    {
        case InitState::HANDSHAKE:
            report_size = 2;

            out_report_.command = SwitchPro::CMD::HID;
            out_report_.sequence_counter = SwitchPro::CMD::HANDSHAKE;

            if (tuh_hid_send_report(address, instance, 0, &out_report_, report_size))
            {
                init_state_ = InitState::TIMEOUT;
            }
            break;
        case InitState::TIMEOUT:
            report_size = 2;

            out_report_.command = SwitchPro::CMD::HID;
            out_report_.sequence_counter = SwitchPro::CMD::DISABLE_TIMEOUT;

            if (tuh_hid_send_report(address, instance, 0, &out_report_, report_size))
            {
                init_state_ = InitState::LED;
            }
            break;
        case InitState::LED:
            report_size = 12;

            out_report_.command = SwitchPro::CMD::AND_RUMBLE;
            out_report_.sub_command = SwitchPro::CMD::LED;
            out_report_.sub_command_args[0] = idx_ + 1;

            if (tuh_hid_send_report(address, instance, 0, &out_report_, report_size))
            {
                init_state_ = InitState::LED_HOME;
            }
            break;
        case InitState::LED_HOME:
            report_size = 14;

            out_report_.command = SwitchPro::CMD::AND_RUMBLE;
            out_report_.sub_command = SwitchPro::CMD::LED_HOME;
            out_report_.sub_command_args[0] = (0 /* Number of cycles */ << 4) | (true ? 0xF : 0);
            out_report_.sub_command_args[1] = (0xF /* LED start intensity */ << 4) | 0x0 /* Number of full cycles */;
            out_report_.sub_command_args[2] = (0xF /* Mini Cycle 1 LED intensity */ << 4) | 0x0 /* Mini Cycle 2 LED intensity */;

            if (tuh_hid_send_report(address, instance, 0, &out_report_, report_size))
            {
                init_state_ = InitState::FULL_REPORT;
            }
            break;
        case InitState::FULL_REPORT:
            report_size = 12;

            out_report_.command = SwitchPro::CMD::AND_RUMBLE;
            out_report_.sub_command = SwitchPro::CMD::MODE;
            out_report_.sub_command_args[0] = SwitchPro::CMD::FULL_REPORT_MODE;

            if (tuh_hid_send_report(address, instance, 0, &out_report_, report_size))
            {
                init_state_ = InitState::IMU;
            }
            break;
        case InitState::IMU:
            report_size = 12;

            out_report_.command = SwitchPro::CMD::AND_RUMBLE;
            out_report_.sub_command = SwitchPro::CMD::GYRO;
            out_report_.sub_command_args[0] = 1 ? 1 : 0;

            if (tuh_hid_send_report(address, instance, 0, &out_report_, report_size))
            {
                init_state_ = InitState::DONE;
                tuh_hid_receive_report(address, instance);
            }
            break;
        default:
            break;
    }
}

static const uint8_t* switchpro_report_payload(const uint8_t* report, uint16_t len, uint16_t& out_payload_len)
{
    out_payload_len = len;
    if (len == sizeof(SwitchPro::InReport))
    {
        return report;
    }
    if (len >= 2U + sizeof(SwitchPro::InReport))
    {
        const uint8_t rid = report[0];
        if (rid == SwitchPro::REPORT_ID_STANDARD || rid == SwitchPro::REPORT_ID_SWITCH2_FULL ||
            rid == SwitchPro::REPORT_ID_FULL_ALT)
        {
            out_payload_len = static_cast<uint16_t>(len - 2U);
            return report + 2;
        }
    }
    return nullptr;
}

void SwitchProHost::process_report(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report,
                                   uint16_t len)
{
    if (switch2_bringup_active_)
    {
        tuh_hid_receive_report(address, instance);
        return;
    }

    if (init_state_ != InitState::DONE)
    {
        init_switch_host(gamepad, address, instance);
        return;
    }

    uint16_t payload_len = 0;
    const uint8_t* payload = switchpro_report_payload(report, len, payload_len);
    if (payload == nullptr || payload_len < sizeof(SwitchPro::InReport))
    {
        tuh_hid_receive_report(address, instance);
        return;
    }

    const SwitchPro::InReport* in_report = reinterpret_cast<const SwitchPro::InReport*>(payload);
    if (std::memcmp(&prev_in_report_.buttons, in_report->buttons, 9) == 0)
    {
        tuh_hid_receive_report(address, instance);
        return;
    }

    Gamepad::PadIn gp_in;

    if (in_report->buttons[0] & SwitchPro::Buttons0::Y)
        gp_in.buttons |= gamepad.MAP_BUTTON_X;
    if (in_report->buttons[0] & SwitchPro::Buttons0::B)
        gp_in.buttons |= gamepad.MAP_BUTTON_A;
    if (in_report->buttons[0] & SwitchPro::Buttons0::A)
        gp_in.buttons |= gamepad.MAP_BUTTON_B;
    if (in_report->buttons[0] & SwitchPro::Buttons0::X)
        gp_in.buttons |= gamepad.MAP_BUTTON_Y;
    if (in_report->buttons[2] & SwitchPro::Buttons2::L)
        gp_in.buttons |= gamepad.MAP_BUTTON_LB;
    if (in_report->buttons[0] & SwitchPro::Buttons0::R)
        gp_in.buttons |= gamepad.MAP_BUTTON_RB;
    if (in_report->buttons[1] & SwitchPro::Buttons1::L3)
        gp_in.buttons |= gamepad.MAP_BUTTON_L3;
    if (in_report->buttons[1] & SwitchPro::Buttons1::R3)
        gp_in.buttons |= gamepad.MAP_BUTTON_R3;
    if (in_report->buttons[1] & SwitchPro::Buttons1::MINUS)
        gp_in.buttons |= gamepad.MAP_BUTTON_BACK;
    if (in_report->buttons[1] & SwitchPro::Buttons1::PLUS)
        gp_in.buttons |= gamepad.MAP_BUTTON_START;
    if (in_report->buttons[1] & SwitchPro::Buttons1::HOME)
        gp_in.buttons |= gamepad.MAP_BUTTON_SYS;
    if (in_report->buttons[1] & SwitchPro::Buttons1::CAPTURE)
        gp_in.buttons |= gamepad.MAP_BUTTON_MISC;

    if (in_report->buttons[2] & SwitchPro::Buttons2::DPAD_UP)
        gp_in.dpad |= gamepad.MAP_DPAD_UP;
    if (in_report->buttons[2] & SwitchPro::Buttons2::DPAD_DOWN)
        gp_in.dpad |= gamepad.MAP_DPAD_DOWN;
    if (in_report->buttons[2] & SwitchPro::Buttons2::DPAD_LEFT)
        gp_in.dpad |= gamepad.MAP_DPAD_LEFT;
    if (in_report->buttons[2] & SwitchPro::Buttons2::DPAD_RIGHT)
        gp_in.dpad |= gamepad.MAP_DPAD_RIGHT;

    gp_in.trigger_l = in_report->buttons[2] & SwitchPro::Buttons2::ZL ? Range::MAX<uint8_t> : Range::MIN<uint8_t>;
    gp_in.trigger_r = in_report->buttons[0] & SwitchPro::Buttons0::ZR ? Range::MAX<uint8_t> : Range::MIN<uint8_t>;

    uint16_t joy_lx = in_report->joysticks[0] | ((in_report->joysticks[1] & 0xF) << 8);
    uint16_t joy_ly = (in_report->joysticks[1] >> 4) | (in_report->joysticks[2] << 4);
    uint16_t joy_rx = in_report->joysticks[3] | ((in_report->joysticks[4] & 0xF) << 8);
    uint16_t joy_ry = (in_report->joysticks[4] >> 4) | (in_report->joysticks[5] << 4);

    std::tie(gp_in.joystick_lx, gp_in.joystick_ly) =
        gamepad.scale_joystick_l(normalize_axis(joy_lx), normalize_axis(joy_ly), true);

    std::tie(gp_in.joystick_rx, gp_in.joystick_ry) =
        gamepad.scale_joystick_r(normalize_axis(joy_rx), normalize_axis(joy_ry), true);

    gamepad.set_pad_in(gp_in);

    tuh_hid_receive_report(address, instance);
    std::memcpy(&prev_in_report_, in_report, sizeof(SwitchPro::InReport));
}

bool SwitchProHost::send_feedback(Gamepad& gamepad, uint8_t address, uint8_t instance)
{
    if (switch2_bringup_active_)
    {
        return false;
    }

    if (init_state_ != InitState::DONE)
    {
        init_switch_host(gamepad, address, instance);
        return false;
    }

    // See: https://github.com/Dan611/hid-procon
    //      https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering
    //      https://github.com/HisashiKato/USB_Host_Shield_Library_2.0

    std::memset(&out_report_, 0, sizeof(out_report_));

    uint8_t report_size = 10;

    out_report_.command = SwitchPro::CMD::RUMBLE_ONLY;
    out_report_.sequence_counter = get_output_sequence_counter();

    Gamepad::PadOut gp_out = gamepad.get_pad_out();

    if (gp_out.rumble_l > 0)
    {
        uint8_t amplitude_l = static_cast<uint8_t>(((gp_out.rumble_l / 255.0f) * 0.8f + 0.5f) * (0xC0 - 0x40) + 0x40);

        out_report_.rumble_l[0] = amplitude_l;
        out_report_.rumble_l[1] = 0x88;
        out_report_.rumble_l[2] = amplitude_l / 2;
        out_report_.rumble_l[3] = 0x61;
    }
    else
    {
        out_report_.rumble_l[0] = 0x00;
        out_report_.rumble_l[1] = 0x01;
        out_report_.rumble_l[2] = 0x40;
        out_report_.rumble_l[3] = 0x40;
    }

    if (gp_out.rumble_r > 0)
    {
        uint8_t amplitude_r = static_cast<uint8_t>(((gp_out.rumble_r / 255.0f) * 0.8f + 0.5f) * (0xC0 - 0x40) + 0x40);

        out_report_.rumble_r[0] = amplitude_r;
        out_report_.rumble_r[1] = 0x88;
        out_report_.rumble_r[2] = amplitude_r / 2;
        out_report_.rumble_r[3] = 0x61;
    }
    else
    {
        out_report_.rumble_r[0] = 0x00;
        out_report_.rumble_r[1] = 0x01;
        out_report_.rumble_r[2] = 0x40;
        out_report_.rumble_r[3] = 0x40;
    }

    return tuh_hid_send_report(address, instance, 0, &out_report_, report_size);
}
