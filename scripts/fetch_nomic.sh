#!/bin/bash

# fetch_nomic.sh
# Safely downloads the Nomic Embed Text v1 Q6_K GGUF model from Hugging Face's CDN.

MODEL_DIR="3rdparty/Nomic"
MODEL_FILE="nomic-embed-text-v1.Q6_K.gguf"
URL="https://huggingface.co/nomic-ai/nomic-embed-text-v1-GGUF/resolve/main/${MODEL_FILE}?download=true"

# Ensure the target directory exists
mkdir -p "$MODEL_DIR"

echo "Fetching ${MODEL_FILE} (108 MB) from Hugging Face..."

# Download using curl or wget depending on system availability
if command -v curl >/dev/null 2>&1; then
    curl -L "$URL" -o "${MODEL_DIR}/${MODEL_FILE}"
elif command -v wget >/dev/null 2>&1; then
    wget -q --show-progress -O "${MODEL_DIR}/${MODEL_FILE}" "$URL"
else
    echo "Error: Neither curl nor wget is installed on this system."
    exit 1
fi

echo "Success! Model securely downloaded to: ${MODEL_DIR}/${MODEL_FILE}"
