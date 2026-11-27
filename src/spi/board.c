// SPDX-License-Identifier: GPL-2.0-or-later
/* DDR is initialized by the retained RTD1395 ROM/FSBL hardware settings. */
#include <config.h>
#include <init.h>
#include <asm/global_data.h>
#include <asm/armv8/mmu.h>
#include <asm/io.h>
DECLARE_GLOBAL_DATA_PTR;
static struct mm_region regions[] = {
    { .virt = 0, .phys = 0, .size = CONFIG_BPI_M4_RAM_SIZE,
      .attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_INNER_SHARE },
    { .virt = 0x98000000, .phys = 0x98000000, .size = 0x08000000,
      .attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) | PTE_BLOCK_NON_SHARE |
               PTE_BLOCK_PXN | PTE_BLOCK_UXN },
    { 0 }
};
struct mm_region *mem_map = regions;
/* Keep DMA and the SB2 controller on the existing uncached protocol. */
void enable_caches(void) { }
int dram_init(void)
{
    gd->ram_base = CFG_SYS_SDRAM_BASE;
    gd->ram_size = CONFIG_BPI_M4_RAM_SIZE - CFG_SYS_SDRAM_BASE;
    return 0;
}
int dram_init_banksize(void)
{
    gd->dram[0].start = gd->ram_base;
    gd->dram[0].size = gd->ram_size;
    return 0;
}
int board_init(void) { gd->bd->bi_boot_params = 0x02100000; return 0; }
int checkboard(void)
{
    puts("Board: Banana Pi M4 SPI, retained vendor DDR/FSBL\n");
    return 0;
}
void reset_cpu(void)
{
    puts("Physical power cycle required\n");
    for (;;) asm volatile("wfi");
}
