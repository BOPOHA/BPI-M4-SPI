// SPDX-License-Identifier: GPL-2.0-or-later
/* Host-only, bounded RTD1395 SPI layout; never opens a physical flash device. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>

struct blob { unsigned char *data; size_t size; };
static void require(int ok, const char *message)
{
    if (!ok) { fprintf(stderr, "SPI layout: %s\n", message); exit(1); }
}
static struct blob load(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long length;
    struct blob b;
    require(fp && !fseek(fp, 0, SEEK_END) && (length = ftell(fp)) >= 0 && length <= 0x200000,
            "invalid input length");
    rewind(fp);
    b.size = (size_t)length;
    b.data = calloc(1, b.size + 1);
    require(b.data && fread(b.data, 1, b.size, fp) == b.size && !ferror(fp) && !fclose(fp),
            "input read failed");
    return b;
}
static void le32(unsigned char *p, uint32_t value)
{
    for (unsigned int i = 0; i < 4; i++) p[i] = (unsigned char)(value >> (8 * i));
}
static void emit(FILE *fp, const void *data, size_t size)
{
    require(fwrite(data, 1, size, fp) == size, "output write failed");
}
static void zeros(FILE *fp, size_t size)
{
    unsigned char zero[1024] = {0};
    while (size) {
        size_t n = size < sizeof(zero) ? size : sizeof(zero);
        emit(fp, zero, n); size -= n;
    }
}
static void segment(FILE *json, const char *name, size_t offset, struct blob b, int last)
{
    unsigned char hash[32];
    unsigned int length;
    require(EVP_Digest(b.data, b.size, hash, &length, EVP_sha256(), NULL) && length == 32,
            "digest failed");
    fprintf(json, "    {\"name\":\"%s\",\"offset\":%zu,\"bytes\":%zu,\"sha256\":\"",
            name, offset, b.size);
    for (unsigned int i = 0; i < length; i++) fprintf(json, "%02x", hash[i]);
    fprintf(json, "\"}%s\n", last ? "" : ",");
}
int main(int argc, char **argv)
{
    if (argc == 6 && !strcmp(argv[1], "header")) {
        struct blob hw = load(argv[2]), ub = load(argv[3]), fsbl = load(argv[4]);
        unsigned char header[64] = {0};
        FILE *fp = fopen(argv[5], "wb");
        require(fp && hw.size >= 520 && ub.size >= 520 && fsbl.size >= 520,
                "invalid vendor header component");
        le32(header, hw.size - 520);
        /* Keep the vendor's unconditional boot_enc header convention, even
         * when Config_Secure_Improve is false and the U-Boot trailer is SHA256.
         * Byte equality with bindAllBins is checked; FSBL behavior needs UART tests.
         */
        le32(header + 4, ub.size - 520);
        le32(header + 8, fsbl.size - 520);
        emit(fp, header, sizeof(header));
        require(!fclose(fp), "header close failed");
        free(hw.data); free(ub.data); free(fsbl.data);
    } else if (argc == 10 && !strcmp(argv[1], "assemble")) {
        const char *names[] = {"arm-rom", "mips-rom", "header", "hwsettings", "uboot", "fsbl"};
        struct blob b[6];
        size_t offsets[6] = {0}, total;
        FILE *fp, *json;
        for (unsigned int i = 0; i < 6; i++) b[i] = load(argv[i + 2]);
        require(b[0].size <= 188 * 1024 && b[1].size <= 4096 && b[2].size == 96,
                "ROM/header size outside RTD1395 layout");
        offsets[1] = 188 * 1024;
        offsets[2] = 188 * 1024 + 4096 + 2048;
        for (unsigned int i = 3; i < 6; i++) offsets[i] = offsets[i - 1] + b[i - 1].size;
        total = offsets[5] + b[5].size;
        require(total <= 0x200000, "candidate overlaps the factory region");
        fp = fopen(argv[8], "wb"); json = fopen(argv[9], "w");
        require(fp && json, "cannot create candidate/layout");
        emit(fp, b[0].data, b[0].size); zeros(fp, offsets[1] - b[0].size);
        emit(fp, b[1].data, b[1].size); zeros(fp, offsets[2] - offsets[1] - b[1].size);
        for (unsigned int i = 2; i < 6; i++) emit(fp, b[i].data, b[i].size);
        require(!fclose(fp), "candidate close failed");
        fprintf(json, "{\n  \"format\":\"RTD1395 vendor SPI boot region\",\n"
                      "  \"bytes\":%zu,\n  \"factory_offset\":2097152,\n"
                      "  \"hardware_validated\":false,\n  \"segments\":[\n", total);
        for (unsigned int i = 0; i < 6; i++) segment(json, names[i], offsets[i], b[i], i == 5);
        fprintf(json, "  ]\n}\n");
        require(!fclose(json), "layout close failed");
        for (unsigned int i = 0; i < 6; i++) free(b[i].data);
    } else {
        fprintf(stderr, "Usage: spi-layout header HW UBOOT FSBL OUT | "
                        "assemble ARM MIPS HEADER HW UBOOT FSBL OUT JSON\n");
        return 2;
    }
    return 0;
}
