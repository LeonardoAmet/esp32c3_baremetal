#include <stdint.h>

/* ------------------- DEFINICIONES DE REGISTROS ------------------- */

// --- GPIO ---
#define GPIO_BASE                   0x60004000UL
#define GPIO_ENABLE_W1TS_REG        (*(volatile uint32_t*)(GPIO_BASE + 0x0024))
#define GPIO_FUNC3_OUT_SEL_CFG_REG  (*(volatile uint32_t*)(GPIO_BASE + 0x0560)) // Offset 0x554 + 4*3

// --- IO MUX ---
#define IO_MUX_BASE                 0x60009000UL
#define IO_MUX_GPIO3_REG            (*(volatile uint32_t*)(IO_MUX_BASE + 0x0010)) // 0x0004 + 4*3
#define IO_MUX_FUNC_SEL_GPIO_3      1 // Función 1 es GPIO (usar Matriz GPIO)
#define IO_MUX_MCU_SEL_S            12 // Shift para el campo MCU_SEL [14:12]

#define LED_GPIO 3
#define LED_MASK (1U << LED_GPIO)

// --- SYSTEM/CLOCK ---
#define SYSTEM_REG_BASE             0x600C0000UL 
#define SYSTEM_PERIP_CLK_EN0_REG    (*(volatile uint32_t*)(SYSTEM_REG_BASE + 0x0010))
#define SYSTEM_PERIP_RST_EN0_REG    (*(volatile uint32_t*)(SYSTEM_REG_BASE + 0x0018))
#define SYSTEM_LEDC_CLK_EN          (1 << 11) // Bit 11 para LEDC
#define SYSTEM_LEDC_RST             (1 << 11) // Bit 11 para LEDC

// --- LEDC ---
#define LEDC_BASE_PWM               0x60019000UL
#define LEDC_CONF_REG               (*(volatile uint32_t*)(LEDC_BASE_PWM + 0x00D0))
#define LEDC_CLK_EN_B               (1 << 31) // Habilita reloj de registros LEDC
#define LEDC_APB_CLK_SEL_S          0         // Shift para seleccionar fuente de clock [bits 1:0]

// Registros del Timer
#define LEDC_TIMER0_CONF_REG        (*(volatile uint32_t*)(LEDC_BASE_PWM + 0x00A0))
#define LEDC_TIMER_RST_B            (1 << 23) // Bit de Reset
#define LEDC_TIMER_PARA_UP_B        (1 << 25) // Bit de actualización de parámetros
#define LEDC_TIMER_DIV_NUM_S        4         // Shift para divisor (Bits 21:4)

// Registros del Canal
#define LEDC_CH0_CONF0_REG          (*(volatile uint32_t*)(LEDC_BASE_PWM + 0x0000))
#define LEDC_CH0_HPOINT_REG         (*(volatile uint32_t*)(LEDC_BASE_PWM + 0x0004))
#define LEDC_CH0_DUTY_REG           (*(volatile uint32_t*)(LEDC_BASE_PWM + 0x0008))
#define LEDC_SIG_OUT_EN_B           (1 << 2)  // Bit de habilitación de salida
#define LEDC_TIMER_SEL_S            0         // Shift [bits 1:0]
#define LEDC_PARA_UP_CH0_B          (1 << 4)  // Bit de actualización de parámetros del canal

/* ------------------- WDT (sin cambios) ------------------- */
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

/* ------------------- Funciones auxiliares (WDT) ------------------- */
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

/* ------------------- PROGRAMA PRINCIPAL (PWM FADE) ------------------- */
int main(void) {
    // 1. Deshabilitar Watchdogs
    disable_timg_wdt(TIMG0_BASE);
    disable_timg_wdt(TIMG1_BASE);
    disable_rtc_wdts();

    // 2. Habilitar la salida en el pin GPIO3 (permiso de salida)
    GPIO_ENABLE_W1TS_REG = LED_MASK;

    // 3. Configurar el IO MUX para que el pin 3 use la función de la matriz GPIO
    // El campo MCU_SEL [14:12] debe ser 1 (IO_MUX_FUNC_SEL_GPIO_3)
    IO_MUX_GPIO3_REG = (IO_MUX_GPIO3_REG & ~(0b111 << IO_MUX_MCU_SEL_S)) | (IO_MUX_FUNC_SEL_GPIO_3 << IO_MUX_MCU_SEL_S);
    
    // 4. Habilitar clock y sacar del reset al periférico LEDC
    SYSTEM_PERIP_CLK_EN0_REG |= SYSTEM_LEDC_CLK_EN;
    SYSTEM_PERIP_RST_EN0_REG &= ~SYSTEM_LEDC_RST;

    // 5. Configurar reloj global de LEDC (habilitar y seleccionar APB_CLK)
    // Registro 32.13. [cite_start]LEDC_CONF_REG [cite: 1328]
    // Bit 31 (LEDC_CLK_EN) = 1 (Habilita clock de registros)
    // Bits 1:0 (LEDC_APB_CLK_SEL) = 1 (Usa APB_CLK como fuente) [cite: 1325]
    LEDC_CONF_REG = LEDC_CLK_EN_B | (1 << LEDC_APB_CLK_SEL_S);

    // 6. Mapear la señal de salida del canal 0 del LEDC (índice 45) al GPIO3 en la matriz GPIO
    // Tabla 5.11-1: Señal 45 es ledc_ls_sig_out0 [cite: 876]
    // Registro 5.17: GPIO_FUNCn_OUT_SEL_CFG_REG [cite: 886]
    GPIO_FUNC3_OUT_SEL_CFG_REG = 45;

    // 7. Configurar el Timer 0 del LEDC
    // Registro 32.7: LEDC_TIMERx_CONF_REG [cite: 1331]
    // Divisor=80 (asumiendo APB_CLK=80MHz, f_ref = 1MHz)
    // Resolución=10 bits (2^10 = 1024 pasos)
    LEDC_TIMER0_CONF_REG = (80 << LEDC_TIMER_DIV_NUM_S) | (10 << 0);
    // Quitar el reset del timer (Bit 23)
    LEDC_TIMER0_CONF_REG &= ~LEDC_TIMER_RST_B; 
    // Aplicar configuración (Bit 25)
    LEDC_TIMER0_CONF_REG |= LEDC_TIMER_PARA_UP_B; 

    // 8. Configurar el Canal 0 del LEDC
    // Registro 32.1: LEDC_CHn_CONF0_REG [cite: 1330]
    // Bits 1:0 (LEDC_TIMER_SEL_CHn) = 0 (Seleccionar Timer 0)
    // Bit 2 (LEDC_SIG_OUT_EN_CHn) = 1 (Habilitar salida de señal)
    LEDC_CH0_CONF0_REG = (0 << LEDC_TIMER_SEL_S) | LEDC_SIG_OUT_EN_B;
    // Aplicar configuración (Bit 4)
    LEDC_CH0_CONF0_REG |= LEDC_PARA_UP_CH0_B; 

    // 9. Establecer el punto de inicio del ciclo (HPOINT) a 0
    // Registro 32.4: LEDC_CHn_HPOINT_REG [cite: 1331]
    LEDC_CH0_HPOINT_REG = 0;
    // Aplicar configuración (Bit 4 en CONF0)
    LEDC_CH0_CONF0_REG |= LEDC_PARA_UP_CH0_B; 

    // Resolución de 10 bits = 1024 pasos
    const uint32_t max_duty = (1 << 10);

    /* ---------- BUCLE PRINCIPAL ---------- */
    while (1) {
        // Fade up (0 a 1023)
        for (uint32_t d = 0; d < max_duty; d++) {
            // Registro 32.5: LEDC_CHn_DUTY_REG [cite: 1331]
            // El manual indica que el duty se carga en los bits [18:4] [cite: 1327]
            LEDC_CH0_DUTY_REG = (d << 4); 
            
            // Aplicar el cambio de duty (actualización de canal)
            // Registro 32.1: LEDC_PARA_UP_CHn [cite: 1330]
            LEDC_CH0_CONF0_REG |= LEDC_PARA_UP_CH0_B;
            
            delay(5000); 
        }
        
        // Fade down (1023 a 0)
        for (uint32_t d = max_duty - 1; d > 0; d--) { 
            LEDC_CH0_DUTY_REG = (d << 4);
            LEDC_CH0_CONF0_REG |= LEDC_PARA_UP_CH0_B; // Aplicar cambio de duty
            delay(5000);
        }
    }

}
