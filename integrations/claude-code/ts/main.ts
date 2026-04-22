/**
 * A Star Trek TOS - Inspired LCARS Signal Bus Display For Splinter
 * Copyright (C) 2026 Tim Post
 * Suitable for debugging and exploratory use and inspired by the ST TOS
 * register displays on the Enterprise Science, Communications and Engineering
 * duty stations. Not meant to be a faithful reproduction, just a useful tool.
 * @license Apache 2
 * 
 */
import { Hono } from "https://deno.land/x/hono@v4.1.0/mod.ts";
import { streamSSE } from "https://deno.land/x/hono@v4.1.0/helper/streaming/index.ts";
import { Splinter } from "/usr/local/share/splinter/splinter.ts";

const app = new Hono();
// Where the stores can be found
const BUS_NAME = Deno.env.get("SPLINTER_CONN_FN");

// Flash effect duration in milliseconds
const FLASH_DURATION_MS = 250;

// deno-lint-ignore no-explicit-any
let store: any;
try {
  // Change this to libsplinter.so if using the in-memory version
  // We want agent scratchpads to persist
  store = Splinter.connect(BUS_NAME, "/usr/local/lib/libsplinter_p.so"); 
  if (!store) {
    throw new Error("Splinter.connect returned null/undefined");
  }
} catch (e) {
  console.error("❌ CRITICAL: Could not connect to Splinter store:");
  console.error(e);
  Deno.exit(1);
}

console.log(`[lcars]: Connected to ${BUS_NAME}`);

// Map to track signal counts
const lastCounts = new BigUint64Array(64);

app.get("/", (c) => {
  return c.html(indexHtml);
});

// Stream event server
app.get("/sse", (c) => {
  console.log('[lcars]: Streaming ...');
  return streamSSE(c, async (stream) => {
    // Basic guard
    if (!store) {
      await stream.writeSSE({ data: "Error: No Splinter Store Connection", event: "error" });
      return;
    }

    // Initialize counts
    for (let i = 0; i < 64; i++) {
      lastCounts[i] = store.getSignalCount(i);
    }

    while (true) {
      for (let i = 0; i < 64; i++) {
        // Double-check store exists in this tick
        const current = store.getSignalCount(i); 
        if (current > lastCounts[i]) {
          await stream.writeSSE({ data: i.toString(), event: "pulse" });
          lastCounts[i] = current;
        }
      }
      // TODO convert this to the splinter watcher 
      await new Promise((r) => setTimeout(r, 30));
    }
  });
});

const indexHtml = `
<!DOCTYPE html>
<html>
<head>
    <title>Splinter Bus Monitor</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <style>
        body { background-color: #000; color: #ff9d00; font-family: sans-serif; overflow: hidden; }
        .grid-container {
            display: grid;
            grid-template-columns: repeat(8, 1fr);
            gap: 4px;
            padding: 10px;
            height: 100vh;
        }
        .light {
            background-color: #332200; /* Dim state */
            border-radius: 2px;
            transition: background-color ${FLASH_DURATION_MS}ms ease-out;
            display: flex;
            align-items: flex-end;
            justify-content: flex-end;
            font-size: 10px;
            padding: 2px;
            color: rgba(255,255,255,0.1);
        }
        // LCARS Colors (purple swapped for green to bring in TOS science display)
        .c-blue   { background-color: #444466; }
        .c-orange { background-color: #553300; }
        .c-red    { background-color: #551111; }
        .c-purple { background-color: #234422; }
        .c-void   { background-color: #2d2e35; }

        // 'void' never technically flahshes, but css automation relies on symmetry in the DOM here.
        .active-blue   { background-color: #99ccff !important; box-shadow: 0 0 15px #99ccff; transition: none; }
        .active-orange { background-color: #ff9d00 !important; box-shadow: 0 0 15px #ff9d00; transition: none; }
        .active-red    { background-color: #ff3333 !important; box-shadow: 0 0 15px #ff3333; transition: none; }
        .active-purple { background-color: #4aed18 !important; box-shadow: 0 0 15px #4aed18; transition: none; }
        .active-void   { background-color: #000000 !important; box-shadow: 0 0 15px #000000; transition: none; }
    </style>
</head>
<body>
    <div class="grid-container" id="grid"></div>

    <script>
        const grid = document.getElementById('grid');
        const colors = ['orange', 'blue', 'orange', 'purple', 'red', 'orange', 'blue', 'orange'];
        
        // Create 64 lights (63  signal + 1 "spacer)
        for (let i = 0; i < 64; i++) {
            const div = document.createElement('div');
            const colorClass = colors[i % colors.length];
            div.id = 'L' + i;
            // the last signal is reserved; we can't pulse it.
            if (i == 63) {
              div.className = 'light c-void';
            } else { 
              div.className = 'light c-' + colorClass;
            }
            div.innerText = i;
            grid.appendChild(div);
        }

        const evtSource = new EventSource("/sse");
        evtSource.addEventListener("pulse", (e) => {
            const id = e.data;
            const el = document.getElementById('L' + id);
            const colorType = el.className.split('c-')[1];
            
            // Trigger flash
            const activeClass = 'active-' + colorType;
            el.classList.add(activeClass);
            
            // Revert after a tiny delay to allow CSS transition to fade it out
            setTimeout(() => {
                el.classList.remove(activeClass);
            }, 50); 
        });
    </script>
</body>
</html>
`;

Deno.serve({ port: 8787 }, app.fetch);
