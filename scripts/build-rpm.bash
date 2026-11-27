#!/usr/bin/bash
# Rebuild a complete SRPM; all compilation and package checks happen in Mock.
set -euo pipefail
if (( $# != 5 )); then
    echo "Usage: $0 CONFIG SRPM WORK UNIQUEEXT JOBS" >&2
    exit 2
fi
config=$1 srpm=$2 work=$3 unique=$4 jobs=$5
[[ $EUID != 0 ]] || { echo 'Run as your regular user.' >&2; exit 1; }
[[ $jobs =~ ^[0-9]+$ ]] && (( jobs >= 1 && jobs <= 64 ))
/usr/bin/mock -r "$config" --uniqueext "$unique" \
    --define "_smp_build_ncpus $jobs" --rebuild "$srpm" \
    --resultdir "$work/result" --cleanup-after >"$work/mock.log" 2>&1 || {
    tail -n 60 "$work/mock.log" >&2
    exit 1
}
