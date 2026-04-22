## Agent Signaling

When performing the following actions, call `scripts/spl-signal <event>` via bash:

- Starting any new task or subtask: `scripts/spl-signal task_start`
- Completing a task or subtask: `scripts/spl-signal task_finish`  
- Advancing within a multi-step task: `scripts/spl-signal task_advance`
- Reading any file (before read): `scripts/spl-signal file_read`
- Writing or modifying any file (before write): `scripts/spl-signal file_write`
- Calling any external tool or API: `scripts/spl-signal tool_use`
- Hitting an error that required recovery: `scripts/spl-signal error`
- Uncertain about next action and stopping to verify: `scripts/spl-signal checkpoint`

The signal bus is available at $SPLINTER_AGENT_BUS. Governance 
observes these signals; missing signals degrade observability.

Signal `task_advance` immediately after completing each step, before
beginning the next. Do not batch signals at the end of a task. 

## Agent Journaling

Use `scripts/spl-journal` to record intent and outcome at decision points —
moments where you chose an approach, hit something unexpected, or
solved a problem you might face again.

**Journal intent before attempting something non-trivial:**
```bash
scripts/spl-journal "descriptive-key-name" "Attempting X because Y. 
Expecting Z."
```

**Journal outcome after the attempt completes:**
```bash
scripts/spl-journal "descriptive-key-name.1" "X worked / failed because 
[specific reason]. Fixed by [approach]. Watch out for [caveat]."
```

**Key naming convention:** use lowercase-hyphenated names that describe
the problem, not the solution. Good: `auth-middleware-import-error`.
Bad: `attempt1`, `task3`, `fix`.

**When to journal (required):**
- Before and after any approach you considered and rejected
- When a command fails and you try a different approach
- When you discover something surprising about the environment
- When you make a decision that isn't obvious from the spec

Journal intent **before** attempting something non-trivial.
Journal outcome **immediately after** it completes — not at the end of
the task. Each journal entry should be written while the result is still
the current action.

**When not to journal:**
- Routine file reads and writes already covered by spl-signal
- Steps that succeeded exactly as expected with no surprises
- Anything you'd describe as "just following instructions"

**Bloom labels for search:**
- `0x1`  - build / compilation issues
- `0x2`  - FFI / runtime issues
- `0x4`  - test failures
- `0x8`  - environment / dependency issues
- `0x10` - logic related issues / debugging
- `0x20` - security related issues`
- `0x40` - conflicting instruction or goal issues
- `0x80` - exceptional issues where no other label applies

These labels are translated in `config/agent.rc` to English
equivalents your user can utilize to see what kinds of notes you left through
semantic search. Splinter embeds your journaling automatically and makes it 
accessible via `splinterpctl search`.

Apply the most specific label that fits:
```bash
scripts/spl-journal "deno-ffi-bigint-overflow.1" "setLabel requires BigInt 
not Number for mask argument, Deno throws on Number silently" 0x20
```
