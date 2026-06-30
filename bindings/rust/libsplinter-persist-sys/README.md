# libsplinter-persist-sys

Low-level (`-sys`) FFI bindings to the **persistent** variant of
[libsplinter](https://github.com/splinterhq/libsplinter) (`splinter_p`, built
with `SPLINTER_PERSISTENT`).

This crate exposes the raw C ABI exactly as declared in `splinter.h`. The
bindings are generated with [bindgen](https://crates.io/crates/bindgen) at build
time, and the vendored C sources are compiled from source with the
[cc](https://crates.io/crates/cc) crate using `-DSPLINTER_PERSISTENT` — so **no
system installation of libsplinter is required**.

For the in-memory variant, see
[`libsplinter-sys`](https://crates.io/crates/libsplinter-sys).

## Usage

```toml
[dependencies]
libsplinter-persist-sys = "1"
```

```rust
use libsplinter_persist_sys::*;
```

## Build requirements

A C compiler and `libclang` (for bindgen) must be available at build time. The
crate links the native library statically; nothing needs to be installed on the
target system at runtime.

## Conventions

Per the [`-sys` crate convention](https://doc.rust-lang.org/cargo/reference/build-scripts.html#-sys-packages),
this crate only declares the native interface (`links = "splinter_p"`) and
contains no higher-level abstractions. Safe, idiomatic wrappers belong in a
separate crate built on top of this one.

## License

Apache-2.0, matching the upstream libsplinter project.
