/*
 * serial.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>
#include <pico/time.h>
#include <hardware/uart.h>
#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <meshroom.h>

#define UART0_ID          uart0
#define UART0_TX_PIN      0
#define UART0_RX_PIN      1
#define UART0_IRQ         UART0_IRQ
#define UART0_BAUD_RATE   115200

#define UART1_ID          uart1
#define UART1_TX_PIN      4
#define UART1_RX_PIN      5
#define UART1_BAUD_RATE   115200

#define UART_DATA_BITS    8
#define UART_STOP_BITS    1
#define UART_PARITY       UART_PARITY_NONE

#define SERIAL_PBUF_SIZE  2048

struct serial_buf {
    unsigned int rp;
    unsigned int wp;
    char buf[32];
};

static char *pbuf = NULL;
static struct serial_buf *serial_buf = NULL;

static void serial_interrupt_handler(void)
{
    uint32_t state = save_and_disable_interrupts();
#if defined(MEASURE_CPU_UTILIZATION)
    uint64_t t_0 = time_us_64();
#endif

    while (uart_is_readable(uart0)) {
        char c = uart_getc(uart0);
        serial_buf[0].buf[serial_buf[0].wp] = c;
        serial_buf[0].wp++;
        serial_buf[0].wp %= sizeof(serial_buf[0].buf);
    }

    while (uart_is_readable(uart1)) {
        char c = uart_getc(uart1);
        serial_buf[1].buf[serial_buf[1].wp] = c;
        serial_buf[1].wp++;
        serial_buf[1].wp %= sizeof(serial_buf[1].buf);
    }

#if defined(MEASURE_CPU_UTILIZATION)
    t_0 = time_us_64() - t_0;
    t_cpu_total += t_0;
    t_cpu_busy += t_0;
#endif

    restore_interrupts(state);
    __sev();
}

void serial_init(void)
{
    pbuf = (char *) malloc(SERIAL_PBUF_SIZE);

    serial_buf = (struct serial_buf *) malloc(sizeof(struct serial_buf) * 2);
    serial_buf[0].rp = 0;
    serial_buf[0].wp = 0;
    serial_buf[1].rp = 0;
    serial_buf[1].wp = 0;

    uart_init(uart0, UART0_BAUD_RATE);
    gpio_set_function(UART0_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(UART0_ID, false, false);
    uart_set_fifo_enabled(UART0_ID, true);
    uart_set_format(UART0_ID, UART_DATA_BITS, UART_STOP_BITS, UART_PARITY);
    irq_set_exclusive_handler(UART0_IRQ, serial_interrupt_handler);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(UART0_ID, true, false);

    uart_init(uart1, UART1_BAUD_RATE);
    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(UART1_ID, false, false);
    uart_set_fifo_enabled(UART1_ID, true);
    uart_set_format(UART1_ID, UART_DATA_BITS, UART_STOP_BITS, UART_PARITY);
    irq_set_exclusive_handler(UART1_IRQ, serial_interrupt_handler);
    irq_set_enabled(UART1_IRQ, true);
    uart_set_irq_enables(UART1_ID, true, false);
}

int serial_write(uart_inst_t *uart, const void *_buf, size_t len)
{
    int ret = 0;
    const uint8_t *buf = (const uint8_t *) _buf;

    if (uart == NULL) {
        ret = -1;
        goto done;
    }

    uint32_t state = save_and_disable_interrupts();

    for (size_t i = 0; i < len; i++) {
        if (!uart_is_writable(uart)) {
            break;
        }

        uart_putc_raw(uart, buf[i]);
        ret++;
    }

    restore_interrupts(state);

done:

    return ret;
}

int serial_rx_ready(uart_inst_t *uart)
{
    int ret = 0;
    int chan = -1;

    if (uart == uart0) {
        chan = 0;
    } else if (uart == uart1) {
        chan = 1;
    } else {
        ret = -1;
        goto done;
    }

    if (serial_buf[chan].wp < serial_buf[chan].rp) {
        ret = sizeof(serial_buf[chan].buf) -
            serial_buf[chan].rp +
            serial_buf[chan].wp;
    } else {
        ret = serial_buf[chan].wp - serial_buf[chan].rp;
    }

done:

    return ret;
}

int serial_read(uart_inst_t *uart, void *_buf, size_t len)
{
    int ret = 0;
    int chan = -1;
    uint8_t *buf = (uint8_t *) _buf;

    if (uart == uart0) {
        chan = 0;
    } else if (uart == uart1) {
        chan = 1;
    } else {
        ret = -1;
        goto done;
    }

    while ((len > 0) &&
           (serial_buf[chan].rp != serial_buf[chan].wp)) {
        *buf = serial_buf[chan].buf[serial_buf[chan].rp];
        len--;
        buf++;
        ret++;
        serial_buf[chan].rp++;
        serial_buf[chan].rp %= sizeof(serial_buf[chan].buf);
    }

done:

    return ret;
}

int serial_print_str(uart_inst_t *uart, const char *str)
{
    int ret = 0;

    while (*str != '\0') {
        if (*str == '\n') {
            while (serial_write(uart, "\r", 1) != 1);
        }

        while (serial_write(uart, str, 1) != 1);

        ret++;
        str++;
    }

    return ret;
}

int serial_printf(uart_inst_t *uart, const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = vsnprintf(pbuf, SERIAL_PBUF_SIZE - 1, fmt, ap);
    va_end(ap);

    ret = serial_print_str(uart, pbuf);

    return ret;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
