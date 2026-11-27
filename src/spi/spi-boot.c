// SPDX-License-Identifier: GPL-2.0-or-later
/* Use upstream extlinux parsing; card limits are discovered, never baked in. */
#include <config.h>
#include <command.h>
#include <env.h>
#include <fs.h>
#include <mmc.h>
#include <part.h>
#include <asm/io.h>
#include <asm/global_data.h>
#include <asm/system.h>
#include <fdt_support.h>
#include <u-boot/sha256.h>
#include <vsprintf.h>
DECLARE_GLOBAL_DATA_PTR;

static u8 card_mac[6];
static bool card_selected;

static bool property_equals(void *fdt, int node, const char *name,
                            const char *expected)
{
    int length;
    const char *value = fdt_getprop(fdt, node, name, &length);
    return value && length == (int)strlen(expected) + 1 &&
           !memcmp(value, expected, length);
}

static int prepare_profile(void *fdt)
{
    static const struct {
        const char *path;
        u32 address, bytes;
    } pools[] = {
        {"/reserved-memory/audio@2600000", 0x02600000, 0x00c00000},
        {"/reserved-memory/media@3200000", 0x03200000, 0x0c700000},
        {"/reserved-memory/acpu@f900000", 0x0f900000, 0x00500000},
        {"/reserved-memory/vo@16000000", 0x16000000, 0x02000000},
    };
    int chosen = fdt_path_offset(fdt, "/chosen");
    int node, length;
    unsigned int i;
    bool media;
    const fdt32_t *reg;
    if (chosen < 0)
        return -EINVAL;
    media = property_equals(fdt, chosen, "bananapi,boot-profile", "media");
    if (!media && fdt_getprop(fdt, chosen, "bananapi,boot-profile", NULL) &&
        !property_equals(fdt, chosen, "bananapi,boot-profile", "server"))
        return -EINVAL;
    for (i = 0; i < (media ? ARRAY_SIZE(pools) : ARRAY_SIZE(pools) - 1); i++) {
        node = fdt_path_offset(fdt, pools[i].path);
        if (node < 0)
            return -EINVAL;
        reg = fdt_getprop(fdt, node, "reg", &length);
        if (!reg || length != 8 || fdt32_to_cpu(reg[0]) != pools[i].address ||
            fdt32_to_cpu(reg[1]) != pools[i].bytes)
            return -EINVAL;
        if (media) {
            if ((fdt_getprop(fdt, node, "status", NULL) &&
                 !property_equals(fdt, node, "status", "okay")) ||
                !fdt_getprop(fdt, node, "no-map", NULL))
                return -EINVAL;
        } else if (!property_equals(fdt, node, "status", "disabled")) {
            return -EINVAL;
        }
    }
    /* Neither mode may inherit a running independent processor. */
    if ((readl((void *)0x98000010) & (1U << 4)) || readl((void *)0x9801a108))
        return -EBUSY;
    if (media) {
        if (current_el() != 3 || readl((void *)0x98081340) == 0xdeaddead)
            return -EPERM;
        writel(0xacd1, (void *)0x98081340);
        mb();
        if (readl((void *)0x98081340) != 0xacd1 ||
            (readl((void *)0x98000010) & (1U << 4)))
            return -EIO;
    }
    if (fdt_setprop_string(fdt, chosen, "bananapi,boot-profile", media ? "media" : "server") ||
        fdt_setprop_string(fdt, chosen, "bananapi,spi-media-handoff", "v1") ||
        fdt_setprop_string(fdt, chosen, "bananapi,media-startup",
                           media ? "secure-armed-linux-test" : "disabled"))
        return -EINVAL;
    printf("BPI_M4_SPI_PROFILE: %s; ACPU firmware deferred to Linux\n",
           media ? "media" : "server");
    return 0;
}

int ft_board_setup(void *fdt, struct bd_info *bd)
{
    int node;
    (void)bd;
    if (!card_selected || fdt_node_check_compatible(fdt, 0, "bananapi,bpi-m4"))
        return -EINVAL;
    /* The kernel DTB comes from SD. Never pass the UART-only control DTB. */
    node = fdt_path_offset(fdt, "/soc@0/ethernet@98016000");
    if (node < 0)
        return node;
    if (fdt_setprop(fdt, node, "local-mac-address", card_mac, sizeof(card_mac)))
        return -EINVAL;
    if (fdt_fixup_memory(fdt, gd->ram_base, gd->ram_size))
        return -EINVAL;
    return prepare_profile(fdt);
}

static int do_bpispiboot(struct cmd_tbl *cmdtp, int flag, int argc,
                        char *const argv[])
{
    struct mmc *mmc;
    struct blk_desc *desc;
    struct disk_partition info;
    char device[16], command[160], args[1536], root[80];
    u8 digest[32];
    int part;
    (void)cmdtp; (void)flag; (void)argc; (void)argv;
    card_selected = false;
    /* Retained server kernels reclaim ACPU heaps. They require inactive RPC. */
    if ((readl((void *)0x98000010) & (1U << 4)) || readl((void *)0x9801a108)) {
        puts("BPI SPI: active ACPU/RPC; power-cycle before booting\n");
        return CMD_RET_FAILURE;
    }
    mmc = find_mmc_device(0);
    if (!mmc || mmc_init(mmc) || !mmc->high_capacity)
        return CMD_RET_FAILURE;
    desc = mmc_get_blk_desc(mmc);
    sha256_csum_wd((const u8 *)mmc->cid, sizeof(mmc->cid), digest, CHUNKSZ_SHA256);
    memcpy(card_mac, digest, sizeof(card_mac));
    card_mac[0] = (card_mac[0] & 0xfc) | 2;
    for (part = 1; part <= 16; part++) {
        if (part_get_info(desc, part, &info) || info.blksz != 512 ||
            !info.start || !info.size || info.start >= desc->lba ||
            info.size > desc->lba - info.start)
            continue;
        snprintf(device, sizeof(device), "0:%d", part);
        if (fs_set_blk_dev("mmc", device, FS_TYPE_ANY) ||
            !fs_exists("/boot/extlinux/extlinux.conf"))
            continue;
        if (!info.uuid[0]) {
            puts("BPI SPI: boot partition has no PARTUUID\n");
            return CMD_RET_FAILURE;
        }
        snprintf(root, sizeof(root), "PARTUUID=%s", info.uuid);
        snprintf(args, sizeof(args),
            "bpi_m4_ethernet.activate=1 "
            "bpi_m4_sd_cmd18.write_enable=1 bpi_m4_sd_cmd18.native_write=1 "
            "bpi_m4_sd_cmd18.scratch_start=%llu bpi_m4_sd_cmd18.scratch_sectors=%llu "
            "bpi_m4_sd_cmd18.scratch_cid=%08x%08x%08x%08x "
            "bpi_m4_sd_cmd18.native_read=1 bpi_m4_sd_cmd18.read_cid=%08x%08x%08x%08x "
            "bpi_m4_sd_cmd18.read_capacity=%llu",
            (unsigned long long)info.start, (unsigned long long)info.size,
            mmc->cid[0], mmc->cid[1], mmc->cid[2], mmc->cid[3],
            mmc->cid[0], mmc->cid[1], mmc->cid[2], mmc->cid[3],
            (unsigned long long)desc->lba);
        if (env_set("bpi_root", root) || env_set("bpi_sd_args", args))
            return CMD_RET_FAILURE;
        card_selected = true;
        snprintf(command, sizeof(command),
            "sysboot mmc %s any 0x02300000 /boot/extlinux/extlinux.conf", device);
        printf("BPI_M4_SPI_BOOT: extlinux on mmc %s, root=%s\n", device, root);
        /* A failed boot returns to the console; do not try another disk. */
        return run_command(command, 0) ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
    }
    puts("BPI SPI: no /boot/extlinux/extlinux.conf found on SD\n");
    return CMD_RET_FAILURE;
}
U_BOOT_CMD(bpispiboot, 1, 0, do_bpispiboot,
           "Boot versioned SD kernels using extlinux", "");
