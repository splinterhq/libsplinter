//! Build script for `libsplinter-persist-sys`.
//!
//! Identical to `libsplinter-sys` except the vendored C source is compiled with
//! `-DSPLINTER_PERSISTENT`, producing the persistent variant of the library
//! (`splinter_p`). The crate is self-contained: no system install is required.

use std::env;
use std::path::PathBuf;

const CSRC: &str = "csrc";

fn main() {
    // Compile the vendored C library as a static archive, persistent variant.
    // cc emits `cargo:rustc-link-lib=static=splinter_p` for us.
    cc::Build::new()
        .file(format!("{CSRC}/splinter.c"))
        .include(CSRC)
        .define("SPLINTER_PERSISTENT", None)
        .warnings(false)
        .compile("splinter_p");

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
