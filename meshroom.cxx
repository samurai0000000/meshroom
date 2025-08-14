/*
 * meshroom.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <time.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <pico/multicore.h>
#include <hardware/uart.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <libmeshtastic.h>
#include <memory>
#include <MeshRoom.hxx>
#include <meshroom.h>
#include "version.h"

#define USE_WATCHDOG_TIMER

#define LED_TASK_STACK_SIZE            configMINIMAL_STACK_SIZE
#define LED_TASK_PRIORITY              (tskIDLE_PRIORITY + 1UL)
#define CONSOLE_TASK_STACK_SIZE        (4 * 1024)
#define CONSOLE_TASK_PRIORITY          (tskIDLE_PRIORITY + 2UL)
#define CONSOLE2_TASK_STACK_SIZE       (4 * 1024)
#define CONSOLE2_TASK_PRIORITY         (tskIDLE_PRIORITY + 3UL)
#define MESHTASTIC_TASK_STACK_SIZE     (16 * 1024)
#define MESHTASTIC_TASK_PRIORITY       (tskIDLE_PRIORITY + 4UL)

shared_ptr<MeshRoom> meshroom = NULL;

void led_task(__unused void *params)
{
    bool led_on = false;

#if defined(USE_WATCHDOG_TIMER)
    watchdog_enable(5000, true);
    watchdog_enable_caused_reboot();
#endif

    for (;;) {
        led_set(led_on);
        led_on = !led_on;
        vTaskDelay(1000);

#if defined(USE_WATCHDOG_TIMER)
        watchdog_update();
#endif
    }
}

void console_task(__unused void *params)
{
    int ret;

    vTaskSetThreadLocalStoragePointer(NULL, 1, (void *) 0);
    vTaskDelay(1500);

    console_printf("\n\x1b[2K");
    console_printf("The meshroom firmware for Raspberry Pi Pico\n");
    console_printf("Version: %s\n", MYPROJECT_VERSION_STRING);
    console_printf("Built: %s@%s %s\n",
                   MYPROJECT_WHOAMI, MYPROJECT_HOSTNAME, MYPROJECT_DATE);
    console_printf("-------------------------------------------\n");
    console_printf("Copyright (C) 2025, Charles Chiou\n");
    console_printf("> ");

    for (;;) {
        ret = console_rx_ready();
        if (ret > 0) {
            ret = shell_process();
        }

        if (ret == 0) {
            vTaskDelay(50);
        }
    }
}


void console2_task(__unused void *params)
{
    int ret;

    vTaskSetThreadLocalStoragePointer(NULL, 0, (void *) 1);

    console2_printf("\n\x1b[2K");
    console2_printf("The meshroom firmware for Raspberry Pi Pico\n");
    console2_printf("Version: %s\n", MYPROJECT_VERSION_STRING);
    console2_printf("Built: %s@%s %s\n",
                   MYPROJECT_WHOAMI, MYPROJECT_HOSTNAME, MYPROJECT_DATE);
    console2_printf("-------------------------------------------\n");
    console2_printf("Copyright (C) 2025, Charles Chiou\n");
    console2_printf("> ");

    for (;;) {
        ret = console2_rx_ready();
        if (ret > 0) {
            ret = shell2_process();
        }

        if (ret == 0) {
            vTaskDelay(50);
        }
    }
}

void meshtastic_task(__unused void *params)
{
    int ret = 0;
    time_t now, last_want_config, last_heartbeat;
    extern SemaphoreHandle_t uart1_sem;

    now = time(NULL);
    last_heartbeat = now;
    last_want_config = 0;

    vTaskDelay(5000);

    for (;;) {
        now = time(NULL);

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

        for (;;) {
            ret = serial_rx_ready();
            if (ret == 0) {
                break;
            } if (ret > 0) {
                ret = mt_serial_process(&meshroom->_mtc, 0);
                if (ret < 0) {
                    console_printf("mt_serial_process failed!\n");
                }
            }
        }

        while (xSemaphoreTake(uart1_sem, portMAX_DELAY) != pdTRUE);
    }
}

int main(void)
{
    TaskHandle_t ledTask;
    TaskHandle_t consoleTask;
    TaskHandle_t console2Task;
    TaskHandle_t meshtasticTask;

    stdio_init_all();
    serial_init();
    led_init();
    shell_init();

    meshroom = make_shared<MeshRoom>();
    meshroom->setClient(meshroom);
    meshroom->sendDisconnect();
    if (meshroom->loadNvm() == false) {
        meshroom->saveNvm();  // Create a default
    }
    meshroom->applyNvmToHomeChat();

    xTaskCreate(led_task,
                "LedTask",
                LED_TASK_STACK_SIZE,
                NULL,
                LED_TASK_PRIORITY,
                &ledTask);

    xTaskCreate(console_task,
                "ConsoleTask",
                CONSOLE_TASK_STACK_SIZE,
                NULL,
                CONSOLE_TASK_PRIORITY,
                &consoleTask);

    xTaskCreate(console2_task,
                "Console2Task",
                CONSOLE2_TASK_STACK_SIZE,
                NULL,
                CONSOLE2_TASK_PRIORITY,
                &console2Task);

    xTaskCreate(meshtastic_task,
                "MeshtasticThread",
                MESHTASTIC_TASK_STACK_SIZE,
                NULL,
                MESHTASTIC_TASK_PRIORITY,
                &meshtasticTask);

#if defined(configUSE_CORE_AFFINITY) && (configNUMBER_OF_CORES > 1)
    vTaskCoreAffinitySet(ledTask, 1);
    vTaskCoreAffinitySet(consoleTask, 1);
    vTaskCoreAffinitySet(console2Task, 1);
    vTaskCoreAffinitySet(meshtasticTask, 1);
#endif

    vTaskStartScheduler();

    return 0;
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
