# BPI-M4-SPI

Standalone pinned U-Boot and vendor DDR/FSBL build, integrity checks and operator update evidence.

Host: Fedora 45; operator installs dependencies `mock mock-core-configs rpkg rpm-build git jq bzip2 gzip cpio python3 gcc` manually. Mock runs
only through `/usr/bin/mock` as the regular user. No sudo/authentication is
started by build scripts. No sibling checkout or submodule is required.

Run `make help` for build/test targets. `make clean` removes this project's
products; `make clean-hard` deletes all ignored content, including cache/work/out.
Preserve useful artifacts outside the checkout first.

See [boot contract](docs/BOOT-CONTRACT.md) and [source provenance](docs/PROVENANCE.md).
The standalone loader passed build and packaging checks. See
[validation](docs/VALIDATION.md). Rebuilt candidates require new hardware
qualification; firmware redistribution rights remain unresolved.

Licensing: extracted code retains GPL/LGPL and imported notices. Selected new
standalone files use [As-Is Noncommercial terms](licenses/As-Is-Noncommercial.txt).
See [license scope](LICENSE.md).
