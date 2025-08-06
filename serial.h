/*
 * serial.h
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef SERIAL_H
#define SERIAL_H

EXTERN_C_BEGIN

extern void serial_init(void);

extern int console_printf(const char *fmt, ...);
extern int console_rx_ready(void);
extern int console_read(uint8_t *data, size_t size);

extern int serial_write(const void *buf, size_t len);
extern int serial_rx_ready(void);
extern int serial_read(void *buf, size_t len);

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
