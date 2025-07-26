/*
 * led.c
 *
 * Copyright (C) 2023, Charles Chiou
 */

#include <pico/stdlib.h>

#define LED_PIN 25

void led_init(void)
{
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, false);
}

void led_set(bool on)
{
    gpio_put(LED_PIN, on);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
