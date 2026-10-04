#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="a2pdf.cpp a2pdf_app.cpp cli.cpp encoding.cpp output_reporter.cpp pdf_generator.cpp ps_generator.cpp text_processor.cpp"

if [ "$1" = "clean" ]; then
    echo "[a2pdf] Cleaning build artifacts..."
    rm -f a2pdf.exe a2pdf *.o *.obj
    echo "[a2pdf] Clean complete."
    exit 0
fi

echo "[a2pdf] Building a2pdf..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[a2pdf] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[a2pdf] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:a2pdf.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o a2pdf.exe
fi

echo "[a2pdf] Build successful: a2pdf.exe"

if [ -d "../../bin" ]; then
    cp a2pdf.exe "../../bin/a2pdf.exe"
    echo "[a2pdf] Installed to bin/a2pdf.exe"
fi
