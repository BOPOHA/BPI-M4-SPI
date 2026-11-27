Python tooling review, 2026-11-27

The checks SPEC no longer requires python3-pyyaml: the four Python regression
tests use only standard-library modules. The Make checks target is explicitly
phony. Firmware source preparation, packing and integrity checks remain Bash/C.

Python test harnesses compile controller C code with simulated registers and
faults, then run those executables. Retaining these harnesses preserves checks
for DMA activity, failed writes, completion faults, clock handoffs and generic
SD selection. Python is used for build/test tooling, not firmware execution.

The U-Boot SPEC's python3/python3-setuptools build dependencies are retained.
They belong to the upstream build environment and are separate from the removed
PyYAML dependency in our checks package.

Validation: make checks passed all four tests inside regular-user Mock with the
reduced declared dependencies. Local make test could not run because the host
has no C compiler. Firmware was not rebuilt, uploaded or flashed for this
build/test dependency cleanup.
