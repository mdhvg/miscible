Write-Host "Building in $Mode mode"
. "$ScriptDir/win32/common.ps1"

New-Item -ItemType Directory -Force -Path "build" | Out-Null
pushd "build"
Write-Host "CWD: $PWD"

$CC  = "clang-cl"
$CXX = "clang-cl"

$jobs = @()
$jobs += Start-Job -ScriptBlock {
    Set-Location $using:PWD
    $Mode = $using:Mode
    if (-not (Test-Path "ggml.lib")) {
        if ($Mode -eq "release") {
            cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Release
        } else {
            cmake -S .. -B . -DCMAKE_BUILD_TYPE=Debug
        }
        cmake --build . -j
    }
    if (Test-Path "compile_commands.json") {
        Remove-Item "compile_commands.json"
    }
} | Out-Null

if (-not (Test-Path "deps_c.obj")) {
    Write-Host "Building CXX deps"
    & $CXX /c /Fo"deps_cxx.obj" $CXXFLAGS $DEFINES $INCLUDES "$ProjectDir/src/deps_unity.cpp"

    Write-Host "Building C deps"
    & $CC /c /Fo"deps_c.obj" $CFLAGS $DEFINES $INCLUDES "$ProjectDir/src/deps_unity.c"
}

Get-Job | Wait-Job | Receive-Job

$LinkerBase = @("/link", "/LIBPATH:.", "/DEBUG", "-incremental:no")

if ($Mode -eq "release") {
    Write-Host "Building Miscible"
    & $CXX $CXXFLAGS $DEFINES $INCLUDES `
        "$ProjectDir/src/main.cpp" "$ProjectDir/src/miscible.cpp" deps_c.obj deps_cxx.obj `
        $ProjectDir/src/ui/pages/pages.cpp `
        $LinkerBase $CommonLibs $ThirdpartyLibs `
        /OUT:Miscible.exe
} else {
    Write-Host "Preprocessing miscible.cpp"
    & $CXX $CXXFLAGS $DEFINES $INCLUDES -DMSCBL_CORE=1 `
        "$ProjectDir/src/miscible.cpp" `
        /P /Fi:miscible.i

    Write-Host "Building libmiscible.dll"
    & $CXX -LD $CXXFLAGS $DEFINES $INCLUDES -DMSCBL_CORE=1 `
        "$ProjectDir/src/miscible.cpp" deps_c.obj deps_cxx.obj `
        $LinkerBase $CommonLibs $ThirdpartyLibs `
        /OUT:libmiscible.dll /IMPLIB:libmiscible.lib

    Write-Host "Building pages${Seed}.dll"
    & $CXX -LD $CXXFLAGS $DEFINES $INCLUDES `
        $ProjectDir/src/ui/pages/pages.cpp `
        $LinkerBase libmiscible.lib `
        /OUT:pages${Seed}.dll

    Write-Host "Building Miscible"
    & $CXX $CXXFLAGS $DEFINES $INCLUDES `
        "$ProjectDir/src/main.cpp" `
        $LinkerBase libmiscible.lib `
        /OUT:Miscible.exe
}

Get-Job | Wait-Job | Receive-Job

popd
popd

$end = Get-Date
Write-Host "Build time: $($end - $start)"
