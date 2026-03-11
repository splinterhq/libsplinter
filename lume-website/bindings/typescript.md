---
title: TypeScript
parent: "Language Bindings (FFI)"
nav_order: 3
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

# TypeScript (Deno FFI / Bun FFI)

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
