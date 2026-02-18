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

---
