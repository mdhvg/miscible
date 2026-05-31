param (
    [string]$Mode = "debug",
    [string]$CompilerArg
)

$start = Get-Date

$ScriptDir = $PSScriptRoot
. "$ScriptDir/win32/util.ps1"

if ($Mode -notin @("debug", "release", "test")) {
    Write-Host "Usage: .\build.ps1 [debug|release] [msvc|gcc|clang|clang-cl]"
    exit 1
}

function Find-Compiler {
    if (Get-Command cl          -ErrorAction SilentlyContinue) { return "msvc" }
    if (Get-Command gcc         -ErrorAction SilentlyContinue) { return "gcc"  }
    if (Get-Command clang-cl    -ErrorAction SilentlyContinue) { return "clang-cl" }
    return $null
}

if ($CompilerArg -notin @("msvc", "gcc", "clang", "clang-cl")) {
    $compiler = Find-Compiler
} else {
    $compiler = $CompilerArg
}

if (-not $compiler) {
    Write-Host "$compiler not found" -ForegroundColor Red
    exit 1
} else {
    Write-Host "$compiler found" -ForegroundColor Yellow
}

$compilerFmt = Format-AnsiString -Text $compiler -R 230 -G 178 -B 45
Write-Host "Detected compiler: $compilerFmt"

pushd $ScriptDir

if ($Mode -in @("test")) {
    $script = "win32/test-$compiler.ps1"
} else {
    $script = "win32/build-$compiler.ps1"
}

if (-not (Test-Path $script)) {
    Write-Host "$script not found" -ForegroundColor Red
    popd
    exit 1
}

del -Force -Recurse ..\build\pages* *> $null
$Seed = Get-Random -Minimum 1 -Maximum 1000

& ".\$script" -Mode $Mode -Seed $Seed
popd
exit 0
