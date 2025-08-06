/*
 * meshroom.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <time.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <hardware/uart.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <libmeshtastic.h>
#include <memory>
#include <MeshRoom.hxx>
#include <meshroom.h>
#include "version.h"

//#define USE_WATCHDOG_TIMER

#if defined(MEASURE_CPU_UTILIZATION)
uint64_t t_cpu_total = 0;
uint64_t t_cpu_busy = 0;
static uint64_t t_busy = time_us_64(), t_idle = 0, t_diff = 0;
#endif

shared_ptr<MeshRoom> meshroom = NULL;

static bool timer_handler(repeating_timer_t *timer)
{
    (void)(timer);

    __sev();

    return true;
}

int main(void)
{
    int ret = 0;
    repeating_timer_t timer;
    bool led_on = false;
    time_t now, last_flip, last_want_config, last_heartbeat;
    int rx0 = 0, rx1 = 0;

    stdio_init_all();

    serial_init();
    led_init();

    meshroom = make_shared<MeshRoom>();
    meshroom->sendDisconnect();

#if defined(LIB_PICO_STDIO_USB)
    sleep_ms(1500);
#else
    sleep_ms(200);
#endif

    console_printf("\n\x1b[2K");
    console_printf("The meshroom firmware for Raspberry Pi Pico\n");
    console_printf("Version: %s\n", MYPROJECT_VERSION_STRING);
    console_printf("Built: %s@%s %s\n",
                   MYPROJECT_WHOAMI, MYPROJECT_HOSTNAME, MYPROJECT_DATE);
    console_printf("-------------------------------------------\n");
    console_printf("Copyright (C) 2025, Charles Chiou\n");

#if defined(USE_WATCHDOG_TIMER)
    watchdog_enable(5000, true);
    watchdog_enable_caused_reboot();
#endif

    shell_init();

    now = time(NULL);
    last_flip = now;
    last_heartbeat = now;
    last_want_config = 0;

    add_repeating_timer_us(-1000000, timer_handler, NULL, &timer);

    for (;;) {
        now = time(NULL);

        if ((now - last_flip) >= 1) {
            led_set(led_on);
            led_on = !led_on;
            last_flip = now;
        }

        if (!meshroom->isConnected() && ((now - last_want_config) >= 5)) {
            ret = meshroom->sendWantConfig();
            if (ret == false) {
                console_printf("sendWantConfig failed!\n");
            }

            last_want_config = now;
        } else if (meshroom->isConnected()) {
            last_want_config = now;
        }

        if (meshroom->isConnected() && ((now - last_heartbeat) >= 60)) {
            ret = meshroom->sendHeartbeat();
            if (ret == false) {
                console_printf("sendHeartbeat failed!\n");
            }

            last_heartbeat = now;
        }

        do {
            rx0 = console_rx_ready();
            if (rx0 > 0) {
                rx0 = shell_process();
            }

            rx1 = serial_rx_ready();
            if (rx1 > 0) {
                ret = mt_serial_process(&meshroom->_mtc, 0);
                if (ret < 0) {
                    console_printf("mt_serial_process failed!\n");
                }
            }

#if defined(USE_WATCHDOG_TIMER)
            watchdog_update();
#endif
        } while ((rx0 > 0) || (rx1 > 0));

#if defined(MEASURE_CPU_UTILIZATION)
        t_idle = time_us_64();
        t_diff = t_idle - t_busy;
        t_cpu_total += t_diff;
        t_cpu_busy += t_diff;
#endif

        __wfe();
#if defined(USE_WATCHDOG_TIMER)
        watchdog_update();
#endif

#if defined(MEASURE_CPU_UTILIZATION)
        t_busy = time_us_64();
        t_diff = t_busy - t_idle;
        t_cpu_total += t_diff;

        if (t_cpu_total > (5 * 1000000)) {
            t_cpu_total /= 2;
            t_cpu_busy /= 2;
        }
#endif
    }

    /* Should not get here... */
    for (;;) {
        led_set(led_on);
        led_on = !led_on;
        sleep_ms(100);
    }

    return ret;
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
