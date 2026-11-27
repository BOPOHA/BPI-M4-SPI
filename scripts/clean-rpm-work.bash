#!/usr/bin/bash
# Remove user-owned RPM work, including read-only extracted directories.
set -euo pipefail

for path in "$@"; do
    # A trailing slash must not turn a symlink into its target directory.
    while [[ $path == */ && $path != / ]]; do path=${path%/}; done
    case "$path" in
        ''|/|.|..) echo "Refusing to clean path: $path" >&2; exit 2;;
    esac
    if [[ -L $path || ! -d $path ]]; then
        rm -f -- "$path"
        continue
    fi
    [[ $path == /* ]] || path=./$path
    # Apply permissions before descending; batched -exec would be too late
    # for unreadable children. -P keeps linked directories untouched.
    chmod u+rwx -- "$path"
    find -P "$path" -type d -exec chmod u+rwx -- {} \;
    rm -rf -- "$path"
done
