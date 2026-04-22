#!/bin/bash
# bigbang.sh - Set up the Splinter / Claude Code demo
# Yell at Tim for this if it's broken.

set -e

DEMO_PORT=8787
LCARS_URL="http://localhost:${DEMO_PORT}"
BUS_FILE="demo_bus.spl"
LCARS_PID_FILE=".lcars.pid"

cleanup() {
    echo ""
    echo "[cleaning up] ..."
    if [ -f "$LCARS_PID_FILE" ]; then
        kill "$(cat $LCARS_PID_FILE)" 2>/dev/null || true
        rm -f "$LCARS_PID_FILE"
    fi
}

# Let the user tear down cleanly with ctrl-c
trap cleanup EXIT INT TERM

[ -f "${BUS_FILE}" ] || {
    echo ""
    echo "Initializing Splinter bus ..."
    bash scripts/bus-init.sh "${BUS_FILE}"
}

export DEMO_ROOT=$(pwd)
# the Deno script needs a literal path
export SPLINTER_CONN_FN="${DEMO_ROOT}/${BUS_FILE}"
# agents already know where they are
export SPLINTER_AGENT_BUS="$BUS_FILE"

echo "Starting LCARS server on port ${DEMO_PORT} ..."
deno run -A ts/main.ts &
LCARS_PID=$!
echo "$LCARS_PID" > "$LCARS_PID_FILE"

# Give Deno a moment to bind the port
echo "Allowing 5 seconds for Deno to start and slower systems to catch up ..."
sleep 5


# Verify it actually came up before telling the user it did
if ! kill -0 "$LCARS_PID" 2>/dev/null; then
    echo "ERROR: LCARS server failed to start. Check ts/main.ts output above."
    exit 1
fi

# ── 3. Open browser ────────────────────────────────────────────────────────
echo "Opening LCARS display ..."
if command -v xdg-open &>/dev/null; then
    xdg-open "$LCARS_URL" 2>/dev/null &
elif command -v open &>/dev/null; then
    # macOS fallback (mostly for contributors)
    open "$LCARS_URL" 2>/dev/null &
else
    echo "(Could not auto-open browser — visit $LCARS_URL manually)"
fi

sleep 2

echo "Exercising the bus (watch the display) ..."

# this makes a nice shimmer across the signal groups
for i in {1..63}
do
	idx=$((i-1))
	splinterpctl --use "${BUS_FILE}" bump lcars_${i} > /dev/null 2>&1
done

echo ""
echo "┌─────────────────────────────────────────────────────────┐"
echo "│  SPLINTER BUS : $BUS_FILE"
echo "│  LCARS DISPLAY: $LCARS_URL"
echo "│"
echo "│  Start the demo by telling Claude Code:"
echo "│  \"Execute the spec in spec.md\""
echo "│"
echo "│  Press Ctrl-C to stop the demo and clean up."
echo "└─────────────────────────────────────────────────────────┘"
echo ""

# Keep the script alive so the trap fires on ctrl-c
# and the user has a clean shutdown path
wait "$LCARS_PID"
