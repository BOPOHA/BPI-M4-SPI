#!/usr/bin/bash
# Assemble prepared sources into an SRPM; do not compile on the host.
set -euo pipefail
if (( $# != 6 )); then
    echo "Usage: $0 RPKG_CONFIG SPEC WORK BUILD_ID PROFILE VERSION" >&2
    exit 2
fi
config=$1 spec=$2 work=$3 build_id=$4 profile=$5 version=$6
[[ $EUID != 0 ]] || { echo 'Run as your regular user.' >&2; exit 1; }
[[ $build_id =~ ^[0-9a-f]{12}$ ]]
[[ $profile == server || $profile == media ]]
printf '%s\n' "$build_id" > "$work/SOURCES/bpi-build-id"
printf '%s\n' "$profile" > "$work/SOURCES/bpi-build-profile"
# rpkg resolves local sources relative to the SPEC's directory.
install -m644 "$spec" "$work/SOURCES/${spec##*/}"
if [[ -n $version ]]; then
    spec_version=$(sed -n 's/^Version:[[:space:]]*//p' "$spec")
    if [[ $version != "$spec_version" ]]; then
        echo "Verified source version $version differs from SPEC version $spec_version" >&2
        exit 1
    fi
fi
rpkg --config-file "$config" srpm --target aarch64 \
    --spec "$work/SOURCES/${spec##*/}" --outdir "$work/srpm-result" \
    >"$work/srpm.log" 2>&1 || {
    tail -n 60 "$work/srpm.log" >&2
    exit 1
}
shopt -s nullglob
srpms=("$work/srpm-result/"*.src.rpm)
(( ${#srpms[@]} == 1 )) || { echo 'rpkg must produce exactly one SRPM.' >&2; exit 1; }
