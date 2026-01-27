param (
    [string]$Mode = "debug"
)

$start = Get-Date


if ($Mode -notin @("debug", "release")) {
    Write-Host "Usage: .\build.ps1 [debug|release]"
    exit 1
}

function Find-Compiler {
    if (Get-Command cl    -ErrorAction SilentlyContinue) { return "msvc" }
    if (Get-Command gcc   -ErrorAction SilentlyContinue) { return "gcc"  }
    return $null
}

$compiler = Find-Compiler
if (-not $compiler) {
    Write-Host "No supported compiler (cl / gcc) found" -ForegroundColor Red
    exit 1
}

Write-Host "Detected compiler: $compiler"

$ScriptDir = $PSScriptRoot
pushd $ScriptDir

$script = "build-$compiler.ps1"
if (-not (Test-Path $script)) {
    Write-Host "$script not found" -ForegroundColor Red
    exit 1
}

& ".\$script" -Mode $Mode
popd

#
# if ($Mode -eq "release") {
#     Write-Host "Building Miscible"
#     & $CXX $CXXFLAGS $DEFINES $INCLUDES `
#         "$ProjectDir/src/main.cpp" deps_c.o deps_cxx.o `
#         -L. -l:libglfw3.a -l:libggml.a -l:libggml-cpu.a -l:libggml-base.a `
#         -o Miscible.exe
# } else {
#     Write-Host "Building pages.so"
#     & $CXX -shared $CXXFLAGS $DEFINES $INCLUDES `
#         "$ProjectDir/src/ui/pages/*.cpp" deps_c.o deps_cxx.o `
#         -L. -l:libglfw3.a -l:libggml.a -l:libggml-cpu.a -l:libggml-base.a `
#         -o pages.dll   # ← Windows uses .dll, not .so
#
#     Write-Host "Building libmiscible.dll"
#     & $CXX -shared $CXXFLAGS $DEFINES $INCLUDES `
#         "$ProjectDir/src/miscible.cpp" deps_c.o deps_cxx.o `
#         -L. -l:libglfw3.a -l:libggml.a -l:libggml-cpu.a -l:libggml-base.a `
#         -o libmiscible.dll
#
#     Write-Host "Building Miscible"
#     & $CXX $CXXFLAGS $DEFINES $INCLUDES `
#         "$ProjectDir/src/main.cpp" `
#         -L. -l:libmiscible -Wl,-rpath,'$ORIGIN' `
#         -o Miscible.exe
# }
#
# popd
# popd
exit 0
