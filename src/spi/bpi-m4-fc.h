/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef BPI_M4_SPI_CONFIG_H
#define BPI_M4_SPI_CONFIG_H
#define CFG_SYS_SDRAM_BASE 0x00100000
#define CPU_RELEASE_ADDR 0x98007f30
#define BPI_M4_SECONDARY_ADDR 0x00030000
#define GICD_BASE 0xff011000
#define GICC_BASE 0xff012000
#define CFG_EXTRA_ENV_SETTINGS \
    "kernel_addr_r=0x20000000\0" \
    "ramdisk_addr_r=0x30000000\0" \
    "fdt_addr_r=0x02100000\0" \
    "scriptaddr=0x02200000\0" \
    "pxefile_addr_r=0x02300000\0" \
    "fdt_high=0x3f000000\0" \
    "initrd_high=0x3f000000\0" \
    "bootm_low=0x20000000\0" \
    "bootm_size=0x1f000000\0"
#endif
