/*
 * shell.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <ctype.h>
#include <malloc.h>
#include <string.h>
#include <strings.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <meshroom.h>
#include <libmeshtastic.h>
#include <memory>
#include <MeshRoom.hxx>

#define shell_printf(...) serial_printf(uart0, __VA_ARGS__)

struct inproc {
    char cmdline[256];
    unsigned int i;
};

struct cmd_handler {
    const char *name;
    int (*f)(int argc, char **argv);
};

static struct inproc inproc;
extern shared_ptr<MeshRoom> meshroom;

static int system(int argc, char **argv)
{
    extern char __StackTop, __StackBottom;
    extern char __StackLimit, __bss_end__;
    struct mallinfo m = mallinfo();
    unsigned int stack_size = &__StackTop - &__StackBottom;
    unsigned int total_heap = &__StackLimit  - &__bss_end__;
    unsigned int used_heap = m.uordblks;
    unsigned int free_heap = total_heap - used_heap;
    unsigned int uptime = (unsigned int) (time_us_64() / 1000000);
    unsigned int days, hour, min, sec;

    (void)(argc);
    (void)(argv);

    sec = uptime % 60;
    min = (uptime / 60) % 60;
    hour = (uptime / 3600) % 24;
    days = (uptime) / 86400;
    if (days == 0) {
        serial_printf(uart0, "  Up-time:  %.2u:%.2u:%.2u",
                      hour, min, sec);
    } else {
        serial_printf(uart0, "  Up-time:  %ud %.2u:%.2u:%.2u",
                      days, hour, min, sec);
    }
    if (watchdog_caused_reboot()) {
        serial_printf(uart0, " (rebooted by watchdog timer)\n");
    } else {
        serial_printf(uart0, "\n");
    }
#if defined(MEASURE_CPU_UTILIZATION)
    serial_printf(uart0, " CPU Util.: %.3f%%\n",
                  ((float) t_cpu_busy) / ((float) t_cpu_total) * 100.0);
#endif
    serial_printf(uart0, "Stack Size: %8u bytes\n", stack_size);
    serial_printf(uart0, "Total Heap: %8u bytes\n", total_heap);
    serial_printf(uart0, " Free Heap: %8u bytes\n", free_heap);
    serial_printf(uart0, " Used Heap: %8u bytes\n", used_heap);

    return 0;
}

static int bootsel(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    meshroom->sendDisconnect();
    serial_printf(uart0, "Rebooting to BOOTSEL mode\n");
    reset_usb_boot(0, 0);

    return 0;
}

static int reboot(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    meshroom->sendDisconnect();
    watchdog_enable(1, 0);
    for (;;);

    return 0;
}

static int status(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);

    if (!meshroom->isConnected()) {
        serial_printf(uart0, "Not connected\n");
    } else {
        unsigned int i;

        serial_printf(uart0, "Me: %s %s\n",
                      meshroom->getDisplayName(meshroom->whoami()).c_str(),
                      meshroom->lookupLongName(meshroom->whoami()).c_str());

#if 0
        serial_printf(uart0, "Channels: %d\n",
                      meshroom->channels().size());
        for (map<uint8_t, meshtastic_Channel>::const_iterator it =
                 meshroom->channels().begin();
             it != meshroom->channels().end(); it++) {
            if (it->second.has_settings &&
                it->second.role != meshtastic_Channel_Role_DISABLED) {
                serial_printf(uart0, "chan#%u: %s\n",
                              (unsigned int) it->second.index,
                              it->second.settings.name);
            }
        }
#else
        ret = 0;
        for (i = 0; i < meshroom->channels().size(); i++) {
            if (meshroom->isChannelValid(i)) {
                ret++;
            }
        }
        serial_printf(uart0, "Channels: %d\n", ret);
        for (i = 0; i < meshroom->channels().size(); i++) {
            if (meshroom->isChannelValid(i)) {
                serial_printf(uart0, "  chan#%u: %s\n", i,
                              meshroom->getChannelName(i).c_str());
            }
        }
#endif

        serial_printf(uart0, "Nodes: %d seen\n",
                      meshroom->nodeInfos().size());
        i = 0;
        for (map<uint32_t, meshtastic_NodeInfo>::const_iterator it =
                 meshroom->nodeInfos().begin();
             it != meshroom->nodeInfos().end(); it++, i++) {
            if ((i % 4) == 0) {
                serial_printf(uart0, "  ");
            }
            serial_printf(uart0, "%16s  ",
                          meshroom->getDisplayName(it->second.num).c_str());
            if ((i % 4) == 3) {
                serial_printf(uart0, "\n");
            }
        }
        if ((i % 4) != 0) {
            serial_printf(uart0, "\n");
        }

        map<uint32_t, meshtastic_DeviceMetrics>::const_iterator dev;
        dev = meshroom->deviceMetrics().find(meshroom->whoami());
        if (dev != meshroom->deviceMetrics().end()) {
            if (dev->second.has_channel_utilization) {
                serial_printf(uart0, "channel_utilization: %.2f\n",
                              dev->second.channel_utilization);
            }
            if (dev->second.has_air_util_tx) {
                serial_printf(uart0, "air_util_tx: %.2f\n",
                              dev->second.air_util_tx);
            }
        }

        map<uint32_t, meshtastic_EnvironmentMetrics>::const_iterator env;
        env = meshroom->environmentMetrics().find(meshroom->whoami());
        if (env != meshroom->environmentMetrics().end()) {
            if (env->second.has_temperature) {
                serial_printf(uart0, "temperature: %.2f\n",
                              env->second.temperature);
            }
            if (env->second.has_relative_humidity) {
                serial_printf(uart0, "relative_humidity: %.2f\n",
                              env->second.relative_humidity);
            }
            if (env->second.has_barometric_pressure) {
                serial_printf(uart0, "barometric_pressure: %.2f\n",
                              env->second.barometric_pressure);
            }
        }
    }

    return ret;
}

static int want_config(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroom->sendWantConfig() != true) {
        serial_printf(uart0, "failed!\n");
        ret = -1;
    }

    return ret;
}

static int disconnect(int argc, char **argv)
{
   int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroom->sendDisconnect() != true) {
        serial_printf(uart0, "failed!\n");
        ret = -1;
    }

    return ret;
}

static int heartbeat(int argc, char **argv)
{
   int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroom->sendHeartbeat() != true) {
        serial_printf(uart0, "failed!\n", ret);
        ret = -1;
    }

    return ret;
}

static int direct_message(int argc, char **argv)
{
    int ret = 0;
    uint32_t dest = 0xffffffffU;
    string message;

    if (argc < 3) {
        serial_printf(uart0, "Usage: %s [name] message\n", argv[0]);
        ret = -1;
        goto done;
    }

    dest = meshroom->getId(argv[1]);
    if ((dest == 0xffffffffU) || (dest == meshroom->whoami())) {
        serial_printf(uart0, "name '%s' is invalid!\n", argv[1]);
        ret = -1;
        goto done;
    }

    for (int i = 2; i < argc; i++) {
        message += argv[i];
        message += " ";
    }

    if (meshroom->textMessage(dest, 0x0U, message) != true) {
        serial_printf(uart0, "failed!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

static int channel_message(int argc, char **argv)
{
    int ret = 0;
    uint8_t channel = 0xffU;
    string message;

    if (argc < 3) {
        serial_printf(uart0, "Usage: %s [chan] message\n", argv[0]);
        ret = -1;
        goto done;
    }

    channel = meshroom->getChannel(argv[1]);
    if (channel == 0xffU) {
        serial_printf(uart0, "channel '%s' is invalid!\n", argv[1]);
        ret = -1;
        goto done;
    }

    for (int i = 2; i < argc; i++) {
        message += argv[i];
        message += " ";
    }

    if (meshroom->textMessage(0xffffffffU, channel, message) != true) {
        serial_printf(uart0, "failed!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

static int help(int argc, char **argv);

static struct cmd_handler cmd_handlers[] = {
    { "help", help, },
    { "system", system, },
    { "bootsel", bootsel, },
    { "reboot", reboot, },
    { "status", status, },
    { "want_config", want_config, },
    { "disconnect", disconnect, },
    { "heartbeat", heartbeat, },
    { "dm", direct_message },
    { "cm", channel_message, },
    { NULL, NULL, },
};

static int help(int argc, char **argv)
{
    int i;

    (void)(argc);
    (void)(argv);

    shell_printf("Available commands:\n");

    for (i = 0; cmd_handlers[i].name && cmd_handlers[i].f; i++) {
        if ((i % 4) == 0) {
            shell_printf("\t");
        }

        shell_printf("%s\t", cmd_handlers[i].name);

        if ((i % 4) == 3) {
            shell_printf("\n");
        }
    }

    if ((i % 4) != 0) {
        shell_printf("\n");
    }

    return 0;
}

static void execute_cmdline(char *cmdline)
{
    int argc = 0;
    char *argv[32];

    bzero(argv, sizeof(argv));

    // Tokenize cmdline into argv[]
    while (*cmdline != '\0' && (argc < 32)) {
        while ((*cmdline != '\0') && isspace((int) *cmdline)) {
            cmdline++;
        }

        if (*cmdline == '\0') {
            break;
        }

        argv[argc] = cmdline;
        argc++;

        while ((*cmdline != '\0') && !isspace((int) *cmdline)) {
            cmdline++;
        }

        if (*cmdline == '\0') {
            break;
        }

        *cmdline = '\0';
        cmdline++;
    }

    if (argc == 0) {
        goto done;
    }

    for (int i = 0; cmd_handlers[i].name && cmd_handlers[i].f; i++) {
        if (strcmp(cmd_handlers[i].name, argv[0]) == 0) {
            cmd_handlers[i].f(argc, argv);
            goto done;
        }
    }

    shell_printf("Unknown command: '%s'!\n", argv[0]);

done:

    return;
}

void shell_init(void)
{
    bzero(&inproc, sizeof(inproc));
    serial_print_str(uart0, "> ");
}

void shell_process(void)
{
    char c;

    while (serial_rx_ready(uart0) > 0) {
        serial_read(uart0, &c, 1);
        if (c == '\r') {
            inproc.cmdline[inproc.i] = '\0';
            serial_print_str(uart0, "\n");
            execute_cmdline(inproc.cmdline);
            serial_print_str(uart0, "> ");
            inproc.i = 0;
            inproc.cmdline[0] = '\0';
        } else if ((c == '\x7f') || (c == '\x08')) {
            if (inproc.i > 0) {
                shell_printf("\b \b", 3);
                inproc.i--;
            }
        } else if (c == '\x03') {
            shell_printf("^C\n> ");
            inproc.i = 0;
        } else if ((c != '\n') && isprint(c)) {
            if (inproc.i < (sizeof(inproc.cmdline) - 1)) {
                serial_write(uart0, &c, 1);
                inproc.cmdline[inproc.i] = c;
                inproc.i++;
            }
        }
    }
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
