Write-Host "Building $Mode"
. "$ScriptDir/win32/common.ps1"

New-Item -ItemType Directory -Force -Path "build" | Out-Null
pushd "build"
Write-Host "CWD: $PWD"

$CC  = "clang-cl"
$CXX = "clang-cl"

Write-Host ""

$outputPath = Format-AnsiString -Text "tests.exe" -R 210 -G 200 -B 20 -Bold
$testFiles = Get-ChildItem "$ProjectDir/tests" -Recurse -Filter *.cpp | ForEach-Object { $_.FullName }
& $CXX $CXXFLAGS $DEFINES $INCLUDES `
    @testFiles `
    $LinkerBase $CommonLibs `
    /OUT:./tests.exe

popd
popd

Write-Host ""
$end = Get-Date
Write-Host "Build time: $($end - $start)"
