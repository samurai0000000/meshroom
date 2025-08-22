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
#include <FreeRTOS.h>
#include <semphr.h>
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

#define SERIAL_BUF_BUF_SIZE  256

struct serial_buf {
    unsigned int rp;
    unsigned int wp;
    char buf[SERIAL_BUF_BUF_SIZE];
};


#define SERIAL_PBUF_SIZE  256
static char pbuf[SERIAL_PBUF_SIZE];

static struct serial_buf uart0_buf = {
    .rp = 0,
    .wp = 0,
    .buf = { 0, },
};

static struct serial_buf uart1_buf = {
    .rp = 0,
    .wp = 0,
    .buf = { 0, },
};

SemaphoreHandle_t uart1_sem = NULL;

static void serial0_interrupt_handler(void)
{
    unsigned int wp;
    char *dst;

    dst = uart0_buf.buf;
    wp = uart0_buf.wp;
    while (uart_is_readable(uart0)) {
        dst[wp] = (char) uart_get_hw(uart0)->dr;
        wp = ((wp + 1) % SERIAL_BUF_BUF_SIZE);
    }
    uart0_buf.wp = wp;

}
static void serial1_interrupt_handler(void)
{
    unsigned int wp;
    char *dst;

    dst = uart1_buf.buf;
    wp = uart1_buf.wp;
    while (uart_is_readable(uart1)) {
        dst[wp] = (char) uart_get_hw(uart1)->dr;
        wp = ((wp + 1) % SERIAL_BUF_BUF_SIZE);
    }
    uart1_buf.wp = wp;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(uart1_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void serial_init(void)
{
    uart_init(uart0, UART0_BAUD_RATE);
    gpio_set_function(UART0_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(uart0, false, false);
    uart_set_fifo_enabled(uart0, true);
    uart_set_format(uart0, UART_DATA_BITS, UART_STOP_BITS, UART_PARITY);
    irq_set_exclusive_handler(UART0_IRQ, serial0_interrupt_handler);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);

    uart_init(uart1, UART1_BAUD_RATE);
    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(uart1, false, false);
    uart_set_fifo_enabled(uart1, true);
    uart_set_format(uart1, UART_DATA_BITS, UART_STOP_BITS, UART_PARITY);
    irq_set_exclusive_handler(UART1_IRQ, serial1_interrupt_handler);
    irq_set_enabled(UART1_IRQ, true);
    uart_set_irq_enables(uart1, true, false);
    uart1_sem = xSemaphoreCreateBinary();
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
    struct serial_buf *serial_buf = NULL;

    if (uart == uart0) {
        serial_buf = &uart0_buf;
    } else if (uart == uart1) {
        serial_buf = &uart1_buf;
    } else {
        ret = -1;
        goto done;
    }

    if (serial_buf->wp < serial_buf->rp) {
        ret = SERIAL_BUF_BUF_SIZE - serial_buf->rp + serial_buf->wp;
    } else {
        ret = serial_buf->wp - serial_buf->rp;
    }

done:

    return ret;
}

static int __serial_read(uart_inst_t *uart, void *buf, size_t len)
{
    int ret = 0;
    struct serial_buf *serial_buf = NULL;
    uint8_t *dst;
    const uint8_t *src;
    unsigned int rp, wp;
    size_t size;

    if (uart == uart0) {
        serial_buf = &uart0_buf;
    } else if (uart == uart1) {
        serial_buf = &uart1_buf;
    } else {
        ret = -1;
        goto done;
    }

    dst = (uint8_t *) buf;
    src = (const uint8_t *) serial_buf->buf;
    size = SERIAL_BUF_BUF_SIZE;
    rp = serial_buf->rp;
    wp = serial_buf->wp;
    while ((len > 0) && (rp != wp)) {
        *dst = src[rp];
        dst++;
        len--;
        ret++;
        rp = (rp + 1) % size;
    }

    serial_buf->rp = rp;

done:

    return ret;
}

int console_write(const uint8_t *data, size_t size)
{
    return stdio_put_string((const char *) data, (int) size, false, false);
}

int console_printf(const char *format, ...)
{
    int ret;
    va_list ap;

    va_start(ap, format);
    ret = console_vprintf(format, ap);
    va_end(ap);

    return ret;
}

int console_vprintf(const char *format, va_list ap)
{
    return stdio_vprintf(format, ap);
}

int console_rx_ready(void)
{
    return 1;
}

int console_read(uint8_t *data, size_t size)
{
    return console_read_timeout_us(data, size, 0);
}

int console_read_timeout_us(uint8_t *data, size_t size,
                            unsigned int timeout_us)
{
    int ret = 0;

    while (size > 0) {
        int c = stdio_getchar_timeout_us(timeout_us);
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
}

int console2_write(const uint8_t *data, size_t size)
{
    return __serial_write(uart0, data, size);
}

int console2_printf(const char *format, ...)
{
    int ret;
    va_list ap;

    va_start(ap, format);
    ret = console2_vprintf(format, ap);
    va_end(ap);

    return ret;
}

int console2_vprintf(const char *format, va_list ap)
{
    int ret;

    ret = vsnprintf(pbuf, SERIAL_PBUF_SIZE - 1, format, ap);

    for (int i = 0; i < ret; i++) {
        if (pbuf[i] == '\n') {
            while (__serial_write(uart0, "\r", 1) != 1);
        }

        while (__serial_write(uart0, pbuf + i, 1) != 1);
    }

    return ret;
}

int console2_rx_ready(void)
{
    return __serial_rx_ready(uart0);
}

int console2_read(uint8_t *data, size_t size)
{
    return __serial_read(uart0, data, size);
}

int consoles_printf(const char *format, ...)
{
    int ret = 0;
    va_list ap;

    va_start(ap, format);
    ret = console_vprintf(format, ap);
    (void) console2_vprintf(format, ap);
    va_end(ap);

    return ret;
}

int consoles_vprintf(const char *format, va_list ap)
{
    int ret = 0;

    ret = console_vprintf(format, ap);
    (void) console2_vprintf(format, ap);

    return ret;
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
