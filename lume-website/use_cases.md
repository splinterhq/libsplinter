---
title: "Splinter Use Cases"
parent: "Splinter Overview"
nav_order: 1
metas:
  lang: en
  description: >-
    Use cases for the libsplinter atomic persistable KV & vector store, from demanding
    LLM workflows to simple cache handling.
  keywords:
    - Cycles Per Operation
    - IPC Performance
    - Lock-Free Benchmarks
    - Socket Tax
    - Hardware Optimization
  robots: true
  generator: true
---

# Great Uses For Splinter

There are many uses ranging from LLM runtime operations to high resolution
physics experiments. Splinter tends to get used a lot like "gaffer tape" once
you know how it works.

Here are some of the most obvious / common uses:

<!-- TOC -->

## Basic Knowledge Graphing

While limited to a total of 64 relations _as-is_, Splinter's bloom tagging and
filtering help store understandings of relationships between different data.

### Exploiting the "Signal Arena" for Graph Traversal

The most powerful aspect of Splinter for this use case is the
`splinter_pulse_watchers()` and `signal_groups` logic. In a traditional graph,
finding related nodes requires "traversing" edges. In Splinter, you can
implement Reactive Graph Traversal:

- The Scenario: Agent A (a Sales Bot) detects a "Pricing Dispute" in a chat log.

- The Action: It pulses the signal_group associated with the bloom_mask for
  "Urgent_Escalation."

- The Result: Agent B (a Manager Bot), which is "watching" that specific bitmask
  via splinter_watch_label_register, is instantly woken up via the Signal Arena.

This turns the Bloom filter from a passive filter into a real-time event router
based on the graph's state.

It's limited also because there's no real way to write information at the edges,
but it's also less than 1k lines of code.

### "Agentic Labeling" and Semantic Overlap

Suppose you let agents modify the bloom, but not the key or value region
(reasonable).

That opens up a kind of fuzzy matching; If multiple agents use the same hashing
strategy for their labels, Splinter’s Bloom filter naturally handles Semantic
Overlap:

- If Agent A labels a node with 0x01 (Finance) and Agent B labels it with 0x02
  (Legal), the slot's mask becomes 0x03.

- An LLM agent looking for "Financial-Legal" overlaps can query the mask 0x03.

- Because Bloom filters can have false positives (but never false negatives),
  the agent might get a few "extra" hits, but it will never miss a valid
  connection.

For a Knowledge Graph, this acts as a "Fuzzy Discovery" mechanism. It's not
perfect but `<gestures at code size again>` you get a lot for a little.

## LLM Runtime / RAG / LLM Training Uses

Large Language Models are brilliant, but they suffer from severe amnesia. The
current industry solution to this is to bolt on a traditional Vector Database,
which forces the LLM to pause its thinking, serialize a JSON request, send it
over a network socket, and wait for a database to reply.

Splinter allows you to build a **Semantic Hippocampus**: a shared memory space
where the LLM’s short-term context and long-term memories live in the exact same
physical RAM, accessible instantly without a single `memcpy()`.

To understand how to use it, you don't need a PhD in linear algebra. You just
need to know your toolkit:

- **Cosine Similarity is your Steam Shovel:** It grabs massive, general scoops
  of memory that are semantically pointing in the same direction.
- **Euclidean Distance is your Sifter:** It shakes out the exact literal matches
  from that massive bucket of context.
- **Feature Flags are your Post-It Notes:** You can slap a 64-bit integer onto
  any memory slot to instantly track metadata (e.g.,
  `flag_user_frustrated = 1`).
- **Bloom Filters are your C-Style Tags:** They allow you to instantly filter
  the entire memory bus for specific concepts without scanning every individual
  slot.

So, what can you actually _build_ with those tools?

### 1. A Hallucination Governor (Preventing Narrative Lysis)

Because Splinter operates at L3 cache speeds, it can sit _inside_ the LLM's
auto-regressive generation loop. This lets observers use read-only
**pointer-level** access to watch what's happening in real-time and:

- Prompt the model back into alignment based on token pattern fingerprints (a
  module for token-level threat detection is in development right now)
- Halt generation
- Escalate conversation distrust
- Trigger costlier string-level inspections

Logging uncertainty at inference and emitting special `<UNC_explanation>` tokens
lets you watch for these events _very_ specifically. A second inference engine
will ship end-of-March 2026 with more code to show how this can work.

### 2. True Zero-Latency RAG (Retrieval-Augmented Generation)

In traditional RAG, embedding vectors are serialized and dragged across the
kernel boundary. With Splinter, the inference engine (like `llama.cpp`) simply
casts a raw C-pointer to the L3 cache. When a user asks a 70B parameter model a
question, the context injection happens instantly. You are feeding the AI its
own memories at the speed of the hardware bus.

### 3. User/Conversational Memory

Splinter provides auditable memory and is especially good at agentic
coordination thanks to raw pointer sharing. Many models can work on a single
context window along with their own independent scratch pads, all in the same
store.

You can also use Splinter to store embeddings-enriched chat transcripts, for
easy semantic recollection. Combined with bloom tags, it's a powerful base
memory substrate.

## High-Resolution Data Recording

One second of time might represent:

- One second of audio
- One second of video
- One second of 20 sensors, each with three orders (velocity, acceleration,
  jitter)
- One second of one or more multi-strata tensors

... simultaneously. Traditional relational databases don't always lend well to
fidelity in timing. Splinter is designed to capture data as fast as possible,
with lots of helpers to backfill what can be derived later.

This lets you capture vectors of motion with the motion as fast as the L3 cache
of your hardware will let you. It's great for field monitoring of all kinds, and
due to static geometry, much kinder to flash-based storage.

## Configuration & Packaging

Slots are great places to package binaries, documentation, or whatever else you
need persistently, and then re-use the store for configuration once you unpack.

You can also just use it for a configuration database, or a place to control
customer feature flags, etc.

## Serverless Semantic KV

Splinter's persistent stores work wonderfully well on DenoKV; use persistent
mode and bloom with pre-filled embeddings to help serve "psychic" ads at the
edge, or whatever you need to serve.

If it allows access to `mmap()`, Splinter will work on it.

## Just A Great Cache Implementation

You can build Splinter without embeddings or any other advanced storage options
and use it solely as a socket-less cache server that supports atomic integer
operations and easy persistence. You implement your own eviction strategy (if
you need one), with whatever governance you choose.
