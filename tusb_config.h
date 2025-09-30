/*
 * tusb_config.h
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#define CFG_TUD_ENABLED         1

// Legacy RHPORT configuration
#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT       0
#endif

#define CFG_TUD_CDC             1
#define CFG_TUD_CDC_RX_BUFSIZE  64
#define CFG_TUD_CDC_TX_BUFSIZE  64
#define CFG_TUD_CDC_EP_BUFSIZE  64

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE  64
#endif

#endif // TUSB_CONFIG_H

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

