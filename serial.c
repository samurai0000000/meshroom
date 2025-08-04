/*
 * serial.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <strings.h>
#include <unistd.h>
#include <hardware/uart.h>
#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <meshroom.h>

#define UART0_TX_PIN      0
#define UART0_RX_PIN      1
#define UART0_IRQ         UART0_IRQ
#define UART0_BAUD_RATE   115200

#define UART1_TX_PIN      4
#define UART1_RX_PIN      5
#define UART1_BAUD_RATE   115200

#define UART_DATA_BITS    8
#define UART_STOP_BITS    1
#define UART_PARITY       UART_PARITY_NONE

#define SERIAL_BUF_BUF_SIZE  32

struct serial_buf {
    unsigned int rp;
    unsigned int wp;
    char buf[SERIAL_BUF_BUF_SIZE];
};

#define SERIAL_PBUF_SIZE  256

static char pbuf[SERIAL_PBUF_SIZE];

static struct serial_buf serial_buf[2] = {
    {
        .rp = 0,
        .wp = 0,
        .buf = { 0, },
    }, {
        .rp = 0,
        .wp = 0,
        .buf = { 0, },
    },
};

static void serial_interrupt_handler(void)
{
    unsigned int wp;
    char *dst;

    dst = serial_buf[0].buf;
    wp = serial_buf[0].wp;
    while (uart_is_readable(uart0)) {
        dst[wp] = (char) uart_get_hw(uart0)->dr;
        wp = ((wp + 1) % SERIAL_BUF_BUF_SIZE);
    }
    serial_buf[0].wp = wp;

    dst = serial_buf[1].buf;
    wp = serial_buf[1].wp;
    while (uart_is_readable(uart1)) {
        dst[wp] = (char) uart_get_hw(uart1)->dr;
        wp = ((wp + 1) % SERIAL_BUF_BUF_SIZE);
    }
    serial_buf[1].wp = wp;

    __sev();
}

void serial_init(void)
{
    uart_init(uart0, UART0_BAUD_RATE);
    gpio_set_function(UART0_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(uart0, false, false);
    uart_set_fifo_enabled(uart0, true);
    uart_set_format(uart0, UART_DATA_BITS, UART_STOP_BITS, UART_PARITY);
    irq_set_exclusive_handler(UART0_IRQ, serial_interrupt_handler);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);

    uart_init(uart1, UART1_BAUD_RATE);
    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(uart1, false, false);
    uart_set_fifo_enabled(uart1, true);
    uart_set_format(uart1, UART_DATA_BITS, UART_STOP_BITS, UART_PARITY);
    irq_set_exclusive_handler(UART1_IRQ, serial_interrupt_handler);
    irq_set_enabled(UART1_IRQ, true);
    uart_set_irq_enables(uart1, true, false);
}

int serial_write(uart_inst_t *uart, const void *_buf, size_t len)
{
    int ret = 0;
    const uint8_t *buf = (const uint8_t *) _buf;

    if (uart == NULL) {
        ret = -1;
        goto done;
    }

    for (size_t i = 0; i < len; i++) {
        if (!uart_is_writable(uart)) {
            break;
        }

        uart_get_hw(uart)->dr = buf[i];
        ret++;
    }

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
        ret =
            SERIAL_BUF_BUF_SIZE -
            serial_buf[chan].rp +
            serial_buf[chan].wp;
    } else {
        ret = serial_buf[chan].wp - serial_buf[chan].rp;
    }

done:

    return ret;
}

int serial_read(uart_inst_t *uart, void *buf, size_t len)
{
    int ret = 0;
    int chan = -1;
    uint8_t *dst;
    const uint8_t *src;
    unsigned int rp, wp;
    size_t size;

    if (uart == uart0) {
        chan = 0;
    } else if (uart == uart1) {
        chan = 1;
    } else {
        ret = -1;
        goto done;
    }

    dst = (uint8_t *) buf;
    src = (const uint8_t *) serial_buf[chan].buf;
    size = SERIAL_BUF_BUF_SIZE;
    rp = serial_buf[chan].rp;
    wp = serial_buf[chan].wp;
    while ((len > 0) && (rp != wp)) {
        *dst = src[rp];
        dst++;
        len--;
        ret++;
        rp = (rp + 1) % size;
    }

    serial_buf[chan].rp = rp;

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
