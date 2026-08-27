/*
 * MeshRoomShell.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <malloc.h>
#include <stdexcept>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <hardware/clocks.h>
#include <hardware/uart.h>
#include <pico/bootrom.h>
#include <FreeRTOS.h>
#include <task.h>
#include <pico-plat.h>
#include <PicoPlatform.hxx>
#include <libmeshtastic.h>
#include <MeshRoom.hxx>
#include <MeshRoomShell.hxx>

extern shared_ptr<MeshRoom> meshroom;

static void flush_serial_console(void)
{
    uart_tx_wait_blocking(uart0);
}

MeshRoomShell::MeshRoomShell(shared_ptr<SimpleClient> client)
    : SimpleShell(client)
{
    _help_list.push_back("ir");
    _help_list.push_back("bootsel");
    _help_list.push_back("tv");
    _help_list.push_back("ac");
    _help_list.push_back("buzz");
    _help_list.push_back("morse");
    _help_list.push_back("reset");
    _help_list.push_back("watchdog");
}

MeshRoomShell::~MeshRoomShell()
{

}

int MeshRoomShell::tx_write(const uint8_t *buf, size_t size)
{
    int ret = 0;
    int console_id = (int) _ctx;

    if (console_id == 1) {
        ret = usbcdc_write(buf, size);
    } else if (console_id == 2) {
        ret = serial0_write(buf, size);
    } else {
        ret = -1;
    }

    return ret;
}

int MeshRoomShell::printf(const char *format, ...)
{
    int ret = 0;
    va_list ap;
    int console_id = (int) _ctx;

    va_start(ap, format);
    if (console_id == 1) {
        ret = usbcdc_vprintf(format, ap);
    } else if (console_id == 2) {
        ret = serial0_vprintf(format, ap);
    } else {
        ret = -1;
    }
    va_end(ap);

    return ret;
}

int MeshRoomShell::rx_ready(void) const
{
    int ret = 0;
    int console_id = (int) _ctx;

    if (console_id == 1) {
        ret = usbcdc_rx_ready();
    } else if (console_id == 2) {
        ret = serial0_rx_ready();
    } else {
        ret = -1;
    }

    return ret;
}

int MeshRoomShell::rx_read(uint8_t *buf, size_t size)
{
    int ret = 0;
    int console_id = (int) _ctx;

    if (console_id == 1) {
        ret = usbcdc_read(buf, size);
    } else if (console_id == 2) {
        ret = serial0_read(buf, size);
    } else {
        ret = -1;
    }

    return ret;
}

int MeshRoomShell::system(int argc, char **argv)
{
    int ret = 0;
    extern char __StackLimit, __bss_end__;

    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [-v]\n", argv[0]);
        this->printf("  Display system uptime, heap, temperature, and FreeRTOS tasks.\n");
        this->printf("Options:\n");
        this->printf("  -v            Verbose output (show clock frequencies)\n");
        return 0;
    }
    struct mallinfo m = mallinfo();
    unsigned int total_heap = &__StackLimit  - &__bss_end__;
    unsigned int used_heap = m.uordblks;
    unsigned int free_heap = total_heap - used_heap;
    char cTaskListBuffer[512];

    SimpleShell::system(argc, argv);
    this->printf("  Platform: %s\n", PicoPlatform::get()->getName().c_str());
    this->printf("Total Heap: %8u bytes\n", total_heap);
    this->printf(" Free Heap: %8u bytes\n", free_heap);
    this->printf(" Used Heap: %8u bytes\n", used_heap);
    this->printf("Board Temp:     %.1fC\n", meshroom->getOnboardTempC());
    if ((argc == 2) && (strcmp(argv[1], "-v") == 0)) {
        this->printf("clk_ref:  %lu Hz\n", clock_get_hz(clk_ref));
        this->printf("clk_sys:  %lu Hz\n", clock_get_hz(clk_sys));
        this->printf("clk_usb:  %lu Hz\n", clock_get_hz(clk_usb));
        this->printf("clk_adc:  %lu Hz\n", clock_get_hz(clk_adc));
        this->printf("clk_peri: %lu Hz\n", clock_get_hz(clk_peri));
    }
    bzero(cTaskListBuffer, sizeof(cTaskListBuffer));
    vTaskListTasks(cTaskListBuffer, sizeof(cTaskListBuffer));
    this->printf("  FreeRTOS:\n");
    this->printf("Name        State  Priority  StackRem   Task#   CPU Affn\n");
    this->printf("--------------------------------------------------------\n");
    this->printf("%s", cTaskListBuffer);

    return ret;
}

int MeshRoomShell::reboot(int argc, char **argv)
{
    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help]\n", argv[0]);
        this->printf("  Reboot the system.\n");
        return 0;
    }

    (void)(argc);
    (void)(argv);

    this->printf("Disconnect from meshtastic\n");
    meshroom->sendDisconnect();
    this->printf("Rebooting ...\n");
    flush_serial_console();
    PicoPlatform::get()->reboot();

    return 0;
}

int MeshRoomShell::bootsel(int argc, char **argv)
{
    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help]\n", argv[0]);
        this->printf("  Reboot microcontroller into USB BOOTSEL mode for firmware flashing.\n");
        return 0;
    }

    (void)(argc);
    (void)(argv);

    meshroom->sendDisconnect();
    this->printf("Rebooting to BOOTSEL mode ...\n");
    flush_serial_console();
    PicoPlatform::get()->bootsel();

    return 0;
}

int MeshRoomShell::ir(int argc, char **argv)
{
    int ret = 0;
    uint32_t ir_flags = meshroom->ir_flags();

    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        this->printf("  Configure infrared remote control protocols.\n");
        this->printf("Commands:\n");
        this->printf("  ir                           Show enabled IR protocols\n");
        this->printf("  ir add <protocol>            Enable an IR protocol (sony_bravia, samsung_tv, panasonic_ac, panasonic_tv)\n");
        this->printf("  ir del <protocol>            Disable an IR protocol\n");
        return 0;
    }

    if (argc == 1) {
        this->printf("infrared:");
        if (ir_flags & MESHROOM_IR_SONY_BRAVIA) {
            this->printf(" sony_bravia ");
        }
        if (ir_flags & MESHROOM_IR_SAMSUNG_TV) {
            this->printf(" samsung_tv ");
        }
        if (ir_flags & MESHROOM_IR_PANASONIC_AC) {
            this->printf(" panasonic_ac ");
        }
        if (ir_flags & MESHROOM_IR_PANASONIC_TV) {
            this->printf(" panasonic_tv ");
        }
        this->printf("\n");
    } else if ((argc == 3) && strcmp(argv[1], "add") == 0) {
        if (strstr(argv[2], "bravia") != NULL || strstr(argv[2], "sony") != NULL) {
            ir_flags |= MESHROOM_IR_SONY_BRAVIA;
        } else if (strstr(argv[2], "samsung") != NULL) {
            ir_flags |= MESHROOM_IR_SAMSUNG_TV;
        } else if (strstr(argv[2], "panasonic_tv") != NULL || strstr(argv[2], "panasonic-tv") != NULL) {
            ir_flags |= MESHROOM_IR_PANASONIC_TV;
        } else if (strstr(argv[2], "panasonic") != NULL) {
            ir_flags |= MESHROOM_IR_PANASONIC_AC;
        } else {
            this->printf("failed!\n");
            ret = -1;
            goto done;
        }
        meshroom->set_ir_flags(ir_flags);
        if (meshroom->saveNvm()) {
            this->printf("ok\n");
        } else {
            this->printf("failed!\n");
        }
    } else if ((argc == 3) && strcmp(argv[1], "del") == 0) {
        if (strstr(argv[2], "bravia") != NULL || strstr(argv[2], "sony") != NULL) {
            ir_flags &= ~MESHROOM_IR_SONY_BRAVIA;
        } else if (strstr(argv[2], "samsung") != NULL) {
            ir_flags &= ~MESHROOM_IR_SAMSUNG_TV;
        } else if (strstr(argv[2], "panasonic_tv") != NULL || strstr(argv[2], "panasonic-tv") != NULL) {
            ir_flags &= ~MESHROOM_IR_PANASONIC_TV;
        } else if (strstr(argv[2], "panasonic") != NULL) {
            ir_flags &= ~MESHROOM_IR_PANASONIC_AC;
        } else {
            this->printf("failed!\n");
            ret = -1;
            goto done;
        }
        meshroom->set_ir_flags(ir_flags);
        if (meshroom->saveNvm()) {
            this->printf("ok\n");
        } else {
            this->printf("failed!\n");
        }
    } else {
        this->printf("syntax error!\n");
        ret = -1;
    }

done:

    return ret;
}

int MeshRoomShell::tv(int argc, char **argv)
{
    int ret = 0;

    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        this->printf("  Control TV power, volume, channel, mute, and input via IR.\n");
        this->printf("Commands:\n");
        this->printf("  tv                           Display TV power, volume, channel, mute, and protocol\n");
        this->printf("  tv on                        Turn TV power on\n");
        this->printf("  tv off                       Turn TV power off\n");
        this->printf("  tv toggle                    Toggle TV power\n");
        this->printf("  tv vol [up|down|<0-100>]     Increase, decrease, or set volume level\n");
        this->printf("  tv chan [up|down|<1-999>]    Increase, decrease, or set channel number\n");
        this->printf("  tv mute [on|off|toggle]      Mute, unmute, or toggle TV audio\n");
        this->printf("  tv input                     Cycle TV video input source\n");
        this->printf("  tv key <0-9>                 Send direct remote numeric key\n");
        return 0;
    }

    if (argc == 1) {
        this->printf("tv: %s\n", meshroom->tvOnOff() ? "on" : "off");
        this->printf("vol: %u\n", meshroom->tvVol());
        this->printf("chan: %u\n", meshroom->tvChan());
        this->printf("mute: %s\n", meshroom->tvMute() ? "on" : "off");
        this->printf("ir: %s\n", meshroom->tvIrProtocolStr().c_str());
    } else if ((argc == 2) && (strcmp(argv[1], "on") == 0)) {
        meshroom->tvOnOff(true);
        this->printf("turn tv on\n");
    } else if ((argc == 2) && (strcmp(argv[1], "off") == 0)) {
        meshroom->tvOnOff(false);
        this->printf("turn tv off\n");
    } else if ((argc == 2) && (strcmp(argv[1], "toggle") == 0)) {
        meshroom->tvOnOff(!meshroom->tvOnOff());
        this->printf("toggle tv %s\n", meshroom->tvOnOff() ? "on" : "off");
    } else if ((argc >= 2) && (strcmp(argv[1], "mute") == 0)) {
        if (argc == 2 || strcmp(argv[2], "toggle") == 0) {
            meshroom->toggleTvMute();
        } else if (strcmp(argv[2], "on") == 0) {
            meshroom->tvMute(true);
        } else if (strcmp(argv[2], "off") == 0) {
            meshroom->tvMute(false);
        } else {
            this->printf("invalid mute argument!\n");
            ret = -1;
            goto done;
        }
        this->printf("tv mute %s\n", meshroom->tvMute() ? "on" : "off");
    } else if ((argc == 2) && ((strcmp(argv[1], "input") == 0) || (strcmp(argv[1], "source") == 0))) {
        meshroom->tvInput();
        this->printf("tv input switch\n");
    } else if ((argc == 3) && (strcmp(argv[1], "key") == 0)) {
        char *endptr = NULL;
        unsigned long key = strtoul(argv[2], &endptr, 10);
        if (*endptr != '\0' || key > 9) {
            this->printf("invalid key argument (must be 0-9)!\n");
            ret = -1;
            goto done;
        }
        meshroom->tvDigit((unsigned int) key);
        this->printf("sent tv key %lu\n", key);
    } else if ((argc == 2) && (strlen(argv[1]) == 1) && isdigit((unsigned char)argv[1][0])) {
        unsigned int key = (unsigned int)(argv[1][0] - '0');
        meshroom->tvDigit(key);
        this->printf("sent tv key %u\n", key);
    } else if ((argc >= 2) && (strcmp(argv[1], "vol") == 0)) {
        if (argc == 2) {
            this->printf("vol: %u\n", meshroom->tvVol());
        } else if (strcmp(argv[2], "up") == 0) {
            meshroom->tvVol(meshroom->tvVol() + 1);
            this->printf("set tv vol to %u\n", meshroom->tvVol());
        } else if (strcmp(argv[2], "down") == 0) {
            meshroom->tvVol(meshroom->tvVol() > 0 ? meshroom->tvVol() - 1 : 0);
            this->printf("set tv vol to %u\n", meshroom->tvVol());
        } else {
            char *endptr = NULL;
            unsigned long vol = strtoul(argv[2], &endptr, 10);
            if (*endptr != '\0' || vol > 100) {
                this->printf("invalid volume argument (0-100)!\n");
                ret = -1;
                goto done;
            }
            meshroom->tvVol((unsigned int) vol);
            this->printf("set tv vol to %u\n", meshroom->tvVol());
        }
    } else if ((argc >= 2) && (strcmp(argv[1], "chan") == 0)) {
        if (argc == 2) {
            this->printf("chan: %u\n", meshroom->tvChan());
        } else if (strcmp(argv[2], "up") == 0) {
            meshroom->tvChan(meshroom->tvChan() + 1);
            this->printf("set tv chan to %u\n", meshroom->tvChan());
        } else if (strcmp(argv[2], "down") == 0) {
            meshroom->tvChan(meshroom->tvChan() > 1 ? meshroom->tvChan() - 1 : 1);
            this->printf("set tv chan to %u\n", meshroom->tvChan());
        } else {
            char *endptr = NULL;
            unsigned long chan = strtoul(argv[2], &endptr, 10);
            if (*endptr != '\0' || chan < 1 || chan > 999) {
                this->printf("invalid channel argument (1-999)!\n");
                ret = -1;
                goto done;
            }
            meshroom->tvChan((unsigned int) chan);
            this->printf("set tv chan to %u\n", meshroom->tvChan());
        }
    } else {
        this->printf("syntax error!\n");
        ret = -1;
    }

done:
    return ret;
}

int MeshRoomShell::ac(int argc, char **argv)
{
    int ret = 0;

    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [command] [args...]\n", argv[0]);
        this->printf("  Control Air Conditioner power, mode, temp, fan, and swing via IR.\n");
        this->printf("Commands:\n");
        this->printf("  ac                                   Display full AC state and parameters\n");
        this->printf("  ac on|off                            Turn AC power on or off\n");
        this->printf("  ac mode <cool|heat|dry|auto|fan>     Set operating mode\n");
        this->printf("  ac temp [up|down|<16-30>]            Increase, decrease, or set temperature (C)\n");
        this->printf("  ac fanspeed [auto|quiet|1-5|max]     Set fan speed level\n");
        this->printf("  ac fandir [auto|up|mid|down|1-5]     Set vertical vane direction\n");
        this->printf("  ac powerful [on|off|toggle]          Enable or disable powerful/turbo mode\n");
        this->printf("  ac quiet [on|off|toggle]             Enable or disable quiet mode\n");
        this->printf("  ac tx                                Re-transmit current AC state packet via IR\n");
        this->printf("  ac set <on|off> <16-30> <mode> <fan> Set multiple parameters in one blast\n");
        return 0;
    }

    if (argc == 1) {
        this->printf("ac: %s\n", meshroom->acOnOff() ? "on" : "off");
        this->printf("mode: %s\n", meshroom->acModeStr().c_str());
        this->printf("temp: %u C\n", meshroom->acTemp());
        this->printf("fanspeed: %s\n", meshroom->acFanSpeedStr().c_str());
        this->printf("fandir: %s\n", meshroom->acFanDirStr().c_str());
        this->printf("powerful: %s\n", meshroom->acPowerful() ? "on" : "off");
        this->printf("quiet: %s\n", meshroom->acQuiet() ? "on" : "off");
        this->printf("ir: %s\n", meshroom->acIrProtocolStr().c_str());
    } else if ((argc == 2) && (strcmp(argv[1], "tx") == 0 || strcmp(argv[1], "blast") == 0)) {
        meshroom->blastAcState();
        this->printf("transmitted ac state via IR\n");
    } else if ((argc == 2) && (strcmp(argv[1], "on") == 0)) {
        meshroom->acOnOff(true);
        this->printf("turn ac on\n");
    } else if ((argc == 2) && (strcmp(argv[1], "off") == 0)) {
        meshroom->acOnOff(false);
        this->printf("turn ac off\n");
    } else if ((argc >= 2) && (strcmp(argv[1], "mode") == 0)) {
        if (argc == 2) {
            this->printf("mode: %s\n", meshroom->acModeStr().c_str());
        } else if (strcmp(argv[2], "ac") == 0 || strcmp(argv[2], "cool") == 0) {
            meshroom->acMode(MeshRoom::AC_AC);
            this->printf("set mode to cool\n");
        } else if (strcmp(argv[2], "heater") == 0 || strcmp(argv[2], "heat") == 0) {
            meshroom->acMode(MeshRoom::AC_HEATER);
            this->printf("set mode to heat\n");
        } else if (strcmp(argv[2], "dehumifier") == 0 || strcmp(argv[2], "dehumidifier") == 0 || strcmp(argv[2], "dry") == 0) {
            meshroom->acMode(MeshRoom::AC_DEHUMIDIFIER);
            this->printf("set mode to dry\n");
        } else if (strcmp(argv[2], "auto") == 0) {
            meshroom->acMode(MeshRoom::AC_AUTO);
            this->printf("set mode to auto\n");
        } else if (strcmp(argv[2], "fan") == 0) {
            meshroom->acMode(MeshRoom::AC_FAN);
            this->printf("set mode to fan\n");
        } else {
            this->printf("invalid mode argument (cool, heat, dry, auto, fan)!\n");
            ret = -1;
            goto done;
        }
    } else if ((argc >= 2) && (strcmp(argv[1], "temp") == 0)) {
        if (argc == 2) {
            this->printf("temp: %u C\n", meshroom->acTemp());
        } else if (strcmp(argv[2], "up") == 0) {
            meshroom->acTemp(meshroom->acTemp() + 1);
            this->printf("set temp to %u C\n", meshroom->acTemp());
        } else if (strcmp(argv[2], "down") == 0) {
            meshroom->acTemp(meshroom->acTemp() - 1);
            this->printf("set temp to %u C\n", meshroom->acTemp());
        } else {
            char *endptr = NULL;
            unsigned long temp = strtoul(argv[2], &endptr, 10);
            if (*endptr != '\0' || temp < 16 || temp > 30) {
                this->printf("invalid temperature argument (16-30)!\n");
                ret = -1;
                goto done;
            }
            meshroom->acTemp((unsigned int) temp);
            this->printf("set temp to %u C\n", meshroom->acTemp());
        }
    } else if ((argc >= 2) && (strcmp(argv[1], "fanspeed") == 0 || strcmp(argv[1], "fan") == 0)) {
        if (argc == 2) {
            this->printf("fanspeed: %s\n", meshroom->acFanSpeedStr().c_str());
        } else if (strcmp(argv[2], "up") == 0) {
            meshroom->acFanSpeed(meshroom->acFanSpeed() + 1);
            this->printf("set fanspeed to %s\n", meshroom->acFanSpeedStr().c_str());
        } else if (strcmp(argv[2], "down") == 0) {
            meshroom->acFanSpeed(meshroom->acFanSpeed() > 0 ? meshroom->acFanSpeed() - 1 : 0);
            this->printf("set fanspeed to %s\n", meshroom->acFanSpeedStr().c_str());
        } else if (strcmp(argv[2], "auto") == 0) {
            meshroom->acFanSpeed(0);
            this->printf("set fanspeed to auto\n");
        } else if (strcmp(argv[2], "quiet") == 0 || strcmp(argv[2], "min") == 0) {
            meshroom->acFanSpeed(1);
            this->printf("set fanspeed to 1 (quiet)\n");
        } else if (strcmp(argv[2], "low") == 0) {
            meshroom->acFanSpeed(2);
            this->printf("set fanspeed to 2 (low)\n");
        } else if (strcmp(argv[2], "med") == 0 || strcmp(argv[2], "medium") == 0) {
            meshroom->acFanSpeed(3);
            this->printf("set fanspeed to 3 (med)\n");
        } else if (strcmp(argv[2], "high") == 0) {
            meshroom->acFanSpeed(4);
            this->printf("set fanspeed to 4 (high)\n");
        } else if (strcmp(argv[2], "max") == 0) {
            meshroom->acFanSpeed(5);
            this->printf("set fanspeed to 5 (max)\n");
        } else {
            char *endptr = NULL;
            unsigned long speed = strtoul(argv[2], &endptr, 10);
            if (*endptr != '\0' || speed > 5) {
                this->printf("invalid fanspeed argument (0-5, auto, quiet, min, med, max)!\n");
                ret = -1;
                goto done;
            }
            meshroom->acFanSpeed((unsigned int) speed);
            this->printf("set fanspeed to %s\n", meshroom->acFanSpeedStr().c_str());
        }
    } else if ((argc >= 2) && (strcmp(argv[1], "fandir") == 0 || strcmp(argv[1], "vane") == 0 || strcmp(argv[1], "swing") == 0)) {
        if (argc == 2) {
            this->printf("fandir: %s\n", meshroom->acFanDirStr().c_str());
        } else if (strcmp(argv[2], "up") == 0) {
            meshroom->acFanDir(1);
            this->printf("set fandir to 1 (up)\n");
        } else if (strcmp(argv[2], "up-mid") == 0 || strcmp(argv[2], "upmid") == 0) {
            meshroom->acFanDir(2);
            this->printf("set fandir to 2 (up-mid)\n");
        } else if (strcmp(argv[2], "mid") == 0 || strcmp(argv[2], "middle") == 0) {
            meshroom->acFanDir(3);
            this->printf("set fandir to 3 (mid)\n");
        } else if (strcmp(argv[2], "down-mid") == 0 || strcmp(argv[2], "downmid") == 0) {
            meshroom->acFanDir(4);
            this->printf("set fandir to 4 (down-mid)\n");
        } else if (strcmp(argv[2], "down") == 0) {
            meshroom->acFanDir(5);
            this->printf("set fandir to 5 (down)\n");
        } else if (strcmp(argv[2], "auto") == 0) {
            meshroom->acFanDir(0);
            this->printf("set fandir to auto\n");
        } else {
            char *endptr = NULL;
            unsigned long dir = strtoul(argv[2], &endptr, 10);
            if (*endptr != '\0' || dir > 5) {
                this->printf("invalid fandir argument (0-5, auto, up, mid, down)!\n");
                ret = -1;
                goto done;
            }
            meshroom->acFanDir((unsigned int) dir);
            this->printf("set fandir to %s\n", meshroom->acFanDirStr().c_str());
        }
    } else if ((argc >= 2) && (strcmp(argv[1], "powerful") == 0 || strcmp(argv[1], "turbo") == 0 || strcmp(argv[1], "boost") == 0)) {
        if (argc == 2 || strcmp(argv[2], "toggle") == 0) {
            meshroom->acPowerful(!meshroom->acPowerful());
        } else if (strcmp(argv[2], "on") == 0) {
            meshroom->acPowerful(true);
        } else if (strcmp(argv[2], "off") == 0) {
            meshroom->acPowerful(false);
        } else {
            this->printf("invalid powerful argument (on, off, toggle)!\n");
            ret = -1;
            goto done;
        }
        this->printf("ac powerful %s\n", meshroom->acPowerful() ? "on" : "off");
    } else if ((argc >= 2) && (strcmp(argv[1], "quiet") == 0 || strcmp(argv[1], "silent") == 0)) {
        if (argc == 2 || strcmp(argv[2], "toggle") == 0) {
            meshroom->acQuiet(!meshroom->acQuiet());
        } else if (strcmp(argv[2], "on") == 0) {
            meshroom->acQuiet(true);
        } else if (strcmp(argv[2], "off") == 0) {
            meshroom->acQuiet(false);
        } else {
            this->printf("invalid quiet argument (on, off, toggle)!\n");
            ret = -1;
            goto done;
        }
        this->printf("ac quiet %s\n", meshroom->acQuiet() ? "on" : "off");
    } else if ((argc >= 3) && (strcmp(argv[1], "set") == 0)) {
        // Compound setter: ac set on 24 cool auto
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "on") == 0) {
                meshroom->acOnOff(true);
            } else if (strcmp(argv[i], "off") == 0) {
                meshroom->acOnOff(false);
            } else if (strcmp(argv[i], "cool") == 0 || strcmp(argv[i], "ac") == 0) {
                meshroom->acMode(MeshRoom::AC_AC);
            } else if (strcmp(argv[i], "heat") == 0 || strcmp(argv[i], "heater") == 0) {
                meshroom->acMode(MeshRoom::AC_HEATER);
            } else if (strcmp(argv[i], "dry") == 0 || strcmp(argv[i], "dehumidifier") == 0) {
                meshroom->acMode(MeshRoom::AC_DEHUMIDIFIER);
            } else if (strcmp(argv[i], "fan") == 0) {
                meshroom->acMode(MeshRoom::AC_FAN);
            } else if (strcmp(argv[i], "auto") == 0) {
                meshroom->acMode(MeshRoom::AC_AUTO);
            } else {
                char *endptr = NULL;
                unsigned long val = strtoul(argv[i], &endptr, 10);
                if (*endptr == '\0') {
                    if (val >= 16 && val <= 30) {
                        meshroom->acTemp((unsigned int)val);
                    } else if (val <= 5) {
                        meshroom->acFanSpeed((unsigned int)val);
                    }
                }
            }
        }
        this->printf("ac: %s, mode: %s, temp: %u C, fan: %s\n",
                     meshroom->acOnOff() ? "on" : "off",
                     meshroom->acModeStr().c_str(),
                     meshroom->acTemp(),
                     meshroom->acFanSpeedStr().c_str());
    } else {
        this->printf("syntax error!\n");
        ret = -1;
    }

done:
    return ret;
}

int MeshRoomShell::buzz(int argc, char **argv)
{
    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [duration_ms]\n", argv[0]);
        this->printf("  Play buzzer tone.\n");
        this->printf("Arguments:\n");
        this->printf("  duration_ms   Buzzer tone duration in milliseconds (default: 100)\n");
        return 0;
    }

    if (argc == 1) {
        meshroom->buzz();
    } else if ((argc == 2)) {
        unsigned int ms;

        try {
            ms = stoul(argv[1]);
            meshroom->buzz(ms);
        } catch (const invalid_argument &e) {
            this->printf("syntax error!\n");
        }
    } else {
        this->printf("syntax error!\n");
    }

    return -1;
}

int MeshRoomShell::morse(int argc, char **argv)
{
    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] <text>\n", argv[0]);
        this->printf("  Transmit text message as audible Morse code on the buzzer.\n");
        this->printf("Arguments:\n");
        this->printf("  text          Text string to play in Morse code\n");
        return 0;
    }

    string text;

    for (int i = 1; i < argc; i++) {
        text += argv[i];
    }

    meshroom->addMorseText(text);

    return 0;
}

int MeshRoomShell::reset(int argc, char **argv)
{
    int ret = 0;

    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [apply]\n", argv[0]);
        this->printf("  Show reset statistics or trigger system reset.\n");
        this->printf("Commands:\n");
        this->printf("  reset         Display reset count and time since last reset\n");
        this->printf("  reset apply   Trigger immediate system reset\n");
        return 0;
    }

    if (argc == 1) {
        time_t now, last;
        unsigned int secs_ago = 0;

        last = meshroom->getLastReset();
        now = time(NULL);
        secs_ago = now - last;

        this->printf("reset count: %u\n", meshroom->getResetCount());
        if (secs_ago != 0) {
            this->printf("last reset: %u seconds ago\n", secs_ago);
        }
    } else if ((argc == 2) && strcmp(argv[1], "apply") == 0) {
        meshroom->reset();
    } else {
        this->printf("syntax error!\n");
        ret = -1;
    }

    return ret;
}

int MeshRoomShell::watchdog(int argc, char **argv)
{
    int ret = 0;

    if ((argc >= 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        this->printf("Usage: %s [-h|--help] [enable|disable]\n", argv[0]);
        this->printf("  Show or configure hardware watchdog timer.\n");
        this->printf("Commands:\n");
        this->printf("  watchdog            Display watchdog status and reboot cause\n");
        this->printf("  watchdog enable     Enable hardware watchdog\n");
        this->printf("  watchdog disable    Disable hardware watchdog\n");
        return 0;
    }

    if (argc == 1) {
        this->printf("watchdog: %s\n", meshroom->isWatchdogEnabled() ? "enabled" : "disabled");
        if (meshroom->isWatchdogEnabled()) {
            this->printf("time remaining: %lu ms\n", watchdog_get_time_remaining_ms());
        }
        this->printf("last reboot caused by watchdog: %s\n",
                     watchdog_caused_reboot() ? "yes" : "no");
        if (watchdog_caused_reboot()) {
            this->printf("watchdog enable caused reboot: %s\n",
                         watchdog_enable_caused_reboot() ? "yes" : "no");
        }
    } else if ((argc == 2) && (strcmp(argv[1], "enable") == 0)) {
        meshroom->setWatchdogEnabled(true);
        this->printf("ok\n");
    } else if ((argc == 2) && (strcmp(argv[1], "disable") == 0)) {
        meshroom->setWatchdogEnabled(false);
        this->printf("ok\n");
    } else {
        this->printf("syntax error!\n");
        ret = -1;
    }

    return ret;
}

int MeshRoomShell::unknown_command(int argc, char **argv)
{
    int ret = 0;

    if (strcmp(argv[0], "bootsel") == 0) {
        ret = this->bootsel(argc, argv);
    } else if (strcmp(argv[0], "ir") == 0) {
        ret = this->ir(argc, argv);
    } else if (strcmp(argv[0], "tv") == 0) {
        ret = this->tv(argc, argv);
    } else if (strcmp(argv[0], "ac") == 0) {
        ret = this->ac(argc, argv);
    } else if (strcmp(argv[0], "buzz") == 0) {
        ret = this->buzz(argc, argv);
    } else if (strcmp(argv[0], "morse") == 0) {
        ret = this->morse(argc, argv);
    } else if (strcmp(argv[0], "reset") == 0) {
        ret = this->reset(argc, argv);
    } else if (strcmp(argv[0], "watchdog") == 0) {
        ret = this->watchdog(argc, argv);
    } else {
        this->printf("Unknown command '%s'!\n", argv[0]);
        ret = -1;
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
