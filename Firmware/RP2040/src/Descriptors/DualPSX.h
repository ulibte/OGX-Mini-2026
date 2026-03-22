#pragma once

#include <cstdint>
#include "Gamepad/Gamepad.h"
#include "tusb.h"

namespace DualPSX
{
    void parse_in_report(const struct InReport* in_report, Gamepad::PadIn* gp_in);

    // The report from the DualPSX is 8 bytes.
    // Byte 0: Report ID (1 or 2)
    // Byte 1: Right Stick Y
    // Byte 2: Right Stick X
    // Byte 3: Left Stick X
    // Byte 4: Left Stick Y
    // Byte 5: Buttons
    // Byte 6: Buttons
    // Byte 7: Unused

    // Red Hat, PS, PS2, Logitech, Pelican, InterAct
    //           [0]      [1]      [2]      [3]      [4]      [5]      [6]      [7]
    // resting:  01       80       80       80       80       0f       00       00
    // dpad up:  01       80       80       80       80       00       00       00
    // dpad d:   01       80       80       80       80       04       00       00
    // dpad l:   01       80       80       80       80       06       00       00
    // dpad r:   01       80       80       80       80       02       00       00
    // square:   01       80       80       80       80       8f       00       00
    // cross:    01       80       80       80       80       4f       00       00
    // circle:   01       80       80       80       80       2f       00       00
    // triangle: 01       80       80       80       80       1f       00       00
    // L1:       01       80       80       80       80       0f       04       00
    // L2:       01       80       80       80       80       0f       01       00
    // R1:       01       80       80       80       80       0f       08       00
    // R2:       01       80       80       80       80       0f       02       00
    // select:   01       80       80       80       80       0f       10       00
    // start:    01       80       80       80       80       0f       20       00


    // [01,7f,7f,7f,7f,0f,00,00]
    // [0] joystick Id
    // [1] analogico direito Y Axis
    // [2] analogico direito X Axis
    // [3] analogico esquerdo X Axis
    // [4] analogico esquerdo Y Axis
    // [5] (0000) 0000 face buttons
    // [5] 0000 (0000) dpad
    // [6] (0000) 0000 start, select, l3, r3
    // [6] 0000 (0000) frontal buttons: r1, r2, l1, l2

    #pragma pack(push, 1)
    struct InReport
    {
        uint8_t report_id;
        uint8_t ry;
        uint8_t rx;
        uint8_t lx;
        uint8_t ly;
        uint8_t buttons1;
        uint8_t buttons2;
        uint8_t end;
    };
    #pragma pack(pop)
    static_assert(sizeof(InReport) == 8, "DualPSX::InReport must be exactly 8 bytes");

    namespace DPad
    {
        static constexpr uint8_t UP    = 0x00;
        static constexpr uint8_t UP_RIGHT = 0x01;
        static constexpr uint8_t RIGHT = 0x02;
        static constexpr uint8_t DOWN_RIGHT = 0x3;
        static constexpr uint8_t DOWN  = 0x4;
        static constexpr uint8_t DOWN_LEFT = 0x5;
        static constexpr uint8_t LEFT  = 0x06;
        static constexpr uint8_t UP_LEFT = 0x07;
        static constexpr uint8_t MASK  = 0x0f;
    };

    namespace Buttons1
    {
        static constexpr uint8_t TRIANGLE = 0x10;
        static constexpr uint8_t CIRCLE   = 0x20;
        static constexpr uint8_t CROSS    = 0x40;
        static constexpr uint8_t SQUARE   = 0x80;
    };

    namespace Buttons2 {
        static constexpr uint8_t L2     = 0x01;
        static constexpr uint8_t R2     = 0x02;
        static constexpr uint8_t L1     = 0x04;
        static constexpr uint8_t R1     = 0x08;
        static constexpr uint8_t SELECT = 0x10;
        static constexpr uint8_t START  = 0x20;
        static constexpr uint8_t L3     = 0x40;
        static constexpr uint8_t R3     = 0x80;
    };

    static const uint8_t STRING_LANGUAGE[]     = { 0x09, 0x04 };
    static const uint8_t STRING_MANUFACTURER[] = "Personal Communication Systems";
    static const uint8_t STRING_PRODUCT[]      = "Twin USB Joystick";
    static const uint8_t STRING_VERSION[]      = { };

    static const uint8_t *STRING_DESCRIPTORS[] __attribute__((unused)) =
    {
        STRING_LANGUAGE,
        STRING_MANUFACTURER,
        STRING_PRODUCT,
        STRING_VERSION
    };


    static const uint8_t DEVICE_DESCRIPTORS[] =
    {
        0x12,        // bLength (18 bytes decimal = 0x12 hex)
        0x01,        // bDescriptorType (Device)
        0x00, 0x01,  // bcdUSB 1.00
        0x00,        // bDeviceClass
        0x00,        // bDeviceSubClass
        0x00,        // bDeviceProtocol
        0x08,        // bMaxPacketSize0 8
        0x10, 0x08,  // idVendor 0x0810 (Personal Communication Systems)
        0x01, 0x00,  // idProduct 0x0001 (Dual PSX Adaptor)
        0x06, 0x01,  // bcdDevice 1.06
        0x00,        // iManufacturer
        0x02,        // iProduct (Index 2: Twin USB Joystick)
        0x00,        // iSerialNumber
        0x01,        // bNumConfigurations 1
    };

    static const uint8_t HID_DESCRIPTORS[] =
    {
        0x09,        // bLength
        0x21,        // bDescriptorType (HID)
        0x10, 0x01,  // bcdHID 1.10
        0x21,        // bCountryCode
        0x01,        // bNumDescriptors
        0x22,        // bDescriptorType[0] (Report)
        0xCA, 0x00,  // wDescriptorLength[0] 202 (0x00CA)
    };

    static const uint8_t CONFIGURATION_DESCRIPTORS[] =
    {
        // --- Configuration Descriptor ---
        0x09,        // bLength
        0x02,        // bDescriptorType (Configuration)
        0x22, 0x00,  // wTotalLength 34 (0x0022)
        0x01,        // bNumInterfaces 1
        0x01,        // bConfigurationValue
        0x00,        // iConfiguration (0 no log)
        0x80,        // bmAttributes (Bus Powered)
        0xFA,        // bMaxPower 500mA (500mA / 2mA = 250 ou 0xFA)

        // --- Interface Descriptor ---
        0x09,        // bLength
        0x04,        // bDescriptorType (Interface)
        0x00,        // bInterfaceNumber 0
        0x00,        // bAlternateSetting
        0x01,        // bNumEndpoints 1
        0x03,        // bInterfaceClass (HID)
        0x00,        // bInterfaceSubClass
        0x00,        // bInterfaceProtocol
        0x00,        // iInterface

        // --- HID Descriptor ---
        0x09,        // bLength
        0x21,        // bDescriptorType (HID)
        0x10, 0x01,  // bcdHID 1.10
        0x21,        // bCountryCode 33
        0x01,        // bNumDescriptors
        0x22,        // bDescriptorType[0] (Report)
        0xCA, 0x00,  // wDescriptorLength[0] 202

        // --- Endpoint Descriptor ---
        0x07,        // bLength
        0x05,        // bDescriptorType (Endpoint)
        0x81,        // bEndpointAddress (EP 1 IN)
        0x03,        // bmAttributes (Interrupt)
        0x08, 0x00,  // wMaxPacketSize 8
        0x0A,        // bInterval 10
    };
}