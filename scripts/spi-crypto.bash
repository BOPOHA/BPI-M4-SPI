#!/usr/bin/bash
# Only the AES-ECB invocation used by the vendor obfuscator reference tests.
# Translate to current OpenSSL; never load the old RtSSL binary/runtime.
set -Eeuo pipefail
[[ $# == 9 && $1 == enc && ($2 == -e || $2 == -d) &&
   $3 == -aes_128_ecb && $4 == -k && $6 == -i && $8 == -o ]] || {
    echo 'Unsupported SPI reference crypto invocation.' >&2; exit 2;
}
exec openssl enc -aes-128-ecb "$2" -K "$5" -nosalt -nopad -in "$7" -out "$9"
