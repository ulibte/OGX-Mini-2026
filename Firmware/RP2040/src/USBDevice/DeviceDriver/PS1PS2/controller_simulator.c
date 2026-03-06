/* PS1/PS2 controller protocol (device side). Ported from PicoGamepadConverter (Loc15).
 * https://github.com/Loc15/PicoGamepadConverter */

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "psxSPI.pio.h"
#include "USBDevice/DeviceDriver/PS1PS2/psx_simulator.h"

#define MODE_DIGITAL        0x41
#define MODE_ANALOG         0x73
#define MODE_ANALOG_PRESSURE 0x79
#define MODE_CONFIG         0xF3

#define CMD_PRES_CONFIG     0x40
#define CMD_POLL_CONFIG_STATUS 0x41
#define CMD_POLL            0x42
#define CMD_CONFIG          0x43
#define CMD_STATUS          0x45
#define CMD_CONST_46        0x46
#define CMD_CONST_47        0x47
#define CMD_CONST_4C        0x4C
#define CMD_ENABLE_RUMBLE   0x4D
#define CMD_POLL_CONFIG     0x4F
#define CMD_ANALOG_SWITCH   0x44

#define UP    0x10
#define RIGHT 0x20
#define DOWN  0x40
#define LEFT  0x80
#define L2    0x01
#define R2    0x02
#define L1    0x04
#define R1    0x08
#define TRI   0x10
#define CIR   0x20
#define X     0x40
#define SQU   0x80

static PIO psx_device_pio;
static uint smCmdReader;
static uint smDatWriter;
static uint offsetCmdReader;
static uint offsetDatWriter;

volatile PSXInputState *inputState;
static void (*core1_function)(void);

static uint8_t mode = MODE_DIGITAL;
static bool config = false;
static bool analogLock = false;
static uint8_t motorBytes[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t pollConfig[4] = { 0x00, 0x00, 0x00, 0x00 };

static void cancel_ack(void) {
    pio_sm_exec(psx_device_pio, smCmdReader, pio_encode_jmp(offsetCmdReader));
}

static void SEND(uint8_t byte) {
    write_byte_blocking(psx_device_pio, smDatWriter, byte);
}

static uint8_t RECV_CMD(void) {
    return read_byte_blocking(psx_device_pio, smCmdReader);
}

static void initController(void) {
    mode = MODE_DIGITAL;
    config = false;
    analogLock = false;
    memset(motorBytes, 0xFF, sizeof(motorBytes));
    pollConfig[0] = 0xFF;
    pollConfig[1] = 0xFF;
    pollConfig[2] = 0x03;
    pollConfig[3] = 0x00;
}

static void processRumble(uint8_t index, uint8_t value) {
    (void)index;
    (void)value;
}

static void processPresConfig(void) {
    if (!config) return;
    uint8_t buf[7] = { 0x5A, 0x00, 0x00, 0x02, 0x00, 0x00, 0x5A };
    for (uint8_t i = 0; i < 7; i++) {
        SEND(buf[i]);
        RECV_CMD();
    }
}

static void processPollConfigStatus(void) {
    if (!config) return;
    uint8_t buf[7] = { 0x5A, (mode == MODE_DIGITAL) ? 0x00 : 0xFF, (mode == MODE_DIGITAL) ? 0x00 : 0xFF, (mode == MODE_DIGITAL) ? 0x00 : 0x03, (mode == MODE_DIGITAL) ? 0x00 : 0x00, 0x00, (mode == MODE_DIGITAL) ? 0x00 : 0x5A };
    for (uint8_t i = 0; i < 7; i++) {
        SEND(buf[i]);
        RECV_CMD();
    }
}

static void processPoll(void) {
    config = false;
    switch (mode) {
    case MODE_DIGITAL: {
        uint8_t buf[3] = { 0x5A, inputState->buttons1, inputState->buttons2 };
        for (uint8_t i = 0; i < 3; i++) {
            SEND(buf[i]);
            processRumble(i, RECV_CMD());
        }
        break;
    }
    case MODE_ANALOG: {
        uint8_t buf[7] = { 0x5A, inputState->buttons1, inputState->buttons2, inputState->rx, inputState->ry, inputState->lx, inputState->ly };
        for (uint8_t i = 0; i < 7; i++) {
            SEND(buf[i]);
            processRumble(i, RECV_CMD());
        }
        break;
    }
    case MODE_ANALOG_PRESSURE: {
        uint8_t buf[19] = {
            0x5A, inputState->buttons1, inputState->buttons2, inputState->rx, inputState->ry, inputState->lx, inputState->ly,
            (inputState->buttons1 & RIGHT) ? 0x00 : 0xFF, (inputState->buttons1 & LEFT) ? 0x00 : 0xFF,
            (inputState->buttons1 & UP) ? 0x00 : 0xFF, (inputState->buttons1 & DOWN) ? 0x00 : 0xFF,
            (inputState->buttons2 & TRI) ? 0x00 : 0xFF, (inputState->buttons2 & CIR) ? 0x00 : 0xFF,
            (inputState->buttons2 & X) ? 0x00 : 0xFF, (inputState->buttons2 & SQU) ? 0x00 : 0xFF,
            (inputState->buttons2 & L1) ? 0x00 : 0xFF, (inputState->buttons2 & R1) ? 0x00 : 0xFF,
            inputState->l2, inputState->r2
        };
        for (uint8_t i = 0; i < 19; i++) {
            SEND(buf[i]);
            processRumble(i, RECV_CMD());
        }
        break;
    }
    default:
        break;
    }
}

static void processConfig(void) {
    switch (config ? MODE_CONFIG : mode) {
    case MODE_CONFIG:
        for (uint8_t i = 0; i < 7; i++) {
            SEND((i == 0) ? 0x5A : 0x00);
            if (i != 1)
                RECV_CMD();
            else
                config = RECV_CMD();
        }
        break;
    case MODE_DIGITAL: {
        uint8_t buf[3] = { 0x5A, inputState->buttons1, inputState->buttons2 };
        for (uint8_t i = 0; i < 3; i++) {
            SEND(buf[i]);
            if (i != 1)
                RECV_CMD();
            else
                config = RECV_CMD();
        }
        break;
    }
    case MODE_ANALOG: {
        uint8_t buf[7] = { 0x5A, inputState->buttons1, inputState->buttons2, inputState->rx, inputState->ry, inputState->lx, inputState->ly };
        for (uint8_t i = 0; i < 7; i++) {
            SEND(buf[i]);
            if (i != 1)
                RECV_CMD();
            else
                config = RECV_CMD();
        }
        break;
    }
    case MODE_ANALOG_PRESSURE: {
        uint8_t buf[19] = {
            0x5A, inputState->buttons1, inputState->buttons2, inputState->rx, inputState->ry, inputState->lx, inputState->ly,
            (inputState->buttons1 & RIGHT) ? 0x00 : 0xFF, (inputState->buttons1 & LEFT) ? 0x00 : 0xFF,
            (inputState->buttons1 & UP) ? 0x00 : 0xFF, (inputState->buttons1 & DOWN) ? 0x00 : 0xFF,
            (inputState->buttons2 & TRI) ? 0x00 : 0xFF, (inputState->buttons2 & CIR) ? 0x00 : 0xFF,
            (inputState->buttons2 & X) ? 0x00 : 0xFF, (inputState->buttons2 & SQU) ? 0x00 : 0xFF,
            (inputState->buttons2 & L1) ? 0x00 : 0xFF, (inputState->buttons2 & R1) ? 0x00 : 0xFF,
            inputState->l2, inputState->r2
        };
        for (uint8_t i = 0; i < 19; i++) {
            SEND(buf[i]);
            if (i != 1)
                RECV_CMD();
            else
                config = RECV_CMD();
        }
        break;
    }
    default:
        break;
    }
}

static uint8_t detectAnalog(void) {
    if ((pollConfig[0] + pollConfig[1] + pollConfig[2] + pollConfig[3]) > 0)
        return MODE_ANALOG_PRESSURE;
    return MODE_ANALOG;
}

static void processAnalogSwitch(void) {
    if (!config) return;
    for (uint8_t i = 0; i < 7; i++) {
        switch (i) {
        case 0:
            SEND(0x5A);
            RECV_CMD();
            break;
        case 1:
            SEND(0x00);
            mode = (RECV_CMD() == 0x01) ? detectAnalog() : MODE_DIGITAL;
            break;
        case 2:
            SEND(0x00);
            analogLock = (RECV_CMD() == 0x03) ? 1 : 0;
            break;
        default:
            SEND(0x00);
            RECV_CMD();
            break;
        }
    }
}

static void processStatus(void) {
    if (!config) return;
    uint8_t buf[7] = { 0x5A, 0x03, 0x02, (mode == MODE_DIGITAL) ? 0x00 : 0x01, 0x02, 0x01, 0x00 };
    for (uint8_t i = 0; i < 7; i++) {
        SEND(buf[i]);
        RECV_CMD();
    }
}

static void processConst46(void) {
    if (!config) return;
    SEND(0x5A);
    RECV_CMD();
    SEND(0x00);
    uint8_t offset = RECV_CMD();
    uint8_t buf[5] = { 0x00, 0x01, (offset == 0x00) ? 0x02 : 0x01, (offset == 0x00) ? 0x00 : 0x01, 0x0F };
    for (uint8_t i = 0; i < 5; i++) {
        SEND(buf[i]);
        RECV_CMD();
    }
}

static void processConst47(void) {
    if (!config) return;
    uint8_t buf[7] = { 0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00 };
    for (uint8_t i = 0; i < 7; i++) {
        SEND(buf[i]);
        RECV_CMD();
    }
}

static void processConst4c(void) {
    if (!config) return;
    SEND(0x5A);
    RECV_CMD();
    SEND(0x00);
    uint8_t offset = RECV_CMD();
    uint8_t buf[5] = { 0x00, 0x00, (offset == 0x00) ? 0x04 : 0x07, 0x00, 0x00 };
    for (uint8_t i = 0; i < 5; i++) {
        SEND(buf[i]);
        RECV_CMD();
    }
}

static void processEnableRumble(void) {
    if (!config) return;
    for (uint8_t i = 0; i < 7; i++) {
        if (i == 0) {
            SEND(0x5A);
            RECV_CMD();
        } else {
            SEND(motorBytes[i - 1]);
            motorBytes[i - 1] = RECV_CMD();
        }
    }
}

static void processPollConfig(void) {
    if (!config) return;
    for (int i = 0; i < 7; i++) {
        if (i >= 1 && i <= 4) {
            SEND(0x00);
            pollConfig[i - 1] = RECV_CMD();
        } else {
            SEND((i == 0 || i == 6) ? 0x5A : 0x00);
            RECV_CMD();
        }
    }
    if ((pollConfig[0] + pollConfig[1] + pollConfig[2] + pollConfig[3]) != 0)
        mode = MODE_ANALOG_PRESSURE;
    else
        mode = MODE_ANALOG;
}

static void process_joy_req(void) {
    SEND(config ? MODE_CONFIG : mode);
    uint8_t cmd = RECV_CMD();
    switch (cmd) {
    case CMD_POLL:
        processPoll();
        break;
    case CMD_CONFIG:
        processConfig();
        break;
    case CMD_STATUS:
        processStatus();
        break;
    case CMD_CONST_46:
        processConst46();
        break;
    case CMD_CONST_47:
        processConst47();
        break;
    case CMD_CONST_4C:
        processConst4c();
        break;
    case CMD_POLL_CONFIG_STATUS:
        processPollConfigStatus();
        break;
    case CMD_ENABLE_RUMBLE:
        processEnableRumble();
        break;
    case CMD_POLL_CONFIG:
        processPollConfig();
        break;
    case CMD_PRES_CONFIG:
        processPresConfig();
        break;
    case CMD_ANALOG_SWITCH:
        processAnalogSwitch();
        break;
    default:
        break;
    }
}

void psx_device_main(void) {
    initController();
    while (true) {
        if (RECV_CMD() == 0x01)
            process_joy_req();
    }
}

static void __time_critical_func(restart_pio_sm)(void) {
    pio_set_sm_mask_enabled(psx_device_pio, (1u << smCmdReader) | (1u << smDatWriter), false);
    pio_restart_sm_mask(psx_device_pio, (1u << smCmdReader) | (1u << smDatWriter));
    pio_sm_exec(psx_device_pio, smCmdReader, pio_encode_jmp(offsetCmdReader));
    pio_sm_exec(psx_device_pio, smDatWriter, pio_encode_jmp(offsetDatWriter));
    pio_sm_clear_fifos(psx_device_pio, smCmdReader);
    pio_sm_drain_tx_fifo(psx_device_pio, smDatWriter);

    multicore_reset_core1();
    if (core1_function)
        core1_function();
    pio_enable_sm_mask_in_sync(psx_device_pio, (1u << smCmdReader) | (1u << smDatWriter));
}

static void __time_critical_func(sel_isr_callback)(void) {
    gpio_acknowledge_irq(PIN_SEL, GPIO_IRQ_EDGE_RISE);
    restart_pio_sm();
}

static void init_pio(void) {
    gpio_set_dir(PIN_DAT, false);
    gpio_set_dir(PIN_CMD, false);
    gpio_set_dir(PIN_SEL, false);
    gpio_set_dir(PIN_CLK, false);
    gpio_set_dir(PIN_ACK, false);
    gpio_disable_pulls(PIN_DAT);
    gpio_disable_pulls(PIN_CMD);
    gpio_disable_pulls(PIN_SEL);
    gpio_disable_pulls(PIN_CLK);
    gpio_disable_pulls(PIN_ACK);

    smCmdReader = pio_claim_unused_sm(psx_device_pio, true);
    smDatWriter = pio_claim_unused_sm(psx_device_pio, true);

    offsetCmdReader = pio_add_program(psx_device_pio, &cmd_reader_program);
    offsetDatWriter = pio_add_program(psx_device_pio, &dat_writer_program);

    cmd_reader_program_init(psx_device_pio, smCmdReader, offsetCmdReader);
    dat_writer_program_init(psx_device_pio, smDatWriter, offsetDatWriter);
}

void psx_device_init(unsigned pio, PSXInputState *data, void (*reset_pio)(void)) {
    psx_device_pio = pio ? pio1 : pio0;
    inputState = data;
    core1_function = reset_pio;

    init_pio();

    gpio_set_slew_rate(PIN_DAT, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(PIN_DAT, GPIO_DRIVE_STRENGTH_12MA);

    gpio_set_irq_enabled(PIN_SEL, GPIO_IRQ_EDGE_RISE, true);
    irq_set_exclusive_handler(IO_IRQ_BANK0, sel_isr_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);
}
