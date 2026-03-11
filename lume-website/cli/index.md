---
title: Command Line Interfaces
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

# Splinter's `splinter_cli` and `splinterctl` Programs

> A persistent version of these programs exists as "splinterp_cli" 
and "splinterpctl", respectively. { .note }

The `splinter_cli` program is a REPL-like environment in which you
can explore and control Splinter stores. You can do things like list,
set and update keys, run Lua scripts, manage bloom labels, couple tandem
keys, and other common tasks.

It features built-in tab completion and hinting, online help for all
commands and consistency in behavior wherever possible. It serves not
just as a way to implement functionality, but also to illustrate how 
the library can be used to build "just enough" storage for any project
that uses KV and vectors over the same physical network.

## Command Help Vs Tool Help

There are commands `splinter_cli` understands how to run, which are:

```sh
 Module     | Description                                                 
-----------------------------------------------------------------
 clear      | Clears the screen.                                          
 cls        | Alias of 'clear'                                            
 config     | Access Splinter bus and slot metadata.                      
 get        | Retrieve the value of a key in the store.                   
 head       | Retrieve just the metadata of a key in the store.           
 help       | Help with commands and features.                            
 hist       | View and clear command history.                             
 list       | List keys in the current store.                             
 set        | Set a key in the store to a specified value.                
 unset      | Unset a key in the store (deletes the key).                 
 use        | Opens a Splinter store by name or path.                     
 watch      | Observes a key for changes and prints updated contents.     
 init       | Initialize Splinter stores.                                 
 export     | Export Splinter data in various formats.                    
 type       | Display or set the named slot type for a key.               
 math       | Perform incr/decr and bitwise ops on named biguint keys.    
 label      | Label a key with bits for its bloom filter                  
 lua        | Run a lua script                                            
 orders     | Manage standard vector orders of a key
```

There's also a way in which `splinterctl` / `splinter_cli` should _be_
run, which you can get by passing `--help` to either:

```sh
Usage:  splinterctl [options] [arguments] *or*
        splinterctl --no-repl <built in command> [arguments] *or*
        splinterctl {no args for REPL}
splinterctl provides a command line interface for Splinter bus interaction.
Where [options] are:
  --help / -h                  Show this help display.
  --history-file / -H <path>   Set the CLI history file to <path>
  --history-len / -l  <len>    Set the CLI history length to <len>
  --list-modules / -L          List available commands.
  --no-repl / -n               Don't enter interactive mode.
  --prefix / -p <prefix>       Prepend <prefix> to read/write ops (namespace)
  --use / -u <store>           Connect to <store> after starting.
  --version / -v               Print splinter version information and exit.

splinterctl will look for SPLINTER_HISTORY_FILE and SPLINTER_HISTORY_LEN in the
environment and use them. However, argument values will always take precedence.

If invoked as "splinterctl", splinterctl automatically turns on --no-repl.

Prefixes (via --prefix) can contain any printing character but '='. This uses
the SPLINTER_NS_PREFIX environmental variable.
```

## Environmental Variables

Both tools look for the following environmental variables:

 - `SPLINTER_NS_PREFIX` - prepended to every I/O operation (allows auto namespacing)
 - `SPLINTER_HISTORY_FILE` - Default is `~/.splinter_history`
 - `SPLINTER_DEFAULT_STORE` - CLI will load this store if set and valid
 - `HOME` - Home directory (system setting)
 - `TERM` - Terminal Slug (e.g. `xterm-256color` - system setting)

## Files

`${HOME}/.splinterrc` is an (optional) list of ctags-style labels before
hex or binary codes that they represent. For instance:

```sh
urgent-label 0x1
debug-label 0x4
```

63 labels can be used (64 are possible, but the 64th is reserved
for proximity)

## `splinterctl` Special Invocation

`splinterctl` is a symbolic link to `splinter_cli` that causes the CLI
to skip REPL setup and some other terminal handling so that you can 
specify commands right on the command line.

### Example - `splinter_cli` vs `splinterctl`:

This is an example of doing the same thing in the interactive REPL-like
console (`splinter_cli`) vs just using `splinterctl` for one-off commands.

```sh
splinter_cli
splinter_cli version 1.1.6 build 857b1f1
To quit, press ctrl-c or ctrl-d.
no-conn # init testing_store
Initializing store: testing_store
  - Slots: 1024 (4096 bytes each, 64 byte alignment)
  - Value Arena: 4194304 bytes, SRS: 7475328 bytes (~7.13 MB)
no-conn # use testing_store
use: now connected to testing_store
testing_store # list
Key Name                          | Epoch      | Val Len    | Named Type     
-------------------------------------------------------------------------------------

testing_store # set test_key "This is a test value."
testing_store # get test_key
21:This is a test value.

testing_store # 
```

And using `splinterctl` with just `zsh`. Note, here we make use of
the shell's `alias` functionality so that we don't have to keep
adding `--use testing_store` each time:

```sh
╭─tinkertim at localhost in ~/code 26-03-11 - 12:41:52
╰─○ splinterctl init testing_store
Initializing store: testing_store
  - Slots: 1024 (4096 bytes each, 64 byte alignment)
  - Value Arena: 4194304 bytes, SRS: 7475328 bytes (~7.13 MB)
╭─tinkertim at localhost in ~/code 26-03-11 - 12:42:03
╰─○ alias splinterctl="splinterctl --use testing_store"
╭─tinkertim at localhost in ~/code 26-03-11 - 12:42:14
╰─○ splinterctl list
Key Name                          | Epoch      | Val Len    | Named Type     
-------------------------------------------------------------------------------------

╭─tinkertim at localhost in ~/code 26-03-11 - 12:42:17
╰─○ splinterctl set test_key "This is a test value from zsh."
╭─tinkertim at localhost in ~/code 26-03-11 - 12:42:34
╰─○ splinterctl get test_key
30:This is a test value from zsh.

╭─tinkertim at localhost in ~/code 26-03-11 - 12:42:42
╰─○ splinterctl list         
Key Name                          | Epoch      | Val Len    | Named Type     
-------------------------------------------------------------------------------------
test_key                          | 2          | 30         | SPL_SLOT_TYPE_VOID

```
