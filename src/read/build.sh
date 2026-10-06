#!/bin/sh
set -e

TOOL_NAME="read"
SOURCE_FILES="read.cpp options.cpp reporter.cpp engine.cpp read_app.cpp"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "Cleaning build artifacts for ${TOOL_NAME}..."
    rm -f *.obj *.o *.exe *.pdb *.ilk
    echo "Clean completed."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="gcc"
elif command -v cl >/dev/null 2>&1; then
    COMPILER="cl"
else
    echo "Error: No suitable C++ compiler found (clang++, g++, or cl)."
    exit 1
fi

echo "Building ${TOOL_NAME} with ${COMPILER}..."

if [ "$COMPILER" = "clang" ]; then
    clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -Wl,/subsystem:console -o "${TOOL_NAME}.exe" ${SOURCE_FILES}
elif [ "$COMPILER" = "gcc" ]; then
    g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -municode -o "${TOOL_NAME}.exe" ${SOURCE_FILES}
elif [ "$COMPILER" = "cl" ]; then
    cl /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /Fe:"${TOOL_NAME}.exe" ${SOURCE_FILES}
fi

echo "Build succeeded: ${TOOL_NAME}.exe"

if [ "$1" != "--no-install" ] && [ "$2" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "${TOOL_NAME}.exe" "${BIN_DIR}/${TOOL_NAME}.exe"
        echo "Installed to ${BIN_DIR}/${TOOL_NAME}.exe"
    fi
fi
