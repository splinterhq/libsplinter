//! Build script for `libsplinter-sys`.
//!
//! Compiles the vendored splinter C source (in `csrc/`) into a static archive
//! and generates FFI bindings from the public header with bindgen. The crate is
//! fully self-contained: no system installation of libsplinter is required.

use std::env;
use std::path::PathBuf;

const CSRC: &str = "csrc";

fn main() {
    // Compile the vendored C library as a static archive. cc emits the
    // appropriate `cargo:rustc-link-lib=static=splinter` for us.
    cc::Build::new()
        .file(format!("{CSRC}/splinter.c"))
        .include(CSRC)
        .warnings(false)
        .compile("splinter");

    // Rebuild if any vendored source changes.
    for f in ["splinter.c", "splinter.h", "config.h", "build.h"] {
        println!("cargo:rerun-if-changed={CSRC}/{f}");
    }
    println!("cargo:rerun-if-changed=build.rs");

    // Generate bindings from the vendored public header into OUT_DIR so the
    // packaged crate never writes into its own source tree.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap()).join("bindings.rs");
    bindgen::Builder::default()
        .header(format!("{CSRC}/splinter.h"))
        .clang_arg(format!("-I{CSRC}"))
        .generate()
        .expect("unable to generate splinter bindings")
        .write_to_file(&out_path)
        .expect("couldn't write splinter bindings");
}
