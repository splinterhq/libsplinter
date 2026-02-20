use std::fs;
use std::path::Path;

fn main() {
    let header_path = "../../../splinter.h";
    let lib_path = "../../../";

    println!("cargo:rustc-link-search=native={}", lib_path);
    println!("cargo:rustc-link-lib=dylib=splinter_p");

    // Ensure output dir exists
    let out_dir = Path::new("src");
    if !out_dir.exists() {
        fs::create_dir_all(out_dir).unwrap();
    }

    let bindings = bindgen::Builder::default()
        .header(header_path)
        .generate()
        .expect("Unable to generate bindings");

    bindings
        .write_to_file("src/bindings.rs")
        .expect("Couldn't write bindings!");
}
