// ESP32-C3 bare-metal — GPIO1: modo DIGITAL o ADC seleccionable por macro
// LED en GPIO3. 3 blinks rápidos al inicio.

// ======= SELECCIÓN DE MODO =======
#define MODE_ADC   0      // 0 = modo DIGITAL (recomendado) | 1 = modo ADC (no funciona)

// ======= TIMING CALIBRABLE =======
#ifndef CALIB_K
#define CALIB_K  5000u    // más grande = más lento. Ajustar hasta que 3 blinks duren ~1 s total.
#endif

// ======= UMBRALES =======
// Digital (ventana 64 muestras con histéresis)
#define DIG_TH_ON   48     // pasa a 1 si >=48/64 altas
#define DIG_TH_OFF  16     // vuelve a 0 si <=16/64 altas
// ADC (cuentas 0..4095 con atenuación alta)
#define ADC_UMBRAL  1900

// ======= BASES / REGISTROS =======
#include <stdint.h>
#include "WDT_FEED_DEFINES.h"

#define DR_REG_GPIO_BASE          0x60004000UL
#define DR_REG_IO_MUX_BASE        0x60009000UL
#define DR_REG_SYSTEM_BASE        0x600C0000UL
#define DR_REG_APB_SARADC_BASE    0x60040000UL

// GPIO
#define GPIO_OUT_W1TS_REG    (DR_REG_GPIO_BASE + 0x0008)
#define GPIO_OUT_W1TC_REG    (DR_REG_GPIO_BASE + 0x000C)
#define GPIO_ENABLE_W1TS_REG (DR_REG_GPIO_BASE + 0x0024)
#define GPIO_ENABLE_W1TC_REG (DR_REG_GPIO_BASE + 0x0028)
#define GPIO_IN_REG          (DR_REG_GPIO_BASE + 0x003C)

// IO_MUX
#define IO_MUX_GPIOn_REG(n)  (DR_REG_IO_MUX_BASE + 0x0004u * ((n) + 1))
#define IO_MUX_GPIO1_REG     IO_MUX_GPIOn_REG(1)
#define IO_MUX_FUN_WPD       (1u << 7)
#define IO_MUX_FUN_WPU       (1u << 8)
#define IO_MUX_FUN_IE        (1u << 9)
#define IO_MUX_MCU_SEL_S     12
#define IO_MUX_MCU_SEL_M     (0x7u << IO_MUX_MCU_SEL_S)
#define IO_MUX_MCU_SEL(v)    (((v)&0x7u) << IO_MUX_MCU_SEL_S)

// SYSTEM (clock/reset ADC)
#define SYSTEM_PERIP_CLK_EN0_REG     (DR_REG_SYSTEM_BASE + 0x000C)
#define SYSTEM_PERIP_RST_EN0_REG     (DR_REG_SYSTEM_BASE + 0x0010)
#define SYSTEM_APB_SARADC_CLK_EN     (1u << 28)
#define SYSTEM_APB_SARADC_RST        (1u << 28)

// APB_SARADC (one-shot simple)
#define APB_SARADC_CTRL_REG            (DR_REG_APB_SARADC_BASE + 0x0000)
#define APB_SARADC_ONETIME_SAMPLE_REG  (DR_REG_APB_SARADC_BASE + 0x0020)  // “A”
#define APB_SARADC_1_DATA_STATUS_REG   (DR_REG_APB_SARADC_BASE + 0x002C)  // “A”

// CTRL fields
#define APB_SARADC_START_FORCE   (1u << 0)
#define APB_SARADC_START         (1u << 1)
#define APB_SARADC_SAR_CLK_GATED (1u << 6)
#define APB_SARADC_SAR_CLK_DIV_S 7
#define APB_SARADC_SAR_CLK_DIV_M (0xFFu << APB_SARADC_SAR_CLK_DIV_S)
#define APB_SARADC_SAR_CLK_DIV(v) (((v)&0xFFu) << APB_SARADC_SAR_CLK_DIV_S)

// ONE-TIME fields
#define APB_SARADC1_ONETIME_SAMPLE    (1u << 0)
#define APB_SARADC_ONETIME_START      (1u << 2)
#define APB_SARADC_ONETIME_CHANNEL(ch) (((ch)&0x7u) << 22)   // CH1 = GPIO1
#define APB_SARADC_ONETIME_ATTEN(att)  (((att)&0x3u) << 24) // 3 ≈ 11/12 dB

// ======= UTIL =======
static inline void delay(volatile uint32_t n){ while(n--) __asm__ volatile("nop"); }
static inline void msleep(unsigned ms){ while(ms--) delay(CALIB_K); }

// LED en GPIO3
static inline void gpio3_as_output(void){ *(volatile uint32_t*)GPIO_ENABLE_W1TS_REG = (1u<<3); }
static inline void led_on(void) { *(volatile uint32_t*)GPIO_OUT_W1TS_REG = (1u<<3); }
static inline void led_off(void){ *(volatile uint32_t*)GPIO_OUT_W1TC_REG = (1u<<3); }
static void blink3(void){ for(int i=0;i<3;i++){ led_on(); msleep(80); led_off(); msleep(80);} }

// ======= DIGITAL (GPIO1 como entrada con filtro + histéresis) =======
static void gpio1_as_input_digital(void){
    volatile uint32_t* mux = (volatile uint32_t*)IO_MUX_GPIO1_REG;
    uint32_t v = *mux;
    v &= ~IO_MUX_MCU_SEL_M;
    v |=  IO_MUX_MCU_SEL(1);                  // función GPIO
    v |=  IO_MUX_FUN_IE;                      // habilitar entrada digital
    v &= ~(IO_MUX_FUN_WPU | IO_MUX_FUN_WPD);  // sin pulls
    *mux = v;
    *(volatile uint32_t*)GPIO_ENABLE_W1TC_REG = (1u << 1); // OE=0 (entrada)
}

static void loop_digital_umbral(void){
    const unsigned PINBIT = (1u << 1); // GPIO1
    uint64_t shift=0; uint8_t sum=0, state=0;

    for(;;){
        uint32_t in = *(volatile uint32_t*)GPIO_IN_REG;
        uint8_t bit = (in & PINBIT) ? 1u : 0u;

        sum -= (uint8_t)(shift & 1u);
        shift = (shift >> 1) | ((uint64_t)bit << 63);
        sum += bit;

        if (!state && sum >= DIG_TH_ON)  state = 1;
        if ( state && sum <= DIG_TH_OFF) state = 0;

        if (state) led_on(); else led_off();

        delay(CALIB_K/8); // ~sampling rate
    }
}

// ======= ADC (GPIO1 como analógico, ADC1_CH1) =======
static void enable_adc_module(void){
    volatile uint32_t *clk=(volatile uint32_t*)SYSTEM_PERIP_CLK_EN0_REG;
    volatile uint32_t *rst=(volatile uint32_t*)SYSTEM_PERIP_RST_EN0_REG;
    *clk |= SYSTEM_APB_SARADC_CLK_EN;
    *rst &= ~SYSTEM_APB_SARADC_RST;
}

static void gpio1_to_analog(void){
    volatile uint32_t *mux=(volatile uint32_t*)IO_MUX_GPIO1_REG;
    uint32_t v=*mux;
    v &= ~IO_MUX_MCU_SEL_M;
    v |=  IO_MUX_MCU_SEL(1);                                // función GPIO (vale con tal de apagar digital)
    v &= ~(IO_MUX_FUN_IE | IO_MUX_FUN_WPU | IO_MUX_FUN_WPD);// sin digital/pulls
    *mux = v;
    *(volatile uint32_t*)GPIO_ENABLE_W1TC_REG = (1u<<1);    // OE=0
}

static inline void saradc_ctrl_basic(void){
    volatile uint32_t *ctrl=(volatile uint32_t*)APB_SARADC_CTRL_REG;
    uint32_t c=*ctrl;
    c |= APB_SARADC_START_FORCE | APB_SARADC_SAR_CLK_GATED;
    c &= ~APB_SARADC_SAR_CLK_DIV_M;
    c |= APB_SARADC_SAR_CLK_DIV(4); // robusto
    *ctrl = c;
}

static uint16_t adc1_read_ch1_avg32(void){
    volatile uint32_t *one  =(volatile uint32_t*)APB_SARADC_ONETIME_SAMPLE_REG;
    volatile uint32_t *data =(volatile uint32_t*)APB_SARADC_1_DATA_STATUS_REG;
    volatile uint32_t *ctrl =(volatile uint32_t*)APB_SARADC_CTRL_REG;

    uint32_t acc=0;
    for (int i=0;i<32;i++){
        saradc_ctrl_basic();
        uint32_t cfg = APB_SARADC1_ONETIME_SAMPLE
                     | APB_SARADC_ONETIME_CHANNEL(1)   // CH1 = GPIO1
                     | APB_SARADC_ONETIME_ATTEN(3);    // ~3.3V
        *one = cfg;
        *one = cfg | APB_SARADC_ONETIME_START;

        delay(CALIB_K/4);
        *ctrl = (*ctrl) | APB_SARADC_START;
        delay(CALIB_K/3);

        acc += (*data & 0x0FFFu);
    }
    return (uint16_t)(acc/32u);
}

static void loop_adc_umbral(void){
    for(;;){
        uint16_t val = adc1_read_ch1_avg32();   // 0..4095 (suele “quedar 0”)
        if (val >= ADC_UMBRAL) led_on(); else led_off();
    }
}

// ======= main =======
int main(void){
    // WDTs off
    disable_timg_wdt(TIMG0_BASE);
    disable_timg_wdt(TIMG1_BASE);
    disable_rtc_wdts();

    // LED listo + handshake
    gpio3_as_output();
    blink3();  // ajustá CALIB_K una sola vez

#if (MODE_ADC==0)
    // ===== MODO DIGITAL =====
    gpio1_as_input_digital();
    loop_digital_umbral();

#else
    // ===== MODO ADC (didáctico) =====
    enable_adc_module();
    gpio1_to_analog();
    loop_adc_umbral();
#endif
}