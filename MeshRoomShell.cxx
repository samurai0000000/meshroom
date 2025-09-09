/*
 * MeshRoomShell.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <malloc.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <hardware/clocks.h>
#include <pico/bootrom.h>
#include <FreeRTOS.h>
#include <task.h>
#include <libmeshtastic.h>
#include <serial.h>
#include <MeshRoom.hxx>
#include <MeshRoomShell.hxx>

extern shared_ptr<MeshRoom> meshroom;

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
}

MeshRoomShell::~MeshRoomShell()
{

}

int MeshRoomShell::tx_write(const uint8_t *buf, size_t size)
{
    int ret = 0;
    int console_id = (int) _ctx;

    if (console_id == 1) {
#if defined(LIB_PICO_STDIO_USB)
        ret = console_write(buf, size);
#endif
    } else if (console_id == 2) {
        ret = console2_write(buf, size);
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
#if defined(LIB_PICO_STDIO_USB)
        ret = console_vprintf(format, ap);
#endif
    } else if (console_id == 2) {
        ret = console2_vprintf(format, ap);
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
#if defined(LIB_PICO_STDIO_USB)
        ret = console_rx_ready();
#endif
    } else if (console_id == 2) {
        ret = console2_rx_ready();
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
#if defined(LIB_PICO_STDIO_USB)
        ret = console_read(buf, size);
#endif
    } else if (console_id == 2) {
        ret = console2_read(buf, size);
    } else {
        ret = -1;
    }

    return ret;
}

int MeshRoomShell::system(int argc, char **argv)
{
    extern char __StackLimit, __bss_end__;
    struct mallinfo m = mallinfo();
    unsigned int total_heap = &__StackLimit  - &__bss_end__;
    unsigned int used_heap = m.uordblks;
    unsigned int free_heap = total_heap - used_heap;

    SimpleShell::system(argc, argv);
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

    return 0;
}

int MeshRoomShell::reboot(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    meshroom->sendDisconnect();
    watchdog_enable(1, 0);
    for (;;);

    return 0;
}

int MeshRoomShell::nvm(int argc, char **argv)
{
    ir(argc, argv);
    SimpleShell::nvm(argc, argv);

    return 0;
}

int MeshRoomShell::bootsel(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    meshroom->sendDisconnect();
    this->printf("Rebooting to BOOTSEL mode\n");
    reset_usb_boot(0, 0);

    return 0;
}

int MeshRoomShell::ir(int argc, char **argv)
{
    int ret = 0;
    uint32_t ir_flags = meshroom->ir_flags();

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
        this->printf("\n");
    } else if ((argc == 3) && strcmp(argv[1], "add") == 0) {
        if (strstr(argv[2], "bravia") != NULL) {
            ir_flags |= MESHROOM_IR_SONY_BRAVIA;
        } else if (strstr(argv[2], "samsung") != NULL) {
            ir_flags |= MESHROOM_IR_SAMSUNG_TV;
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
        if (strstr(argv[2], "bravia") != NULL) {
            ir_flags &= ~MESHROOM_IR_SONY_BRAVIA;
        } else if (strstr(argv[2], "samsung") != NULL) {
            ir_flags &= ~MESHROOM_IR_SAMSUNG_TV;
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

    if (argc == 1) {
        this->printf("tv: %s\n", meshroom->tvOnOff() ? "on" : "off");
        if (meshroom->tvOnOff()) {
            this->printf("vol: %u\n", meshroom->tvVol());
            this->printf("chan: %u\n", meshroom->tvChan());
        }
    } else if ((argc == 2) && (strcmp(argv[1], "on") == 0)) {
        meshroom->tvOnOff(true);
        this->printf("turn tv on\n");
    } else if ((argc == 2) && (strcmp(argv[1], "off") == 0)) {
        meshroom->tvOnOff(false);
        this->printf("turn tv off\n");
    } else if ((argc == 3) && (strcmp(argv[1], "vol") == 0) &&
               (strcmp(argv[2], "up") == 0)) {
        meshroom->tvVol(meshroom->tvVol() + 1);
        this->printf("set tv vol to %u\n", meshroom->tvVol());
    } else if ((argc == 3) && (strcmp(argv[1], "vol") == 0) &&
               (strcmp(argv[2], "down") == 0)) {
        meshroom->tvVol(meshroom->tvVol() - 1);
        this->printf("set tv vol to %u\n", meshroom->tvVol());
    } else if ((argc == 3) && (strcmp(argv[1], "vol") == 0)) {
        unsigned int vol = 0;

        try {
            vol = stoi(argv[2]);
        } catch (const invalid_argument &e) {
            this->printf("invalid volume argument!\n");
            ret = -1;
            goto done;
        }

        meshroom->tvVol(vol);
        this->printf("set tv vol to %u\n", meshroom->tvVol());
    } else if ((argc == 3) && (strcmp(argv[1], "chan") == 0) &&
               (strcmp(argv[2], "up") == 0)) {
        meshroom->tvChan(meshroom->tvChan() + 1);
        this->printf("set tv chan to %u\n", meshroom->tvChan());
    } else if ((argc == 3) && (strcmp(argv[1], "chan") == 0) &&
               (strcmp(argv[2], "down") == 0)) {
        meshroom->tvChan(meshroom->tvChan() - 1);
        this->printf("set tv chan to %u\n", meshroom->tvChan());
    } else if ((argc == 3) && (strcmp(argv[1], "chan") == 0)) {
        unsigned int chan = 0;

        try {
            chan = stoi(argv[2]);
        } catch (const invalid_argument &e) {
            this->printf("invalid channel argument!\n");
            ret = -1;
            goto done;
        }

        meshroom->tvChan(chan);
        this->printf("set tv chan to %u\n", meshroom->tvChan());
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

    if (argc == 1) {
        this->printf("ac: %s\n", meshroom->acOnOff() ? "on" : "off");
        this->printf("mode: %s\n", meshroom->acModeStr().c_str());
        if (meshroom->acOnOff()) {
            this->printf("temp: %u\n", meshroom->acTemp());
            this->printf("fanspeed: %u\n", meshroom->acFanSpeed());
            this->printf("fandir: %u\n", meshroom->acFanDir());
        }
    } else if ((argc == 2) && (strcmp(argv[1], "on") == 0)) {
        meshroom->acOnOff(true);
        this->printf("turn ac on\n");
    } else if ((argc == 2) && (strcmp(argv[1], "off") == 0)) {
        meshroom->acOnOff(false);
        this->printf("turn ac off\n");
    } else if ((argc == 3) && (strcmp(argv[1], "mode") == 0) &&
               (strcmp(argv[2], "ac") == 0)) {
        meshroom->acMode(MeshRoom::AC_AC);
    } else if ((argc == 3) && (strcmp(argv[1], "mode") == 0) &&
               (strcmp(argv[2], "heater") == 0)) {
        meshroom->acMode(MeshRoom::AC_HEATER);
    } else if ((argc == 3) && (strcmp(argv[1], "mode") == 0) &&
               (strcmp(argv[2], "dehumifier") == 0)) {
        meshroom->acMode(MeshRoom::AC_DEHUMIDIFIER);
    } else if ((argc == 3) && (strcmp(argv[1], "mode") == 0) &&
               (strcmp(argv[2], "auto") == 0)) {
        meshroom->acMode(MeshRoom::AC_AUTO);
    } else if ((argc == 3) && (strcmp(argv[1], "temp") == 0) &&
               (strcmp(argv[2], "up") == 0)) {
        meshroom->acTemp(meshroom->acTemp() + 1);
        this->printf("set temp to %u\n", meshroom->acTemp());
    } else if ((argc == 3) && (strcmp(argv[1], "temp") == 0) &&
               (strcmp(argv[2], "down") == 0)) {
        meshroom->acTemp(meshroom->acTemp() - 1);
        this->printf("set temp to %u\n", meshroom->acTemp());
    } else if ((argc == 3) && (strcmp(argv[1], "temp") == 0)) {
        unsigned int temp = 0;

        try {
            temp = stoi(argv[2]);
        } catch (const invalid_argument &e) {
            this->printf("invalid temperature argument!\n");
            ret = -1;
            goto done;
        }

        meshroom->acTemp(temp);
        this->printf("set temp to %u\n", meshroom->acTemp());
    } else if ((argc == 3) && (strcmp(argv[1], "fanspeed") == 0) &&
               (strcmp(argv[2], "up") == 0)) {
        meshroom->acFanSpeed(meshroom->acFanSpeed() + 1);
        this->printf("set fanspeed to %u\n", meshroom->acFanSpeed());
    } else if ((argc == 3) && (strcmp(argv[1], "fanspeed") == 0) &&
               (strcmp(argv[2], "down") == 0)) {
        meshroom->acFanSpeed(meshroom->acFanSpeed() - 1);
        this->printf("set fanspeed to %u\n", meshroom->acFanSpeed());
    } else if ((argc == 3) && (strcmp(argv[1], "fanspeed") == 0)) {
        unsigned int fanspeed = 0;

        try {
            fanspeed = stoi(argv[2]);
        } catch (const invalid_argument &e) {
            this->printf("invalid fanspeed argument!\n");
            ret = -1;
            goto done;
        }

        meshroom->acFanSpeed(fanspeed);
        this->printf("set fanspeed to %u\n", meshroom->acFanSpeed());
    } else if ((argc == 3) && (strcmp(argv[1], "fandir") == 0) &&
               (strcmp(argv[2], "up") == 0)) {
        meshroom->acFanDir(meshroom->acFanDir() + 1);
        this->printf("set fandir to %u\n", meshroom->acFanDir());
    } else if ((argc == 3) && (strcmp(argv[1], "fandir") == 0) &&
               (strcmp(argv[2], "down") == 0)) {
        meshroom->acFanDir(meshroom->acFanDir() - 1);
        this->printf("set fandir to %u\n", meshroom->acFanDir());
    } else if ((argc == 3) && (strcmp(argv[1], "fandir") == 0)) {
        unsigned int fandir = 0;

        try {
            fandir = stoi(argv[2]);
        } catch (const invalid_argument &e) {
            this->printf("invalid fandir argument!\n");
            ret = -1;
            goto done;
        }

        meshroom->acFanDir(fandir);
        this->printf("set fandir to %u\n", meshroom->acFanDir());
    } else {
        this->printf("syntax error!\n");
        ret = -1;
    }

done:

    return ret;
}

int MeshRoomShell::buzz(int argc, char **argv)
{
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
