/*
 * shell.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <pico/stdlib.h>
#include <hardware/sync.h>
#include <meshroom.h>
#include <libmeshtastic.h>

#define shell_printf(...) serial_printf(inproc.chan, __VA_ARGS__)

struct inproc {
    int chan;
    char cmdline[256];
    unsigned int i;
};

struct cmd_handler {
    const char *name;
    int (*f)(int argc, char **argv);
};

static struct inproc inproc;
static struct cmd_handler cmd_handlers[];

void shell_init(int chan)
{
    bzero(&inproc, sizeof(inproc));
    inproc.chan = chan;
    serial_print_str(0, "> ");
}

static void execute_cmdline(char *cmdline);

void shell_process(void)
{
    char c;

    while (serial_rx_ready(inproc.chan) > 0) {
        serial_read(inproc.chan, &c, 1);
        if (c == '\r') {
            inproc.cmdline[inproc.i] = '\0';
            serial_print_str(inproc.chan, "\n");
            execute_cmdline(inproc.cmdline);
            serial_print_str(inproc.chan, "> ");
            inproc.i = 0;
            inproc.cmdline[0] = '\0';
        } else if ((c == '\x7f') || (c == '\x08')) {
            shell_printf("\b \b", 3);
            inproc.i--;
        } else if (c == '\x03') {
            shell_printf("^C\n> ");
            inproc.i = 0;
        } else if ((c != '\n') && isprint(c)) {
            if (inproc.i < (sizeof(inproc.cmdline) - 1)) {
                serial_write(inproc.chan, &c, 1);
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

int wcfg(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    if (G_mtc == NULL) {
        shell_printf("mtc is NULL!\n");
    } else {
        mt_send_want_config(G_mtc);
    }

    return 0;
}

static struct cmd_handler cmd_handlers[] = {
    { "help", help, },
    { "wcfg", wcfg, },
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
