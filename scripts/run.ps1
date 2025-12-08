clear
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path

python "$SCRIPT_DIR/command_gen.py"

$warnColors=@("Black","DarkRed","DarkMagenta","DarkYellow","Gray","DarkGray","Red","Magenta","Yellow","White")
$originalFG = $Host.UI.RawUI.ForegroundColor

Write-Host "Building..." -ForegroundColor Cyan

$Host.UI.RawUI.ForegroundColor = (Get-random -InputObject $warnColors)
& "$SCRIPT_DIR\build2.bat"
$Host.UI.RawUI.ForegroundColor = $originalFG

Push-Location "$SCRIPT_DIR\..\build"

Write-Host "Running..." -ForegroundColor Cyan
$Host.UI.RawUI.ForegroundColor = "Green"
& .\Pics.exe
$Host.UI.RawUI.ForegroundColor = $originalFG

Pop-Location