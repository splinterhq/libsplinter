---
title: Python 3
parent: "Language Bindings (FFI)"
nav_order: 2
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

# Python3x (Native Ctypes)

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
