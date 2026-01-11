# Splinter 🐀

**Splinter** is a minimalist, high-speed message bus and subscribable KV store
that operates in shared memory (`memfd`) or uses memory-mapped files for
persistent backing.

Designed to support AI ephemeral memory workflows and IPC, Splinter synchronizes
state between inference, retrieval, UI, and coordination layers through
convenient but guarded shared memory regions. Rust, TypeScript and even Bash
scripts can all communicate using the same memory region, with a simple and
convenient interface.

But, Splinter isn't limited to inference, training and generation workflows; it
can power advanced IPC and messaging systems at extreme scales, or just act as a
lightweight, subscribe-able and reliable KV store. Splinter is ideal for backing
inter-process or even inter-service communication through RDMA.

Rust and TypeScript (Deno) bindings are included, and thoughtful design means
other languages are easy to support as well through dynamic import. Splinter is
designed to be used in code, but ships with a production-quality CLI for
debugging and administration.

Inspired by the wisdom and discretion of _[Master Splinter][4]_, this bus never
speaks unless spoken to, but is always ready.

---

## Key Features

- 🧠 Shared memory layout: low-overhead, mmap-based store
- 📥 `set`, `unset`, `get`, `list`, and `poll` operations
- 🔑 Lock-free atomic ops, seqlock for contention, EAGAIN for non-block i/o)
- 🧹 Auto-vacuuming for hygienic memory mode; toggle on/off instantly.
- 🧵 Thread-safe single-writer, multi-reader semantics.
- ✨ 100% Valgrind clean! Well-tested and easy to integrate.
- 🕰️ Built-in version tracking via atomic epoch counters
- 🔧 Configurable slot count and max value size
- 💾 Optional persistent mode via file-backed `mmap`
- 🦕 Deno, Bun and Rust bindings included!
- ⚙️ Embeddable and extendable; easy to build upon!
- 🔎 Epoch-based semantics provides performant watches & easy pub/sub.

Splinter is _entirely self-contained_; it has no external dependencies other
than the GNU C library. The CLI uses
[line noise](https://github.com/antirez/linenoise) to manage history, completion
and suggestions, but it ships in the repo with Splinter.

## System Requirements:

GNU/Linux hosting GCC 12.1 with glibc 2.35 or newer. Any fairly-recent
distribution should work just fine; the author uses Debian. Windows users should
use WSL.

### MacOS Isn't Supported (For Now)

Work would be needed to "spoof" `memfd()` on MacOS with some sketchy/racey
`unlink()` magic; if you want to attempt and maintain a port, I'm happy to have
it! But, I have no plans to attempt it (nor a Mac to attempt it with). Some
poly/backfill for glibc extensions would also be needed (not much, off hand).

### Prerequisites:

You'll need `build-essential`, or the equivalent of it for your distribution of
choice. It installs the compiler, development headers, etc. No additional
libraries are needed for a regular build.

### Testing & Profiling:

For testing, [Valgrind][7] is used to make sure no memory access violations
occur, even under extreme circumstances. You can install it using your distro's
package manager, along with the development headers which Splinter can use in
its test harnesses for tighter integration. It's not required, but highly
recommended, especially if you intend to make major modifications to the
library.

## Quick Orientation For Beginners via CLI:

After downloading the code, run the following commands:

```bash
make
sudo make install
splinterctl init demo
alias splinterctl="splinterctl --use demo"
splinterctl list
splinterctl set foo "hello, world"
splinterctl get foo
splinterctl head foo
splinterctl unset foo
splinterctl list
```

These commands also work if you just use `splinter_cli` interactively.

You can now explore [the code docs][5] to understand the C api, or check out the
counter part [TypeScript bindings][6]. Note, not all hosts provide access to
`memfd()` in their security layer, so you may need to use persistent mode for
TypeScript projects.

## How Does Splinter Compare With Other Stores?

It's important to first re-state: Splinter isn’t a database — it’s a
**shared-memory exchange** that can persist.

It’s designed for the **multi-reader, single-writer (MRSW)** case where speed
matters more than schema, flow is anticipated, there just needs to be a bridge
for information to flow through. Splinter was created to provide _just enough_
safety for things like `llama.cpp`, `Deno`, `Rust` and `btrfs` to be able to
share memory together and synchronize work.

It was created when the author was thinking

> "_I need something like `/proc` but that I can do in user space, preferably
> without mounting anything new. Something like XenStore but no hypervisor ..._"

Essentially, Splinter provides _auditable_, _consistent_ and _safe_ access to
shared memory, across languages and entire workflows, with no other opinions
imposed.

And, because it was developed specifically _**for LLM workloads**_, like:

- Combining and feeding output from sanitation workflows
- Providing short-term, manageable, auditable and persistent memory capabilities
  to serverless MCP servers (see [Tieto][3] for a companion project)
- Providing real-time auditable and replay-able feedback mechanisms for NLP
  training in assistive-use scenarios,
- Ephemeral caching where mutations need to trigger or happen as part of other
  reactions.

Splinter is designed to (by default) not ever allow old information to leak into
new, which is something very innovative for a store that (by primary design)
operates in shared memory without persistence. But: _**not everyone needs "hyper
scale LLM levels" of paranoia in their engineering**_, so Splinter gives you a
choice:

- **Sterile mode (`auto_vacuum=on`)** — every write zeroes old contents before
  reuse. Splinter uses static geometry, so there's no row reclamation needed.
  This is perfect for LLM scratchpads and training contexts where stale data
  must never leak back. It's like boiling a hotel room every time a new guest
  arrives (if only that were possible!!!). No contamination, but it takes time
  to write twice.

- **Throughput mode (`auto_vacuum=off`)** — skips scrubbing for maximum raw
  speed. Ideal for message buses, ephemeral caches, or event streams where
  under-reading will not happen (or matter). Reading past the atomic advertised
  value length (up to the max value length) could result in fetching random
  stale data. This isn't possible through normal use of the library, but could
  happen if you've got synchronization issues going on in your code.

Splinter was written because nothing else was lean enough with the very specific
set of desired features present and verifiable. The choice was to eviscerate and
contort SQLite, or just write Splinter. The latter made so much more sense. It's
the only user-space C library that the author knows of that combines:

- Lock-free reads (seqlock snapshots)
- Easy TTL or LRU eviction for clients
- Single-writer atomicity without global mutexes
- Optional "sterile memory" mode for training-safe hygiene which can be toggled
  at will / whim instantly without structural concern under normal use
- Lightweight enough to double as a pub/sub message bus or IPC backing
- No external dependencies by default / drop-in two files to use the C API,
  or use dynamic linking
- Global MRSW contract with disjointed-slot MRMW safety; splinter assumes you know
  what you're doing.

It's a _systems workbench_ as much as it is a library.

Sharing address space between languages (and entire workflows) is easier when
you have something like splinter managing the guard rails you need to do it
dynamically. Just figure out your key space naming and go!

## Programs Included:

Splinter ships with (in addition to the shared and static objects) several
programs to help illustrate how to use the C version of the library, as well as
a comprehensive CLI for managing splinter stores in debugging / production
monitoring.

### `splinter_cli` (and how it behaves when invoked as `splinterctl`):

*Note: For persistent mode, use `splinterpctl` and `splinterp_cli`, respectively.*

`splinter_cli` offers a variety of commands to create, query, watch and update splinter
stores in an interactive way with type completion, hinting, history and on-line help.

`splinterctl` is a special invocation of `splinter_cli` that bypasses all of the
interactive stuff and just does what is asked.

It's expected that you'll develop automation workflows, work on debugging and generally
just _explore_ what Splinter can do using the interactive CLI. `splinterctl` is there
for you when you're ready to script stuff, or just need one-off functionality.

#### Typical `splinter_cli` Interactive Use:

```cli
splinter_cli version 0.9.0 build 4e09f5e
To quit, press ctrl-c or ctrl-d.
no-conn # init splinter_test
Creating 'splinter_test' with default geometry.
no-conn # use splinter_test
use: now connected to splinter_test
splinter_test # set test_one "Hello, world!"
splinter_test # set test_two "How are you?"
splinter_test # list
Key Name                          | Epoch           | Value Length   
------------------------------------------------------------------
test_two                          | 2               | 12             
test_one                          | 2               | 13                         

splinter_test # watch test_two --oneshot
... update from another terminal ...
13:Fine, thanks!

splinter_test # export
{
  "store": {
    "total_slots": 1024,
    "active_keys": 2
  },
  "keys": [
    {
      "key": "test_two",
      "epoch": 4,
      "value_length": 13
    },
    {
      "key": "test_one",
      "epoch": 2,
      "value_length": 13
    }
  ]
}

splinter_test # 
```

Splinter's CLI is designed just as much to show you how to embrace the library in
code as it is to produce a functional tool. Splinter itself ***is for storage only***;
that's what chiefly makes it not-a-database. The CLI adds opnionated sugar and 
other functionality as needed (as well as shards, which are loadable modules, coming
soon!).

### Namespaces

You can pass `--prefix=namespace::identifier` via the command line, or set 
`SPLINTER_NS_PREFIX` in the environment. This is not enforced in the library, 
but rather a convenience offered by the CLI: your custom string is just 
pre-pended to I/O operations in a way that you don't have to always type
the namespace:

```txt
user@system # splinter_cli --prefix demo::namespace::
splinter_cli version 0.9.0 build 4e09f5e
To quit, press ctrl-c or ctrl-d.
no-conn # init test
Creating 'test' with default geometry.
no-conn # use test
use: now connected to test
test # list
Key Name                          | Epoch           | Value Length   
------------------------------------------------------------------

test # set foo bar
test # list
Key Name                          | Epoch           | Value Length   
------------------------------------------------------------------
demo::namespace::foo              | 2               | 3              

test # get foo
3:bar

test # 
```
However, remember - this is a per-session setting, so each time you
exit the CLI, it resets. If you want to make it automatic, set the
`SPLINTER_NS_PREFIX` environmental variable.

### TypeScript Bindings

In the `bindings/ts/` directory you'll find FFI bindings for Deno and Bun. There
are two shared files (the class, and types), and then `deno/` and `bun/` have the
platform-specific FFI bindings & symbols as well as unit tests for each platform
respectively.

Splinter works perfectly fine on Deno Deploy using persistent mode;
[here is a demo][8] that includes code running on Deploy now that you can 
grab and begin using now. I'm not sure what hosts are using Bun, but Splinter works
very well on it.

The _**primary driver**_ for this project was a ned to connect TypeScript (Deno)
with Inference (in my case, llama.cpp) in a way that was close to bare-metal
`select() / poll()` style latency. Sockets were the very first thing to get
thrown out the window, and, well, here we are.

There's always special 3> for Deno (the author used to work there), and tight 
Bun integration means it's available to the yarn/npm world too, with Bun.

I don't use / can't support Node, but if you'd like to maintain bindings for
it, I'm happy to see a PR `:)`

### Rust Bindings

Both persistent and in-memory versions are built for rust (see `bindings/rust` for the
workgroup setup). I would really appreciate help maintaining proper crates; if you'd
like to help out with it and know Rust well enough, please reach out to me via email
at `timthepost@protonmail.com` or open an issue. 

## Building

```bash
make        # builds both memory and persistent modes
make tests
```

Prompts for additional tests will be shown after the build.

This builds:

- `libsplinter.so` : default memory-backed bus
- `libsplinter_p.so` : persistent file-backed variant
- `libsplinter.a` : static version of the default bus
- `libsplinter_p.a` : static version of the persistent bus

## Making Stores Persist (Persistence Mode) 

If you want bus persistence, use the persistent CLI tool or link your
application (or set dlopen()) to `libsplinter_p.so` instead of `libsplinter.so`.

If you're just building `splinter.c` and `splinter.h` into your application
(which is very encouraged), set `-DSPLINTER_PERSISTENT` when compiling.

The persistent version:

- Maps from a file instead of RAM (`open()` + `mmap()` instead of `memfd()` or
  `shm_open()`.
- Survives system reboots (does not require any external checking for integrity)
  
- Can be snapshot/dumped with `dd`, `cp` (if you can manage not writing to it
  while that happens)

However, understand the limitations of the underlying file system and whatever
media is backing it. NVMe storage will be quite fast, rotating media will be
much slower.

The MRSW stress tool will give you some indication of throughput, however you
should scale it down thread-wise as well as payload or you'll just thrash the
storage and accomplish little else.

## Network Synchronization

Network synchronization is currently not _officially_ supported and probably
will not be unless the project picks up equipment donations or people willing to
give access to machines with high-bandwidth cards in them for testing and
development.

If you'd like to help in this way, please email me directly at
`timthepost@protonmail.com` with libsplinter somewhere in the subject line.

That said, you have some options you can try right now, and I'll do my best to
help if I can:

### If You're Working With A Funded Startup Budget:

For **high-bandwidth** network solutions like 25G RoCE, RDMA becomes very
practical (albeit latency goes from a few nanoseconds to maybe 10ms). If that's
not a problem, then no need to even use the full 200GB that things like the
NVIDIA/Mellanox Cards go up to. You can also use the persistent mode on
NVMe/RDMA setups. This won't be as fast as "native", and can still get racey if
latency is inconsistent.

### If You're Working With A Small Budget:

You have options! They just require a little more time on your part.

#### NFS (Best Bet)

Your easiest route is going to be to use persistent mode and NFS.

You can have a setup where a network server hosts the stores, and all compute
nodes mount via NFS. They all mmap the same region, so while it's a little
slower, it's definitely sound and will work probably without issue.

Use small block sizes on the client side to lower latency and think about the
size of your writes / reads, something like:

```bash
mount -t nfs -o rsize=8192,wsize=8192,timeo=14,intr,vers=3 \
  server:/export/splinter /mnt/splinter
```

You may have to play with / tune this, but it should work reliably.

#### Make A UDP Bridge

You can broadcast key updates over UDP and have nodes write them upon receiving
the packets. Hey, it's UDP - it is what it is. But it's an option and on a
decent local network it should be fine.

I'd love a PR with whatever you come up with.

#### Make A Deno / Oak -> REST Gateway

Use the Deno FFI bindings + Oak to make a very simple REST interface. I'll
include a sample one at some point, but any LLM can roll this for you in a few
seconds with the included FFI bindings and TS class for Splinter.

### Next Sprint Major Goals:

See the `CHANGELOG` file for plans for the next few versions, as well as changes already
implemented (all managed in one place for convenience).

[1]: https://github.com/HelenOS/helenos/tree/master/uspace/app/bdsh
[2]: https://helenos.ogr
[3]: https://github.com/timthepost/tieto
[4]: https://en.wikipedia.org/wiki/Splinter_(Teenage_Mutant_Ninja_Turtles)
[5]: https://github.com/timthepost/libsplinter/tree/main/docs
[6]: https://github.com/timthepost/libsplinter/tree/main/bindings/ts
[7]: https://valgrind.org
[8]: https://github.com/timthepost/splinter-deploy
