/*
 * serial.h
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef SERIAL_H
#define SERIAL_H

EXTERN_C_BEGIN

extern void serial_init(void);
extern void serial_deinit(void);

#if defined(LIB_PICO_STDIO_USB)
extern int console_write(const uint8_t *data, size_t size);
extern int console_printf(const char *format, ...);
extern int console_vprintf(const char *format, va_list ap);
extern int console_rx_ready(void);
extern int console_read(uint8_t *data, size_t size);
extern int console_read_timeout_us(uint8_t *data, size_t size,
				   unsigned int timeout_us);
#endif

extern int console2_write(const uint8_t *data, size_t size);
extern int console2_printf(const char *format, ...);
extern int console2_vprintf(const char *format, va_list ap);
extern int console2_rx_ready(void);
extern int console2_read(uint8_t *data, size_t size);

extern int consoles_printf(const char *format, ...);
extern int consoles_vprintf(const char *format, va_list ap);

extern int serial_write(const void *buf, size_t len);
extern int serial_rx_ready(void);
extern int serial_read(void *buf, size_t len);

#if defined(SEMAPHORE_H)
extern SemaphoreHandle_t cdc_sem;
extern SemaphoreHandle_t uart0_sem;
extern SemaphoreHandle_t uart1_sem;
#endif

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
