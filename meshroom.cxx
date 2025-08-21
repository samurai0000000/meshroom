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
#include <MeshRoomShell.hxx>
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
static shared_ptr<MeshRoomShell> shell = NULL;
static shared_ptr<MeshRoomShell> shell2 = NULL;

static string banner = "The meshroom firmware for Raspberry Pi Pico";
static string version = string("Version: ") + string(MYPROJECT_VERSION_STRING);
static string built = string("Built: ") +
    string(MYPROJECT_WHOAMI) + string("@") +
    string(MYPROJECT_HOSTNAME) + string(" ") + string(MYPROJECT_DATE);
static string copyright = string("Copyright (C) 2025, Charles Chiou");

static void led_task(__unused void *params)
{

#if defined(USE_WATCHDOG_TIMER)
    watchdog_enable(5000, true);
    watchdog_enable_caused_reboot();
#endif

    for (;;) {
        meshroom->flipOnboardLed();
        vTaskDelay(pdMS_TO_TICKS(1000));

#if defined(USE_WATCHDOG_TIMER)
        watchdog_update();
#endif
    }
}

static void console_task(__unused void *params)
{
    int ret;

    vTaskDelay(pdMS_TO_TICKS(1500));

    console_printf("\n\x1b[2K");
    console_printf("%s\n", shell->banner().c_str());
    console_printf("%s\n", shell->version().c_str());
    console_printf("%s\n", shell->built().c_str());
    console_printf("-------------------------------------------\n");
    console_printf("%s\n", shell->copyright().c_str());
    console_printf("> ");

    for (;;) {
        do {
            ret = shell->process();
        } while (ret > 0);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void console2_task(__unused void *params)
{
    int ret;

    console2_printf("\n\x1b[2K");
    console2_printf("%s\n", shell2->banner().c_str());
    console2_printf("%s\n", shell2->version().c_str());
    console2_printf("%s\n", shell2->built().c_str());
    console2_printf("-------------------------------------------\n");
    console2_printf("%s\n", shell2->copyright().c_str());
    console2_printf("> ");

    for (;;) {
        do {
            ret = shell2->process();
        } while (ret > 0);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void meshtastic_task(__unused void *params)
{
    int ret = 0;
    time_t now, last_want_config, last_heartbeat;
    extern SemaphoreHandle_t uart1_sem;

    if (meshroom->loadNvm() == false) {
        meshroom->saveNvm();
    }
    meshroom->applyNvmToHomeChat();

    now = time(NULL);
    last_heartbeat = now;
    last_want_config = 0;

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

        if (meshroom->isConnected() &&
            ((now - last_heartbeat) >= 60)) {
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

        xSemaphoreTake(uart1_sem, pdMS_TO_TICKS(1000));
    }
}

int main(void)
{
    TaskHandle_t ledTask;
    TaskHandle_t consoleTask;
    TaskHandle_t console2Task;
    TaskHandle_t meshtasticTask;

    stdio_usb_init();
    stdio_init_all();
    serial_init();

    meshroom = make_shared<MeshRoom>();
    meshroom->setClient(meshroom);
    meshroom->setNvm(meshroom);
    meshroom->sendDisconnect();

    shell = make_shared<MeshRoomShell>();
    shell->setBanner(banner);
    shell->setVersion(version);
    shell->setBuilt(built);
    shell->setCopyright(copyright);
    shell->setClient(meshroom);
    shell->setNvm(meshroom);
    shell->attach((void *) 0);

    shell2 = make_shared<MeshRoomShell>();
    shell2->setBanner(banner);
    shell2->setVersion(version);
    shell2->setBuilt(built);
    shell2->setCopyright(copyright);
    shell2->setClient(meshroom);
    shell2->setNvm(meshroom);
    shell2->attach((void *) 1);

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
    vTaskCoreAffinitySet(ledTask, 0x2);
    vTaskCoreAffinitySet(consoleTask, 0x2);
    vTaskCoreAffinitySet(console2Task, 0x2);
    vTaskCoreAffinitySet(meshtasticTask, 0x1);
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
