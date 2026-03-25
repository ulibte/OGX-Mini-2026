#ifndef _SWITCH_PRO_HOST_H_
#define _SWITCH_PRO_HOST_H_

#include <array>
#include <cstdint>

#include "Descriptors/SwitchPro.h"
#include "USBHost/HostDriver/HostDriver.h"

struct tuh_xfer_s;

class SwitchProHost : public HostDriver
{
public:
    SwitchProHost(uint8_t idx)
        : HostDriver(idx) {}

    void initialize(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report_desc, uint16_t desc_len) override;
    void process_report(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report, uint16_t len) override;
    bool send_feedback(Gamepad& gamepad, uint8_t address, uint8_t instance) override;

    static bool is_switch2_usb_family(uint16_t vid, uint16_t pid);

protected:
    enum class InitState
    {
        HANDSHAKE,
        TIMEOUT,
        FULL_REPORT,
        LED,
        LED_HOME,
        IMU,
        DONE
    };

    InitState init_state_{InitState::HANDSHAKE};
    uint8_t sequence_counter_{0};

    SwitchPro::InReport prev_in_report_{};
    SwitchPro::OutReport out_report_{};

    uint8_t hid_instance_{0};
    uint8_t switch2_dev_addr_{0};
    uint8_t switch2_ep_out_{0};
    uint8_t switch2_ep_in_{0};
    bool switch2_bringup_active_{false};
    Gamepad* switch2_gamepad_{nullptr};
    std::array<uint8_t, 512> switch2_cfg_buf_{};
    std::array<uint8_t, 64> switch2_bulk_out_buf_{};
    std::array<uint8_t, 64> switch2_bulk_in_buf_{};
    unsigned switch2_pkt_idx_{0};

    void switch2_start_config_read();
    bool switch2_parse_open_iface1(const uint8_t* desc_cfg, uint16_t buflen);
    void switch2_submit_out_packet();
    void switch2_submit_in_read();
    void switch2_after_packet_round();
    void switch2_finish_bringup();

    static void switch2_cfg_cb(tuh_xfer_s* xfer);
    static void switch2_out_cb(tuh_xfer_s* xfer);
    static void switch2_in_cb(tuh_xfer_s* xfer);

    void init_switch_host(Gamepad& gamepad, uint8_t address, uint8_t instance);
    uint8_t get_output_sequence_counter();

    static inline int16_t normalize_axis(uint16_t value)
    {
        int32_t normalized_value = (value - 2047) * 22;
        return Range::clamp<int16_t>(normalized_value);
    }
};

#endif // _SWITCH_PRO_HOST_H_