# Spec: Python GIL Bypass via Splinter Shared Memory

## Objective

Implement a working Python 3 demonstration of GIL-free inter-process
communication using Splinter's shared memory substrate via ctypes.

The artifact should be immediately useful to a Python developer who wants
to understand why shared memory IPC avoids both the GIL and GC pressure,
and how to use Splinter to do it.

## Reference Material

- `reference/splinter.h` — public API and all constants
- `reference/splinter.c` — implementation (for context only, do not modify)
- `libsplinter.so` — pre-built shared object in the project root

## What To Build

Produce a single well-commented file: `demo/gil_bypass.py`

It should demonstrate three things in order:

### 1. Basic IPC (two functions, one bus)

A writer function and a reader function that communicate through a Splinter
slot without going through Python objects, the GIL, or the GC heap.

The writer sets a key to a numeric value using splinter_set().
The reader retrieves it zero-copy using splinter_get_raw_ptr() and casts
the result directly to a ctypes float or uint64 — no Python int conversion,
no memcpy.

Show the epoch before and after the write to demonstrate the seqlock
discipline. Verify consistency by checking epoch matches before and after
the raw pointer read.

### 2. Threaded writer, zero-copy reader

Spawn a standard Python threading.Thread as the writer.
The reader runs on the main thread.
The writer updates a shared slot in a tight loop for 1 second.
The reader samples it via raw pointer without acquiring the GIL.

The point: the writer thread holds the GIL for Python bookkeeping,
but the reader never needs it — it's reading directly from mmap'd memory
that the C library manages. Show the read count vs write count at the end
to illustrate the throughput asymmetry.

### 3. Subprocess IPC (the actual GIL bypass)

Spawn a subprocess (using multiprocessing or subprocess) as the writer.
The writer process opens the same Splinter bus by name and writes
incrementing values in a loop.
The main process reads them zero-copy via raw pointer.

This is the real demonstration: two separate Python interpreters,
two separate GILs, one shared memory region. Neither process is
blocked by the other's GIL. Show wall-clock throughput for both sides.

## Constraints

- Use only the Python 3 standard library. No pip, no third-party packages
  of any kind — not numpy, not cffi, not multiprocess, nothing outside
  of what ships with a standard Python 3 installation.
- ctypes is the only FFI mechanism permitted.
- The FFI boundary setup should be in a small SplinterFFI helper class
  at the top of the file so a reader can see exactly what's being bound
  and why.
- Every splinter_get_raw_ptr() call must validate the epoch before and
  after the read. Show this discipline explicitly even if it means a
  small retry loop.
- The bus name is read from the environment variable SPLINTER_AGENT_BUS.
  Fall back to "demo_bus.spl" if unset.
- Clean up (splinter_close) in a finally block or atexit handler.
- Do not use splinter_get() for the zero-copy demonstrations.
  The whole point is raw pointer access.
- multiprocessing and threading are both standard library and permitted.
  subprocess is permitted. Nothing else for concurrency.

## Signal The Bus

Use the provided scripts throughout:

- `scripts/spl-signal task_start` before beginning
- `scripts/spl-signal task_advance` after each of the three sections compiles and runs
- `scripts/spl-signal task_finish` when the artifact is complete
- `scripts/spl-signal error` if you hit something unexpected and have to recover
- `scripts/spl-journal` to record any surprising FFI behavior or ctypes
  gotchas you encounter — these will be embedded and searchable

## Definition Of Done

- `demo/gil_bypass.py` runs cleanly from the project root with no arguments
- All three demonstrations produce visible output with throughput numbers
- The seqlock epoch discipline is demonstrated and visible in the output
- The file is self-contained and readable as a tutorial by someone who
  has never seen Splinter before
- No Python exceptions escape to the top level