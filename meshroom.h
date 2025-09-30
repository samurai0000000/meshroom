/*
 * meshroom.h
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHROOM_H
#define MESHROOM_H

#if !defined(EXTERN_C_BEGIN)
#if defined(__cplusplus)
#define EXTERN_C_BEGIN extern "C" {
#else
#define EXTERN_C_BEGIN
#endif
#endif

#if !defined(EXTERN_C_END)
#if defined(__cplusplus)
#define EXTERN_C_END }
#else
#define EXTERN_C_END
#endif
#endif

#include <pico-plat.h>

EXTERN_C_BEGIN

extern void led_init(void);
extern void led_set(bool on);

extern void shell_init(void);
extern int shell_process(void);
extern int shell2_process(void);

extern int consoles_printf(const char *format, ...);
extern int consoles_vprintf(const char *format, va_list ap);

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
