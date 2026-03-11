---
title: Language Bindings (FFI)
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

# Splinter Works Across Many Language Runtimes!

Splinter is a very "`dlopen()`-friendly" library; it doesn't require complex
aggregation or construction, it doesn't implement callbacks and it doesn't rely
on type-punning. Because of this, it tends to work without heroics on modern
runtimes that can read Linux DSOs.

If you benefit from Splinter commercially, please consider a small donation to
the author to help fund independent research that drives Splinter's development
and unrelenting standards for integration. If you could cover a book, or some
lab supplies, hosting or just some pizza - I'm always grateful!

<div style="margin: 1em auto">
  <a
    href="https://www.buymeacoffee.com/timthepost"
    id="donateLink"
    target="_blank"
  >
    <img
      src="https://cdn.buymeacoffee.com/buttons/v2/default-blue.png"
      alt="Buy Splinter's Author A Coffee"
      title="Splinter Development Requires Coffee"
      style="height: 60px !important; width: 217px !important"
    >
  </a>
</div>

## A Note On "Far" Pointers

Looking at a region of memory through FFI is one point where that old
_information superhighway_ cliche actually bears meaning. When you can travel
between borders freely, without a checkpoint, there's no barrier to speed.

If you can drive very fast but have to slow down just a little for your license
tag to be captured, or for a RFID fob to be read, then it's a slight choke
point.

If you have to completely stop, show your paperwork, declare everything in your
possession and what you intend to do with it, and then get back on your way,
it's a blocking clog.

Foreign function interfaces each treat "unsafe" pointers in their own way. Those
that essentially let data flow to -> from them freely do exceptionally well with
splinter. Those that don't still achieve massive speedup, but anywhere the
client has to wait for a list of things (keys, embeddings, modules) will be
slower than "single-shot" I/O.
