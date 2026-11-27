#!/usr/bin/bash
# Byte comparisons against pinned vendor algorithms. Runs inside Mock only.
set -Eeuo pipefail
export LC_ALL=C
tools=$(realpath "$1") vendor=$(realpath "$2") crypto=$(realpath "$3")
test_dir=$(mktemp -d)
trap 'rm -rf -- "$test_dir"' EXIT
cd "$test_dir"
# Rebuild the vendor SHA/padding reference with the current host compiler.
gcc -std=gnu11 -O2 "$vendor/tools/do_sha256/"{do_sha256,sha256}.c -o sha-reference
for size in 0 1 15 16 55 56 63 64 65 127 128 4096; do
    head -c "$size" /dev/zero >input
    ./sha-reference input reference-pad reference-digest >sha.log
    "$tools" pad input padding digest
    cmp padding reference-pad
    cmp digest reference-digest
done
# The vendor obfuscator depends only on current glibc. It is a host reference,
# not an updater, and is never used to produce the shipped candidate.
for size in 16 64 96 4096; do
    head -c "$size" /dev/zero >input
    for vector in 0 1; do
        if (( vector == 0 )); then
            cp "$vendor/image/aes_module_id.bin" module
            cp "$vendor/image/aes_sck_flash.bin" sck
            cp "$vendor/image/aes_vendor_id.bin" vendor-id
        else
            printf '0123456789abcdef' >module
            printf 'fedcba9876543210' >sck
            printf 'rt1395-test-data' >vendor-id
        fi
        "$vendor/tools/obfuse/obfuse_h" "$crypto" 97 module module sck vendor-id \
            input reference-encrypted >obfuscation.log
        ! grep -Eq 'failed|error|does not exist' obfuscation.log
        "$tools" obfuscate module sck vendor-id input encrypted
        cmp encrypted reference-encrypted
    done
done
# Reference the vendor's own bigint/RR/inverse conversion, with current
# OpenSSL pkeyutl replacing the removed/deprecated rsautl command frontend.
mkdir reference-bin
cat >reference-bin/openssl <<'SH'
#!/usr/bin/bash
set -Eeuo pipefail
if [[ $1 == rsautl && $2 == -encrypt && $3 == -inkey &&
      $5 == -raw && $6 == -in && $# == 7 ]]; then
    exec /usr/bin/openssl pkeyutl -encrypt -inkey "$4" \
        -pkeyopt rsa_padding_mode:none -in "$7"
fi
exec /usr/bin/openssl "$@"
SH
chmod +x reference-bin/openssl
export PATH="$PWD/reference-bin:$PATH"
sed '/^use Switch/d' "$vendor/tools/script/gen_rsa_pattern64_0001.pl" >rsa-reference.pl
for name in rsa_key_2048.pem rsa_key_2048.fw.pem rsa_key_2048.tee.pem; do
    key="$vendor/image/$name"
    /usr/bin/openssl rsa -text -in "$key" -out key.text 2>rsa.log
    # Explicitly sign raw SHA256 bytes with PKCS#1 v1.5, as the vendor does.
    /usr/bin/openssl pkeyutl -sign -inkey "$key" -in digest \
        -pkeyopt rsa_padding_mode:pkcs1 -out signature
    perl rsa-reference.pl --key key.text --msg signature --binary >>rsa.log 2>&1
    cat msg.bin RRModN.bin np_inv32.bin >reference-signature
    "$tools" sign "$key" digest signed
    cmp signed reference-signature
done
printf 'SPI tools: 12 SHA/padding, 8 obfuscation, 3 RSA reference comparisons passed.\n'
