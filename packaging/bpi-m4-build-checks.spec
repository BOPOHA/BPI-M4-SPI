%global debug_package %{nil}
Name: bpi-m4-spi-checks
Version: 1
Source99: bpi-build-id
%global bpi_recipe %(cat %{SOURCE99})
Release: 1.%{bpi_recipe}.fc45
Summary: Build regression checks for the BPI-M4 SPI tools
License: GPL-2.0-or-later
BuildArch: noarch
Source0: bpi-m4-spi-checks.tar.gz
BuildRequires: gcc
BuildRequires: make
BuildRequires: git-core
BuildRequires: patch
BuildRequires: python3
BuildRequires: python3-pyyaml
BuildRequires: rpm
BuildRequires: jq
BuildRequires: gzip
BuildRequires: util-linux-core

%description
Runs the Python regression suite and compiled controller harnesses inside Mock.
This package records successful checks and is not installed into SD images.

%prep
%setup -q -n bpi-m4-spi-checks

%build

%install
install -Dpm644 licenses/GPL-2.0.txt "%{buildroot}/usr/share/licenses/%{name}/COPYING"
mkdir -p "%{buildroot}/usr/share/doc/%{name}"

%check
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -v
printf 'Regression suite passed in Mock.\n' > "%{buildroot}/usr/share/doc/%{name}/checks.txt"

%files
%license /usr/share/licenses/%{name}
%doc /usr/share/doc/%{name}
