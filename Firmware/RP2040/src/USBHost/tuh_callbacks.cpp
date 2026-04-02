#include <cstdint>
#include "pico/time.h"

#include "tusb.h"
#include "host/usbh.h"
#include "class/hid/hid_host.h"

#include "USBHost/HostDriver/XInput/tuh_xinput/tuh_xinput.h"
#include "USBHost/HostManager.h"
#include "OGXMini/OGXMini.h"
#include "pico/bootrom.h"

// Gamesir dongle bug
static bool have_to_wait_for_xinput = true;

usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count) {
    *driver_count = 1;
    return tuh_xinput::class_driver();
}

//HID

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    OGXM_LOG("╔════════════════════════════════════════╗\n");
    OGXM_LOG("║     🔌 HID DEVICE DETECTED 🔌        ║\n");
    OGXM_LOG("╚════════════════════════════════════════╝\n");
    OGXM_LOG("Device Address: %d | Instance: %d\n", dev_addr, instance);
    OGXM_LOG("VID: 0x%04X | PID: 0x%04X\n", vid, pid);
    OGXM_LOG("Report Descriptor Length: %d bytes\n\n", desc_len);
    OGXM_LOG("Raw HID Descriptor (HEX):\n");
    OGXM_LOG_HEX(desc_report, desc_len);
    OGXM_LOG("\n");

    // Hack para forçar fallback de Switch Pro (HID) para Xbox 360 (XInput) no dongle do gamesir t4 pro
    if (vid == 0x057E && pid == 0x2009) {
        if (have_to_wait_for_xinput) {
            // O gamesir t4 pro só finge ser xbox 360 se demorar para o setup.
            // valores testatos que não foram suficiente no modo debug: 20, 40, 80
            // 160 funcionou no debug, mas nao no release.
            // não funcionou no release: 160, 320, 480, 520
            // 2000, 1320, 980, 810  funcionou.
            // 540, 560, 580, 600, 640 funcionou uma vez
            // 800 sem sleep no reset não funcionou
            // 820 sem sleep no reset funciou
            // 900 as vezes continua switch pro, e as vezes nao funciona
            sleep_ms(1000);
            have_to_wait_for_xinput = false;
            uint8_t rhport = usbh_get_rhport(dev_addr);
            tuh_rhport_reset_bus(rhport, true);
            //sleep_ms(20);
            tuh_rhport_reset_bus(rhport, false);
            return; 
        }
        // resetar o rp2040 para testes
        //reset_usb_boot(0, 0);
    }
    have_to_wait_for_xinput=true;

    HostManager& host_manager = HostManager::get_instance();

    if (host_manager.setup_driver(HostManager::get_type({ vid, pid }), dev_addr, instance, desc_report, desc_len)) {
        OGXM_LOG("✅ Driver setup SUCCESS!\n");
        OGXMini::host_mounted(true);
    } else {
        OGXM_LOG("❌ Driver setup FAILED!\n");
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    HostManager& host_manager = HostManager::get_instance();
    host_manager.deinit_driver(HostManager::DriverClass::HID, dev_addr, instance);

    if (!host_manager.any_mounted()) {
        OGXMini::host_mounted(false);
    }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    HostManager::get_instance().process_report(dev_addr, instance, report, len);
}

//XINPUT

void tuh_xinput::mount_cb(uint8_t dev_addr, uint8_t instance, const tuh_xinput::Interface* interface) {
    HostManager& host_manager = HostManager::get_instance();
    HostDriverType host_type = HostManager::get_type(interface->dev_type);

    if (host_manager.setup_driver(host_type, dev_addr, instance)) {
        OGXMini::host_mounted(true, host_type);
    }
}

void tuh_xinput::unmount_cb(uint8_t dev_addr, uint8_t instance, const tuh_xinput::Interface* interface) {
    HostManager& host_manager = HostManager::get_instance();
    host_manager.deinit_driver(HostManager::DriverClass::XINPUT, dev_addr, instance);

    if (!host_manager.any_mounted()) {
        OGXMini::host_mounted(false);
    }
}

void tuh_xinput::report_received_cb(uint8_t dev_addr, uint8_t instance, const uint8_t* report, uint16_t len) {
    HostManager::get_instance().process_report(dev_addr, instance, report, len);
}

void tuh_xinput::xbox360w_connect_cb(uint8_t dev_addr, uint8_t instance) {
    uint8_t idx = HostManager::get_instance().get_gamepad_idx(  HostManager::DriverClass::XINPUT, 
                                                                dev_addr, instance);
    OGXMini::wireless_connected(true, idx);
    HostManager::get_instance().connect_cb(dev_addr, instance);
}

void tuh_xinput::xbox360w_disconnect_cb(uint8_t dev_addr, uint8_t instance) {
    uint8_t idx = HostManager::get_instance().get_gamepad_idx(  HostManager::DriverClass::XINPUT, 
                                                                dev_addr, instance);
    OGXMini::wireless_connected(false, idx);
    HostManager::get_instance().disconnect_cb(dev_addr, instance);
}