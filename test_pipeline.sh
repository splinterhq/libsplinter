#!/bin/bash

# generated script to coordinate the inference signal group test.

# --- Configuration ---
BUS_NAME="nomic_text"
KEY_NAME="nomic_test_key"
MODEL_PATH="../nomic-embed-text-v1.Q6_K.gguf"
GROUP_ID=0

echo "=== Splinter Inference Pipeline End-to-End Test ==="

# Cleanly kill all background jobs when this script exits
trap 'echo -e "\nCleaning up background jobs..."; kill $(jobs -p) 2>/dev/null; exit' EXIT INT TERM

# 1. Compile a quick helper tool to register the signal group
echo "[1/5] Compiling C helper to bind '$KEY_NAME' to Group $GROUP_ID..."
cat << 'EOF' > bind_watch.c
#include "splinter.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (splinter_open(argv[1]) != 0) return 1;
    splinter_watch_register(argv[2], atoi(argv[3]));
    printf("Successfully bound key '%s' to group %s\n", argv[2], argv[3]);
    splinter_close();
    return 0;
}
EOF
# Assuming you are running this inside build/
gcc -O2 -o bind_watch bind_watch.c -I.. -L. -lsplinter -Wl,-rpath,.

# 2. Initialize the bus
echo "[2/5] Initializing Splinter Bus: $BUS_NAME..."
splinterctl init $BUS_NAME 2>/dev/null || echo "Bus already exists."

# We MUST set an initial value so the slot hash is established before binding
splinterctl --use $BUS_NAME set $KEY_NAME "init"
./bind_watch $BUS_NAME $KEY_NAME $GROUP_ID

# 3. Start the Inference Daemon
echo "[3/5] Starting splinference daemon in the background..."
splinference $BUS_NAME $MODEL_PATH $GROUP_ID &

# 4. Start the CLI Watcher
echo "[4/5] Starting CLI watcher on Group $GROUP_ID..."
splinterctl --use $BUS_NAME watch --group $GROUP_ID &

# Give llama.cpp a few seconds to map the GGUF model into memory
sleep 5

# 5. Trigger the Pipeline
echo "[5/5] Dropping new text into the bus to trigger the pipeline..."
splinterctl --use $BUS_NAME set $KEY_NAME "This is a live test of the zero-copy LLM ingestion pipeline."

echo "Waiting for inference and watcher output..."
sleep 3

echo "Test complete!"