# Logging via UART no OGX-Mini (RP2040) - Debug Físico

### Conversor USB-to-UART

Conecte seu conversor (ex: CP2102, CH340, FT232) assim:

```
Pico          Conversor
─────────────────────────
GPIO 8 (TX) → RX
GPIO 9 (RX) → TX
GND         → GND
(não precisa VCC, o Pico alimenta via USB principal)
```

⚠️ **Importante**: Use um conversor que suporte 3.3V (não 5V!)

# faltava set(PICO_SDK_PATH ${EXTERNAL_DIR}/pico-sdk) no CMakeLists.txt
# no CMakeLists.txt adicione na parte do debug: add_compile_definitions(CFG_TUSB_DEBUG=2)
# tudo começa em Firmware/external/tinyusb/src/host/usbh.c quando conecta o controle

# copia o .uf2 para /media/ulibte/RPI-RP2/

### O que muda em Debug:

```cmake
# Debug build:
pico_enable_stdio_uart(${FW_NAME} 1)  # ← UART habilitada
target_compile_definitions(${FW_NAME} PRIVATE
    PICO_DEFAULT_UART=1
    PICO_DEFAULT_UART_TX_PIN=8
    PICO_DEFAULT_UART_RX_PIN=9
)

# Release build:
pico_enable_stdio_uart(${FW_NAME} 0)  # ← UART desabilitada
# (usa USB CDC ao invés)
```

## Verificar Conversor USB-to-UART no PC

### Linux

```bash
# Liste todas as portas seriais
ls /dev/ttyUSB*
ls /dev/ttyACM*

# Verifique informações do dispositivo
lsusb | grep -i "serial\|uart"

## Leitura de Logs no terminal

```bash
screen /dev/ttyUSB0 115200
# Sair: Ctrl+A, k, y
```
## Leitura de Logs em um arquivo
```bash
screen -L -Logfile /mnt/HD/Downloads/uart_log.txt /dev/ttyUSB0 115200
# Sair: Ctrl+A, k, y
```

## Troubleshooting

### "Vejo caracteres estranhos"

- ❌ Baud rate errado (cheque 115200)
- ❌ Pinos TX/RX invertidos
- ❌ Voltagem errada (conversor 5V em Pico 3.3V)

# Certifique-se que está usando `CMAKE_BUILD_TYPE=Debug`. Em Release, UART é desabilitada por padrão.

## Próximos Passos


 **Teste básico**
   ```cpp
   printf("OGX-Mini initialized!\n");
   printf("Testing UART @ pin 8 (TX)\n");
   ```
 **Logging**
   - Use `ogxm_log::log()` para mensagens que vc acha importante ficar pra sempre no projeto
   - Reserve `printf()` para debug rapido

 **Logs de hexadecimal**
   - Capture com `ogxm_log::log_hex()`