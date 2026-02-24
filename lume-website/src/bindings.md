---
title: Splinter Bindings For Other Languages
date: "2026-02-20"
author: Tim Post
draft: false
metas:
  lang: en
  description: >-
    Learn how to integrate Splinter with Python, Rust, Java, TypeScript, and Lua using Foreign Function Interfaces (FFI) for zero-copy shared memory access.
  keywords:
    - Foreign Function Interface
    - Language Bindings
    - Zero-Copy Access
    - Python ctypes
    - Java Panama FFM
  robots: true
  generator: true
---
# Splinter Bindings For Other Languages

Splinter is a very "`dlopen()`-friendly" library; it doesn't require complex
aggregation or construction, it doesn't implement callbacks and it doesn't rely
on type-punning. Because of this, it tends to work without heroics on modern
runtimes that can read Linux DSOs.

If you benefit from Splinter commercially, please consider a small donation to 
the author to help fund independent research that drives Splinter's development 
and unrelenting standards for integration. If you could cover a book, or some
lab supplies, hosting or just some pizza - I'm always grateful!

<div style="margin: 1em auto">
  <a
    href="https://www.buymeacoffee.com/timthepost"
    id="donateLink"
    target="_blank"
  >
    <img
      src="https://cdn.buymeacoffee.com/buttons/v2/default-blue.png"
      alt="Buy Splinter's Author A Coffee"
      title="Splinter Development Requires Coffee"
      style="height: 60px !important; width: 217px !important"
    >
  </a>
</div>

## A Note On "Far" Pointers

Looking at a region of memory through FFI is one point where that old
_information superhighway_ cliche actually bears meaning. When you can travel
between borders freely, without a checkpoint, there's no barrier to speed.

If you can drive very fast but have to slow down just a little for your license
tag to be captured, or for a RFID fob to be read, then it's a slight choke
point.

If you have to completely stop, show your paperwork, declare everything in your
possession and what you intend to do with it, and then get back on your way,
it's a blocking clog.

Foreign function interfaces each treat "unsafe" pointers in their own way. Those
that essentially let data flow to -> from them freely do exceptionally well with
splinter. Those that don't still achieve massive speedup, but anywhere the
client has to wait for a list of things (keys, embeddings, modules) will be
slower than "single-shot" I/O.

**Now, the supported languages:**

### TypeScript (Deno FFI / Bun FFI)

There's an included class in `bindings/ts/splinter.ts` that you can import and
just start using (Bun or Deno, it uses a factory to provide the correct
bindings).

You will likely see better performance with Bun's FFI for things like listing
keys and tandem reads (due to pointer diligence), otherwise they're pretty much
equal.

Here's an example:

```ts
import { Splinter } from "./splinter.ts";

// open the bus
const store = Splinter.connect("nomic_text");

// write a value
store.set("ts_key", "Hello from TypeScript!");

// read a value
const val = store.getString("ts_key");
console.log(`Value: ${val}`);

// check an epoch (seqlock verification)
const epoch = store.getEpoch("ts_key");
console.log(`Current Epoch: ${epoch}`);

// not absolutely necessary unless you want to
// open another store
store.close();
```

The class contains the basics to get you started. If you want more of the
library exposed in the class, just upload the class itself, `splinter.c` and
`splinter.h` to any decent code LLM and tell it what additional methods you want
to include.

The bare minimum is just sort of how we do things in the main distribution.

### Rust (Identical To C Build)

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

### Python3x (Native Ctypes)

Splinter abhors unnecessary data duplication. It's actually a really good
GIL-bypass for vector data. Because Splinter uses `mmap()` to act as a passive
substrate, Python can use its built-in `ctypes` library to cast a Splinter slot
directly into a `NumPy` array or a native Python memory view:

```python
import ctypes
import os

# load the shared library
lib_path = os.path.abspath("./libsplinter.so")
splinter = ctypes.CDLL(lib_path)

# define argument and return types for the FFI boundary
splinter.splinter_open.argtypes = [ctypes.c_char_p]
splinter.splinter_open.restype = ctypes.c_int

splinter.splinter_set.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t]
splinter.splinter_set.restype = ctypes.c_int

# splinter_get_raw_ptr(const char *key, size_t *out_sz, uint64_t *out_epoch)
splinter.splinter_get_raw_ptr.argtypes = [
    ctypes.c_char_p, 
    ctypes.POINTER(ctypes.c_size_t), 
    ctypes.POINTER(ctypes.c_uint64)
]
splinter.splinter_get_raw_ptr.restype = ctypes.c_void_p

class SplinterBus:
    def __init__(self, bus_name: str):
        if splinter.splinter_open(bus_name.encode('utf-8')) != 0:
            raise RuntimeError(f"Failed to connect to Splinter bus: {bus_name}")

    def set_string(self, key: str, value: str):
        data = value.encode('utf-8')
        splinter.splinter_set(key.encode('utf-8'), data, len(data))

    def get_embeddings_zero_copy(self, key: str, dimensions=768):
        """
        Reads 768 floats directly from the L3 cache without copying bytes!
        """
        out_sz = ctypes.c_size_t(0)
        out_epoch = ctypes.c_uint64(0)
        
        # get the raw memory pointer from the Splinter manifold
        raw_ptr = splinter.splinter_get_raw_ptr(
            key.encode('utf-8'), 
            ctypes.byref(out_sz), 
            ctypes.byref(out_epoch)
        )
        
        if not raw_ptr:
            return None
            
        # cast the raw memory address directly into a Python Float Array.
        # this is true zero-copy. If the C daemon updates the slot, 
        # this array updates instantly because it points to the same RAM.
        FloatArrayType = ctypes.c_float * dimensions
        return ctypes.cast(raw_ptr, ctypes.POINTER(FloatArrayType)).contents

# Usage:
# bus = SplinterBus("nomic_text")
# vec = bus.get_embeddings_zero_copy("npc_dialogue_vector")
# print(f"Vector Head: {vec[0]}, {vec[1]}, {vec[2]}")
```

In fact, slow Python scrapers can trickle data in while the very tiny
`splinference` Nomic Text inference engine (included) deposits vectors silently
(no `memcpy()` in the background either) as it chugs along.

Information Physics (from social data) is often done this way (GDELT, RSS, Etc)
and Splinter was written specifically to handle that.

### Java (Panama)

Because Splinter utilizes a static memory geometry mapped directly via the OS,
you can use Java's new MemorySegment and Arena classes to read and write directly to
Splinter:

```java
import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;

public class SplinterWire {
    private static final Linker linker = Linker.nativeLinker();
    private static final SymbolLookup lib = SymbolLookup.libraryLookup("./libsplinter.so", Arena.global());

    // Bindings to the C functions
    private static final MethodHandle splinterOpen = linker.downcallHandle(
        lib.find("splinter_open").get(),
        FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS)
    );

    private static final MethodHandle splinterGetRawPtr = linker.downcallHandle(
        lib.find("splinter_get_raw_ptr").get(),
        FunctionDescriptor.of(ValueLayout.ADDRESS, ValueLayout.ADDRESS, ValueLayout.ADDRESS, ValueLayout.ADDRESS)
    );

    public static void main(String[] args) throws Throwable {
        // open the bus (Off-heap string allocation)
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment busName = arena.allocateFrom("nomic_text");
            int rc = (int) splinterOpen.invokeExact(busName);
            if (rc != 0) {
                System.out.println("Failed to open Splinter bus.");
                return;
            }

            System.out.println("Connected to Splinter substrate!");

            // fetch the Raw Pointer (Zero-Copy)
            MemorySegment key = arena.allocateFrom("nomic_test_key");
            MemorySegment outSz = arena.allocate(ValueLayout.JAVA_LONG);
            MemorySegment outEpoch = arena.allocate(ValueLayout.JAVA_LONG);

            // This returns a MemorySegment pointing directly into /dev/shm
            MemorySegment rawDataPtr = (MemorySegment) splinterGetRawPtr.invokeExact(key, outSz, outEpoch);

            if (!rawDataPtr.equals(MemorySegment.NULL)) {
                long size = outSz.get(ValueLayout.JAVA_LONG, 0);
                long epoch = outEpoch.get(ValueLayout.JAVA_LONG, 0);

                // read the memory DIRECTLY from the C bus. 
                // the JVM Garbage Collector never sees this data, so it can never cause a lag spike.
                // we reinterpret the zero-length pointer to the actual size of the data.
                MemorySegment readableData = rawDataPtr.reinterpret(size);
                
                // if a string:
                String val = readableData.getString(0);
                System.out.println("Epoch " + epoch + " -> Read: " + val);
                
                // if a vector, then it would be:
                // float firstDimension = readableData.get(ValueLayout.JAVA_FLOAT, 0);
            }
        }
    }
}
```

Because the data lives in Splinter's static slots and not the JVM heap, you can
cache gigabytes of Rank-2 tensors without ever triggering a single Garbage
Collection pause.

Splinter also allows atomic INC, DEC, AND, and OR bitwise operations on BIGUINT
flags directly in shared memory, allowing thousands of Java threads to
coordinate state without relying on slow Java-level synchronized locks.

And, you still have the per-slot feature flags. It's made for IPC without tick
rate impacts, and we count inference as IPC.

### Lua

Just like you'd expect, mostly:

```lua
local result = bus.set("test_multi", multi) or -1
print(result)

bus.set("test_integer", 1)
bus.math("test_integer", "inc", 65535)
```

See `splinter_cli_cmd_lua.c` for the actual coupling and breakout box to add
more definitions and functionality.

If someone wants to expose the whole store as a table in Lua, I'd love a patch!

### Bash / Shell

Just use `splinterctl` or `splinterpctl` respectively. There's also
`splinter_cli` and `splinterp_cli` for interactive use.

See the [CLI](/cli) page for more information.
