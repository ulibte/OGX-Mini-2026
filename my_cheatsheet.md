## Compilação para Debug com UART

```bash
cd Firmware/RP2040

rm -rf build
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DOGXM_BOARD=PI_PICO \
  -DMAX_GAMEPADS=1
cmake --build build
```
```bash
cd Firmware/RP2040

rm -rf build
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOGXM_BOARD=PI_PICO \
  -DMAX_GAMEPADS=1
cmake --build build
```

# atalho do vscode para compilar mais rapido: ctrl + shift + b
# mas precisa do arquivo tasks.json na pasta .vscode, contendo algo parecido com isso:
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build OGX-Mini Debug",
            "type": "shell",
            "command": "bash",
            "args": [
                "-c",
                "cd Firmware/RP2040 && rm -rf build && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DOGXM_BOARD=PI_PICO -DMAX_GAMEPADS=1 && cmake --build build"
            ],
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "presentation": {
                "echo": true,
                "reveal": "always",
                "focus": false,
                "panel": "shared"
            },
            "problemMatcher": "$gcc"
        }
    ]
}

# copia o .uf2 para /media/ulibte/RPI-RP2/

# Codigo apagado por causa de if statement tipo #if defined(CONFIG_OGXM_DEBUG), vc tem que ir em c_cpp_properties.json da pasta .vscode, e em defines vc faz isso:
"defines": [
       "CFG_TUH_ENABLED=1",
       "CONFIG_OGXM_DEBUG=1"
],

# ativar modulo usbmon
sudo modprobe usbmon

# para saber qual o hub esta o dispositivo conectado, exemplo: Bus 001
lsusb

# para conseguir o descriptor completo:
sudo lsusb -v -d VID:PID

# vai no wireshark "sudo wireshark" e verifique aquele hub, e descubra o dispositivo, uma maneira e ficar conectando e desconectando. filtre pelor src, ou dist. exemplo:
usb.src == "1.27.1" or usb.dst == "1.27.1"

# mas isso não vai capturar os pacotes que são enviados antes do dispositivo ter um endereço, então seja rapido em ligar a captura do wireshark, conectar o dispositivo, desconectar o dispositivo, e parar a captura do wireshark.

# para exportar como texto, va em file, export packet dissections, as plain text

# Tem uma versão do pio usb no pr #186 que conserta isso, então essas mudanças não são necessárias.
pio_usb.c
uint8_t __no_inline_not_in_flash_func(pio_usb_bus_wait_handshake)(pio_port_t* pp) {
  int16_t t = 500;

Config.h
120000 khz

# comandos usados antes de colocar o codigo novo do pr #186:
cd Firmware/external/Pico-PIO-USB/
git stash
cd /container/OGX-Mini-Ulibte
git submodule update --remote --merge Firmware/external/Pico-PIO-USB
cd Firmware/external/Pico-PIO-USB/
git log -1
# se esse ultimo commit for mais novo que a mudança do pr #186, vc não precisa fazer nada.
# mas se for mais antiga vc tem que copiar as mudanças do usb pio_usb.c do pr #186 para o seu


# Para consertar os erros de lint:
# Ctrl + Shift + p para abrir a pesquisa, CMake: Configure, escolha arm-none-eabi

# para filtrar o estado neutro verdadeiro(sem o analogico ligado):
(usbhid.data != 01:7f:7f:7f:7f:0f:00:00) && (usbhid.data != 02:7f:7f:7f:7f:0f:00:00) && (usb.src == "1.5.1")

# vou apertando o botão rapido e clicando e vendo a diferença de valor e descobri isso:
Analogico direito
usbhid.data[1] 7f NEUTRO
index 1, Y Axis, cima 00, , baixo ff
usbhid.data[2] 7f NEUTRO
index 2, X Axis, direita ff, meio ,esquerda 00

Analogico esquerdo
usbhid.data[3] 7f NEUTRO
index 3, X Axis, direita ff, meio ,esquerda 00
usbhid.data[4] 7f NEUTRO
index 4, Y Axis, cima 00, , baixo ff

# sabendo disso vou filtrar os analogicos a não ser que seja o valor ff(valor extremo de um lado do axis), e se for outro botão no index 6 ou 7 fora do neutro:
(usbhid.data != 01:7f:7f:7f:7f:0f:00:00) && (usbhid.data != 02:7f:7f:7f:7f:0f:00:00)  && ( (usbhid.data[1] == 0xff) || (usbhid.data[2] == 0xff) || (usbhid.data[3] == 0xff) || (usbhid.data[4] == 0xff) || (usbhid.data[5] != 0x0f) || (usbhid.data[6] != 0x00) || (usbhid.data[7] != 0x00) )

# guia basico:
Nome / Total de digitos / Exemplo em bits 
Bit	    1	1
Nibble	4	0011       
Byte	  8	0000 0011

Nibble é um dígito em hexadecimal

# descobri todos os botoes assim:
0:01,1:7f,2:7f,3:7f,4:7f,5:0f,6:00,7:00

usbhid.data[0] 01 CONTROLE 1
usbhid.data[0] 02 CONTROLE 2

Analogico direito
usbhid.data[1] 7f NEUTRO
index 1, Y Axis, cima 00, , baixo ff
usbhid.data[2] 7f NEUTRO
index 2, X Axis, direita ff, meio ,esquerda 00

Analogico esquerdo
usbhid.data[3] 7f NEUTRO
index 3, X Axis, direita ff, meio ,esquerda 00
usbhid.data[4] 7f NEUTRO
index 4, Y Axis, cima 00, , baixo ff

usbhid.data[5] 0x0f NEUTRO
0000 1111
0000 são os botoes normais
1111 são as setas
usbhid.data[5] 0x1f TRIANGULO
usbhid.data[5] 0x2f BOLA
usbhid.data[5] 0x4f XIS
usbhid.data[5] 0x8f QUADRADO
usbhid.data[5] 0x02 SETA DIREITA
usbhid.data[5] 0x04 SETA BAIXO
usbhid.data[5] 0x06 SETA ESQUERDA
usbhid.data[5] 0x00 SETA CIMA

usbhid.data[6] 0x00 NEUTRO
0000 0000
os primeiro 0000 são o start, select, l3, r3
o segundo grupo de 0000 são os frontais: r1, l1, r2, l2
usbhid.data[6] 0x08 R1
usbhid.data[6] 0x02 R2
usbhid.data[6] 0x04 L1
usbhid.data[6] 0x01 L2
usbhid.data[6] 0x20 START
usbhid.data[6] 0x10 SELECT
usbhid.data[6] 0x80 R3
usbhid.data[6] 0x40 L3

# lembrando que quando aperta dois botoes os valores fazer o bitwise, exemplo:
se partar BOLA 0x2f + XIS 0x4f = 0x6f

S
# o simbolo |= basicamente adiciona o valor da direita na esquerda, exemplo:
gamepad.buttons |= gamepad.MAP_BUTTON_X;
0x2f |= 0x4f
0x6f

0x2f = 0101111
0x4f = 1001111
       ------- |
0x6f = 1101111

# o sombolo & retorna os valores que são iguais nos dois lados, exemplo:
# assim da pra saber se um botao ja estava pressionado como neste caso pra saber se o 0x4f esta pressionado:
0x6f = 01101111
0x4f = 01001111
       -------- &
       01001111

# aqui um exemplo se um botão não estiver pressionado:
0x1f = 0001 1111
0x4f = 0100 1111
       --------- &
0x0f = 0000 1111
0x0f = NEUTRO

Só os quatro bites da esquerda são relevantes nesse caso, ja que o meu interesse aqui é os face buttons (x, y, a, b).

o problema é que o neutro não é 0x00, então fazemos isso, assim o F não atrapalha o &.
face buttons mask = 1111 0000
assim da pra usar esse mask para limpar os valores da esquerda ssim:
mask  = 1111 0000
sujo  = 0100 1110
        ---------&
limpo = 0100 0000

botoes pressionados antes: 0001 0000
0x10 = 0001 0000
0x40 = 0100 0000
       --------- &
0x00 = 0000 0000

então logo:
if (antes(0x10) & depois(0x40))
if (0x00)
if (false)

# TRIANGLE = 1U << 20, é o mesmo que TRIANGLE = 0x0000000000100000
# Ou:
# 0000000000000000000000000000000000000000000100000000000000000000

## Guia pra implementar no código.

# Descriptors/
fazer os arquivos, MyDevice.cpp. MyDevice.h

# USBHost/HostDriver
MyDevice/MyDevice.cpp
MyDevice/MyDevice.h
HostDriverTypes.h

# USBHost
HardwareIDs.h
HostManager.h

# CMakeLists.txt
if(EN_USB_HOST)
    message(STATUS "USB host enabled.")
    add_compile_definitions(CONFIG_EN_USB_HOST=1)
    list(APPEND SOURCES_BOARD
        ${SRC}/USBHost/tuh_callbacks.cpp
        # HID
        ${SRC}/USBHost/HostDriver/DInput/DInput.cpp
        ${SRC}/USBHost/HostDriver/PSClassic/PSClassic.cpp
        ${SRC}/USBHost/HostDriver/MyDevice/MyDevice.cpp




