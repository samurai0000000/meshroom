/*
 * serial.h
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef SERIAL_H
#define SERIAL_H

#define console_chan 0
#define device_chan 1

EXTERN_C_BEGIN

extern void serial_init(void);
extern int serial_write(int chan, const void *buf, size_t len);
extern int serial_rx_ready(int chan);
extern int serial_read(int chan, void *buf, size_t len);
extern int serial_print_str(int chan, const char *str);
extern int serial_printf(int chan, const char *fmt, ...);

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
