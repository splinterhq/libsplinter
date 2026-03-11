---
title: Constants & Bits
parent: "Core Library"
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

# Constants & Placeholder Bits

Splinter has quite a few features, so there are quite a few constants
representing bitmask flags to activate them.

## Splinter's Magic Number

This is used to determine if the structure being read is a Splinter header very
quickly.

It also has some philosophical significance, bytes `53 4C 4E 54` convert to
ASCII `S L N T`. Splinter's original tagline was "_The bus that never speaks
unless spoken to_", owning to the noise-less design when idle:

```c
#define SPLINTER_MAGIC 0x534C4E54
```

## Global Config Constants:

The following are necessary, but don't bucket well into groups:

```c
/** @brief Version of the splinter data format (not the library version). */
#define SPLINTER_VER 2
/** @brief Maximum length of a key string, including null terminator. */
#define SPLINTER_KEY_MAX 64
/** @brief Nanoseconds per millisecond for time calculations. */
#define NS_PER_MS 1000000ULL
#ifdef SPLINTER_EMBEDDINGS
/** @brief The number of dimensions Splinter should support (OpenAI style is 768) */
#define SPLINTER_EMBED_DIM 768
#endif

/** @brief The maximum number of watch signal groups for a slot */
#define SPLINTER_MAX_GROUPS 64
```

Note that `SPLINTER_MAX_GROUPS` correlates to per-slot bloom length.

## Reserved Global (Store) Flags:

These directly correlate to `atomic_uint_least8_t core_flags` in
`struct splinter_header`:

```c
/** @brief Reserved store system flags */
#define SPL_SYS_AUTO_SCRUB     (1u << 0)
#define SPL_SYS_HYBRID_SCRUB   (1u << 1)
#define SPL_SYS_RESERVED_2     (1u << 2)
#define SPL_SYS_RESERVED_3     (1u << 3)
```

## User-Aliasable Global (Store) Flags

These can be used for whatever you want, and directly correlate to
`atomic_uint_least8_t user_flags` in `struct splinter_header`.

```c
/** @brief User store flags for aliasing */
#define SPL_SUSR1              (1u << 4)
#define SPL_SUSR2              (1u << 5)
#define SPL_SUSR3              (1u << 6)
#define SPL_SUSR4              (1u << 7)
```

## Named-type Slot Configuration Flags

These help declare what kind of data is expected to be in a slot:

```c
/** @brief Named type flags */
#define SPL_SLOT_TYPE_VOID     (1u << 0)
#define SPL_SLOT_TYPE_BIGINT   (1u << 1)
#define SPL_SLOT_TYPE_BIGUINT  (1u << 2)
#define SPL_SLOT_TYPE_JSON     (1u << 3)
#define SPL_SLOT_TYPE_BINARY   (1u << 4)
#define SPL_SLOT_TYPE_IMGDATA  (1u << 5)
#define SPL_SLOT_TYPE_AUDIO    (1u << 6)
#define SPL_SLOT_TYPE_VARTEXT  (1u << 7)
```

Upon transformation to BIGUINT, bit-depth is automatically expanded by the
library. The rest are just handy tags.

There is a default:

```c
#define SPL_SLOT_DEFAULT_TYPE SPL_SLOT_TYPE_VOID
```

## Per-Slot Feature Flags For User-Aliasing:

These correlate directly with each slot's `atomic_uint_least8_t user_flag` and
you can set your own constants to them (or just use the same bitmasks):

```c
/** @brief Per-slot user flags for aliasing */
#define SPL_FUSR1              (1u << 0)
#define SPL_FUSR2              (1u << 1)
#define SPL_FUSR3              (1u << 2)
#define SPL_FUSR4              (1u << 3)
#define SPL_FUSR5              (1u << 4)
#define SPL_FUSR6              (1u << 5)
#define SPL_FUSR7              (1u << 6)
#define SPL_FUSR8              (1u << 7)
```

## Temporal Operation Flags

When doing a time-op (with or without lag) the client library needs to know if
you want to set `ctime` OR `atime` (pick one for each call).

```c
/** @brief Modes for invoking slot timestamp updates */
#define SPL_TIME_CTIME         0
#define SPL_TIME_ATIME         1
```

If `MTIME` is ever added, this will become an enum.

## Scientific Order Separator

Splinter's tandem slots let you create, map and store vectors for multiple
orders of the same value or variable. Let's say you have a key named `car`, and
you needed to store semantic vectors about the car, but directional vectors
about its speed and acceleration.

You could create a 3-pair tandem key where these would be accessible as `car`,
`car.1` (velocity), and `car.2` (acceleration), or even `car.3` for jerk, with
each key being functional as independent, but paired together as a standard
order by the library.

Sometimes you have to pick something that will _not_ naturally appear in your
key space, which rules `.` out quickly. In this case any UTF-8 character like 💩
or even more esoteric works just fine. And it makes analysis a little less
boring `:)`

```c
#define SPL_ORDER_ACCESSOR "."
```
