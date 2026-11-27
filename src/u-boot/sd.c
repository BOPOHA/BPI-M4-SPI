// SPDX-License-Identifier: GPL-2.0-only
/* Read-only RTD1395 SD adapter, derived from our audited Linux controller.
 * Protocol provenance: Realtek GPL BSP; see docs/PROVENANCE.md.
 * One-sector commands, no writes, no UHS. Preserve card power and normalize
 * the audited SPI SDR104 handoff to the legacy PLL state required by Linux.
 * Caches are disabled in this second stage. Uncertain DMA freezes the host.
 */
#include <config.h>
#include <dm.h>
#include <mmc.h>
#include <malloc.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <asm/unaligned.h>
#include <time.h>
#include <dm/device_compat.h>
#ifdef CONFIG_BPI_M4_SPI
int bpi_spi_sd_init(void __iomem *regs);
#endif
#define DMA1 0x004
#define DMA2 0x008
#define DMA3 0x00c
#define SYNC 0x020
#define ISR 0x024
#define ISREN 0x028
#define CKGEN 0x078
#define STOP 0x103
#define CARD_OE 0x104
#define CARD_SELECT 0x10e
#define CARD_CLOCK 0x129
#define EXIST 0x11f
#define CFG1 0x180
#define CFG2 0x181
#define CFG3 0x182
#define ST1 0x183
#define ST2 0x184
#define CMD0 0x189
#define BYTELO 0x18f
#define BYTEHI 0x190
#define BLOCKLO 0x191
#define BLOCKHI 0x192
#define TRANS 0x193

struct bpi_sd {
    void __iomem *regs;
    u8 *buffer;
    bool failed;
    u32 data_clock;
};
struct bpi_sd_plat { struct mmc_config cfg; struct mmc mmc; };
static void sync_bus(void) {
    asm volatile("dsb sy" ::: "memory");
    writel(0, (void *)0x9801a020);
    asm volatile("dsb sy" ::: "memory");
}
/* Audited RTD1395 PLL/FIFO sequence from rtk_sdmmc_init(), GPL BSP revision
 * 25f5b88ec4ba34029f964693dc34028b26e6c67c; see docs/PROVENANCE.md.
 * The SPI loader may tune SDR104 before loading us. Merely accepting that
 * state would give our legacy dividers the wrong source clock. No GPIO,
 * card-power, persistent environment or flash registers are changed here.
 */
static int legacy_init(struct bpi_sd *h) {
    void __iomem *crt = (void *)0x98000000;
    void __iomem *pad = (void *)0x9804e000;
    u32 pll1 = readl(crt + 0x1e0), pll3 = readl(crt + 0x1e8);
    bool legacy = pll1 == 0x2003 && pll3 == 0x00564388;
    /* SPI tuning varies TX[7:3] and RX[12:8] across physical boots. */
    bool sdr104 = (pll1 & ~0x1ff8U) == 0x6003 && pll3 == 0x00b64388;
    if ((!legacy && !sdr104) || readl(pad + 8) != 0x24a92000 ||
        readl(pad + 12) != 1 || readl(h->regs + 0x74) != 0 ||
        (readl(h->regs + DMA3) & 1)) {
        printf("BPI SD handoff rejected: PLL=%08x/%08x DMA=%08x\n",
               pll1, pll3, readl(h->regs + DMA3));
        return -EINVAL;
    }
    if (sdr104) {
        puts("BPI SD: normalize audited SPI SDR104 handoff to legacy clock\n");
        writeb(0xff, h->regs + STOP);
        writeb(0, h->regs + CARD_CLOCK);
        writel(readl(crt + 0x1ec) | 7, crt + 0x1ec);
        writel(0x2003, crt + 0x1e0);
        udelay(100);
        writel(6, crt + 0x1ec);
        writel(readl(h->regs + CKGEN) | 0x70000, h->regs + CKGEN);
        udelay(2000);
        writel(7, crt + 0x1ec);
        udelay(300);
        writel(6, crt + 0x1ec);
        writel(0x04517893, crt + 0x1e4);
        writel(0x00564388, crt + 0x1e8);
        udelay(2000);
        writel(7, crt + 0x1ec);
        udelay(300);
        writel(6, crt + 0x1ec);
        writel(readl(h->regs + CKGEN) & 0xfff8ffff, h->regs + CKGEN);
        udelay(100);
        writel(7, crt + 0x1ec);
        udelay(200);
        writel(0x198cd99b, pad + 0x34);
        writel(0x000cf99f, pad + 0x38);
        writel(0x000cf99f, pad + 0x3c);
    }
    sync_bus();
    if (readl(crt + 0x1e0) != 0x2003 || readl(crt + 0x1e8) != 0x00564388)
        return -EIO;
    return 0;
}
static int wait_transfer(struct bpi_sd *h, bool dma) {
    ulong start = get_timer(0);
    for (;;) {
        u8 t;
        sync_bus();
        t = readb(h->regs + TRANS);
        if ((t & 0x10) || (readb(h->regs + ST2) & 1)) break;
        if ((t & 0x60) == 0x60 && (!dma || !(readl(h->regs + DMA3) & 1))) return 0;
        if (get_timer(start) > 1000) break;
        udelay(10);
    }
    h->failed = true;
    printf("BPI SD frozen: trans=%02x st1=%02x st2=%02x DMA=%08x\n",
           readb(h->regs + TRANS), readb(h->regs + ST1),
           readb(h->regs + ST2), readl(h->regs + DMA3));
    return -EIO;
}
static int send_cmd(struct udevice *dev, struct mmc_cmd *cmd, struct mmc_data *data) {
    struct bpi_sd *h = dev_get_priv(dev);
    bool r2 = cmd->resp_type & MMC_RSP_136;
    bool short_data = data && data->blocksize <= 64;
    u32 bytes = data ? data->blocksize * data->blocks : 0;
    u8 cfg2, cfg3 = 5, transfer = 8;
    int i, ret;
    if (h->failed) return -EIO;
    /* U-Boot b_max=1 guarantees CMD17, never an unchecked stream. */
    switch (cmd->cmdidx) {
    case 0: case 2: case 3: case 6: case 7: case 8: case 9: case 10:
    case 13: case 16: case 17: case 41: case 51: case 55: break;
    default: return -EOPNOTSUPP;
    }
    if (data && (data->flags != MMC_DATA_READ || data->blocks != 1 ||
                 !bytes || bytes > 512)) return -EINVAL;
    if (r2 || data) memset(h->buffer, 0, 512);
    memset(cmd->response, 0, sizeof(cmd->response));
    if (!(cmd->resp_type & MMC_RSP_PRESENT)) cfg2 = 0x74;
    else if (r2) cfg2 = 0x42;
    else cfg2 = data ? 0x01 : 0x41;
    if (!(cmd->resp_type & MMC_RSP_CRC)) cfg2 |= 4;
    if (cmd->resp_type & MMC_RSP_BUSY) cfg2 |= 8;
    if (cmd->cmdidx == 0 || cmd->cmdidx == 8) {
        writeb(0xd0, h->regs + CFG1); cfg3 = 0;
    }
    if (cmd->cmdidx == 41) cfg3 = 0;
    if (cmd->cmdidx == 51) cfg2 = 0x41;
    if (cmd->cmdidx == 13) cfg3 = readb(h->regs + CFG3);
    if (data) {
        writeb(readb(h->regs + CFG1) & ~0x10, h->regs + CFG1);
        writel(h->data_clock, h->regs + CKGEN);
        writeb(0x10, h->regs + CFG1);
        transfer = cmd->cmdidx == 17 ? 0x0e : 0x0c;
    }
    writeb(0, h->regs + STOP);
    writel(0x17, h->regs + ISREN);
    writel(0x17, h->regs + ISR);
    writeb(cfg2, h->regs + CFG2);
    writeb(cfg3, h->regs + CFG3);
    writeb(0x40 | cmd->cmdidx, h->regs + CMD0);
    for (i = 0; i < 4; i++) writeb(cmd->cmdarg >> (24 - i * 8), h->regs + CMD0 + 1 + i);
    writeb(0, h->regs + CMD0 + 5);
    if (r2 || data) {
        unsigned int count = short_data ? 64 : 512;
        writeb(count, h->regs + BYTELO);
        writeb(count >> 8, h->regs + BYTEHI);
        writeb(1, h->regs + BLOCKLO);
        writeb(0, h->regs + BLOCKHI);
        writel((ulong)h->buffer >> 3, h->regs + DMA1);
        writel(1, h->regs + DMA2);
        sync_bus();
        writel(3 | (r2 ? 0x10 : short_data ? 0x20 : 0), h->regs + DMA3);
    }
    sync_bus();
    writeb(0x80 | transfer, h->regs + TRANS);
    ret = wait_transfer(h, r2 || data);
    if (ret) return ret;
    sync_bus();
    if ((cmd->resp_type & MMC_RSP_CRC) &&
        ((readb(h->regs + ST1) & 0x82) || (readb(h->regs + ST2) & 3))) {
        h->failed = true; return -EILSEQ;
    }
    if (r2) {
        u32 words[4];
        for (i = 0; i < 4; i++) words[i] = get_unaligned_be32(h->buffer + i * 4);
        for (i = 0; i < 3; i++) cmd->response[i] = words[i] << 8 | words[i + 1] >> 24;
        cmd->response[3] = words[3] << 8 | readb(h->regs + CMD0 + 5);
    } else if ((cmd->resp_type & MMC_RSP_PRESENT) && (cfg2 & 3) == 1) {
        for (i = 0; i < 4; i++) cmd->response[0] = cmd->response[0] << 8 | readb(h->regs + CMD0 + 1 + i);
    }
    if (data) {
        if ((readb(h->regs + ST1) & 0xc2) || (readb(h->regs + ST2) & 3) ||
            (cmd->response[0] & 0xfff9a000U) || (cmd->response[0] & (1U << 25))) {
            h->failed = true; return -EILSEQ;
        }
        memcpy(data->dest, h->buffer, bytes);
    }
    return 0;
}
static int set_ios(struct udevice *dev) {
    struct bpi_sd *h = dev_get_priv(dev);
    struct mmc *mmc = mmc_get_mmc_dev(dev);
    if (h->failed || mmc->bus_width != 1) return -EINVAL;
    h->data_clock = mmc->clock >= 25000000 ? 0x2101 : 0x2103;
    writel(mmc->clock > 400000 ? h->data_clock : 0x2100, h->regs + CKGEN);
    writeb(mmc->clock > 400000 ? 0x10 : 0xd0, h->regs + CFG1);
    return 0;
}
static int get_cd(struct udevice *dev) {
    struct bpi_sd *h = dev_get_priv(dev);
    return !!(readb(h->regs + EXIST) & 4);
}
static int probe(struct udevice *dev) {
    struct bpi_sd *h = dev_get_priv(dev);
    struct bpi_sd_plat *p = dev_get_plat(dev);
    struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
    u32 value;
    h->regs = dev_read_addr_ptr(dev);
#ifdef CONFIG_BPI_M4_SPI
    if (!h->regs || bpi_spi_sd_init(h->regs)) return -EINVAL;
#endif
    if (!h->regs || legacy_init(h)) return -EINVAL;
    /* Kept for lifetime of the host, including uncertain completion. */
    h->buffer = memalign(64, 512);
    if (!h->buffer || ((ulong)h->buffer >> 32)) return -ENOMEM;
    h->data_clock = 0x2103;
    p->cfg.name = "RTD1395 read-only SD";
    p->cfg.voltages = MMC_VDD_32_33 | MMC_VDD_33_34;
    p->cfg.host_caps = 0;
    p->cfg.f_min = 400000;
    p->cfg.f_max = 25000000;
    p->cfg.b_max = 1;
    p->mmc.cfg = &p->cfg;
    p->mmc.dev = dev;
    upriv->mmc = &p->mmc;
    writel(readl(h->regs + SYNC) | 2, h->regs + SYNC);
    writeb(0xff, h->regs + STOP);
    writel(0, h->regs + DMA3);
    value = readl(h->regs + SYNC);
    writel(value & ~1, h->regs + SYNC);
    writel(value | 1, h->regs + SYNC);
    sync_bus();
    writeb(readb(h->regs + CFG1) & ~0x10, h->regs + CFG1);
    udelay(100);
    writeb(0, h->regs + STOP);
    writeb(2, h->regs + CARD_SELECT);
    writeb(4, h->regs + CARD_OE);
    writeb(4, h->regs + CARD_CLOCK);
    writeb(0, h->regs + ST2);
    writeb(0, h->regs + 0x187); /* SD_SAMPLE_POINT_CTL */
    writeb(0, h->regs + 0x188); /* SD_PUSH_POINT_CTL */
    return 0;
}
static int bind(struct udevice *dev) {
    struct bpi_sd_plat *p = dev_get_plat(dev);
    return mmc_bind(dev, &p->mmc, &p->cfg);
}
static const struct dm_mmc_ops ops = { .send_cmd = send_cmd, .set_ios = set_ios, .get_cd = get_cd };
static const struct udevice_id ids[] = { { .compatible = "bananapi,bpi-m4-fc-sd" }, { } };
U_BOOT_DRIVER(bpi_m4_fc_sd) = {
    .name = "bpi_m4_fc_sd", .id = UCLASS_MMC, .of_match = ids,
    .ops = &ops, .probe = probe, .bind = bind,
    .priv_auto = sizeof(struct bpi_sd), .plat_auto = sizeof(struct bpi_sd_plat),
};
