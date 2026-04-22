#!/bin/bash
# littlebang.sh - Set up the Splinter / Claude Code demo (no LCARS)

set -e

BUS_FILE="demo_bus.spl"

cleanup() {
    exit 0
}

trap cleanup EXIT INT TERM

[ -f "${BUS_FILE}" ] || {
    echo ""
    echo "Initializing Splinter bus ..."
    bash scripts/bus-init.sh "${BUS_FILE}"
}

export SPLINTER_CONN_FN="$BUS_FILE"
export SPLINTER_AGENT_BUS="$BUS_FILE"

echo ""
echo "┌─────────────────────────────────────────────────────────┐"
echo "│  SPLINTER BUS : $BUS_FILE"
echo "│"
echo "│  Start the demo by telling Claude Code:"
echo "│  \"Execute the spec in spec.md\""
echo "│"
echo "| Press CTRL-C To End The Demo"
echo "└─────────────────────────────────────────────────────────┘"
echo ""

# Keep alive for ctrl-c
while true; do sleep 60; done
