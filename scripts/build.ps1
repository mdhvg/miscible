param (
    [string]$Mode = "debug",
    [string]$CompilerArg
)

$start = Get-Date
$exitStat = 0

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

Write-Host ""
if ($Mode -in @("release")) {
    $IconScript = "$ScriptDir/win32/icon-$compiler.ps1"

    if (-not (Test-Path $IconScript)) {
        $exitStat = 1
        Write-Host "$IconScript not found" -ForegroundColor Red
    } else {
        & "$IconScript" -Mode $Mode
    }
}

Write-Host ""
del -Force -Recurse ..\build\pages* *> $null
$Seed = Get-Random -Minimum 1 -Maximum 1000

if ($Mode -in @("test")) {
    $BuildScript = "$ScriptDir/win32/test-$compiler.ps1"
} else {
    $BuildScript = "$ScriptDir/win32/build-$compiler.ps1"
}

if (-not (Test-Path $BuildScript)) {
    $exitStat = 1
    Write-Host "$BuildScript not found" -ForegroundColor Red
} else {
    & "$BuildScript" -Mode $Mode -Seed $Seed
}

exit $exitStat
