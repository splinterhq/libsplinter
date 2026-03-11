---
title: "Splinter Use Cases"
parent: "Splinter Overview"
nav_order: 1
---

# Great Uses For Splinter

There are many uses ranging from LLM runtime operations to high resolution
physics experiments. Splinter tends to get used a lot like "gaffer tape" once
you know how it works.

Here are some of the most obvious / common uses:

## LLM Runtime / RAG / Training Uses

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

### 1. The Hallucination Governor (Preventing Narrative Lysis)

Because Splinter operates at L3 cache speeds, it can sit _inside_ the LLM's
auto-regressive generation loop. Using your Steam Shovel and Sifter, you can
constantly measure the "Tension" of the text the LLM is generating. If the LLM
starts speaking highly confidently about a subject where it has zero factual
grounding in its memory slots, a Splinter watcher detects that "Vacuum State."
It can physically halt the LLM mid-sentence before a hallucination escapes into
the user's terminal.

Logging uncertainty at inference and emitting special `<UNC_explanation>` tokens
lets you watch for these events _very_ specifically. Custom information physics
engines can provide even greater accuracy, such as those that Splinter is being
used to research.

### 2. True Zero-Latency RAG (Retrieval-Augmented Generation)

In traditional RAG, embedding vectors are serialized and dragged across the
kernel boundary. With Splinter, the inference engine (like `llama.cpp`) simply
casts a raw C-pointer to the L3 cache. When a user asks a 70B parameter model a
question, the context injection happens instantly. You are feeding the AI its
own memories at the speed of the hardware bus.

### 3. User-space Conversational Memory

Splinter gives you a cheap semantic key->value store for personalizations that
would work blazingly fast persistently, too. If ctags style bloom isn't enough,
a 1024x1024 store just for their needs works perfectly.
