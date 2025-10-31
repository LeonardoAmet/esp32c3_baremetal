/* ---- WDT BASES ---- */
#define TIMG0_BASE                      0x6001F000UL
#define TIMG1_BASE                      0x60020000UL
#define TIMG_WDTCONFIG0_OFFSET          0x0048
#define TIMG_WDTWPROTECT_OFFSET         0x0064
#define TIMG_WDT_UNLOCK_KEY             0x50D83AA1U
#define RTC_CNTL_BASE                   0x60008000UL
#define RTC_CNTL_WDTCONFIG0_OFFSET      0x0090
#define RTC_CNTL_WDTWPROTECT_OFFSET     0x00A8
#define RTC_CNTL_SWD_CONF_OFFSET        0x00AC
#define RTC_CNTL_SWD_WPROTECT_OFFSET    0x00B0
#define RTC_CNTL_WDT_UNLOCK_KEY         0x50D83AA1U
#define RTC_CNTL_SWD_UNLOCK_KEY         0x8F1D312AU

/* ---- FUNCIONES WDT ---- */
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
    while (cycles--) __asm__ volatile ("nop");
}