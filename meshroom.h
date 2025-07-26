/*
 * meshroom.h
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHROOM_H
#define MESHROOM_H

extern void led_init(void);
extern void led_set(bool on);

extern void serial_init(void);
extern int serial_write(int chan, const void *buf, size_t len);
extern int serial_rx_ready(int chan);
extern int serial_read(int chan, void *buf, size_t len);
extern int serial_print_str(int chan, const char *str);
extern int serial_printf(int chan, const char *fmt, ...);

extern void shell_init(int chan);
extern void shell_process(void);

struct mt_client;
extern struct mt_client *G_mtc;

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
