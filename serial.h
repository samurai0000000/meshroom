/*
 * serial.h
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef SERIAL_H
#define SERIAL_H

EXTERN_C_BEGIN

typedef struct uart_inst uart_inst_t;

extern void serial_init(void);
extern int serial_write(uart_inst_t *uart, const void *buf, size_t len);
extern int serial_rx_ready(uart_inst_t *uart);
extern int serial_read(uart_inst_t *uart, void *buf, size_t len);
extern int serial_print_str(uart_inst_t *uart, const char *str);
extern int serial_printf(uart_inst_t *uart, const char *fmt, ...);

EXTERN_C_END

#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
