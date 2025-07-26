/*
 * meshroom.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <pico/stdlib.h>
#include <hardware/sync.h>
#include <libmeshtastic.h>
#include <meshroom.h>

static struct mt_client _client;
struct mt_client *G_mtc = &_client;

int main(void)
{
    int ret = 0;
    struct mt_client mtc;
    bool led_on = false;

    stdio_init_all();
    led_init();
    serial_init();
    serial_print_str(0,"\e[1;1H\e[2J\n");
    serial_print_str(0, "The meshroom firmware for Raspberry Pi Pico\n");
    serial_print_str(0, "-------------------------------------------\n");
    serial_print_str(0, "Copyright (C) 2025, Charles Chiou\n");
    shell_init(0);

    ret = mt_serial_attach(&mtc, "uart1");
    if (ret != 0) {
        goto done;
    }

    for (unsigned int loop = 0; true; loop++) {
        uint8_t buf[1024];

        led_set(led_on);
        led_on = !led_on;

        ret = serial_rx_ready(1);
        if (ret > 0) {
            ret = serial_read(1, buf, ret);
            serial_printf(0, "got %d bytes from uart 1\n", ret);
        }

        shell_process();
        best_effort_wfe_or_timeout(make_timeout_time_us(500000));
    }

done:

    for (unsigned int loop = 0; true; loop++) {
        led_set(led_on);
        led_on = !led_on;
        sleep_ms(50);
    }

    return ret;
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
