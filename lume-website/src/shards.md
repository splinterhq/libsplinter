# Splinter Logic Shards

By design, Splinter does no calculation or transformation of any kind during
atomic I/O. Once acquiring a sequence lock, Splinter's mission becomes to write
as quickly as possible so it doesn't stand on the sequence lock. It wants to
minimize the chance of another operation being told to try again.

Any data transformation or extrapolation is expected to run only _after_ the
primary irreplaceable data has been published in the store, normally triggered
_by_ the data being published in the store.

Shards are a way to "shard" logic into separate loosely-coupled routines.

## A Practical Shard Example: Simple "Sidecar" Vectors

In a Splinter workflow, we don't compute _and_ write synchronously, ever. If
your workflow is "grab this text, run embeddings, save text + embeddings" then
it actually becomes three atomic operations for Splinter:

1. Receive the text; pulse virtual watch file descriptor to awaken any watchers.

2. Your shard sees text has just been published to a slot type-named `VARTEXT`,
   grabs a guided pointer to the value and embedding regions, runs a slim
   Llama/Nomic inference loop, writes values directly to the addresses where
   they will remain unless deleted. No `memcpy()` or duplication of data. No
   sockets.

If you use tandem slots (let's say you're tracking the velocity, acceleration
and jerk of the same base key), then those vectors get quietly deposited next,
or even simultaneously if you run threaded inference.

There will be an example shard that does this loading Nomic Text V2 from a
`.gguf` file in the next release.

## How Lua Helps

If you manage your splinter key space so that `___` means a hidden, reserved
thing (similar to underscore precedence in software), you can name keys like:

- `___on_read`: Lua code stored in the value region of this key should be
  executed every time a key in this store is read.

- `___on_write`: Lua code stored in the value region of this key should be
  executed every time a key in this store is written.

- `___on_write/REGEX/`: Lua code stored in the value region of this key should
  be executed every time a write operation happens in this store where the key
  matches `/REGEX/`.

This is of course on the honor system and up for the shard to adhere to (the
store itself is passive and incapable of enforcement), but it's one way you can
achieve the same effect as Lua meta tables themselves. This is also a great
trigger system. See the `splinter_cli_cmd_lua.c` for what's exposed / how and
some idea of how to use it.

## How Shards Are Structured

Shards are just like any other dynamically-loadable module you'd see in C. They
have entry points, shutdown points, metadata and checks.

TODO: Show shard structure once done.

## Why Shards Set Memory Expectations and Force Accounting

Splinter's CLI provides some abstractions to coordinate necessary work where
lots of dynamic allocation may be required for processing, or for advising the
kernel when recently used memory is no longer needed. Splinter doesn't use
`mlock()` anywhere in the code (it could be easily added, if the goal was to
make splinter "appliances" where no system processes ran), but barring that
specialized use Splinter just leaves it up to the kernel.

Because of this, it's _extremely_ helpful to coordinate. We don't bother the
kernel unless we need to, correctly, but "I'm not using this huge amount of RAM
you reserved for me anymore" is polite in a process that never really calls
"free()" as a matter of global routine.

This is why shards have reconciliation macros (TODO, implement fully). As your
code flows, you set assertions for how you believe memory will be used. When
done, the accountant checks /proc/self to see if your prediction matched use,
and advises the kernel further if not. We do this so it doesn't add an
accounting burden to your logic.

---
