param (
    [string]$Mode = "debug"
)

$start = Get-Date

$ScriptDir = $PSScriptRoot
. "$ScriptDir/win32/util.ps1"

if ($Mode -notin @("debug", "release", "test")) {
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

& ".\$script" -Mode $Mode
popd
exit 0
