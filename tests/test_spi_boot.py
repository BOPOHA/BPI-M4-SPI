"""Exercise SPI cold initialization and generic SD selection with MMIO faults."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def compile_run(source):
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder)
        (path / 'test.c').write_text(source)
        subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                        path / 'test.c', '-o', path / 'test'], check=True)
        subprocess.run([path / 'test'], check=True, stdout=subprocess.PIPE)


class SpiBootTests(unittest.TestCase):
    def test_cold_sd_init_refuses_active_dma_and_detects_failed_writes(self):
        source = (ROOT / 'src/spi/sd-init.c').read_text()
        function = source[source.index('int bpi_spi_sd_init'):]
        function = function.replace('asm volatile("dsb sy" ::: "memory");', 'barrier();')
        compile_run(r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define __iomem
static uint32_t regs[0x50000 / 4];
static unsigned writes, barriers, fault;
static uint32_t readl(void *p) {
    uintptr_t offset = (uintptr_t)p - 0x98000000;
    assert(offset < sizeof(regs) && !(offset % 4));
    return regs[offset / 4];
}
static void writel(uint32_t v, void *p) {
    uintptr_t offset = (uintptr_t)p - 0x98000000;
    assert(offset < sizeof(regs) && !(offset % 4));
    writes++;
    if (!(fault && offset == 0x1e8)) regs[offset / 4] = v;
}
static void udelay(unsigned n) { assert(n <= 2000); }
static void barrier(void) { barriers++; }
''' + function + r'''
int main(void) {
    void *sd = (void *)0x98010400;
    regs[(0x10400 + 0xc) / 4] = 1;
    assert(bpi_spi_sd_init(sd) == -EBUSY && !writes);
    regs[(0x10400 + 0xc) / 4] = 0;
    regs[4 / 4] = 0x10000001;
    regs[12 / 4] = 0x40000001;
    regs[0x7100 / 4] = 0x12345678;
    assert(!bpi_spi_sd_init(sd));
    assert(writes && barriers == 2);
    assert(regs[4 / 4] == (0x10000001 | (1U << 10)));
    assert(regs[12 / 4] == (0x40000001 | (1U << 25) | (1U << 31)));
    assert(regs[0x7100 / 4] == 0x12345678);
    assert(regs[0x4e008 / 4] == 0x24a92000);
    assert(regs[0x4e00c / 4] == 1);
    assert(!(regs[(0x10400 + 0x78) / 4] & 0x70000));
    assert(regs[(0x10400 + 0x74) / 4] == 0);
    fault = 1; regs[0x1e8 / 4] = 0;
    assert(bpi_spi_sd_init(sd) == -EIO);
    return 0;
}
''')

    def test_generic_boot_uses_detected_partition_and_card_limits(self):
        source = (ROOT / 'src/spi/spi-boot.c').read_text()
        functions = source[source.index('static u8 card_mac'):source.index('U_BOOT_CMD(')]
        compile_run(r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
typedef unsigned char u8;
typedef uint32_t u32;
typedef uint32_t fdt32_t;
#define fdt32_to_cpu(x) (x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
struct bd_info { int unused; };
struct cmd_tbl { int unused; };
struct global_data { unsigned long ram_base, ram_size; };
static struct global_data gd_value = {0x100000, 0x7ff00000}, *gd = &gd_value;
struct mmc { bool high_capacity; uint32_t cid[4]; };
struct blk_desc { uint64_t lba; };
struct disk_partition { uint64_t start, size; unsigned blksz; char uuid[40]; };
#define CMD_RET_FAILURE 1
#define CMD_RET_SUCCESS 0
#define FS_TYPE_ANY 0
#define CHUNKSZ_SHA256 0
static struct mmc card = {true, {0x12345678, 0xabcdef01, 0x10102020, 0x30304040}};
static struct blk_desc disk = {61071360};
static unsigned fault, commands, selected;
static bool media_mode;
static uint32_t startup;
static char media_startup[40], capability[8];
static unsigned int current_el(void) { return fault == 12 ? 2 : 3; }
static void mb(void) {}
static char root[80], args[1536], bootcmd[160];
static uint32_t readl(void *p) {
    if ((uintptr_t)p == 0x98081340) return fault == 14 ? 0xdeaddead : startup;
    return fault == 1 && (uintptr_t)p == 0x98000010 ? 16 : 0;
}
static void writel(uint32_t value, void *p) {
    assert((uintptr_t)p == 0x98081340 && value == 0xacd1);
    if (fault != 13) startup = value;
}
static struct mmc *find_mmc_device(int n) { assert(!n); return &card; }
static int mmc_init(struct mmc *m) { assert(m == &card); return fault == 2; }
static struct blk_desc *mmc_get_blk_desc(struct mmc *m) { assert(m == &card); return &disk; }
static void sha256_csum_wd(const u8 *p, unsigned n, u8 *out, unsigned chunk) {
    assert(p == (const u8 *)card.cid && n == sizeof(card.cid) && !chunk);
    memset(out, 0x11, 32);
}
static int part_get_info(struct blk_desc *d, int n, struct disk_partition *p) {
    assert(d == &disk);
    if (n != 2) return -1;
    *p = (struct disk_partition){526336, 61071360 - 526336, 512, "12345678-02"};
    if (fault == 3) p->size = UINT64_MAX;
    if (fault == 4) p->start = disk.lba;
    if (fault == 5) p->uuid[0] = 0;
    return 0;
}
static int fs_set_blk_dev(const char *kind, const char *device, int type) {
    assert(!strcmp(kind, "mmc") && !strcmp(device, "0:2") && type == FS_TYPE_ANY);
    selected++; return 0;
}
static int fs_exists(const char *path) {
    assert(!strcmp(path, "/boot/extlinux/extlinux.conf")); return fault != 6;
}
static int env_set(const char *name, const char *value) {
    if (!strcmp(name, "bpi_root")) snprintf(root, sizeof(root), "%s", value);
    else if (!strcmp(name, "bpi_sd_args")) snprintf(args, sizeof(args), "%s", value);
    else assert(0);
    return 0;
}
static int run_command(const char *command, int flags) {
    assert(!flags); commands++; snprintf(bootcmd, sizeof(bootcmd), "%s", command);
    return fault == 7;
}
static int fdt_node_check_compatible(void *fdt, int node, const char *compat) {
    assert(fdt && !node && !strcmp(compat, "bananapi,bpi-m4")); return fault == 8;
}
static int fdt_path_offset(void *fdt, const char *path) {
    assert(fdt);
    if (!strcmp(path, "/chosen")) return 1;
    if (!strcmp(path, "/soc@0/ethernet@98016000")) return 10;
    if (!strcmp(path, "/reserved-memory/audio@2600000")) return 20;
    if (!strcmp(path, "/reserved-memory/media@3200000")) return 21;
    if (!strcmp(path, "/reserved-memory/acpu@f900000")) return 22;
    if (!strcmp(path, "/reserved-memory/vo@16000000")) return 23;
    assert(0); return -1;
}
static const void *fdt_getprop(void *fdt, int node, const char *name, int *length) {
    static const uint32_t regs[][2] = {
        {0x2600000, 0xc00000}, {0x3200000, 0xc700000},
        {0xf900000, 0x500000}, {0x16000000, 0x2000000}};
    static const uint32_t wrong[2] = {0x2600000, 0x800000};
    const void *value = NULL;
    int size = 0;
    assert(fdt);
    if (node == 1 && !strcmp(name, "bananapi,boot-profile")) {
        value = fault == 9 ? "unknown" : media_mode ? "media" : "server";
        size = strlen(value) + 1;
    } else if (node >= 20 && node <= 23) {
        if (!strcmp(name, "reg")) { value = fault == 10 ? wrong : regs[node-20]; size = 8; }
        else if (!strcmp(name, "no-map")) value = "";
        else if (!strcmp(name, "status") && (!media_mode || fault == 11)) { value = "disabled"; size = 9; }
    }
    if (length) *length = size;
    return value;
}
static int fdt_setprop_string(void *fdt, int node, const char *name, const char *value) {
    assert(fdt && node == 1);
    if (!strcmp(name, "bananapi,media-startup")) snprintf(media_startup, sizeof(media_startup), "%s", value);
    else if (!strcmp(name, "bananapi,spi-media-handoff")) snprintf(capability, sizeof(capability), "%s", value);
    else assert(!strcmp(name, "bananapi,boot-profile"));
    return 0;
}
static int fdt_setprop(void *fdt, int node, const char *name, const void *data, int length) {
    assert(fdt && node == 10 && !strcmp(name, "local-mac-address") && length == 6);
    assert(((const u8 *)data)[0] == 0x12); return 0;
}
static int fdt_fixup_memory(void *fdt, unsigned long base, unsigned long size) {
    assert(fdt && base == gd->ram_base && size == gd->ram_size); return 0;
}
''' + functions + r'''
int main(void) {
    for (fault = 1; fault <= 6; fault++) {
        commands = selected = 0;
        assert(do_bpispiboot(NULL, 0, 1, NULL) == CMD_RET_FAILURE && !commands);
        assert(ft_board_setup((void *)1, NULL) == -EINVAL);
        if (fault <= 4) assert(!selected);
    }
    fault = 0;
    assert(!do_bpispiboot(NULL, 0, 1, NULL) && commands == 1);
    assert(!strcmp(root, "PARTUUID=12345678-02"));
    assert(strstr(args, "scratch_start=526336 "));
    assert(strstr(args, "scratch_sectors=60545024 "));
    assert(strstr(args, "scratch_cid=12345678abcdef011010202030304040"));
    assert(strstr(args, "read_capacity=61071360"));
    assert(!strcmp(bootcmd, "sysboot mmc 0:2 any 0x02300000 /boot/extlinux/extlinux.conf"));
    assert(!ft_board_setup((void *)1, NULL));
    assert(!strcmp(media_startup, "disabled") && !strcmp(capability, "v1") && !startup);
    fault = 8; assert(ft_board_setup((void *)1, NULL) == -EINVAL);
    fault = 7; assert(do_bpispiboot(NULL, 0, 1, NULL) == CMD_RET_FAILURE);
    fault = 0; assert(!do_bpispiboot(NULL, 0, 1, NULL));
    media_mode = true;
    for (fault = 9; fault <= 14; fault++) {
        startup = 0;
        assert(ft_board_setup((void *)1, NULL) < 0);
    }
    fault = 0; startup = 0;
    assert(!ft_board_setup((void *)1, NULL) && startup == 0xacd1);
    assert(!strcmp(media_startup, "secure-armed-linux-test"));
    fault = 1; assert(ft_board_setup((void *)1, NULL) == -EBUSY);
    return 0;
}
''')
