// SPDX-License-Identifier: GPL-2.0-or-later
/* Host-only RTD1395 packaging primitives, using current OpenSSL EVP/BN APIs.
 * Format: pinned flash_writer_vm sources. Obfuscation mode 97 is reconstructed
 * from the supplied obfuse_h and compared with it in Mock. No flash operations.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

struct blob { unsigned char *data; size_t size; };
static void require(int ok, const char *message)
{
    if (!ok) { fprintf(stderr, "SPI packaging: %s\n", message); exit(1); }
}
static struct blob read_blob(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long size;
    struct blob b;
    require(fp != NULL, "cannot open input");
    require(!fseek(fp, 0, SEEK_END) && (size = ftell(fp)) >= 0 && size <= 0x200000,
            "input exceeds the 2 MiB build limit");
    rewind(fp);
    b.size = (size_t)size;
    b.data = calloc(1, b.size + 16);
    require(b.data != NULL && fread(b.data, 1, b.size, fp) == b.size && !ferror(fp),
            "input read failed");
    require(!fclose(fp), "input close failed");
    return b;
}
static void write_blob(const char *path, const void *data, size_t size)
{
    FILE *fp = fopen(path, "wb");
    require(fp != NULL, "cannot create output");
    require(fwrite(data, 1, size, fp) == size && !fclose(fp), "output write failed");
}
static void sha256(const void *data, size_t size, unsigned char digest[32])
{
    unsigned int length;
    require(EVP_Digest(data, size, digest, &length, EVP_sha256(), NULL) && length == 32,
            "SHA256 failed");
}
static void aes(int encrypt, const unsigned char key[16], const void *input,
                size_t size, unsigned char *output)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int used, last;
    require(!(size % 16) && size <= 0x200000, "AES input is not block aligned");
    require(ctx && EVP_CipherInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL, encrypt) &&
            EVP_CIPHER_CTX_set_padding(ctx, 0) &&
            EVP_CipherUpdate(ctx, output, &used, input, (int)size) &&
            EVP_CipherFinal_ex(ctx, output + used, &last) && (size_t)(used + last) == size,
            "AES-128-ECB failed");
    EVP_CIPHER_CTX_free(ctx);
}
static void obfuscation_key(const unsigned char module[16], const unsigned char sck[16],
                            const unsigned char vendor[16], unsigned char key[16])
{
    static const unsigned char base[16] = {
        0xf0,0xa8,0x63,0x10,0xd6,0x7e,0x78,0xe0,0x0b,0x5c,0x92,0x91,0x04,0x74,0xa9,0xde };
    static const unsigned char wrapped[3][16] = {
        {0x17,0x1e,0x2e,0x27,0x32,0xde,0xa3,0xdd,0x5d,0xb9,0x1b,0xb6,0x55,0xba,0xb6,0xc0},
        {0x4d,0x9b,0x09,0xca,0xae,0x9c,0x26,0xdc,0x23,0xc0,0x16,0xb1,0x18,0xbf,0x0c,0xdc},
        {0x0f,0x98,0x33,0x22,0xba,0x45,0x1e,0xa2,0xfa,0x93,0xbe,0xc9,0xdb,0xb1,0x55,0xec} };
    unsigned char k1[32], k2[32], k3[32], tmp[32];
    aes(1, sck, vendor, 16, k1);
    aes(1, base, vendor, 16, tmp);
    aes(1, k1, tmp, 16, k2);
    aes(1, k2, module, 16, k3);
    aes(0, k3, wrapped[0], 16, tmp);
    aes(0, tmp, wrapped[1], 16, k1);
    aes(0, k1, wrapped[2], 16, tmp);
    memcpy(key, tmp, 16);
}
static void pad(const char *input, const char *padding_path, const char *digest_path)
{
    struct blob b = read_blob(input);
    unsigned char padding[128] = {0x80}, digest[32];
    size_t length = 64 - ((b.size + 8) % 64) + 8;
    uint64_t bits = (uint64_t)b.size * 8;
    for (unsigned int i = 0; i < 8; i++)
        padding[length - 1 - i] = (unsigned char)(bits >> (i * 8));
    sha256(b.data, b.size, digest);
    write_blob(padding_path, padding, length);
    write_blob(digest_path, digest, sizeof(digest));
    free(b.data);
}
static void sign(const char *key_path, const char *digest_path, const char *output)
{
    FILE *fp = fopen(key_path, "rb");
    EVP_PKEY *key;
    EVP_PKEY_CTX *ctx;
    BN_CTX *bnctx = BN_CTX_new();
    BIGNUM *n = NULL, *power = BN_new(), *rr = BN_new(), *radix = BN_new(), *neg = BN_new();
    BIGNUM *inverse;
    struct blob digest = read_blob(digest_path);
    unsigned char signature[256], record[520];
    size_t size = sizeof(signature);
    require(fp != NULL && digest.size == 32, "RSA key/digest input invalid");
    key = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
    require(!fclose(fp) && key && EVP_PKEY_is_a(key, "RSA") && EVP_PKEY_get_bits(key) == 2048,
            "expected pinned RSA-2048 key");
    ctx = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    /* Match vendor rsautl -sign: PKCS#1 v1.5 wraps the raw 32-byte hash. */
    require(ctx && EVP_PKEY_sign_init(ctx) > 0 &&
            EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) > 0 &&
            EVP_PKEY_sign(ctx, signature, &size, digest.data, digest.size) > 0 && size == 256,
            "RSA signature failed");
    for (size_t i = 0; i < size; i++) record[i] = signature[size - 1 - i];
    require(bnctx && power && rr && radix && neg &&
            EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_RSA_N, &n) &&
            BN_one(power) && BN_lshift(power, power, 4352) &&
            BN_mod(rr, power, n, bnctx) && BN_bn2lebinpad(rr, record + 256, 256) == 256 &&
            BN_one(radix) && BN_lshift(radix, radix, 64) &&
            BN_mod(neg, n, radix, bnctx) && BN_sub(neg, radix, neg), "RSA format failed");
    inverse = BN_mod_inverse(NULL, neg, radix, bnctx);
    /* The vendor stores signature/RR little endian, but emits np_inv64 as
     * an unreversed hexadecimal string: its final eight bytes are big endian.
     */
    require(inverse && BN_bn2binpad(inverse, record + 512, 8) == 8, "RSA inverse failed");
    write_blob(output, record, sizeof(record));
    BN_free(inverse); BN_free(n); BN_free(power); BN_free(rr); BN_free(radix); BN_free(neg);
    BN_CTX_free(bnctx); EVP_PKEY_CTX_free(ctx); EVP_PKEY_free(key); free(digest.data);
}
int main(int argc, char **argv)
{
    if (argc == 5 && !strcmp(argv[1], "pad")) pad(argv[2], argv[3], argv[4]);
    else if (argc == 5 && !strcmp(argv[1], "sign")) sign(argv[2], argv[3], argv[4]);
    else if (argc == 7 && !strcmp(argv[1], "obfuscate")) {
        struct blob module = read_blob(argv[2]), sck = read_blob(argv[3]);
        struct blob vendor = read_blob(argv[4]), data = read_blob(argv[5]);
        unsigned char key[16], *output = calloc(1, data.size + 16);
        require(module.size == 16 && sck.size == 16 && vendor.size == 16 && output,
                "mode 97 requires three 16-byte inputs");
        obfuscation_key(module.data, sck.data, vendor.data, key);
        aes(1, key, data.data, data.size, output);
        write_blob(argv[6], output, data.size);
        free(module.data); free(sck.data); free(vendor.data); free(data.data); free(output);
    } else {
        fprintf(stderr, "Usage: spi-tools pad INPUT PADDING DIGEST | sign KEY DIGEST OUT | "
                        "obfuscate MODULE SCK VENDOR INPUT OUT\n");
        return 2;
    }
    return 0;
}
