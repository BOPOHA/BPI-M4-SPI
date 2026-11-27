# BPI-M4-SPI

Standalone pinned U-Boot and vendor DDR/FSBL build, integrity checks and operator update evidence.

Host: Fedora 45; operator installs dependencies `mock mock-core-configs rpkg rpm-build git jq bzip2 gzip cpio python3 gcc` manually. Mock runs
only through `/usr/bin/mock` as the regular user. No sudo/authentication is
started by build scripts. No sibling checkout or submodule is required.

Run `make help` for build/test targets. `make clean` removes this project's
products; `make clean-hard` deletes all ignored content, including cache/work/out.
Preserve useful artifacts outside the checkout first.

See [boot contract](docs/BOOT-CONTRACT.md), [extraction](docs/EXTRACTION.md),
[approved plan](docs/REPOSITORY-SPLIT.md) and [provenance](docs/PROVENANCE.md).
Native COPR publication and new hardware qualification are pending; no migrated
artifact inherits hardware qualification automatically.

Licensing: extracted code retains GPL/LGPL and imported notices. Selected new
standalone files use [As-Is Noncommercial terms](licenses/As-Is-Noncommercial.txt).
See [license scope](LICENSE.md).

See [Python tooling review](docs/PYTHON-TOOLING.md) for retained build/test roles
and removed dependencies.
