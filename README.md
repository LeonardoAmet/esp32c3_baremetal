ESP32-C3 Bare-Metal – Potenciómetro → LED (GPIO/ADC)

Proyecto didáctico bare-metal (sin ESP-IDF) para ESP32-C3 DevKit.
Objetivo: al girar un potenciómetro, si supera un umbral, encender LED en GPIO3.
Dos modos conmutables por macro: digital (GPIO1) y ADC (ADC1_CH1 en GPIO1).

🧩 Estructura
.
├─ Makefile
├─ linker.ld
├─ include/
│  └─ WDT_FEED_DEFINES.h    # tus rutinas para deshabilitar WDTs (TIMG/RTC)
└─ src/
   ├─ startup.S              # arranque mínimo: stack, .bss, .data, salto a main
   └─ main.c                 # lógica: modo digital/ADC seleccionable por macro

🛠️ Requisitos

Toolchain: riscv32-esp-elf-gcc (Espressif).

esptool.py en PATH.

DevKit ESP32-C3 con bootloader + tabla de particiones en 0x0.

⚙️ Compilar y flashear
make           # compila y enlaza (genera .elf, .map, .dis)
make flash     # empaqueta y flashea la app en 0x10000
make clean


El flash asume bootloader/partitions vigentes en 0x0 (típico DevKit).
Si no arranca, reflasheá bootloader/partitions con ESP-IDF y volvé a make flash.

🔌 Conexiones

LED: en GPIO3 (salida).

Potenciómetro 10 k:

Un extremo a 3V3.

El otro a GND.

Wiper a GPIO1 (modo por defecto).
(Si probás GPIO0 en algún experimento, ajustá el código; el README asume GPIO1).

🧭 Modos de operación (macro)

Abrí src/main.c y tocá sólo esta macro:

#define MODE_ADC  0   // 0 = Modo DIGITAL (recomendado en tu placa)
                      // 1 = Modo ADC (didáctico: ADC1_CH1 en GPIO1)

Modo DIGITAL (recomendado hoy)

Usa GPIO1 como entrada digital (Schmitt trigger).

Filtro de ventana 64 muestras + histéresis:

#define DIG_TH_ON   48   // pasa a 1 si >=48/64 altas  (~75%)
#define DIG_TH_OFF  16   // vuelve a 0 si <=16/64      (~25%)


Resultado: al cruzar “la mitad” del pote, el LED en GPIO3 se enciende estable.

Modo ADC (didáctico)

Usa ADC1_CH1 → GPIO1, one-shot con atenuación alta (~3.3 V).

Umbral en cuentas:

#define ADC_UMBRAL  1900   // 0..4095


En tu DevKit el camino analógico no arrojó lecturas válidas durante el práctico; se deja este modo para reusar en otro C3 donde ADC1_CH1 esté expuesto.

⏱️ Calibración de tiempos

Los delays son busy-wait (NOPs): calibrá una sola vez con:

#define CALIB_K  5000u   // más grande = más lento


Regla práctica: los 3 blinks iniciales deberían durar ~1 s total.
Si ves que tardan mucho, bajá CALIB_K; si van muy rápido, subí CALIB_K.

🧪 Cómo probar rápido

Conectá el pote (GND/3V3 wiper→GPIO1).

Dejá MODE_ADC 0 (digital).

make flash. Mirá 3 blinks de vida (calibrá CALIB_K si hace falta).

Girá el pote: al pasar por la zona media, el LED en GPIO3 debe encender.

¿Querés que prenda “más tarde”? Subí DIG_TH_ON (y quizá DIG_TH_OFF para mantener histéresis).

¿Demasiado sensible? Bajá un poco DIG_TH_ON y ajustá DIG_TH_OFF.

(Opcional) Probá MODE_ADC 1: si tu placa expone ADC1_CH1 a GPIO1, al superar ADC_UMBRAL el LED encenderá.

🧯 Watchdogs

Se deshabilitan TIMG0/TIMG1 WDT y RTC_CNTL WDT/SWD en WDT_FEED_DEFINES.h para evitar resets durante loops de prueba.
Registros tocados (resumen):

TIMGx: WDTWPROTECT, WDTCONFIG0..5, WDTFEED.

RTC_CNTL: WDTCONFIG0..4, WDTFEED, WDTWPROTECT, SWD_CONF, SWD_WPROTECT.

🧵 GPIO / IO_MUX / ADC (registros clave)

GPIO base 0x60004000

GPIO_OUT_W1TS_REG / ...W1TC_REG: set/clear salida.

GPIO_ENABLE_W1TS_REG / ...W1TC_REG: habilitar OE.

GPIO_IN_REG: leer entradas.

IO_MUX base 0x60009000

IO_MUX_GPIOn_REG(n) = BASE + 0x0004*(n+1)

Bits: FUN_WPD(7), FUN_WPU(8), FUN_IE(9), MCU_SEL[14:12].

Digital: IE=1, sin pulls; Analógico: IE=0, sin pulls, OE=0.

SYSTEM 0x600C0000

PERIP_CLK_EN0: bit28 APB_SARADC_CLK_EN.

PERIP_RST_EN0: bit28 APB_SARADC_RST.

APB_SARADC 0x60040000

CTRL (0x0000): START_FORCE, START, SAR_CLK_GATED, SAR_CLK_DIV.

ONETIME_SAMPLE_REG (0x0020): ADC1_ONETIME_SAMPLE, CHANNEL[24:22] (CH1), ATTEN[25:24], ONETIME_START.

ADC1_DATA_STATUS_REG (0x002C): 12 bits de dato.

🔗 Startup y Linker (resumen)

startup.S

Setea SP con _stack_top.

Limpia .bss (_sbss.._ebss).

Copia .data desde _sidata (FLASH) a _sdata.._edata (DRAM).

call main y lazo infinito si retorna.

linker.ld

IROM (0x4200_0000) → .text

DROM (0x3C00_0000) → .rodata

DRAM (0x3FC8_0000) → .data, .bss, stack

PROVIDE(_sidata = LOADADDR(.data)), PROVIDE(_stack_top = ORIGIN(DRAM)+LENGTH(DRAM))

Cuidar no solapar LMA entre .rodata y .data.

🧰 Troubleshooting

Se queda “en bootloader” o abort: revisá startup.S y linker.ld (símbolos _stack_top, _sidata, .data AT de .rodata).

Guru Meditation / abort(): típico de secciones mal ubicadas o acceso fuera de rango; validá con objdump -h/-t.

LED no enciende:

Probá blink de vida justo antes del loop para aislar si GPIO3 está bien.

En modo digital, leé GPIO_IN_REG (bit de GPIO1) y ajustá DIG_TH_*.

Blinks lentísimos: calibrá CALIB_K (bajalo).

Modo ADC no responde: no insistas—en tu DevKit el camino analógico no está expuesto en GPIO1; quedate con el modo digital.

📌 Notas finales

El modo digital cumple el enunciado “umbral → LED” y quedó estable.

El modo ADC se deja para reuso en otra placa C3 (mismo main.c, solo cambiás MODE_ADC a 1).

Este repo es ideal para entender ABI mínima, startup/linker, mapeo de registros, y GPIO/IO_MUX/ADC sin ESP-IDF.
