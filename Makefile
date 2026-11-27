.DEFAULT_GOAL := help
export PATH := $(CURDIR)/scripts/rpm-tools:$(PATH)
export PYTHONDONTWRITEBYTECODE := 1
JOBS ?= 8
SPI_DRAM_GIB ?= 2
.PHONY: help spi srpm-spi test checks clean clean-hard
help:
	@echo 'make spi / srpm-spi / test; no OS/image build or flashing'
spi:
	./scripts/build-spi.bash build $(JOBS) $(SPI_DRAM_GIB)
srpm-spi:
	./scripts/build-spi.bash srpm $(JOBS) $(SPI_DRAM_GIB)
test:
	python3 -m unittest discover -s tests -v
checks:
	./scripts/build-checks.bash $(JOBS)
clean:
	./scripts/clean-rpm-work.bash work/rpms out/spi out/srpms
clean-hard:
	git clean -dffX
