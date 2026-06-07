Write-Host "Generating icon resource"
. "$ScriptDir/win32/common.ps1"

Write-Host "CWD: $PWD"
New-Item -ItemType Directory -Force -Path "build" | Out-Null
pushd "build"
Write-Host "CWD: $PWD"

$AbsIcoPath = Convert-Path $LogoPath
$AbsIcoPath = $AbsIcoPath.Replace('\', '\\')
$IcoStr = Format-AnsiString -Text $AbsIcoPath -R 196 -G 167 -B 231
Write-Host "Icon file: $IcoStr"

"1 ICON `"$AbsIcoPath`"" | Out-File -FilePath $LogoRcPath -Encoding ascii
rc /nologo /fo $LogoResPath $LogoRcPath

popd

$end = Get-Date
Write-Host "Build time: $($end - $start)"
