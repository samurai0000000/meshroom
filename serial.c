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
#include <pico/stdio.h>
#include <hardware/uart.h>
#include <hardware/gpio.h>
#include <hardware/sync.h>
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


#if !defined(LIB_PICO_STDIO_USB)
#define SERIAL_PBUF_SIZE  256
static char pbuf[SERIAL_PBUF_SIZE];
#endif

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

#if !defined(LIB_PICO_STDIO_USB)
    dst = serial_buf[0].buf;
    wp = serial_buf[0].wp;
    while (uart_is_readable(uart0)) {
        dst[wp] = (char) uart_get_hw(uart0)->dr;
        wp = ((wp + 1) % SERIAL_BUF_BUF_SIZE);
    }
    serial_buf[0].wp = wp;
#endif

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
#if !defined(LIB_PICO_STDIO_USB)
    uart_init(uart0, UART0_BAUD_RATE);
    gpio_set_function(UART0_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(uart0, false, false);
    uart_set_fifo_enabled(uart0, true);
    uart_set_format(uart0, UART_DATA_BITS, UART_STOP_BITS, UART_PARITY);
    irq_set_exclusive_handler(UART0_IRQ, serial_interrupt_handler);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);
#endif

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

static int __serial_write(uart_inst_t *uart, const void *_buf, size_t len)
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

static int __serial_rx_ready(uart_inst_t *uart)
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

static int __serial_read(uart_inst_t *uart, void *buf, size_t len)
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

int console_printf(const char *fmt, ...)
{
    int ret;
    va_list ap;

#if !defined(LIB_PICO_STDIO_USB)
    va_start(ap, fmt);
    ret = vsnprintf(pbuf, SERIAL_PBUF_SIZE - 1, fmt, ap);
    va_end(ap);

    for (int i = 0; i < ret; i++) {
        if (pbuf[i] == '\n') {
            while (__serial_write(uart0, "\r", 1) != 1);
        }

        while (__serial_write(uart0, pbuf + i, 1) != 1);
    }
#else
    va_start(ap, fmt);
    ret = vprintf(fmt, ap);
    va_end(ap);
    stdio_flush();
#endif

    return ret;
}

int console_rx_ready(void)
{
#if !defined(LIB_PICO_STDIO_USB)
    return __serial_rx_ready(uart0);
#else
    return 1;
#endif
}

int console_read(uint8_t *data, size_t size)
{
#if !defined(LIB_PICO_STDIO_USB)
    return __serial_read(uart0, data, size);
#else
    int ret = 0;

    while (size > 0) {
        int c = stdio_getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) {
            break;
        } else {
            *data = (char) c;
            data++;
            size--;
            ret++;
        }
    }

    return ret;
#endif
}

int serial_write(const void *buf, size_t len)
{
    return __serial_write(uart1, buf, len);
}

int serial_rx_ready(void)
{
    return __serial_rx_ready(uart1);
}

int serial_read(void *buf, size_t len)
{
    return __serial_read(uart1, buf, len);
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
