#!/bin/sh
# Sync the canonical C sources into each Rust -sys crate's csrc/ directory.
#
# The published -sys crates vendor (build from source) and must therefore carry
# a self-contained copy of the C sources inside the crate. The repository root
# remains the single source of truth; run this whenever splinter.c/.h (or the
# config/build headers) change so the vendored copies do not drift.
#
# Usage: scripts/sync-rust-vendor.sh
set -eu

root="$(cd "$(dirname "$0")/.." && pwd)"
files="splinter.c splinter.h config.h build.h"
crates="bindings/rust/libsplinter-sys bindings/rust/libsplinter-persist-sys"

for crate in $crates; do
    dest="$root/$crate/csrc"
    mkdir -p "$dest"
    for f in $files; do
        cp "$root/$f" "$dest/$f"
    done
    echo "synced -> $crate/csrc"
done
