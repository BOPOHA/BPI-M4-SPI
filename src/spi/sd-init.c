// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2015 Realtek. RTD1395 rtk_sdmmc_init() adaptation from
 * drivers/mmc/rtksdmmc_rtd1395.c at BSP 25f5b88ec4ba34029f964693dc34028b26e6c67c.
 * This is the SPI cold-start path; the SD second stage retains its handoff checks.
 */
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/errno.h>
int bpi_spi_sd_init(void __iomem *regs)
{
    void __iomem *crt = (void *)0x98000000;
    void __iomem *pad = (void *)0x9804e000;

    /* Do not reset a controller that could still be writing through DMA. */
    if (readl(regs + 0x00c) & 1)
        return -EBUSY;
    writel(0x400, pad + 0x30);
    writel(0x24a92000, pad + 8);
    writel(1, pad + 12);
    writel(readl(crt + 0x1ec) | 7, crt + 0x1ec);
    writel(0x2003, crt + 0x1e0);
    udelay(100);
    writel(6, crt + 0x1ec);
    writel(readl(regs + 0x78) | 0x70000, regs + 0x78);
    udelay(2000);
    writel(7, crt + 0x1ec);
    udelay(300);
    writel(6, crt + 0x1ec);
    writel(0x04517893, crt + 0x1e4);
    writel(0x00564388, crt + 0x1e8);
    udelay(2000);
    writel(7, crt + 0x1ec);
    udelay(200);
    writel(readl(crt + 4) | (1U << 10), crt + 4);
    writel(readl(crt + 12) | (1U << 25) | (1U << 31), crt + 12);
    writel(readl(regs + 0x20) | 2, regs + 0x20);
    writel(readl(regs + 0x20) & ~1U, regs + 0x20);
    writel(readl(regs + 0x20) | 1, regs + 0x20);
    udelay(100);
    writel(6, crt + 0x1ec);
    writel(readl(regs + 0x78) & 0xfff8ffff, regs + 0x78);
    udelay(100);
    writel(7, crt + 0x1ec);
    udelay(200);
    writel(0x198cd99b, pad + 0x34);
    writel(0x000cf99f, pad + 0x38);
    writel(0x000cf99f, pad + 0x3c);
    writel(0, regs + 0x74);
    asm volatile("dsb sy" ::: "memory");
    writel(0, (void *)0x9801a020);
    asm volatile("dsb sy" ::: "memory");
    if (readl(crt + 0x1e0) != 0x2003 ||
        readl(crt + 0x1e8) != 0x00564388 ||
        readl(pad + 8) != 0x24a92000 || readl(pad + 12) != 1)
        return -EIO;
    return 0;
}
