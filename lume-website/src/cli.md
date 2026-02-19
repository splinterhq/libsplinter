# Splinter's CLI and 'splinterctl' Utilities

## "CLI" Means `splinter_cli` and `splinterctl`.

Splinter comes with an extensible production-grade CLI tool that lets you manage
and interact with stores by hand or through the use of simple shell scripts.
Splinter's CLI has robust features like:

- Command auto completion
- Built-in help
- Easy regex filtering
- Export options
- Access to almost all code possibilities
- Run Lua
- REPL or One-shot.

## `splinterctl` vs `splinter_cli`

The `splinter_cli` program slightly (and safely, with bounds checking) alters
its behavior if invoked as `splinterctl` so that interactive-mode (the REPL) is
not entered. This allows you to run any command you could in `splinter_cli`
manually with `splinterctl` through a script.

## Command Modules

The following modules (commands) are available in interactive as well as
non-interactive mode. To see them, run `splinterctl --list`

| Module | Description                                             |
| ------ | ------------------------------------------------------- |
| clear  | Clears the screen                                       |
| cls    | Alias of 'clear'                                        |
| config | Access Splinter bus and slot metadata                   |
| get    | Retrieve the value of a key in the store                |
| head   | Retrieve just the metadata of a key in the store        |
| help   | Help with commands and features                         |
| hist   | View and clear command history                          |
| list   | List keys in the current store                          |
| set    | Set a key in the store to a specified value             |
| unset  | Unset a key in the store (deletes the key)              |
| use    | Opens a Splinter store by name or path                  |
| watch  | Observes a key for changes and prints updated contents  |
| init   | Initialize Splinter stores                              |
| export | Export Splinter data in various formats                 |
| type   | Display or set the named slot type for a key            |
| math   | Perform incr/decr and bitwise ops on named biguint keys |
| label  | Label a key with bits for its bloom filter              |
| lua    | Run a lua script                                        |
| orders | Manage standard vector orders of a key                  |

## Command Line Use

Take a look at the output of `splinterctl --help` to see the available options
you can set for automation.

Here is how you can use shell to create a new store, then automatically use it
every time you run `splinterctl`:

```bash
% splinterctl --init test_store
Initializing store: test_store
  - Slots: 1024 (4096 bytes each, 64 byte alignment)
  - Value Arena: 4194304 bytes, SRS: 7475328 bytes (~7.13 MB)

% alias splinterctl="splinterctl --use test_store"

% splinterctl list
Key Name                          | Epoch      | Val Len    | Named Type     
-------------------------------------------------------------------------------------

% splinterctl set foo_key "A perfectly good value for foo."
% splinterctl get foo_key
31:A perfectly good value for foo.

% splinterctl list
Key Name                          | Epoch      | Val Len    | Named Type     
-------------------------------------------------------------------------------------
foo_key                           | 2          | 31         | SPL_SLOT_TYPE_VOID
```

If you want to persist the in-memory created store, you can simply run:

```bash
dd if=/dev/shm/test_store of=/path/to/save.spl bs=64
```

The same can be used to move a store from storage back to `/dev/shm`.

Be sure to check out `lua` for scripting support (there's a test script in the
repo), and `splinter_cli` directly for the interactive version with a few artful
`@antirez` creature comforts.

## Source Code

The CLI sources are `splinter_cli_*.c` as well as their included headers, which
are named similarly. It was inspired by
[BDSH](https://www.helenos.org/index.fcgi/browser/mainline/uspace/app/bdsh),
which the author previously wrote for HelenOS.
