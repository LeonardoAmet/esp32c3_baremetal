#include <stdint.h>
#include "WDT_FEED.h"

#define BIT(n) (1U << (n))                    // Máscara de un bit
#define REG32(addr) (*(volatile uint32_t *)(addr)) // Acceso directo a registro de 32 bits

#define DR_REG_GPIO_BASE        0x60004000UL  // Base periférico GPIO
#define DR_REG_IO_MUX_BASE      0x60009000UL  // Base IO_MUX (selección de función/pulls)
#define DR_REG_SYSTEM_BASE      0x600C0000UL  // Base registro de sistema (clocks/resets)
#define DR_REG_APB_SARADC_BASE  0x60040000UL  // Base ADC SAR digital

#define GPIO_OUT_W1TS_REG   (DR_REG_GPIO_BASE + 0x0008)  // Set pin high (write-1-to-set)
#define GPIO_OUT_W1TC_REG   (DR_REG_GPIO_BASE + 0x000C)  // Set pin low  (write-1-to-clear)
#define GPIO_ENABLE_W1TS_REG (DR_REG_GPIO_BASE + 0x0024) // Habilitar OE
#define GPIO_ENABLE_W1TC_REG (DR_REG_GPIO_BASE + 0x0028) // Deshabilitar OE

#define IO_MUX_GPIO0_REG    (DR_REG_IO_MUX_BASE + 0x0004) // IO_MUX para GPIO0 (ADC)
#define IO_MUX_GPIO3_REG    (DR_REG_IO_MUX_BASE + 0x0010) // IO_MUX para GPIO3 (LED)
#define IO_MUX_FUN_IE       BIT(9)   // Input enable digital
#define IO_MUX_FUN_PU       BIT(8)   // Pull-up digital
#define IO_MUX_FUN_PD       BIT(7)   // Pull-down digital
#define IO_MUX_MCU_SEL_MASK (0x7U << 12) // Selector de función
#define IO_MUX_MCU_SEL_GPIO 1U       // Función GPIO

#define SYSTEM_PERIP_CLK_EN0_REG (DR_REG_SYSTEM_BASE + 0x0010) // Registro de clocks
#define SYSTEM_PERIP_RST_EN0_REG (DR_REG_SYSTEM_BASE + 0x0018) // Registro de resets
#define SYSTEM_APB_SARADC_CLK_EN BIT(28) // Bit de clock para ADC SAR
#define SYSTEM_APB_SARADC_RST    BIT(28) // Bit de reset para ADC SAR

#define APB_SARADC_CTRL_REG            (DR_REG_APB_SARADC_BASE + 0x0000) // Control general ADC
#define APB_SARADC_START_FORCE         BIT(0)  // Forzar arranque digital
#define APB_SARADC_START               BIT(1)  // Señal start SW
#define APB_SARADC_SAR_CLK_GATED       BIT(6)  // Clock gated para SAR
#define APB_SARADC_SAR_CLK_DIV_S       7       // Shift divisor clock
#define APB_SARADC_SAR_CLK_DIV_M       (0xFFU << APB_SARADC_SAR_CLK_DIV_S)
#define APB_SARADC_XPD_SAR_FORCE_S     27      // Shift modo power
#define APB_SARADC_XPD_SAR_FORCE_M     (0x3U << APB_SARADC_XPD_SAR_FORCE_S)

#define APB_SARADC_ONETIME_SAMPLE_REG  (DR_REG_APB_SARADC_BASE + 0x0020) // Control oneshot
#define APB_SARADC1_ONETIME_SAMPLE     BIT(31) // Selecciona ADC1
#define APB_SARADC_ONETIME_START       BIT(29) // Lanzar conversión
#define APB_SARADC_ONETIME_CHANNEL_S   25      // Shift canal
#define APB_SARADC_ONETIME_CHANNEL_M   (0xFU << APB_SARADC_ONETIME_CHANNEL_S)
#define APB_SARADC_ONETIME_ATTEN_S     23      // Shift atenuación
#define APB_SARADC_ONETIME_ATTEN_M     (0x3U << APB_SARADC_ONETIME_ATTEN_S)

#define APB_SARADC_1_DATA_STATUS_REG   (DR_REG_APB_SARADC_BASE + 0x002C) // Resultado ADC1

#define APB_SARADC_INT_ENA_REG         (DR_REG_APB_SARADC_BASE + 0x0040) // Enable de flags
#define APB_SARADC_ADC1_DONE_INT_ENA   BIT(31) // Habilita flag ADC1 done
#define APB_SARADC_INT_ST_REG          (DR_REG_APB_SARADC_BASE + 0x0048) // Estado de flags
#define APB_SARADC_ADC1_DONE_INT_ST    BIT(31) // Flag ADC1 conversión terminada
#define APB_SARADC_INT_CLR_REG         (DR_REG_APB_SARADC_BASE + 0x004C) // Clear de flags
#define APB_SARADC_ADC1_DONE_INT_CLR   BIT(31) // Limpia flag done

#define LED_GPIO        3U
#define POT_GPIO        0U
#define LED_MASK        BIT(LED_GPIO)
#define POT_MASK        BIT(POT_GPIO)

#define ADC_ATTEN_11DB  3U
#define ADC_THRESHOLD   2000U
#define LOOP_DELAY      5000U

static void gpio_init(void) {
    // GPIO3 como salida digital para el LED (sin pulls, solo función GPIO)
    uint32_t reg = REG32(IO_MUX_GPIO3_REG);
    reg &= ~(IO_MUX_FUN_IE | IO_MUX_FUN_PU | IO_MUX_FUN_PD | IO_MUX_MCU_SEL_MASK);
    reg |= (IO_MUX_MCU_SEL_GPIO << 12);
    REG32(IO_MUX_GPIO3_REG) = reg;
    REG32(GPIO_ENABLE_W1TS_REG) = LED_MASK;

    // GPIO0 en modo analógico (sin OE ni pulls) para el potenciómetro
    reg = REG32(IO_MUX_GPIO0_REG);
    reg &= ~(IO_MUX_FUN_IE | IO_MUX_FUN_PU | IO_MUX_FUN_PD | IO_MUX_MCU_SEL_MASK);
    REG32(IO_MUX_GPIO0_REG) = reg;
    REG32(GPIO_ENABLE_W1TC_REG) = POT_MASK;
}

static void adc_init(void) {
    // Clock/reset del SARADC
    REG32(SYSTEM_PERIP_CLK_EN0_REG) |= SYSTEM_APB_SARADC_CLK_EN;
    REG32(SYSTEM_PERIP_RST_EN0_REG) |= SYSTEM_APB_SARADC_RST;
    REG32(SYSTEM_PERIP_RST_EN0_REG) &= ~SYSTEM_APB_SARADC_RST;

    // Forzar ADC encendido, activar clock y fijar divisor
    uint32_t ctrl = REG32(APB_SARADC_CTRL_REG);
    ctrl |= APB_SARADC_SAR_CLK_GATED;
    ctrl &= ~APB_SARADC_XPD_SAR_FORCE_M;
    ctrl |= (3U << APB_SARADC_XPD_SAR_FORCE_S);
    ctrl &= ~APB_SARADC_SAR_CLK_DIV_M;
    ctrl |= (4U << APB_SARADC_SAR_CLK_DIV_S);
    ctrl &= ~(APB_SARADC_START_FORCE | APB_SARADC_START);
    REG32(APB_SARADC_CTRL_REG) = ctrl;

    // Configurar canal 0 con atenuación 11 dB (full scale ~3.3 V)
    uint32_t sample = REG32(APB_SARADC_ONETIME_SAMPLE_REG);
    sample |= APB_SARADC1_ONETIME_SAMPLE;
    sample &= ~APB_SARADC_ONETIME_CHANNEL_M;
    sample |= (0U << APB_SARADC_ONETIME_CHANNEL_S);
    sample &= ~APB_SARADC_ONETIME_ATTEN_M;
    sample |= (ADC_ATTEN_11DB << APB_SARADC_ONETIME_ATTEN_S);
    sample &= ~APB_SARADC_ONETIME_START;
    REG32(APB_SARADC_ONETIME_SAMPLE_REG) = sample;

    // Habilitar y limpiar flag de conversión terminada
    REG32(APB_SARADC_INT_ENA_REG) |= APB_SARADC_ADC1_DONE_INT_ENA;
    REG32(APB_SARADC_INT_CLR_REG) = APB_SARADC_ADC1_DONE_INT_CLR;
}

static uint16_t adc_sample_once(void) {
    // Pulso de start (low→high) para disparar conversión oneshot
    uint32_t sample = REG32(APB_SARADC_ONETIME_SAMPLE_REG);
    sample &= ~APB_SARADC_ONETIME_START;
    REG32(APB_SARADC_ONETIME_SAMPLE_REG) = sample;
    for (volatile uint32_t i = 0; i < 32; ++i) {
        __asm__ volatile("nop");
    }
    sample |= APB_SARADC_ONETIME_START;
    REG32(APB_SARADC_ONETIME_SAMPLE_REG) = sample;

    while ((REG32(APB_SARADC_INT_ST_REG) & APB_SARADC_ADC1_DONE_INT_ST) == 0U) {
    }

    // Capturo 12 bits útiles y limpio flag
    uint32_t raw = REG32(APB_SARADC_1_DATA_STATUS_REG) & 0x1FFFFU;
    REG32(APB_SARADC_INT_CLR_REG) = APB_SARADC_ADC1_DONE_INT_CLR;
    return (uint16_t)(raw & 0x0FFFU);
}

static void short_delay(void) {
    // Busy-wait simple (no timers configurados)
    for (volatile uint32_t i = 0; i < LOOP_DELAY; ++i) {
        __asm__ volatile("nop");
    }
}

int main(void) {
    // Deshabilitar watchdogs para bucle infinito didáctico
    disable_timg_wdt(TIMG0_BASE);
    disable_timg_wdt(TIMG1_BASE);
    disable_rtc_wdts();

    // Inicializaciones básicas de GPIO y ADC
    gpio_init();
    adc_init();

    // Loop principal: muestrea ADC y enciende/apaga según umbral
    while (1) {
        uint16_t sample = adc_sample_once();
        if (sample >= ADC_THRESHOLD) {
            REG32(GPIO_OUT_W1TS_REG) = LED_MASK;
        } else {
            REG32(GPIO_OUT_W1TC_REG) = LED_MASK;
        }
        short_delay();
    }
}
