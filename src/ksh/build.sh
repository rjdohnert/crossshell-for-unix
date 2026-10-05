#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f ksh ksh.exe ksh_modular ksh_modular.exe amalgamate amalgamate.exe *.o *.obj *.pdb *.ilk
    echo "[ksh] Clean complete."
    exit 0
fi

# Detect C++ compiler
CXX=""
if command -v clang++ >/dev/null 2>&1; then
    CXX="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX="g++"
else
    echo "[ksh] Error: Neither clang++ nor g++ found in PATH." >&2
    exit 1
fi

run_amalgamate() {
    if [ -f "./amalgamate.exe" ]; then
        ./amalgamate.exe --source-dir . --output ksh.cpp
    elif [ -f "./amalgamate" ]; then
        ./amalgamate --source-dir . --output ksh.cpp
    elif command -v pwsh >/dev/null 2>&1; then
        pwsh -NoProfile -File amalgamate.ps1 -OutputFile ksh.cpp -SourceDir .
    elif command -v powershell >/dev/null 2>&1; then
        powershell -NoProfile -File amalgamate.ps1 -OutputFile ksh.cpp -SourceDir .
    else
        echo "[ksh] Compiling amalgamate tool..."
        $CXX -std=c++17 -O2 amalgamate.cpp -o amalgamate
        ./amalgamate --source-dir . --output ksh.cpp
    fi
}

if [ "$1" = "amalgamate" ]; then
    echo "[ksh] Generating amalgamation..."
    run_amalgamate
    echo "[ksh] Amalgamation updated: ksh.cpp"
    exit 0
fi

# Determine target binary name and flags based on OS
IS_WIN=false
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]] || uname -s | grep -qi "mingw\|msys\|cygwin"; then
    IS_WIN=true
    TARGET="ksh.exe"
    MOD_TARGET="ksh_modular.exe"
    LIBS="-lAdvapi32 -lUser32 -Wl,/subsystem:console"
else
    TARGET="ksh"
    MOD_TARGET="ksh_modular"
    LIBS=""
fi

if [ "$1" = "modular" ]; then
    echo "[ksh] Compiling modular $MOD_TARGET with $CXX..."
    $CXX -std=c++17 -O2 -I include src/*.cpp $LIBS -o "$MOD_TARGET"
    echo "[ksh] Build successful: $MOD_TARGET"

    if [ "$2" = "test" ]; then
        echo "[ksh] Running modular self-tests..."
        "./$MOD_TARGET" --self-test
    fi
    exit 0
fi

if [ ! -f "ksh.cpp" ]; then
    echo "[ksh] Generating initial amalgamation..."
    run_amalgamate
fi

echo "[ksh] Compiling amalgamated $TARGET with $CXX..."
$CXX -std=c++17 -O2 ksh.cpp $LIBS -o "$TARGET"
echo "[ksh] Build successful: $TARGET"

if [ "$1" = "test" ]; then
    echo "[ksh] Running self-tests..."
    "./$TARGET" --self-test
fi

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[ksh] Installed to ../../bin/$TARGET"
    fi
fi
