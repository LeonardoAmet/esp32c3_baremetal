# ESP32-C3 Bare-Metal: Potenciómetro en GPIO0 → LED en GPIO3 (umbral con ADC1)

Repositorio didáctico para **ESP32-C3 DevKit-M-1/DevKitC-02** que mide un potenciómetro en **GPIO0** usando el **SAR ADC1** en **modo one-shot** (atenuación fija **11 dB**) y enciende un **LED en GPIO3** al superar un **umbral**. Todo en **bare-metal** (sin ESP-IDF): inicialización mínima, watchdogs deshabilitados, acceso directo a **IO_MUX**, **GPIO** y **APB_SARADC**.

---

## 🧩 Objetivo

* Configurar **GPIO3** como salida (LED).
* Poner **GPIO0** en **modo analógico real** (sin entrada digital ni pulls; OE=0).
* Encender el bloque analógico del **SAR** y su reloj.
* Lanzar **conversiones one-shot** en **ADC1_CH0 (GPIO0)** con **11 dB**.
* Leer el dato de **`APB_SARADC_1_DATA_STATUS_REG` (0x2C)**, enmascarar a **12 bits (0..4095)**.
* Comparar con un **umbral** y conmutar el LED.

---

## 🛠️ Hardware y conexiones

* **ESP32-C3 DevKit**.
* **Potenciómetro 10 kΩ**:

  * Extremos a **3V3** y **GND**.
  * Cursor (**wiper**) a **GPIO0**.
* **LED** en **GPIO3** (interno en muchas placas; si usás uno externo, poner resistencia serie ~220–330 Ω).

> Importante: No uses **GPIO0** con resistencias **pull-up/down** activas; el código desactiva los pulls en **IO_MUX**.

---

## 📁 Estructura mínima

```
.
├─ include/
│  └─ WDT_FEED.h              # helpers para deshabilitar WDT (TIMG0/TIMG1/RTC)
├─ src/
│  ├─ startup.S               # SP, .bss, .data, salto a main
│  ├─ main.c                  # ESTE ejemplo ADC + LED
│  └─ (otros)
├─ linker.ld                  # código en IROM (0x4200_0000), datos en DRAM
├─ Makefile                   # rv32imc bare-metal + esptool.py
└─ README.md
```

---

## 🧱 Build & flash

Requisitos: **riscv32-esp-elf-gcc**, **esptool.py**.

```bash
make           # compila, enlaza, genera .elf/.bin/.dis y muestra tamaños
make flash     # empaqueta con elf2image y flashea la app en 0x10000
make clean
```

> Este flujo asume que ya tenés **bootloader** y **partition table** en la flash (típico de una placa con IDF grabado). Si no arranca, flasheá bootloader/particiones con IDF una vez y luego usá este bare-metal.

---

## 🧠 Cómo funciona (paso a paso)

1. **Startup & Linker**
   `startup.S` inicializa **SP**, limpia **.bss**, copia **.data** desde flash y llama a `main`.
   `linker.ld` coloca **.text** en **0x4200_0000 (XIP)** y **datos/stack** en **DRAM** (0x3FC8_0000).

2. **Watchdogs OFF**
   `disable_timg_wdt(TIMG0/1)` y `disable_rtc_wdts()` para evitar reset en bucles.

3. **GPIO3 salida (LED)**
   Se limpia `FUN_IE/WPU/WPD` en `IO_MUX_GPIO3_REG`, y se activa OE con `GPIO_ENABLE_W1TS`.

4. **GPIO0 analógico**
   En `IO_MUX_GPIO0_REG`: `FUN_IE=0`, `WPU/WPD=0`, `OE=0` en `GPIO_ENABLE_W1TC`.
   Así el pin queda en alta impedancia digital → **no contamina** la medición analógica.

5. **ADC on + reloj**
   En `SYSTEM_PERIP_CLK_EN0` se habilita **APB_SARADC_CLK_EN**.
   Pulso de reset en `SYSTEM_PERIP_RST_EN0`.
   En `APB_SARADC_CTRL_REG`:

   * `SAR_CLK_GATED=1`, `SAR_CLK_DIV=4` (frecuencia interna estable)
   * **`XPD_SAR_FORCE=3`** → fuerza **ON** el bloque analógico.

6. **Config one-shot**
   En `APB_SARADC_ONETIME_SAMPLE_REG`:

   * `ADC1_ONETIME_SAMPLE=1`
   * `CHANNEL=0` (**GPIO0**), `ATTEN=3` (**11 dB**, ~3.3 V rango)
   * `START=0` (reposo)

7. **Disparo con flanco y espera por DONE**
   En cada muestra:

   * Escribir **START=0**, pequeña espera, luego **START=1** (flanco).
   * **Poll** de `APB_SARADC_INT_ST_REG.bit31 (ADC1_DONE)` hasta que termine.
   * Leer **`APB_SARADC_1_DATA_STATUS_REG (0x2C)`**, enmascarar **12 bits**.
   * Limpiar `INT_CLR`.

8. **Umbral y LED**
   Si `sample ≥ ADC_THRESHOLD` (p.ej. **2000**), LED **ON**, si no **OFF**.

---

## 🔧 Parámetros ajustables

* `ADC_ATTEN_11DB` (fijada a 11 dB).
* `ADC_THRESHOLD` (0..4095).
* `LOOP_DELAY` (NOPs entre lecturas; sólo para espaciar).

---

## ✅ Verificación esperada

* **Wiper a GND** → `sample ≈ 0` → LED **apagado**.
* **Wiper a 3V3** → `sample ≈ 4095` → LED **encendido**.
* Moviendo el pote, el valor crudo cambia suave; el LED conmuta al cruzar el **umbral**.

---

## 🧪 Troubleshooting

* LED no cambia:

  * Revisá **conexiones** del pote (extremos a 3V3/GND, wiper a GPIO0).
  * Confirmá que **IO_MUX_GPIO0** tiene **`FUN_IE/WPU/WPD=0`** y **OE=0**.
  * Verificá que el dato salga por **0x2C** (no 0x30).
* Lecturas **0** constantes:

  * Chequear **`XPD_SAR_FORCE=3`** (force ON) y **clock** habilitado.
  * Asegurar **flanco START** (0→1) y **poll de DONE** + **INT_CLR**.
* Ruido:

  * Subí el divisor de reloj (`SAR_CLK_DIV`) o promediá varias muestras.

---

## 📝 Nota didáctica (qué nos trababa antes)

El SAR del C3 **no convierte** si el bloque analógico no está **encendido** (**`XPD_SAR_FORCE`**) y si no recibe un **flanco de START** válido. Además, la **ruta de dato** puede ser **0x2C** (no 0x30) según revisión. Al **clonar** exactamente el esquema de **CTRL/ONETIME/INTS** que usa el driver del IDF, el bare-metal queda **estable y determinista**.

---

¡Listo! Con esto tenés un punto de partida claro, reproducible y fácil de explicar en tu presentación.

