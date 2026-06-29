#!/bin/bash
# scripts/bigbang.sh — Splinter full-stack installer for Debian / Ubuntu
#
# Usage:
#   wget -qO- https://raw.githubusercontent.com/splinterhq/libsplinter/main/scripts/bigbang.sh | bash
#   curl -fsSL https://raw.githubusercontent.com/splinterhq/libsplinter/main/scripts/bigbang.sh | bash

set -euo pipefail

BOLD='\033[1m'
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
DIM='\033[2m'
RESET='\033[0m'

LOG="$HOME/splinter_install.log"
# Sources live under /usr/local/src so they can be git-pulled and rebuilt later.
SRC_ROOT="/usr/local/src"
LLAMA_SRC="$SRC_ROOT/llama.cpp"
SPLINTER_SRC="$SRC_ROOT/libsplinter"
SPLINTER_REPO="https://github.com/splinterhq/libsplinter.git"
LLAMA_REPO="https://github.com/ggerganov/llama.cpp.git"
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)

touch "$LOG"
exec > >(tee -a "$LOG") 2>&1

ts()      { date '+%Y-%m-%d %H:%M:%S'; }
log()     { printf "${DIM}[%s]${RESET} %s\n" "$(ts)" "$*"; }
section() {
    printf "\n${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
    printf "  %s\n" "$*"
    printf "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}\n"
    log "SECTION: $*"
}
ok()   { printf "${GREEN}  ✓  %s${RESET}\n" "$*";   log "OK: $*";   }
warn() { printf "${YELLOW}  ⚠  %s${RESET}\n" "$*";  log "WARN: $*"; }
die()  {
    printf "${RED}${BOLD}  ✗  FATAL: %s${RESET}\n" "$*"
    log "FATAL: $*"
    printf "\nInstallation log: ${BOLD}%s${RESET}\n" "$LOG"
    exit 1
}

# Reads a keystroke directly from the terminal even when stdin is a pipe.
confirm_sudo() {
    local msg="${1:-The next step requires sudo access.}"
    printf "\n${YELLOW}${BOLD}  ⚠  %s${RESET}\n" "$msg"
    printf "     Press ${BOLD}ENTER${RESET} to continue or ${BOLD}Ctrl-C${RESET} to cancel … "
    read -r < /dev/tty
    printf "\n"
}

printf "\n${BOLD}${CYAN}"
cat <<'BANNER'
  ____  ____  _     ___ _   _ _____ _____ ____
 / ___||  _ \| |   |_ _| \ | |_   _| ____|  _ \
 \___ \| |_) | |    | ||  \| | | | |  _| | |_) |
  ___) |  __/| |___ | || |\  | | | | |___|  _ <
 |____/|_|   |_____|___|_| \_| |_| |_____|_| \_\

BANNER
printf "${RESET}"
printf "  Full-stack installer for ${BOLD}Debian / Ubuntu${RESET}\n"
printf "  Log: ${DIM}%s${RESET}\n\n" "$LOG"
log "bigbang.sh started"

cat <<'INTRO'
Welcome to the Big Bang Installer for Splinter!
This script will set up a complete environment for Splinter inference and 
classificiation development on Debian or Ubuntu systems. It will:
  - Check your platform and sudo access
  - Install necessary system packages
  - Build and install llama.cpp from source
  - Build and install Splinter from source (embeddings + llama + lua)
  - Provide a summary of installed components and next steps
To abort this process, press Ctrl-C now. Otherwise, hit [enter] to continue and 
enjoy the show!

You can watch the installation log in real time at: ${BOLD}$LOG${RESET}
INTRO

read nothing

section "Platform Check"

[[ -f /etc/os-release ]] || die "Cannot detect OS. This script requires Debian or Ubuntu."
# shellcheck disable=SC1091
source /etc/os-release

case "${ID:-}" in
    ubuntu|debian) ok "Detected: ${PRETTY_NAME:-${ID}}" ;;
    *)
        if [[ "${ID_LIKE:-}" == *debian* ]]; then
            warn "Debian-like distro: ${PRETTY_NAME:-${ID}} — proceeding."
        else
            die "Unsupported OS '${ID:-unknown}'. Expected Debian or Ubuntu."
        fi ;;
esac

# Verify sudo is available (caches credentials early; prompts once here).
if ! sudo -v 2>/dev/null; then
    die "sudo is required. Please run as a user with sudo access."
fi
ok "sudo access confirmed."

# Sources are kept under /usr/local/src so they can be updated (git pull) and
# rebuilt later. Make the directory writable by the current user so subsequent
# updates don't require sudo for the clone/build steps.
log "Preparing source directory $SRC_ROOT…"
sudo mkdir -p "$SRC_ROOT"
sudo chown "$(id -u):$(id -g)" "$SRC_ROOT"
ok "Source directory ready: $SRC_ROOT"

section "Installing System Packages"

APT_PACKAGES=(
    build-essential
    g++
    cmake
    pkg-config
    git
    curl
    wget
    unzip
    ca-certificates
    figlet
    libopenblas-dev
    libopenblas0
    liblapack-dev
    lua5.4
    liblua5.4-dev
)

log "apt-get update"
sudo apt-get update -qq

log "Installing: ${APT_PACKAGES[*]}"
sudo apt-get install -y "${APT_PACKAGES[@]}"
ok "System packages installed."

section "Installing Deno"

if command -v deno &>/dev/null; then
    ok "Deno already present: $(deno --version 2>/dev/null | head -1)"
else
    log "Running official Deno installer…"
    curl -fsSL https://deno.land/install.sh | sh

    export DENO_INSTALL="$HOME/.deno"
    export PATH="$DENO_INSTALL/bin:$PATH"

    # Persist to common shell profiles so deno survives new terminals.
    for _rc in "$HOME/.bashrc" "$HOME/.zshrc" "$HOME/.profile"; do
        [[ -f "$_rc" ]] || continue
        grep -q 'DENO_INSTALL' "$_rc" && continue
        {
            printf '\n# Deno (added by bigbang.sh)\n'
            printf 'export DENO_INSTALL="$HOME/.deno"\n'
            printf 'export PATH="$DENO_INSTALL/bin:$PATH"\n'
        } >> "$_rc"
        ok "Added Deno to $_rc"
    done

    ok "Deno installed: $(deno --version 2>/dev/null | head -1)"
fi

section "Building llama.cpp from Source"

[[ -d "$LLAMA_SRC" ]] && { warn "Removing stale build dir $LLAMA_SRC"; rm -rf "$LLAMA_SRC"; }

log "Cloning llama.cpp (shallow)…"
git clone --depth=1 "$LLAMA_REPO" "$LLAMA_SRC"

# llama.cpp renamed BLAS flags to GGML_ around b3500; detect which to use.
if grep -qr 'GGML_BLAS' "$LLAMA_SRC/CMakeLists.txt" 2>/dev/null; then
    _blas_flags="-DGGML_BLAS=ON -DGGML_BLAS_VENDOR=OpenBLAS"
    log "Using GGML_BLAS flags (modern llama.cpp)"
else
    _blas_flags="-DLLAMA_BLAS=ON -DLLAMA_BLAS_VENDOR=OpenBLAS"
    log "Using LLAMA_BLAS flags (classic llama.cpp)"
fi

log "Configuring…"
cmake -S "$LLAMA_SRC" -B "$LLAMA_SRC/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    ${_blas_flags}

log "Building with $NPROC jobs (this will take a few minutes)…"
cmake --build "$LLAMA_SRC/build" --config Release -j"$NPROC"
ok "llama.cpp built."

section "Installing llama.cpp"
confirm_sudo "About to install llama.cpp libraries and headers to /usr/local and run ldconfig."

log "Installing llama.cpp to /usr/local…"
sudo cmake --install "$LLAMA_SRC/build" --prefix /usr/local

log "ldconfig…"
sudo ldconfig
ok "llama.cpp installed to /usr/local; ldconfig complete."

section "Building Splinter (embeddings + llama + lua)"

[[ -d "$SPLINTER_SRC" ]] && { warn "Removing stale $SPLINTER_SRC"; rm -rf "$SPLINTER_SRC"; }

log "Cloning libsplinter (shallow)…"
git clone --depth=1 "$SPLINTER_REPO" "$SPLINTER_SRC"

log "Configuring Splinter (--with-embeddings --with-llama --with-lua)…"
cd "$SPLINTER_SRC"
./configure --with-embeddings --with-llama --with-lua

log "Building Splinter with $NPROC jobs…"
make -j"$NPROC"
ok "Splinter built."

section "Installing Splinter"
confirm_sudo "About to install Splinter to /usr/local/bin and run ldconfig."

log "sudo -E make install…"
sudo -E make install
ok "Splinter installed."

section "All Done"
cd "$HOME"    # leave the build dir

printf "\n${BOLD}  Installed:${RESET}\n"
printf "    llama.cpp     →  /usr/local/lib  /usr/local/include\n"
printf "    splinter_cli  →  $(command -v splinter_cli 2>/dev/null || echo '/usr/local/bin/splinter_cli')\n"
printf "    splinterctl   →  $(command -v splinterctl  2>/dev/null || echo '/usr/local/bin/splinterctl')\n"
printf "    splinference  →  $(command -v splinference 2>/dev/null || echo '/usr/local/bin/splinference')\n"
printf "    Deno          →  $(command -v deno 2>/dev/null || echo '~/.deno/bin/deno')\n"
printf "    Log           →  %s\n\n" "$LOG"
printf "${BOLD}${GREEN}"
printf "\n"
printf "  %% type 'splinterctl --help' TO CONTINUE.\n"
printf "  %% visit https://splinterhq.github.io/cli for an overview.\n"
printf "\n"
figlet -c "Shall we play"
figlet -c "a game?"
printf "${RESET}\n"
printf "Installation log : ${BOLD}%s${RESET}\n" "$LOG"
printf "Splinter source  : ${BOLD}%s${RESET}\n" "$SPLINTER_SRC"
printf "Llama.cpp source : ${BOLD}%s${RESET}\n" "$LLAMA_SRC"

log "bigbang.sh completed successfully."
log "To install all possible build dependencies, run $SPLINTER_SRC/configure --help."
