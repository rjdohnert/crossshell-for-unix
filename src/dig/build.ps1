$target = "dig"
$binDir = "..\..\bin"
$src = @("dig.cpp", "dig_app.cpp", "options.cpp", "query_engine.cpp", "reporter.cpp")

if (-not (Test-Path $binDir)) {
    New-Item -ItemType Directory -Path $binDir | Out-Null
}

$exePath = Join-Path $binDir "$target.exe"

if (Get-Command clang++ -ErrorAction SilentlyContinue) {
    Write-Host "Building $target with clang++..."
    & clang++ -std=c++17 -O2 $src -ldnsapi -lws2_32 -o $exePath
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Build successful: $exePath"
        exit 0
    }
}

if (Get-Command cl -ErrorAction SilentlyContinue) {
    Write-Host "Building $target with MSVC cl..."
    & cl /std:c++17 /O2 /EHsc /Fe:$exePath $src dnsapi.lib ws2_32.lib
    if ($LASTEXITCODE -eq 0) {
        Get-ChildItem -Filter "*.obj" | Remove-Item -Force
        Write-Host "Build successful: $exePath"
        exit 0
    }
}

if (Get-Command g++ -ErrorAction SilentlyContinue) {
    Write-Host "Building $target with g++..."
    & g++ -std=c++17 -O2 $src -ldnsapi -lws2_32 -o $exePath
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Build successful: $exePath"
        exit 0
    }
}

Write-Error "No suitable C++ compiler found (clang++, cl, or g++)."
exit 1
