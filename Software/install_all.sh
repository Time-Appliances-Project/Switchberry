#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./install_all.sh              # run from inside switchberry/
#   ./install_all.sh /path/to/switchberry

ROOT=""
AUTO_CONFIRM=0

# Parse arguments
for arg in "$@"; do
  if [[ "$arg" == "-y" ]]; then
    AUTO_CONFIRM=1
  elif [[ -z "$ROOT" ]]; then
    ROOT="$arg"
  fi
done

# Default to current directory if not specified
if [[ -z "$ROOT" ]]; then
  ROOT="$(pwd)"
fi

echo "Root directory: ${ROOT}"

if [[ "$AUTO_CONFIRM" -eq 1 ]]; then
  echo "Auto-confirm enabled: Skipping prompt."
else
  # Optional safety prompt
  read -rp "This will run 'make' and 'sudo make install' for EVERY Makefile under ${ROOT}. Continue? [y/N] " ans
  case "${ans,,}" in
    y|yes) ;;
    *) echo "Aborting."; exit 1 ;;
  esac
fi

# Find all Makefiles and process each directory
find "${ROOT}" -name 'Makefile' -print0 | while IFS= read -r -d '' mk; do
    dir="$(dirname "${mk}")"
    echo
    echo ">>> Entering ${dir}"
    (
        cd "${dir}"
        echo "    Running: make"
        make
        echo "    Running: sudo make install"
        sudo make install
    )
done

#cd "linuxptp"
#chmod +x incdefs.sh version.sh
#make all
#sudo make install

# Install Python helper scripts into /usr/local/sbin
echo ">>> Installing Python helper scripts to /usr/local/sbin"
SBIN_DIR="/usr/local/sbin"
PY_SCRIPTS=(
    "${ROOT}/sb_status_web.py"
    "${ROOT}/clockmatrix/generate_ptp_conf.py"
    "${ROOT}/clockmatrix/apply_network.py"
)
for script in "${PY_SCRIPTS[@]}"; do
    if [[ -f "$script" ]]; then
        echo "    Installing $(basename "$script") -> ${SBIN_DIR}/"
        sudo install -m 0755 "$script" "${SBIN_DIR}/$(basename "$script")"
    else
        echo "    WARNING: $script not found, skipping."
    fi
done

echo
echo "All done."

