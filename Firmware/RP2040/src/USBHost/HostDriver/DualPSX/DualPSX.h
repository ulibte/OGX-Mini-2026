#ifndef _DUAL_PSX_HOST_H_
#define _DUAL_PSX_HOST_H_

#include <cstdint>

#include "Descriptors/DualPSX.h"
#include "USBHost/HostDriver/HostDriver.h"

class DualPSXHost : public HostDriver
{
public:
    DualPSXHost(uint8_t idx)
        : HostDriver(idx) {}

    void initialize(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report_desc, uint16_t desc_len) override;
    void process_report(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report, uint16_t len) override;
    bool send_feedback(Gamepad& gamepad, uint8_t address, uint8_t instance) override;

private:
    DualPSX::InReport prev_report_{};
};

#endif // _DUAL_PSX_HOST_H_