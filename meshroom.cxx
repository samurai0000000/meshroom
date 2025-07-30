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

#if defined(MEASURE_CPU_UTILIZATION)
uint64_t t_cpu_total = 0;
uint64_t t_cpu_busy = 0;
static uint64_t t_busy = time_us_64(), t_idle = 0, t_diff = 0;
static uint32_t state;
#endif

shared_ptr<MeshRoom> meshroom = NULL;

int main(void)
{
    int ret = 0;
    bool led_on = false;
    unsigned int loop;
    time_t now, last_want_config;
    int rx0 = 0, rx1 = 0;

    stdio_init_all();

    serial_init();
    serial_printf(uart0, "\e[1;1H\e[2J\n");
    serial_printf(uart0, "The meshroom firmware for Raspberry Pi Pico\n");
    serial_printf(uart0, "Version: %s\n", MYPROJECT_VERSION_STRING);
    serial_printf(uart0, "Built: %s@%s %s\n",
                  MYPROJECT_WHOAMI, MYPROJECT_HOSTNAME, MYPROJECT_DATE);
    serial_printf(uart0, "-------------------------------------------\n");
    serial_printf(uart0, "Copyright (C) 2025, Charles Chiou\n");

    led_init();

    meshroom = make_shared<MeshRoom>();
    meshroom->sendDisconnect();

    shell_init();

    watchdog_enable(5000, 0);
    watchdog_enable_caused_reboot();

    now = time(NULL);
    last_want_config = now - 3;

    for (loop = 0; true; loop++) {
        led_set(led_on);
        led_on = !led_on;

        now = time(NULL);
        if (!meshroom->isConnected() && ((now - last_want_config) >= 5)) {
            ret = meshroom->sendWantConfig();
            if (ret != false) {
                last_want_config = time(NULL);
            }
        }

        if (meshroom->isConnected() &&
            ((loop % 60) == 0)) {
            meshroom->sendHeartbeat();
        }

        while (((rx0 = serial_rx_ready(uart0)) > 0) ||
               ((rx1 = serial_rx_ready(uart1)) > 0)) {
            if (rx0 > 0) {
                shell_process();
            }
            if (rx1 > 0) {
                mt_serial_process(&meshroom->_mtc, 0);
            }
            watchdog_update();
        }

#if defined(MEASURE_CPU_UTILIZATION)
        state = save_and_disable_interrupts();
        t_idle = time_us_64();
        t_diff = t_idle - t_busy;
        t_cpu_total += t_diff;
        t_cpu_busy += t_diff;
        restore_interrupts(state);
#endif

        best_effort_wfe_or_timeout(make_timeout_time_us(1000000));
        watchdog_update();

#if defined(MEASURE_CPU_UTILIZATION)
        state = save_and_disable_interrupts();
        t_busy = time_us_64();
        t_diff = t_busy - t_idle;
        t_cpu_total += t_diff;

        if (t_cpu_total > (5 * 1000000)) {
            t_cpu_total /= 2;
            t_cpu_busy /= 2;
        }
        restore_interrupts(state);
#endif
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
