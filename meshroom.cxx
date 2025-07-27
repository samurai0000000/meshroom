/*
 * meshroom.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <pico/stdlib.h>
#include <hardware/sync.h>
#include <libmeshtastic.h>
#include <MeshRoom.hxx>
#include <meshroom.h>

static MeshRoom meshroom;

int main(void)
{
    int ret = 0;
    bool led_on = false;
    unsigned int loop;

    stdio_init_all();

    led_init();

    serial_init();
    serial_print_str(0,"\e[1;1H\e[2J\n");
    serial_print_str(0, "The meshroom firmware for Raspberry Pi Pico\n");
    serial_print_str(0, "-------------------------------------------\n");
    serial_print_str(0, "Copyright (C) 2025, Charles Chiou\n");
    shell_init();

    for (loop = 0; true; loop++) {
        led_set(led_on);
        led_on = !led_on;

        if (!meshroom.isConnected()) {
            meshroom.sendWantConfig();
        }

        while (serial_rx_ready(device_chan)) {
            mt_serial_process(&meshroom._mtc, 0);
        }
        shell_process();
        best_effort_wfe_or_timeout(make_timeout_time_us(1000000));

        if ((loop % 60) == 0) {
            meshroom.sendHeartbeat();
        }
    }

    return ret;
}

EXTERN_C_BEGIN

int status(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);

    if (!meshroom.isConnected()) {
        serial_printf(console_chan, "Not connected\n");
    } else {
        unsigned int i;

        serial_printf(console_chan, "Me: %s %s\n",
                      meshroom.getDisplayName(meshroom.whoami()).c_str(),
                      meshroom.lookupLongName(meshroom.whoami()).c_str());
        serial_printf(console_chan, "Nodes: %d seen\n",
                      meshroom.nodeInfos().size());
        i = 0;
        for (map<uint32_t, meshtastic_NodeInfo>::const_iterator it =
                 meshroom.nodeInfos().begin();
             it != meshroom.nodeInfos().end(); it++, i++) {
            if ((i % 6) == 0) {
                serial_printf(console_chan, "\t");
            }
            if (it->second.has_user) {
                serial_printf(console_chan, "%s\t",
                              it->second.user.short_name);
            } else {
                serial_printf(console_chan, "!%.8x\t",
                              it->second.num);
            }
            if ((i % 6) == 5) {
                serial_printf(console_chan, "\n");
            }
        }
        if ((i % 6) != 0) {
            serial_printf(console_chan, "\n");
        }

        map<uint32_t, meshtastic_DeviceMetrics>::const_iterator dev;
        dev = meshroom.deviceMetrics().find(meshroom.whoami());
        if (dev != meshroom.deviceMetrics().end()) {
            if (dev->second.has_channel_utilization) {
                serial_printf(console_chan, "channel_utilization: %.2f\n",
                              dev->second.channel_utilization);
            }
            if (dev->second.has_air_util_tx) {
                serial_printf(console_chan, "air_util_tx: %.2f\n",
                              dev->second.air_util_tx);
            }
        }

        map<uint32_t, meshtastic_EnvironmentMetrics>::const_iterator env;
        env = meshroom.environmentMetrics().find(meshroom.whoami());
        if (env != meshroom.environmentMetrics().end()) {
            if (env->second.has_temperature) {
                serial_printf(console_chan, "temperature: %.2f\n",
                              env->second.temperature);
            }
            if (env->second.has_relative_humidity) {
                serial_printf(console_chan, "relative_humidity: %.2f\n",
                              env->second.relative_humidity);
            }
            if (env->second.has_barometric_pressure) {
                serial_printf(console_chan, "barometric_pressure: %.2f\n",
                              env->second.barometric_pressure);
            }
        }
    }

    return ret;
}

int want_config(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroom.sendWantConfig() != true) {
        serial_printf(console_chan, "failed!\n");
        ret = -1;
    }

    return ret;
}

int disconnect(int argc, char **argv)
{
   int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroom.sendDisconnect() != true) {
        serial_printf(console_chan, "failed!\n");
        ret = -1;
    }

    return ret;
}

int heartbeat(int argc, char **argv)
{
   int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroom.sendHeartbeat() != true) {
        serial_printf(console_chan, "failed!\n", ret);
        ret = -1;
    }

    return ret;
}

int direct_message(int argc, char **argv)
{
    int ret = 0;
    uint32_t dest = 0xffffffffU;
    string message;

    if (argc < 3) {
        serial_printf(console_chan, "Usage: %s [name] message\n", argv[0]);
        ret = -1;
        goto done;
    }

    dest = meshroom.getId(argv[1]);
    if ((dest == 0xffffffffU) || (dest == meshroom.whoami())) {
        serial_printf(console_chan, "name '%s' is invalid!\n", argv[1]);
        ret = -1;
        goto done;
    }

    for (int i = 2; i < argc; i++) {
        message += argv[i];
        message += " ";
    }

    if (meshroom.textMessage(dest, 0x0U, message.c_str()) != true) {
        serial_printf(console_chan, "failed!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

int channel_message(int argc, char **argv)
{
    int ret = 0;
    uint8_t channel = 0xffU;
    string message;

    if (argc < 3) {
        serial_printf(console_chan, "Usage: %s [chan] message\n", argv[0]);
        ret = -1;
        goto done;
    }

    channel = meshroom.getChannel(argv[1]);
    if (channel == 0xffU) {
        serial_printf(console_chan, "channel '%s' is invalid!\n", argv[1]);
        ret = -1;
        goto done;
    }

    for (int i = 2; i < argc; i++) {
        message += argv[i];
        message += " ";
    }

    if (meshroom.textMessage(0xffffffffU, channel, message.c_str()) != true) {
        serial_printf(console_chan, "failed!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

EXTERN_C_END

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
