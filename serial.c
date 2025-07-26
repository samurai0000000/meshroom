/*
 * serial.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdio.h>
#include <stdarg.h>
#include <strings.h>
#include <hardware/uart.h>
#include <hardware/gpio.h>
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

struct serial_buf {
    char buf[2048];
    unsigned int rp;
    unsigned int wp;
};

static struct serial_buf serial_buf[2];

static void serial0_interrupt_handler(void)
{
    while (uart_is_readable(UART0_ID)) {
        char c = uart_getc(UART0_ID);
        serial_buf[0].buf[serial_buf[0].wp] = c;
        serial_buf[0].wp++;
    }
}

static void serial1_interrupt_handler(void)
{
    while (uart_is_readable(UART1_ID)) {
        char c = uart_getc(UART1_ID);
        serial_buf[1].buf[serial_buf[1].wp] = c;
        serial_buf[1].wp++;
    }
}

void serial_init(void)
{
    bzero(serial_buf, sizeof(serial_buf));

    uart_init(UART0_ID, UART0_BAUD_RATE);
    gpio_set_function(UART0_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(UART0_ID, false, false);
    uart_set_format(UART0_ID, UART_DATA_BITS, UART_STOP_BITS, UART_PARITY);
    irq_set_exclusive_handler(UART0_IRQ, serial0_interrupt_handler);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(UART0_ID, true, false);

    uart_init(UART1_ID, UART1_BAUD_RATE);
    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(UART1_ID, false, false);
    uart_set_format(UART1_ID, UART_DATA_BITS, UART_STOP_BITS, UART_PARITY);
    irq_set_exclusive_handler(UART1_IRQ, serial1_interrupt_handler);
    irq_set_enabled(UART1_IRQ, true);
    uart_set_irq_enables(UART1_ID, true, false);
}

static uart_inst_t *serial_chan2inst(int chan)
{
    uart_inst_t *ret = NULL;

    if (chan == 0) {
        ret = UART0_ID;
    } else if (chan == 1) {
        ret = UART1_ID;
    }

    return ret;
}

int serial_write(int chan, const void *_buf, size_t len)
{
    int ret = 0;
    const uint8_t *buf = (const uint8_t *) _buf;
    uart_inst_t *uart = serial_chan2inst(chan);

    if (uart == NULL) {
        ret = -1;
        goto done;
    }

    for (size_t i = 0; i < len; i++) {
        uart_putc_raw(uart, buf[i]);
        ret++;
    }

done:

    return ret;
}

int serial_rx_ready(int chan)
{
    int ret = 0;

    if ((chan >= 0) && (chan < 2)) {
        if (serial_buf[chan].wp < serial_buf[chan].rp) {
            ret = sizeof(serial_buf[chan].buf) -
                serial_buf[chan].rp +
                serial_buf[chan].wp;
        } else {
            ret = serial_buf[chan].wp - serial_buf[chan].rp;
        }
    }

    return ret;
}

int serial_read(int chan, void *_buf, size_t len)
{
    int ret = 0;
    uint8_t *buf = (uint8_t *) _buf;

    if ((chan >= 0) && (chan < 2)) {
        unsigned int i = serial_buf[chan].rp;
        for (i = serial_buf[chan].rp;
             (ret < (int) len) && (i != serial_buf[chan].wp);
             i++) {
            i %= sizeof(serial_buf[chan].buf);
            *buf = serial_buf[chan].buf[i];
            buf++;
            ret++;
        }
        serial_buf[chan].rp = i;
    }

    return ret;
}

int serial_print_str(int chan, const char *str)
{
    int ret = 0;
    uart_inst_t *uart = serial_chan2inst(chan);

    if (uart == NULL) {
        ret = -1;
        goto done;
    }

    while (*str != '\0') {
        if (*str == '\n') {
            uart_putc_raw(uart, '\r');
        }
        uart_putc_raw(uart, *str);
        ret++;
        str++;
    }

done:

    return ret;
}

int serial_printf(int chan, const char *fmt, ...)
{
    int ret;
    va_list ap;
    char buf[1024];

    va_start(ap, fmt);
    ret = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);

    ret = serial_print_str(chan, buf);

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
