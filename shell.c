/*
 * shell.c
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

#define shell_printf(...) serial_printf(console_chan, __VA_ARGS__)

struct inproc {
    char cmdline[256];
    unsigned int i;
};

struct cmd_handler {
    const char *name;
    int (*f)(int argc, char **argv);
};

static struct inproc inproc;
static struct cmd_handler cmd_handlers[];

void shell_init(void)
{
    bzero(&inproc, sizeof(inproc));
    serial_print_str(0, "> ");
}

static void execute_cmdline(char *cmdline);

void shell_process(void)
{
    char c;

    while (serial_rx_ready(console_chan) > 0) {
        serial_read(console_chan, &c, 1);
        if (c == '\r') {
            inproc.cmdline[inproc.i] = '\0';
            serial_print_str(console_chan, "\n");
            execute_cmdline(inproc.cmdline);
            serial_print_str(console_chan, "> ");
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
                serial_write(console_chan, &c, 1);
                inproc.cmdline[inproc.i] = c;
                inproc.i++;
            }
        }
    }
}

static void execute_cmdline(char *cmdline)
{
    int argc = 0;
    char *argv[16];

    bzero(argv, sizeof(argv));

    while (*cmdline != '\0') {
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

int help(int argc, char **argv)
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

int system(int argc, char **argv)
{
    extern char __StackLimit, __bss_end__;;
    struct mallinfo m = mallinfo();
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
        serial_printf(console_chan, "Up-time:    %.2u:%.2u:%.2u",
                      hour, min, sec);
    } else {
        serial_printf(console_chan, "Up-time:    %ud %.2u:%.2u:%.2u",
                      days, hour, min, sec);
    }
    if (watchdog_caused_reboot()) {
        serial_printf(console_chan, " (rebooted by watchdog timer)\n");
    } else {
        serial_printf(console_chan, "\n");
    }
    serial_printf(console_chan, "Total Heap: %8u bytes\n", total_heap);
    serial_printf(console_chan, " Free Heap: %8u bytes\n", free_heap);
    serial_printf(console_chan, " Used Heap: %8u bytes\n", used_heap);

    return 0;
}

int bootsel(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    serial_printf(console_chan, "Rebooting to BOOTSEL mode\n");
    reset_usb_boot(0, 0);

    return 0;
}

int reboot(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    watchdog_enable(1, 0);
    for (;;);

    return 0;
}

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

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
