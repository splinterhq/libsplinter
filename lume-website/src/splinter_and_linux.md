# Why Linux, x86_64 and Splinter Form a Symphony

Splinter is designed with **Mechanical Sympathy** for the Linux kernel. By
rejecting the "arrogance" of heavy abstractions, Splinter works with the grain
of the OS rather than against it.

### 1. The Symbiosis of `mmap()` and the Page Cache

Instead of implementing a custom buffer manager (which often fights the kernel),
Splinter uses `mmap()` to map the store directly into the process address space.

- **Zero-Copy Efficiency**: When a process reads a Splinter slot, it is reading
  directly from the kernel's Page Cache. No data is copied from kernel-space to
  user-space.
- **Intelligent Eviction**: By not locking memory, you allow the Linux VMM to
  decide which parts of the Splinter manifold should stay in RAM based on actual
  system pressure. If a slot isn't accessed, the kernel can swap it out
  gracefully without Splinter ever needing to "know."

### 2. Static Geometry & Huge Pages

Modern Linux kernels use **Transparent Huge Pages (THP)** to reduce the overhead
of memory mapping.

- **Predictable Layout**: Because Splinter uses a fixed-geometry arena, the
  kernel can easily identify large, contiguous blocks of memory.
- **TLB Optimization**: The CPU’s Translation Lookaside Buffer (TLB) can map
  millions of Splinter operations using just a few "Huge" entries, significantly
  reducing the "address translation tax" during high-frequency ingestion.

### 3. 64-Byte Alignment: The L1 Cache Sweet Spot

The Linux scheduler and the CPU hardware move data in **64-byte cache lines**.

- **Atomic Harmony**: Splinter aligns every slot and its 64-bit epoch to these
  boundaries. This prevents "Split Locks" and False Sharing, allowing multiple
  CPU cores to strike the bus simultaneously without bruising each other's
  cache.

### 4. The "Hybrid Mop" as an I/O Governor

The Linux kernel's write-back threads (`pdflush`/`kswapd`) love predictable
patterns.

- **Write Combining**: When **Hybrid Scrub** zeroes out the 64-byte tail of a
  write, it signals the CPU and the kernel that a full cache line is "dirty" and
  ready for a burst write.
- **Pacing Persistence**: This minor "work" prevents the writer from
  overwhelming the kernel's dirty-page limit, leading to the **6x throughput
  increase** observed in persistent benchmarks.

### 5. Deterministic Reliability

Splinter doesn't use complex "journaling" or "WAL" (Write-Ahead Logging) that
adds I/O wait.

- **Persistence via Physics**: Because Splinter is a simple memory-mapped file,
  a "save" is just the kernel's standard way of flushing a dirty page to disk.
- **Unix Tool Synergy**: You don't need a proprietary backup tool. Standard
  Linux utilities like `dd`, `cp`, and `btrfs send` work natively with Splinter
  because, to Linux, it's just a file.

### Conclusion

Splinter is a "Good Citizen" because it provides **Informed Intent**. It gives
the kernel a clean, aligned, and static structure, then lets the VMM do what it
has been optimized to do for 30 years: move pages efficiently.
