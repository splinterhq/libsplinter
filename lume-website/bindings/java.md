---
title: Java
parent: "Language Bindings (FFI)"
nav_order: 2
metas:
  lang: en
  description: >-
    Splinter externalizes KV storage to shared memory, eliminating problems with GC activity causing latency spikes in shared regions. Splinter lets you bring Inference
    to Minecraft Modding.
  keywords:
    - Vector Anti-Database
    - Shared-Memory Substrate
    - Lock-Free IPC
    - LLM Semantic Memory
    - Inter-Process Communication
  robots: true
  generator: true
---

# Java (Panama)

Because Splinter utilizes a static memory geometry mapped directly via the OS,
you can use Java's new MemorySegment and Arena classes to read and write
directly to Splinter:

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
