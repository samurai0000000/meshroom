/*
 * meshroom.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <time.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <pico/cyw43_arch.h>
#include <hardware/uart.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <hardware/clocks.h>
#include <hardware/pll.h>
#include <tusb.h>
#include <bsp/board_api.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <memory>
#include <libmeshtastic.h>
#include <MeshRoom.hxx>
#include <MeshRoomShell.hxx>
#include <meshroom.h>
#include "version.h"

#define WATCHDOG_TASK_STACK_SIZE       1024
#define WATCHDOG_TASK_PRIORITY         30
#define LED_TASK_STACK_SIZE            1024
#define LED_TASK_PRIORITY              25
#define USB_TASK_STACK_SIZE            2048
#define USB_TASK_PRIORITY              20
#define MORSEBUZZER_TASK_STACK_SIZE    1024
#define MORSEBUZZER_TASK_PRIORITY      29
#define MESHTASTIC_TASK_STACK_SIZE     4096
#define MESHTASTIC_TASK_PRIORITY       15
#define SHELL0_TASK_STACK_SIZE         2048
#define SHELL0_TASK_PRIORITY           10
#define SHELL1_TASK_STACK_SIZE         2048
#define SHELL1_TASK_PRIORITY           10

shared_ptr<MeshRoom> meshroom = NULL;
static shared_ptr<MeshRoomShell> shell0 = NULL;
static shared_ptr<MeshRoomShell> shell1 = NULL;

static string banner = "The meshroom firmware for Raspberry Pi Pico";
static string version = string("Version: ") + string(MYPROJECT_VERSION_STRING);
static string built = string("Built: ") +
    string(MYPROJECT_WHOAMI) + string("@") +
    string(MYPROJECT_HOSTNAME) + string(" ") + string(MYPROJECT_DATE);
static string copyright = string("Copyright (C) 2025, Charles Chiou");

static void watchdog_task(__unused void *params)
{
    watchdog_enable(5000, true);
    watchdog_enable_caused_reboot();

    for (;;) {
        watchdog_update();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void led_task(__unused void *params)
{
    for (;;) {
        meshroom->flipOnboardLed();
        if ((meshroom->meshDeviceLastRecivedSecondsAgo() <= 1) ||
            (meshroom->isMorseEmpty() == false)) {
            meshroom->setAlertLed(true);
        } else {
            meshroom->setAlertLed(false);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void usb_task(__unused void *params)
{
    for (;;) {
        usbcdc_task();
        taskYIELD();
    }
}

static void morsebuzzer_task(__unused void *params)
{
    for (;;) {
        meshroom->runMorseThread();
    }
}

static void meshtastic_task(__unused void *params)
{
    int ret = 0;
    time_t now, last_want_config, last_heartbeat;

    if (meshroom->loadNvm() == false) {
        meshroom->saveNvm();
    }
    meshroom->applyNvmToHomeChat();

    now = time(NULL);
    last_heartbeat = now;
    last_want_config = now;

    meshroom->addMorseText("s");

    for (;;) {
        now = time(NULL);

        if (meshroom->isConnected() &&
            (meshroom->meshDeviceLastRecivedSecondsAgo() > 300) &&
            (meshroom->getLastResetSecsAgo() > 300)) {
            consoles_printf("detected meshtastic stuck!\n");
            meshroom->reset();
        }

        if (!meshroom->isConnected() && ((now - last_want_config) >= 5)) {
            ret = meshroom->sendWantConfig();
            if (ret == false) {
                consoles_printf("sendWantConfig failed!\n");
            }

            last_want_config = now;
        } else if (meshroom->isConnected()) {
            last_want_config = now;
        }

        if (meshroom->isConnected() &&
            ((now - last_heartbeat) >= 60)) {
            ret = meshroom->sendHeartbeat();
            if (ret == false) {
                consoles_printf("sendHeartbeat failed!\n");
            }

            last_heartbeat = now;
        }

        for (;;) {
            ret = serial1_rx_ready();
            if (ret == 0) {
                break;
            } if (ret > 0) {
                ret = mt_serial_process(&meshroom->_mtc, 0);
                if (ret < 0) {
                    consoles_printf("mt_serial_process failed!\n");
                }
            }
            taskYIELD();
        }

        ret = serial1_check_markers();
        if (ret != 0) {
            consoles_printf("serial1 markers violated: %d\n", ret);
        }

        xSemaphoreTake(uart1_sem, pdMS_TO_TICKS(1000));
    }
}

static void shell0_task(__unused void *params)
{
    vTaskDelay(pdMS_TO_TICKS(1500));

    shell0->showWelcome();
    for (;;) {
        int ret = 0;
        do {
            ret = shell0->process();
        } while (ret > 0);
        xSemaphoreTake(cdc_sem, pdMS_TO_TICKS(1000));
    }
}

static void shell1_task(__unused void *params)
{
    serial0_printf("\n\x1b[2K");
    shell1->showWelcome();

    for (;;) {
        int ret = 0;
        do {
            ret = shell1->process();
        } while (ret > 0);

        ret = serial0_check_markers();
        if (ret != 0) {
            consoles_printf("serial0 markers violated: %d\n", ret);
        }

        xSemaphoreTake(uart0_sem, pdMS_TO_TICKS(1000));
    }
}

int consoles_printf(const char *format, ...)
{
    int ret = 0;
    va_list ap;

    va_start(ap, format);
    ret = usbcdc_vprintf(format, ap);
    va_end(ap);

    va_start(ap, format);
    ret = serial0_vprintf(format, ap);
    va_end(ap);

    return ret;
}

int consoles_vprintf(const char *format, va_list ap)
{
    int ret = 0;

    ret = usbcdc_vprintf(format, ap);
    ret = serial0_vprintf(format, ap);

    return ret;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)(xTask);

    usbcdc_printf("stack over-flow: %s!\n", pcTaskName);
    serial0_printf("stack over-flow: %s!\n", pcTaskName);
    for (;;);
}

void vApplicationIdleHook(void)
{

}

int main(void)
{
    TaskHandle_t watchdogTask;
    TaskHandle_t ledTask;
    TaskHandle_t usbTask;
    TaskHandle_t morsebuzzerTask;
    TaskHandle_t meshtasticTask;
    TaskHandle_t shell0Task;
    TaskHandle_t shell1Task;

    board_init();
    tusb_init();
    stdio_init_all();
    if (board_init_after_tusb) {
        board_init_after_tusb();
    }
    usbcdc_init();
    serial_init();
    cyw43_arch_init();

    meshroom = make_shared<MeshRoom>();
    meshroom->setBanner(banner);
    meshroom->setVersion(version);
    meshroom->setBuilt(built);
    meshroom->setCopyright(copyright);
    meshroom->setClient(meshroom);
    meshroom->setNvm(meshroom);
    meshroom->sendDisconnect();

    shell0 = make_shared<MeshRoomShell>();
    shell0->setClient(meshroom);
    shell0->setNvm(meshroom);
    shell0->attach((void *) 1);

    shell1 = make_shared<MeshRoomShell>();
    shell1->setClient(meshroom);
    shell1->setNvm(meshroom);
    shell1->attach((void *) 2);

    xTaskCreate(watchdog_task,
                "Watchdog",
                WATCHDOG_TASK_STACK_SIZE,
                NULL,
                WATCHDOG_TASK_PRIORITY,
                &watchdogTask);

    xTaskCreate(led_task,
                "Led",
                LED_TASK_STACK_SIZE,
                NULL,
                LED_TASK_PRIORITY,
                &ledTask);

    xTaskCreate(usb_task,
                "USB",
                USB_TASK_STACK_SIZE,
                NULL,
                USB_TASK_PRIORITY,
                &usbTask);

    xTaskCreate(morsebuzzer_task,
                "MorseBuzzer",
                MORSEBUZZER_TASK_STACK_SIZE,
                NULL,
                MORSEBUZZER_TASK_PRIORITY,
                &morsebuzzerTask);

    xTaskCreate(meshtastic_task,
                "Meshtastic",
                MESHTASTIC_TASK_STACK_SIZE,
                NULL,
                MESHTASTIC_TASK_PRIORITY,
                &meshtasticTask);

    xTaskCreate(shell0_task,
                "Shell0",
                SHELL0_TASK_STACK_SIZE,
                NULL,
                SHELL0_TASK_PRIORITY,
                &shell0Task);

    xTaskCreate(shell1_task,
                "Shell1",
                SHELL1_TASK_STACK_SIZE,
                NULL,
                SHELL1_TASK_PRIORITY,
                &shell1Task);

#if defined(configUSE_CORE_AFFINITY) && (configNUMBER_OF_CORES > 1)
    vTaskCoreAffinitySet(watchdogTask, 0x1);
    vTaskCoreAffinitySet(ledTask, 0x1);
    vTaskCoreAffinitySet(usbTask, 0x1);
    vTaskCoreAffinitySet(morsebuzzerTask, 0x1);
    vTaskCoreAffinitySet(meshtasticTask, 0x2);
    vTaskCoreAffinitySet(shell0Task, 0x2);
    vTaskCoreAffinitySet(shell1Task, 0x2);
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
