#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Fetch the onboard green LED using Zephyr's standard lowercase identifier */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/* Fixed period window for our software bit-bang cycle (10 milliseconds) */
#define SOFT_PWM_PERIOD_MS 10

int main(void)
{
    int ret;

    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("Error: Onboard LED GPIO device is not ready.");
        return -EIO;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Error: Failed to configure LED pin (error %d).", ret);
        return ret;
    }

    /* Fallback default boundaries if 'Advanced LED settings' are unchecked */
    uint32_t max_brightness_pct = 100;
    int total_fade_duration_ms = 500;

    #ifdef CONFIG_LED_BRIGHTNESS
        max_brightness_pct = CONFIG_LED_BRIGHTNESS;
    #endif

    #ifdef CONFIG_LED_FADE_DURATION_MS
        total_fade_duration_ms = CONFIG_LED_FADE_DURATION_MS;
    #endif

    /* Calculate how many timing loop intervals fit inside the requested fade duration window */
    int total_steps = total_fade_duration_ms / SOFT_PWM_PERIOD_MS;
    if (total_steps < 1) {
        total_steps = 1;
    }

    LOG_INF("Software-Dimming Started: Brightness Max = %d%%, Fade Window = %d ms", 
            max_brightness_pct, total_fade_duration_ms);

    while (1) {
        /* Phase 1: Smoothly fade UP toward max brightness target */
        for (int step = 0; step <= total_steps; step++) {
            /* Compute the dynamic duty cycle scaling percentage for this step */
            uint32_t dynamic_pct = (max_brightness_pct * step) / total_steps;
            int on_time = (SOFT_PWM_PERIOD_MS * dynamic_pct) / 100;
            int off_time = SOFT_PWM_PERIOD_MS - on_time;

            if (on_time > 0) {
                gpio_pin_set_dt(&led, 1);
                k_msleep(on_time);
            }
            if (off_time > 0) {
                gpio_pin_set_dt(&led, 0);
                k_msleep(off_time);
            }
        }

        /* Phase 2: Smoothly fade DOWN back to completely off */
        for (int step = total_steps; step >= 0; step++) {
            uint32_t dynamic_pct = (max_brightness_pct * step) / total_steps;
            int on_time = (SOFT_PWM_PERIOD_MS * dynamic_pct) / 100;
            int off_time = SOFT_PWM_PERIOD_MS - on_time;

            if (on_time > 0) {
                gpio_pin_set_dt(&led, 1);
                k_msleep(on_time);
            }
            if (off_time > 0) {
                gpio_pin_set_dt(&led, 0);
                k_msleep(off_time);
            }
        }
    }

    return 0;
}
