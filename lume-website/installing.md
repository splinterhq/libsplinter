---
title: "Building & Installing"
parent: "Splinter Overview"
nav_order: 1
metas:
  lang: en
  description: >-
    Building and installing libsplinter, a persistable in-memory KV and Vector store that ingests data lock-free at L3 speeds.
  keywords:
    - Cycles Per Operation
    - IPC Performance
    - Lock-Free Benchmarks
    - Socket Tax
    - Hardware Optimization
  robots: true
  generator: true
---

# Building & Installing Splinter

Go to the source root and type `make`, you'll see the following menu:

```bash
------------------------------------
| Type 'make' + target_name to use |
------------------------------------
sense     | "dev" + Valgrind
dev       | "prod" + Llama + Rust
prod      | Numa + Lua + Embeddings
mini      | Embeddings only
tiny      | No Embeddings (KV Only)
clean     | Clean build artifacts
distclean | Purge repo
install   | Install Splinter
uninstall | Uninstall Splinter

You can also run CMake manually, for example, if you wanted a 
stripped-down build + Rust bindings for it. Edit this Makefile to see how.

If unsure where to start, try 'make mini' - it requires no dependencies and
enables many features.
```

**Most users will just want to type `make prod`**, if you have the Lua and Numa
development packages installed on your system. Otherwise, try `make mini` to
build with just the self-contained features like embeddings / cli / etc.

See the `Makefile` for how CMake options work.

## Manual Build Configuration (CMake)

You can also run CMake manually:

```bash
git clone git@github.com:splinterhq/libsplinter.git

cd libsplinter
mkdir build
cd build

cmake -D{your flags} ..
ctest --output-on-failure
sudo -E make install
```

## Note on `sudo -E` With Rust During `make install`:

If building the Rust bindings, make sure you preserve the user environment
(`-E`) when using `sudo` for `make install`.

Otherwise, Rust's tools might not be able to resolve locations and the build will fail.
