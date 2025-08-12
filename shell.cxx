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
#include "version.h"

#define CMDLINE_SIZE 256

struct inproc {
    char cmdline[CMDLINE_SIZE];
    unsigned int i;
};

struct cmd_handler {
    const char *name;
    int (*f)(int argc, char **argv);
};

static struct inproc inproc;
extern shared_ptr<MeshRoom> meshroom;

static int version(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    console_printf("Version: %s\n", MYPROJECT_VERSION_STRING);
    console_printf("Built: %s@%s %s\n",
                   MYPROJECT_WHOAMI, MYPROJECT_HOSTNAME, MYPROJECT_DATE);

    return 0;
}

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
        console_printf("   Up-time: %.2u:%.2u:%.2u",
                       hour, min, sec);
    } else {
        console_printf("   Up-time: %ud %.2u:%.2u:%.2u",
                       days, hour, min, sec);
    }
    if (watchdog_caused_reboot()) {
        console_printf(" (rebooted by watchdog timer)\n");
    } else {
        console_printf("\n");
    }
#if defined(MEASURE_CPU_UTILIZATION)
    console_printf(" CPU Util.: %7.3f%%\n",
                   ((float) t_cpu_busy) / ((float) t_cpu_total) * 100.0);
#endif
    console_printf("Stack Size: %8u bytes\n", stack_size);
    console_printf("Total Heap: %8u bytes\n", total_heap);
    console_printf(" Free Heap: %8u bytes\n", free_heap);
    console_printf(" Used Heap: %8u bytes\n", used_heap);
    if (serial_rx_overflow > 0) {
        console_printf(" Serial RX Overflow: %u\n", serial_rx_overflow);
    }

    return 0;
}

static int bootsel(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    meshroom->sendDisconnect();
    console_printf("Rebooting to BOOTSEL mode\n");
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
        console_printf("Not connected\n");
    } else {
        unsigned int i;

        console_printf("Me: %s %s\n",
                       meshroom->getDisplayName(meshroom->whoami()).c_str(),
                       meshroom->lookupLongName(meshroom->whoami()).c_str());

        console_printf("Channels: %d\n",
                       meshroom->channels().size());
        for (map<uint8_t, meshtastic_Channel>::const_iterator it =
                 meshroom->channels().begin();
             it != meshroom->channels().end(); it++) {
            if (it->second.has_settings &&
                it->second.role != meshtastic_Channel_Role_DISABLED) {
                console_printf("chan#%u: %s\n",
                               (unsigned int) it->second.index,
                               it->second.settings.name);
            }
        }

        console_printf("Nodes: %d seen\n",
                       meshroom->nodeInfos().size());
        i = 0;
        for (map<uint32_t, meshtastic_NodeInfo>::const_iterator it =
                 meshroom->nodeInfos().begin();
             it != meshroom->nodeInfos().end(); it++, i++) {
            if ((i % 4) == 0) {
                console_printf("  ");
            }
            console_printf("%16s  ",
                           meshroom->getDisplayName(it->second.num).c_str());
            if ((i % 4) == 3) {
                console_printf("\n");
            }
        }
        if ((i % 4) != 0) {
            console_printf("\n");
        }

        map<uint32_t, meshtastic_DeviceMetrics>::const_iterator dev;
        dev = meshroom->deviceMetrics().find(meshroom->whoami());
        if (dev != meshroom->deviceMetrics().end()) {
            if (dev->second.has_channel_utilization) {
                console_printf("channel_utilization: %.2f\n",
                               dev->second.channel_utilization);
            }
            if (dev->second.has_air_util_tx) {
                console_printf("air_util_tx: %.2f\n",
                               dev->second.air_util_tx);
            }
        }

        map<uint32_t, meshtastic_EnvironmentMetrics>::const_iterator env;
        env = meshroom->environmentMetrics().find(meshroom->whoami());
        if (env != meshroom->environmentMetrics().end()) {
            if (env->second.has_temperature) {
                console_printf("temperature: %.2f\n",
                               env->second.temperature);
            }
            if (env->second.has_relative_humidity) {
                console_printf("relative_humidity: %.2f\n",
                               env->second.relative_humidity);
            }
            if (env->second.has_barometric_pressure) {
                console_printf("barometric_pressure: %.2f\n",
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
        console_printf("failed!\n");
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
        console_printf("failed!\n");
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
        console_printf("failed!\n", ret);
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
        console_printf("Usage: %s [name] message\n", argv[0]);
        ret = -1;
        goto done;
    }

    dest = meshroom->getId(argv[1]);
    if ((dest == 0xffffffffU) || (dest == meshroom->whoami())) {
        console_printf("name '%s' is invalid!\n", argv[1]);
        ret = -1;
        goto done;
    }

    for (int i = 2; i < argc; i++) {
        message += argv[i];
        message += " ";
    }

    if (meshroom->textMessage(dest, 0x0U, message) != true) {
        console_printf("failed!\n");
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
        console_printf("Usage: %s [chan] message\n", argv[0]);
        ret = -1;
        goto done;
    }

    channel = meshroom->getChannel(argv[1]);
    if (channel == 0xffU) {
        console_printf("channel '%s' is invalid!\n", argv[1]);
        ret = -1;
        goto done;
    }

    for (int i = 2; i < argc; i++) {
        message += argv[i];
        message += " ";
    }

    if (meshroom->textMessage(0xffffffffU, channel, message) != true) {
        console_printf("failed!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

static int ir(int argc, char **argv)
{
    int ret = 0;
    uint32_t ir_flags = meshroom->ir_flags();

    if (argc == 1) {
        console_printf("infrared:");
        if (ir_flags & MESHROOM_IR_SONY_BRAVIA) {
            console_printf(" sony_bravia ");
        }
        if (ir_flags & MESHROOM_IR_SAMSUNG_TV) {
            console_printf(" samsung_tv ");
        }
        if (ir_flags & MESHROOM_IR_PANASONIC_AC) {
            console_printf(" panasonic_ac ");
        }
        console_printf("\n");
    } else if ((argc == 3) && strcmp(argv[1], "add") == 0) {
        if (strstr(argv[2], "bravia") != NULL) {
            ir_flags |= MESHROOM_IR_SONY_BRAVIA;
        } else if (strstr(argv[2], "samsung") != NULL) {
            ir_flags |= MESHROOM_IR_SAMSUNG_TV;
        } else if (strstr(argv[2], "panasonic") != NULL) {
            ir_flags |= MESHROOM_IR_PANASONIC_AC;
        } else {
            console_printf("failed!\n");
            ret = -1;
            goto done;
        }
        meshroom->set_ir_flags(ir_flags);
        console_printf("ok\n");
    } else if ((argc == 3) && strcmp(argv[1], "del") == 0) {
        if (strstr(argv[2], "bravia") != NULL) {
            ir_flags &= ~MESHROOM_IR_SONY_BRAVIA;
        } else if (strstr(argv[2], "samsung") != NULL) {
            ir_flags &= ~MESHROOM_IR_SAMSUNG_TV;
        } else if (strstr(argv[2], "panasonic") != NULL) {
            ir_flags &= ~MESHROOM_IR_PANASONIC_AC;
        } else {
            console_printf("failed!\n");
            ret = -1;
            goto done;
        }
        meshroom->set_ir_flags(ir_flags);
        console_printf("ok\n");
    } else {
        console_printf("syntax error!\n");
        ret = -1;
    }

done:

    return ret;
}

static int authchan(int argc, char **argv)
{
    int ret = 0;
    bool result;

    if (argc == 1) {
        console_printf("list of authchans:\n");
        for (unsigned int i = 0; i < meshroom->nvmAuthchans().size(); i++) {
            console_printf("  %s\n", meshroom->nvmAuthchans()[i].name);
        }
    } else if ((argc == 3) && strcmp(argv[1], "add") == 0) {
        result = meshroom->addNvmAuthChannel(argv[2], *meshroom);
        if (result == false) {
            console_printf("addNvmAuthChannel failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroom->applyNvmToHomeChat();
        if (result == false) {
            console_printf("applyNvmToHomeChat failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroom->saveNvm();
        if (result == false) {
            console_printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        console_printf("ok\n");
    } else if ((argc == 3) && strcmp(argv[1], "del") == 0) {
        result = meshroom->delNvmAuthChannel(argv[2]);
        if (result == false) {
            console_printf("delNvmAuthChannel failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroom->applyNvmToHomeChat();
        if (result == false) {
            console_printf("applyNvmToHomeChat failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroom->saveNvm();
        if (result == false) {
            console_printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        console_printf("ok\n");
    } else {
        console_printf("syntax error!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

static int admin(int argc, char **argv)
{
    int ret = 0;
    bool result;

    if (argc == 1) {
        unsigned int i;
        console_printf("list of admins:\n");
        for (i = 0; i < meshroom->nvmAdmins().size(); i++) {
            uint32_t node_num = meshroom->nvmAdmins()[i].node_num;
            if ((i % 4) == 0) {
                console_printf("  ");
            }
            console_printf("%16s  ",
                           meshroom->getDisplayName(node_num).c_str());
            if ((i % 4) == 3) {
                console_printf("\n");
            }
        }
        if ((i % 4) != 0) {
            console_printf("\n");
        }
    } else if ((argc == 3) && strcmp(argv[1], "add") == 0) {
        result = meshroom->addNvmAdmin(argv[2], *meshroom);
        if (result == false) {
            console_printf("addNvmAdmin failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroom->applyNvmToHomeChat();
        if (result == false) {
            console_printf("applyNvmToHomeChat failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroom->saveNvm();
        if (result == false) {
            console_printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        console_printf("ok\n");
    } else if ((argc == 3) && strcmp(argv[1], "del") == 0) {
        result = meshroom->delNvmAdmin(argv[2], *meshroom);
        if (result == false) {
            console_printf("delNvmAdmin failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroom->applyNvmToHomeChat();
        if (result == false) {
            console_printf("applyNvmToHomeChat failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroom->saveNvm();
        if (result == false) {
            console_printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        console_printf("ok\n");
    } else {
        console_printf("syntax error!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

static int mate(int argc, char **argv)
{
    int ret = 0;
    bool result;

    if (argc == 1) {
        unsigned int i;
        console_printf("list of mates:\n");
        for (i = 0; i < meshroom->nvmMates().size(); i++) {
            uint32_t node_num = meshroom->nvmMates()[i].node_num;
            if ((i % 4) == 0) {
                console_printf("  ");
            }
            console_printf("%16s  ",
                           meshroom->getDisplayName(node_num).c_str());
            if ((i % 4) == 3) {
                console_printf("\n");
            }
        }
        if ((i % 4) != 0) {
            console_printf("\n");
        }
    } else if ((argc == 3) && strcmp(argv[1], "add") == 0) {
        result = meshroom->addNvmMate(argv[2], *meshroom);
        if (result == false) {
            console_printf("addNvmMate failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroom->applyNvmToHomeChat();
        if (result == false) {
            console_printf("applyNvmToHomeChat failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroom->saveNvm();
        if (result == false) {
            console_printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        console_printf("ok\n");
    } else if ((argc == 3) && strcmp(argv[1], "del") == 0) {
        result = meshroom->delNvmMate(argv[2], *meshroom);
        if (result == false) {
            console_printf("delNvmMate failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroom->applyNvmToHomeChat();
        if (result == false) {
            console_printf("applyNvmToHomeChat failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroom->saveNvm();
        if (result == false) {
            console_printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        console_printf("ok\n");
    } else {
        console_printf("syntax error!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

static int nvm(int argc, char **argv)
{
    if (argc != 1) {
        console_printf("syntax error!\n");
        return -1;
    }

    ir(argc, argv);
    authchan(argc, argv);
    admin(argc, argv);
    mate(argc, argv);

    return 0;
}

static int help(int argc, char **argv);

static struct cmd_handler cmd_handlers[] = {
    { "help", help, },
    { "version", version, },
    { "system", system, },
    { "bootsel", bootsel, },
    { "reboot", reboot, },
    { "status", status, },
    { "want_config", want_config, },
    { "disconnect", disconnect, },
    { "heartbeat", heartbeat, },
    { "dm", direct_message },
    { "cm", channel_message, },
    { "ir", ir, },
    { "authchan", authchan, },
    { "admin", admin, },
    { "mate", mate, },
    { "nvm", nvm, },
    { NULL, NULL, },
};

static int help(int argc, char **argv)
{
    int i;

    (void)(argc);
    (void)(argv);

    console_printf("Available commands:\n");

    for (i = 0; cmd_handlers[i].name && cmd_handlers[i].f; i++) {
        if ((i % 4) == 0) {
            console_printf("\t");
        }

        console_printf("%s\t", cmd_handlers[i].name);

        if ((i % 4) == 3) {
            console_printf("\n");
        }
    }

    if ((i % 4) != 0) {
        console_printf("\n");
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

    console_printf("Unknown command: '%s'!\n", argv[0]);

done:

    return;
}

void shell_init(void)
{
    bzero(&inproc, sizeof(inproc));
    console_printf("> ");
}

int shell_process(void)
{
    int ret = 0;
    int rx;
    char c;

    while (console_rx_ready() > 0) {
        rx = console_read((uint8_t *) &c, 1);
        if (rx == 0) {
            break;
        }

        ret += rx;

        if (c == '\r') {
            inproc.cmdline[inproc.i] = '\0';
            console_printf("\n");
            execute_cmdline(inproc.cmdline);
            console_printf("> ");
            inproc.i = 0;
            inproc.cmdline[0] = '\0';
        } else if ((c == '\x7f') || (c == '\x08')) {
            if (inproc.i > 0) {
                console_printf("\b \b");
                inproc.i--;
            }
        } else if (c == '\x03') {
            console_printf("^C\n> ");
            inproc.i = 0;
        } else if ((c != '\n') && isprint(c)) {
            if (inproc.i < (CMDLINE_SIZE - 1)) {
                console_printf("%c", c);
                inproc.cmdline[inproc.i] = c;
                inproc.i++;
            }
        }
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
