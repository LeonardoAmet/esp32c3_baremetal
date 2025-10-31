#include <stdint.h>
#include <wdtfix.h>

#define BIT(n) (1U << (n))                    // Máscara de un bit
#define LOOP_DELAY      5000U
#define ADC_THRESHOLD   2000U
#define LED_GPIO        3
#define LED_MASK        BIT(LED_GPIO)
#define LED2_GPIO       5U
#define LED2_MASK       BIT(LED2_GPIO)
#define ADC_ZERO_BIAS   1650U   // Cuentas residuales con cursor a GND (ajustar según hardware)
#define POT_GPIO        0U
#define POT_MASK        BIT(POT_GPIO)

/* ---- ---- REGISTROS BASE ---- ---- */
#define MR_GPIO_BASE       0x60004000UL // Base periférico GPIO /*OK*/
#define MR_IO_MUX_BASE     0x60009000UL // Base IO_MUX (selección de función/pulls) /*OK*/
#define MR_LEDC_BASE       0x60019000UL // Base bloque LEDC (PWM hardware) /*OK*/
#define MR_SYSTEM_BASE     0x600C0000UL // Base registro de sistema (clocks/resets) /*OK*/

#define MR_IO_MUX_REG      (*(volatile uint32_t*)(MR_IO_MUX_BASE + 0x0018))

/* ---- ---- REGISTROS GPIO ---- ---- */
#define GPIO_OUT_W1TS_REG       (*(volatile uint32_t*)(MR_GPIO_BASE + 0x0008))  // Set pin high (write-1-to-set)
#define GPIO_OUT_W1TC_REG       (*(volatile uint32_t*)(MR_GPIO_BASE + 0x000C))  // Set pin low  (write-1-to-clear)
#define GPIO_ENABLE_W1TS_REG    (*(volatile uint32_t*)(MR_GPIO_BASE + 0x0024)) // Habilitar OE
#define GPIO_ENABLE_W1TC_REG    (*(volatile uint32_t*)(MR_GPIO_BASE + 0x0028)) // Deshabilitar OE
#define GPIO_FUNC3_OUT_SEL_CFG_REG  (*(volatile uint32_t*)(MR_GPIO_BASE + 0x0560))
#define GPIO_FUNC3_OEN_INV_SEL      BIT(10)
#define GPIO_FUNC3_OEN_SEL          BIT(9)
#define GPIO_FUNC3_OUT_INV_SEL      BIT(8)
#define GPIO_FUNC3_OUT_SEL_M        ((0xFFU) << 0)
#define GPIO_FUNC3_OUT_SEL_S        0

/* ---- ---- REGISTROS IOMUX ---- ---- */
#define IO_MUX_GPIO0_REG    (*(volatile uint32_t *)(MR_IO_MUX_BASE + 0x0004)) // IO_MUX para GPIO0 (ADC)
#define IO_MUX_GPIO3_REG    (*(volatile uint32_t *)(MR_IO_MUX_BASE + 0x0010)) // IO_MUX para GPIO3 (LED)
#define IO_MUX_FUN_IE       BIT(9)   // Input enable digital
#define IO_MUX_FUN_PU       BIT(8)   // Pull-up digital
#define IO_MUX_FUN_PD       BIT(7)   // Pull-down digital
#define IO_MUX_MCU_SEL_MASK (0x7U << 12) // Selector de función
#define IO_MUX_MCU_SEL_GPIO 1U       // Función GPIO

/* ---- ---- REGISTROS SYSTEM ---- ---- */
#define SYSTEM_PERIP_CLK_EN0_REG (*(volatile uint32_t *)(MR_SYSTEM_BASE + 0x0010)) // Registro de clocks
#define SYSTEM_PERIP_RST_EN0_REG (*(volatile uint32_t *)(MR_SYSTEM_BASE + 0x0018))// Registro de resets
#define SYSTEM_LEDC_CLK_EN       BIT(11) // Bit de clock para LEDC
#define SYSTEM_LEDC_RST          BIT(11) // Bit de reset para LEDC

/* ---- ---- REGISTROS PWM ---- ---- */
#define LEDC_TIMER0_CONF_REG   (*(volatile uint32_t *)(MR_LEDC_BASE + 0x00A0))
#define LEDC_TIMER0_PARA_UP    BIT(25)
#define LEDC_TIMER0_RST        BIT(23)
#define LEDC_TIMER0_PAUSE      BIT(22)
#define LEDC_CLK_DIV_TIMER0_M  (*(volatile uint32_t *)((0x0003FFFFU) << 4))
#define LEDC_CLK_DIV_TIMER0_S  4
#define LEDC_TIMER0_DUTY_RES_M ((0xFU) << 0)
#define LEDC_TIMER0_DUTY_RES_S 0

#define LEDC_CONF_REG            (*(volatile uint32_t *)(MR_LEDC_BASE + 0x00D0))
#define LEDC_CLK_EN              BIT(31)
#define LEDC_APB_CLK_SEL_M       (*(volatile uint32_t *)((0x3U) << 0))
#define LEDC_APB_CLK_SEL_S       0
#define LEDC_APB_CLK_SEL_APB     1U

#define LEDC_CH0_CONF0_REG     (*(volatile uint32_t *)(MR_LEDC_BASE + 0x0000))
#define LEDC_PARA_UP_LSCH0       BIT(4)
#define LEDC_IDLE_LV_LSCH0       BIT(3)
#define LEDC_SIG_OUT_EN_LSCH0    BIT(2)
#define LEDC_TIMER_SEL_LSCH0_M   ((0x3U) << 0)
#define LEDC_TIMER_SEL_LSCH0_S   0

#define LEDC_CH0_HPOINT_REG    (*(volatile uint32_t *)(MR_LEDC_BASE + 0x0004))
#define LEDC_CH0_DUTY_REG      (*(volatile uint32_t *)(MR_LEDC_BASE + 0x0008))

#define LEDC_CH0_CONF1_REG     (*(volatile uint32_t *)(MR_LEDC_BASE + 0x000C))
#define LEDC_DUTY_START_LSCH0    BIT(31)

#define LEDC_PWM_FREQ_HZ       2000ULL
#define LEDC_TIMER_RES_BITS    10U
#define LEDC_TIMER_SOURCE_HZ   80000000ULL
#define LEDC_CLK_DIV_FRAC_BITS 8U
#define LEDC_TIMER_DIVIDER_NUM (LEDC_TIMER_SOURCE_HZ << LEDC_CLK_DIV_FRAC_BITS)
#define LEDC_TIMER_DIVIDER_DEN (LEDC_PWM_FREQ_HZ * (1ULL << LEDC_TIMER_RES_BITS))
#define LEDC_TIMER_DIVIDER     (*(volatile uint32_t *)(LEDC_TIMER_DIVIDER_NUM / LEDC_TIMER_DIVIDER_DEN))
#define LEDC_DUTY_MAX          ((1U << LEDC_TIMER_RES_BITS) - 1U)
#define LEDC_DUTY_SHIFT        4U
#define LEDC_LS_SIG_OUT0_IDX   45U  // Señal PWM canal 0 (low-speed)


static void gpio_init(void) {
// GPIO3 queda como salida controlada por LEDC (sin pulls, función GPIO)
    uint32_t reg = IO_MUX_GPIO3_REG;
    reg &= ~(IO_MUX_FUN_IE | IO_MUX_FUN_PU | IO_MUX_FUN_PD | IO_MUX_MCU_SEL_MASK);
    reg |= (IO_MUX_MCU_SEL_GPIO << 12);
    IO_MUX_GPIO3_REG = reg;
    GPIO_ENABLE_W1TS_REG = LED_MASK;

    reg = (MR_IO_MUX_BASE + 0x0018);      // IO_MUX_GPIO5_REG (MTDI)
    reg &= ~(IO_MUX_FUN_IE | IO_MUX_FUN_PU | IO_MUX_FUN_PD | IO_MUX_MCU_SEL_MASK);
    reg |= (IO_MUX_MCU_SEL_GPIO << 12);
    MR_IO_MUX_REG = reg;
    GPIO_ENABLE_W1TS_REG = LED2_MASK;

    // GPIO0 en modo analógico (sin OE ni pulls) para el potenciómetro
    reg = IO_MUX_GPIO0_REG;
    reg &= ~(IO_MUX_FUN_IE | IO_MUX_FUN_PU | IO_MUX_FUN_PD | IO_MUX_MCU_SEL_MASK);
    IO_MUX_GPIO0_REG = reg;
    GPIO_ENABLE_W1TC_REG = POT_MASK;
}

static void ledc_set_duty(uint32_t duty) {
    if (duty > LEDC_DUTY_MAX) {
        duty = LEDC_DUTY_MAX;
    }
    LEDC_CH0_DUTY_REG = duty << LEDC_DUTY_SHIFT;
    LEDC_CH0_CONF1_REG |= LEDC_DUTY_START_LSCH0;
    LEDC_CH0_CONF0_REG |= LEDC_PARA_UP_LSCH0;
}

static void ledc_init(void) {
    // Activar clock/reset de LEDC
    SYSTEM_PERIP_CLK_EN0_REG |= SYSTEM_LEDC_CLK_EN;
    SYSTEM_PERIP_RST_EN0_REG |= SYSTEM_LEDC_RST;
    SYSTEM_PERIP_RST_EN0_REG &= ~SYSTEM_LEDC_RST;
 
    // Seleccionar reloj APB (80 MHz) y habilitar módulo
    uint32_t ledc_conf = LEDC_CONF_REG;
    ledc_conf |= LEDC_CLK_EN;
    ledc_conf &= ~LEDC_APB_CLK_SEL_M;
    ledc_conf |= (LEDC_APB_CLK_SEL_APB << LEDC_APB_CLK_SEL_S);
    LEDC_CONF_REG = ledc_conf;
 
    // Configurar temporizador low-speed 0: resolución y divisor fraccionario (8 bits fracc.)
    uint32_t timer_conf = LEDC_TIMER0_CONF_REG;
    timer_conf &= ~(LEDC_CLK_DIV_TIMER0_M | LEDC_TIMER0_DUTY_RES_M | LEDC_TIMER0_PAUSE);
    timer_conf |= ((LEDC_TIMER_DIVIDER << LEDC_CLK_DIV_TIMER0_S) & LEDC_CLK_DIV_TIMER0_M);
    timer_conf |= ((LEDC_TIMER_RES_BITS << LEDC_TIMER0_DUTY_RES_S) & LEDC_TIMER0_DUTY_RES_M);
    LEDC_TIMER0_CONF_REG = timer_conf;
    LEDC_TIMER0_CONF_REG |= LEDC_TIMER0_RST;
    LEDC_TIMER0_CONF_REG &= ~LEDC_TIMER0_RST;
    LEDC_TIMER0_CONF_REG |= LEDC_TIMER0_PARA_UP;
 
    // Inicializar canal 0: duty 0, usa timer 0, habilita salida
    LEDC_CH0_HPOINT_REG = 0;
    LEDC_CH0_DUTY_REG = 0;
    uint32_t ch0_conf0 = LEDC_CH0_CONF0_REG;
    ch0_conf0 &= ~(LEDC_TIMER_SEL_LSCH0_M | LEDC_IDLE_LV_LSCH0 | LEDC_SIG_OUT_EN_LSCH0);
    ch0_conf0 |= LEDC_SIG_OUT_EN_LSCH0; // Timer 0 (valor 0)
    LEDC_CH0_CONF0_REG = ch0_conf0;
    LEDC_CH0_CONF0_REG |= LEDC_PARA_UP_LSCH0;
    uint32_t ch0_conf1 = LEDC_CH0_CONF1_REG;
    ch0_conf1 |= LEDC_DUTY_START_LSCH0;
    LEDC_CH0_CONF1_REG = ch0_conf1;
 
    // Conectar señal LEDC canal 0 a GPIO3
    uint32_t func3 = GPIO_FUNC3_OUT_SEL_CFG_REG;
    func3 &= ~(GPIO_FUNC3_OEN_INV_SEL | GPIO_FUNC3_OEN_SEL | GPIO_FUNC3_OUT_INV_SEL | GPIO_FUNC3_OUT_SEL_M);
    func3 |= (LEDC_LS_SIG_OUT0_IDX << GPIO_FUNC3_OUT_SEL_S) & GPIO_FUNC3_OUT_SEL_M;
    GPIO_FUNC3_OUT_SEL_CFG_REG = func3;
 
    ledc_set_duty(0);
}

static void short_delay(void) {
    // Busy-wait simple (no timers configurados)
    for (volatile uint32_t i = 0; i < LOOP_DELAY; ++i) {
        __asm__ volatile("nop");
    }
}



/* ---- PROGRAMA PRINCIPAL ---- */
int main(void) {
    // Deshabilitar watchdogs para bucle infinito didáctico
    disable_timg_wdt(TIMG0_BASE);
    disable_timg_wdt(TIMG1_BASE);
    disable_rtc_wdts();
    
    gpio_init();
    ledc_init();

    // Loop principal: LED rojo por umbral digital, LED azul via PWM proporcional
    while (1) {
        // ---- Fade UP ----
        for (uint32_t d = 0; d <= 1024; d += 4) {
            ledc_set_duty(d);
            short_delay(); // controlá la velocidad del fade
        }

        // ---- Fade DOWN ----
        for (uint32_t d = 1023; d > 0; d -= 4) {
            ledc_set_duty(d);
            short_delay();
        }
    }

    // Nunca llega acá
    return 0;
}