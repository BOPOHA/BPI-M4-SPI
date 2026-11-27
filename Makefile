.DEFAULT_GOAL := help
export PATH := $(CURDIR)/scripts/rpm-tools:$(PATH)
export PYTHONDONTWRITEBYTECODE := 1
JOBS ?= 8
SPI_DRAM_GIB ?= 2
.PHONY: help spi srpm-spi spi-vendor srpm-spi-vendor test checks clean clean-hard
help:
	@echo 'make spi / srpm-spi / spi-vendor / test; no OS/image build or flashing'
spi:
	./scripts/build-spi.bash build $(JOBS) $(SPI_DRAM_GIB)
srpm-spi:
	./scripts/build-spi.bash srpm $(JOBS) $(SPI_DRAM_GIB)
spi-vendor:
	./scripts/build-spi-vendor.bash build $(JOBS) $(SPI_DRAM_GIB)
srpm-spi-vendor:
	./scripts/build-spi-vendor.bash srpm $(JOBS) $(SPI_DRAM_GIB)
test:
	python3 -m unittest discover -s tests -v
checks:
	./scripts/build-checks.bash $(JOBS)
clean:
	./scripts/clean-rpm-work.bash work/rpms out/spi out/srpms out/spi-vendor
clean-hard:
	git clean -dffX
