#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

SOURCES="chgrp.cpp chgrp_app.cpp group_manager.cpp options.cpp reporter.cpp"
TARGET="chgrp"

if [ "${1:-}" = "clean" ]; then
    echo "[chgrp] Cleaning build artifacts..."
    rm -f "${TARGET}" "${TARGET}.exe" *.o *.obj
    echo "[chgrp] Clean complete."
    exit 0
fi

EXTRA_FLAGS=""
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    TARGET="chgrp.exe"
    EXTRA_FLAGS="-municode -ladvapi32"
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[chgrp] Error: Neither clang++ nor g++ was found in PATH." >&2
    exit 1
fi

echo "[chgrp] Compiling with ${COMPILER}..."
${COMPILER} -std=c++17 -O2 ${SOURCES} ${EXTRA_FLAGS} -o "${TARGET}"

echo "[chgrp] Build successful: ${TARGET}"

if [ -d "../../bin" ]; then
    cp -f "${TARGET}" "../../bin/${TARGET}"
    echo "[chgrp] Installed to bin/${TARGET}"
fi
