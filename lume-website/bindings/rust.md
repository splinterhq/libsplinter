---
title: Rust
parent: "Language Bindings (FFI)"
nav_order: 1
metas:
  lang: en
  description: >-
    Splinter is a minimalist, lock-free key-value manifold designed for high-frequency data ingestion and retrieval across disjointed runtimes using IPC.
  keywords:
    - Vector Anti-Database
    - Shared-Memory Substrate
    - Lock-Free IPC
    - LLM Semantic Memory
    - Inter-Process Communication
  robots: true
  generator: true
---

# Rust (Identical To C Build)

Rust is (aside from C/C++) absolutely the easiest, most straight-forward and
efficient way to use Splinter:

```rust
use std::ffi::{CString, CStr};
use std::slice;
use std::str;

// the build/ directory has the crates
// they need a maintainer
// wink wink
use splinter_memory::*; 

fn main() {
    // Rust strings must be converted to C-compatible null-terminated strings
    let bus_name = CString::new("nomic_text").expect("CString::new failed");
    let key_name = CString::new("rust_test_key").expect("CString::new failed");
    
    // the test manifold payload
    let payload = b"Hello from Rust on the bare metal!";

    // any interaction with C FFI requires an unsafe block
    unsafe {
        // 1. Connect to the Splinter bus
        if splinter_open(bus_name.as_ptr()) != 0 {
            panic!("Failed to connect to Splinter bus. Is it initialized?");
        }
        println!("Connected to Splinter substrate!");

        // publish
        // we cast the Rust byte array pointer to a C void pointer
        let rc = splinter_set(
            key_name.as_ptr(), 
            payload.as_ptr() as *const std::ffi::c_void, 
            payload.len()
        );
        
        if rc != 0 {
            eprintln!("Failed to write to Splinter slot.");
        }

        // the zero-copy read
        let mut out_sz: usize = 0;
        let mut out_epoch: u64 = 0;

        let raw_ptr = splinter_get_raw_ptr(
            key_name.as_ptr(),
            &mut out_sz,
            &mut out_epoch
        );

        if !raw_ptr.is_null() {
            // cast the C pointer directly into a Rust slice.
            // the Rust compiler now treats the L3 cache memory as a standard 
            // byte slice, tracking its lifetime safely. 
            // NO memcpy()!
            let data_slice = slice::from_raw_parts(raw_ptr as *const u8, out_sz);
            
            // convert the raw bytes back to a UTF-8 string for printing
            match str::from_utf8(data_slice) {
                Ok(msg) => println!("Epoch {} -> Read: {}", out_epoch, msg),
                Err(e) => eprintln!("Invalid UTF-8 sequence: {}", e),
            }
        } else {
            println!("Key not found or empty.");
        }

        // politely disconnect
        splinter_close();
    }
}
```

Rust is not Splinter's primary dev's primary language, but something like that
wrapped in a struct with `sync` and `send` could work? A better example would be
appreciated.
