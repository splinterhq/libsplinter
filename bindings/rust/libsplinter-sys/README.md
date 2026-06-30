# libsplinter-sys

Low-level (`-sys`) FFI bindings to [libsplinter](https://github.com/splinterhq/libsplinter),
the shared-memory splinter core (in-memory variant).

This crate exposes the raw C ABI exactly as declared in `splinter.h`. The
bindings are generated with [bindgen](https://crates.io/crates/bindgen) at build
time, and the vendored C sources are compiled from source with the
[cc](https://crates.io/crates/cc) crate — so **no system installation of
libsplinter is required**.

For the on-disk persistent variant, see
[`libsplinter-persist-sys`](https://crates.io/crates/libsplinter-persist-sys).

## Usage

```toml
[dependencies]
libsplinter-sys = "1"
```

```rust
// Everything in the C header is exposed under the crate root.
use libsplinter_sys::*;
```

## Build requirements

A C compiler and `libclang` (for bindgen) must be available at build time. The
crate links the native library statically; nothing needs to be installed on the
target system at runtime.

## Conventions

Per the [`-sys` crate convention](https://doc.rust-lang.org/cargo/reference/build-scripts.html#-sys-packages),
this crate only declares the native interface (`links = "splinter"`) and contains
no higher-level abstractions. Safe, idiomatic wrappers belong in a separate
crate built on top of this one.

## License

Apache-2.0, matching the upstream libsplinter project.
