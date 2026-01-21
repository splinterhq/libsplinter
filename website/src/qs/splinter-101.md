---
title: Splinter 101
layout: layouts/page.vto
metas:
  description: A quick introduction to Splinter, a practical lock-free atomic KV store and message bus for Linux/GNU systems. 
---

# Splinter 101 - What It Is & Isn't

Splinter is, at heart, an atomic lock-free KV store built to bring 
convenience to modern systems workflows such as:

 - Inter-process communication between interpreted languages, compiled languages,
   inference layers and the host operating system that resides entirely in shared
   memory.

 - Persistent, auditable short-term and / or working memory for large language 
   models (LLMs)

 - Timing and sharing advanced state between different parts of training workflows

 - Any application where pre-allocated slot geometry is critical (flash drive 
   storage) using persistent mode

 - Auditable and exportable configuration storage with 8 custom defined feature 
   flag bits per slot.

 - Building blocks for scripting automation through easy CLI features (including
   pub/sub for bash scripts, etc)

 - Embedded / on-device KV with console (appliance / IoT)

At <b>only 376 lines of code not counting comments</b> the core API is easily embedded in any 
C application by just adding <code>splinter.c</code> and <code>splinter.h</code> to the build.

## Opinions Reside In Modules; Truth In Storage


