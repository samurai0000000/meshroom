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
}

MeshRoomShell::~MeshRoomShell()
{

}

int MeshRoomShell::tx_write(const uint8_t *buf, size_t size)
{
    int ret;
    int console_id = (int) _ctx;

    if (console_id == 0) {
        ret = console_write(buf, size);
    } else {
        ret = console2_write(buf, size);
    }

    return ret;
}

int MeshRoomShell::printf(const char *format, ...)
{
    int ret = 0;
    va_list ap;
    int console_id = (int) _ctx;

    va_start(ap, format);
    if (console_id == 0) {
        ret = console_vprintf(format, ap);
    } else {
        ret = console2_vprintf(format, ap);
    }
    va_end(ap);

    return ret;
}

int MeshRoomShell::rx_ready(void) const
{
    int ret;
    int console_id = (int) _ctx;

    if (console_id == 0) {
        ret = console_rx_ready();
    } else {
        ret = console2_rx_ready();
    }

    return ret;
}

int MeshRoomShell::rx_read(uint8_t *buf, size_t size)
{
    int ret;
    int console_id = (int) _ctx;

    if (console_id == 0) {
        ret = console_read(buf, size);
    } else {
        ret = console2_read(buf, size);
    }

    return ret;
}

int MeshRoomShell::system(int argc, char **argv)
{
    extern char __StackTop, __StackBottom;
    extern char __StackLimit, __bss_end__;
    struct mallinfo m = mallinfo();
    unsigned int stack_size = &__StackTop - &__StackBottom;
    unsigned int total_heap = &__StackLimit  - &__bss_end__;
    unsigned int used_heap = m.uordblks;
    unsigned int free_heap = total_heap - used_heap;

    SimpleShell::system(argc, argv);
    this->printf("Stack Size: %8u bytes\n", stack_size);
    this->printf("Total Heap: %8u bytes\n", total_heap);
    this->printf(" Free Heap: %8u bytes\n", free_heap);
    this->printf(" Used Heap: %8u bytes\n", used_heap);

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

int MeshRoomShell::bootsel(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    meshroom->sendDisconnect();
    this->printf("Rebooting to BOOTSEL mode\n");
    reset_usb_boot(0, 0);

    return 0;
}

int MeshRoomShell::nvm(int argc, char **argv)
{
    ir(argc, argv);
    SimpleShell::nvm(argc, argv);

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

int MeshRoomShell::unknown_command(int argc, char **argv)
{
    int ret = 0;

    if (strcmp(argv[0], "ir") == 0) {
        ret = this->ir(argc, argv);
    } else if (strcmp(argv[0], "bootsel") == 0) {
        ret = this->bootsel(argc, argv);
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
