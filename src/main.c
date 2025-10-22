#include <stdint.h>

/* ------------------- GPIO BASE ------------------- */
#define GPIO_BASE              0x60004000UL
#define GPIO_OUT_W1TS_REG           (*(volatile uint32_t*)(GPIO_BASE + 0x0008))
#define GPIO_OUT_W1TC_REG           (*(volatile uint32_t*)(GPIO_BASE + 0x000C))
#define GPIO_ENABLE_W1TS_REG        (*(volatile uint32_t*)(GPIO_BASE + 0x0024))
#define GPIO_FUNC3_OUT_SEL_CFG_REG  (*(volatile uint32_t*)(GPIO_BASE + 0x0554))

#define LED_GPIO 3
#define LED_MASK (1U << LED_GPIO)

/* ------------------- LED PWM BASE ------------------- */
#define LEDC_BASE_PWM           0x60019000UL //Definicion de la Base

// Activacion de los registro del Clock
#define SYSTEM_PERIP_CLK_EN0_REG    (*(volatile uint32_t*)(LEDC_BASE_PWM + 0x0010))
#define SYSTEM_PERIP_RST_EN0_REG    (*(volatile uint32_t*)(LEDC_BASE_PWM + 0x0018))

// Configuracion de los registros del Timer
#define LEDC_TIMER0_CONF_REG        (*(volatile uint32_t*)(LEDC_BASE_PWM + 0x00A0))
#define LEDC_CH0_HPOINT_REG         (*(volatile uint32_t*)(LEDC_BASE_PWM + 0x0004))
#define LEDC_CH0_DUTY_REG           (*(volatile uint32_t*)(LEDC_BASE_PWM + 0x0008))
#define LEDC_TIMER0_CONF_REG_DIV_NUM_S 4

// Configuracion del Canal 
#define LEDC_CH0_CONF0_REG          (*(volatile uint32_t*)(LEDC_BASE_PWM + 0x0000))
#define LEDC_CH0_CONF1_REG          (*(volatile uint32_t*)(LEDC_BASE_PWM + 0x000C))

//
#define GPIO_FUNC3_OUT_SEL_CFG_REG (*(volatile uint32_t*)(GPIO_BASE + 0x554 + 12))


#define SYSTEM_LEDC_CLK_EN        (1 << 3)   // bit 3, ejemplo
#define SYSTEM_LEDC_RST           (1 << 3)

/* ------------------- WDT BASES ------------------- */
#define TIMG0_BASE 0x6001F000UL
#define TIMG1_BASE 0x60020000UL
#define TIMG_WDTCONFIG0_OFFSET 0x0048
#define TIMG_WDTWPROTECT_OFFSET 0x0064
#define TIMG_WDT_UNLOCK_KEY 0x50D83AA1U

#define RTC_CNTL_BASE 0x60008000UL
#define RTC_CNTL_WDTCONFIG0_OFFSET 0x0090
#define RTC_CNTL_WDTWPROTECT_OFFSET 0x00A8
#define RTC_CNTL_SWD_CONF_OFFSET    0x00AC
#define RTC_CNTL_SWD_WPROTECT_OFFSET 0x00B0
#define RTC_CNTL_WDT_UNLOCK_KEY 0x50D83AA1U
#define RTC_CNTL_SWD_UNLOCK_KEY 0x8F1D312AU

/* ------------------- Funciones auxiliares ------------------- */
static void disable_timg_wdt(uint32_t timer_base) {
    volatile uint32_t *wdt_protect = (volatile uint32_t *)(timer_base + TIMG_WDTWPROTECT_OFFSET);
    volatile uint32_t *wdt_config0 = (volatile uint32_t *)(timer_base + TIMG_WDTCONFIG0_OFFSET);
    *wdt_protect = TIMG_WDT_UNLOCK_KEY;
    *wdt_config0 &= ~(1U << 31); // deshabilitar WDT
    *wdt_protect = 0;
}

static void disable_rtc_wdts(void) {
    volatile uint32_t *wdt_protect = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_WDTWPROTECT_OFFSET);
    volatile uint32_t *wdt_config0 = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_WDTCONFIG0_OFFSET);
    volatile uint32_t *swd_protect = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_SWD_WPROTECT_OFFSET);
    volatile uint32_t *swd_conf    = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_SWD_CONF_OFFSET);

    *wdt_protect = RTC_CNTL_WDT_UNLOCK_KEY;
    *wdt_config0 &= ~(1U << 31); // deshabilitar WDT principal
    *wdt_protect = 0;

    *swd_protect = RTC_CNTL_SWD_UNLOCK_KEY;
    *swd_conf |= (1U << 30); // deshabilitar super WDT
    *swd_protect = 0;
}

static void delay(volatile uint32_t cycles) {
    while (cycles--) {
        __asm__ volatile ("nop");
    }
}

/* ------------------- PROGRAMA PRINCIPAL ------------------- */
int main(void) {
    disable_timg_wdt(TIMG0_BASE);
    disable_timg_wdt(TIMG1_BASE);
    disable_rtc_wdts();

    // Habilitar GPIO3 como salida
    GPIO_ENABLE_W1TS_REG = LED_MASK;

    // PWM
    SYSTEM_PERIP_CLK_EN0_REG |= SYSTEM_LEDC_CLK_EN;     // habilita el clock
    SYSTEM_PERIP_RST_EN0_REG &= ~SYSTEM_LEDC_RST;       // saca el reset
    GPIO_FUNC3_OUT_SEL_CFG_REG = 160;
    LEDC_TIMER0_CONF_REG = (80 << LEDC_TIMER0_CONF_REG_DIV_NUM_S) | (10 << 0) | (1 << 31);
    LEDC_CH0_CONF0_REG = (0 << 0) | (1 << 31);
   

    /* ---------- BUCLE PRINCIPAL ---------- */
    while (1) {
        for (int d = 0; d < 1024; d++) {
            LEDC_CH0_DUTY_REG = (d << 4);
            LEDC_CH0_CONF1_REG |= (1 << 0); // duty_start
        delay(5000);
}
    for (int d = 1023; d >= 0; d--) {   
        LEDC_CH0_DUTY_REG = (d << 4);
        LEDC_CH0_CONF1_REG |= (1 << 0);
        delay(5000);
    }
}
}
