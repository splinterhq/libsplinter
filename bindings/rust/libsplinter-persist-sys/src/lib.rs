//! Native FFI bindings to [libsplinter](https://github.com/splinterhq/libsplinter),
//! persistent variant (`splinter_p`, built with `SPLINTER_PERSISTENT`).
//!
//! This is a `-sys` crate: it exposes the raw, auto-generated C ABI and nothing
//! more. The vendored C sources are compiled from source by the build script,
//! so no system installation of libsplinter is required. Bindings are generated
//! with bindgen at build time and written to `OUT_DIR`.
#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals)]
#![allow(dead_code)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
