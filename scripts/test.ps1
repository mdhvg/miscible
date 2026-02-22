$start = Get-Date

Write-Host ""

$ProjectDir = Join-Path $PSScriptRoot ".."

. "$ProjectDir/scripts/win32/util.ps1"

pushd $ProjectDir

& "build/tests.exe"

popd

$end = Get-Date
Write-Host ""
Write-Host "Test time: $($end - $start)"
