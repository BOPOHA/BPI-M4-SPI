// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2015 by Tom Ting <tom_ting@realtek.com>
 * Audited adaptation of RTD1395 bootup_slave_cpu(), GPL BSP revision
 * 25f5b88ec4ba34029f964693dc34028b26e6c67c. See docs/PROVENANCE.md.
 * Release only the secondary cores through the retained FSBL.
 * Linux then supplies its checked MMIO spin-table entry at 0x98007f30.
 */
#include <config.h>
#include <asm/io.h>
#include <bootm.h>
#include <stdio.h>
#ifdef CONFIG_BPI_M4_SPI
#include <asm/cache.h>
#include <linux/kernel.h>
#include <string.h>

extern const unsigned char bpi_m4_secondary_pen[], bpi_m4_secondary_pen_end[];

static int prepare_spi_secondary_pen(void)
{
    const unsigned long address = BPI_M4_SECONDARY_ADDR;
    const unsigned long size = bpi_m4_secondary_pen_end - bpi_m4_secondary_pen;

    _Static_assert(BPI_M4_SECONDARY_ADDR + 0x1000 <= CFG_SYS_SDRAM_BASE,
                   "FSBL secondary entry must remain outside Linux RAM");
    if (!size || size > 0x1000)
        return -1;
    memcpy((void *)address, bpi_m4_secondary_pen, size);
    flush_dcache_range(address, ALIGN(address + size, ARCH_DMA_MINALIGN));
    asm volatile("dsb sy\n\tic iallu\n\tdsb sy\n\tisb" ::: "memory");
    if (memcmp((void *)address, bpi_m4_secondary_pen, size))
        return -1;
    printf("BPI-M4: secondary RAM pen at %08lx, %lu bytes\n", address, size);
    return 0;
}
#endif
static void complete_write(unsigned long address, unsigned int value) {
    writel(value, (void *)address);
    asm volatile("isb sy\n\tdsb sy" ::: "memory");
    writel(0, (void *)0x9801a020);
    asm volatile("dsb sy" ::: "memory");
}
void board_quiesce_devices(void) {
#ifdef CONFIG_BPI_M4_SPI
    if (prepare_spi_secondary_pen()) {
        puts("BPI-M4: RAM pen verification failed; secondary CPUs remain held\n");
        return;
    }
#endif
    puts("BPI-M4: release CPUs 1-3 to existing FSBL holding pen\n");
    complete_write(0x98007f30, 0);
    complete_write(0x9801d538, 0xff0);
    complete_write(0x9801d510, 0);
    complete_write(0x9801d100, 0x8f1f3bff);
}
