Write-Host "Building in $Mode mode"

$ScriptDir = $PSScriptRoot
pushd (Join-Path $ScriptDir "../..")
$ProjectDir = $PWD.Path

New-Item -ItemType Directory -Force -Path "build" | Out-Null
pushd "build"
Write-Host "CWD: $PWD"

$CC  = "cl"
$CXX = "cl"

Write-Host "Using compiler: $CC, $CXX"

# warning C4244: '=': conversion from 'float' to 'int', possible loss of data
# warning C4477: 'printf' : format string '%.*s' requires an argument of type
# warning C4996: 'strcpy': This function or variable may be unsafe. Consider using strcpy_s instead.
# warning C4034: sizeof returns 0
# warning C4068: unknown pragma 'GCC'
# warning C4458: declaration of 'scalar_t' hides class member
# warning C4267: 'initializing': conversion from 'size_t' to 'int32_t', possible loss of data
# warning C4702: unreachable code
# warning C4245: 'initializing': conversion from 'int' to 'size_t', signed/unsigned mismatch
# warning C4701: potentially uninitialized local variable 'new_size' used
# warning C4068: unknown pragma 'GCC'
# warning C4127: conditional expression is constant
# warning C4305: 'argument': truncation from 'double' to 'float'
# warning C4005: 'APIENTRY': macro redefinition
if ($Mode -eq "release") {
    $CFLAGS = @("/nologo", "/O2", "/Oi", "/GR", "/EHa", "/MD", "/FC", "/W4",
                "/wd4244", "/wd4201", "/wd4100", "/wd4505", "/wd4189", "/wd4457",
                "/wd4456", "/wd4819", "/wd5287", "/wd4458", "/wd4267", "/wd4702",
                "/wd4245", "/wd4324", "/wd4068", "/wd4477", "/wd4996", "/wd4701",
                "/wd4127", "/wd4305", "/wd4005")
    $CXXFLAGS = $CFLAGS + @("/std:c++20")
    $DEFINES  = @("-D_CRT_SECURE_NO_WARNINGS=1", "-DSQLITE_CORE=1", "-DROOT_DIR=`\""$($ProjectDir -replace '\\','/')`\""")
    $LINK_MODE = "static"
} else {
    $CFLAGS = @("/nologo", "/Od", "/Oi", "/GR", "/EHa", "/MDd", "/Zi", "/FC", "/W4",
                "/wd4244", "/wd4201", "/wd4100", "/wd4505", "/wd4189", "/wd4457",
                "/wd4456", "/wd4819", "/wd5287", "/wd4458", "/wd4267", "/wd4702",
                "/wd4245", "/wd4324", "/wd4068", "/wd4477", "/wd4996", "/wd4701",
                "/wd4127", "/wd4305", "/wd4005")
    $CXXFLAGS = $CFLAGS + @("/std:c++20")
    $DEFINES  = @("-DDBG=1", "-D_CRT_SECURE_NO_WARNINGS=1", "-DSQLITE_CORE=1", "-DROOT_DIR=`\""$($ProjectDir -replace '\\','/')`\""")
    $LINK_MODE = "hotreload"
}

$DEFINES += '-DIMGUI_USER_CONFIG=\"mscbl_imconfig.h\"'

$INCLUDES = @(
    "-I$ProjectDir/src"
    "-I$ProjectDir/deps/glfw/include"
    "-I$ProjectDir/deps/glfw/src"
    "-I$ProjectDir/deps/glad/include"
    "-I$ProjectDir/deps/glad/src"
    "-I$ProjectDir/deps/imgui"
    "-I$ProjectDir/deps/icons"
    "-I$ProjectDir/deps/stb"
    "-I$ProjectDir/deps/tinyfiledialogs"
    "-I$ProjectDir/deps/easy-args"
    "-I$ProjectDir/deps/sqlite"
    "-I$ProjectDir/deps/ggml/src"
    "-I$ProjectDir/deps/ggml/src/ggml-cpu"
    "-I$ProjectDir/deps/ggml/include"
    "-I$ProjectDir/deps/usearch/include"
    "-I$ProjectDir/deps/usearch/fp16/include"
    "-I$ProjectDir/deps/usearch/stringzilla/include"
    "-I$ProjectDir/deps/usearch/sqlite"
)

$CommonLibs = @(
    "glfw3.lib",
    "ggml.lib",
    "ggml-cpu.lib",
    "ggml-base.lib",
    "user32.lib",
    "gdi32.lib",
    "opengl32.lib",
    "shell32.lib",
    "advapi32.lib",
    "Rpcrt4.lib",
    "Ole32.lib",
    "Comdlg32.lib"
)

$jobs = @()
$jobs += Start-Job -ScriptBlock {
    Set-Location $using:PWD
    $Mode = $using:Mode
    if (-not (Test-Path "ggml.lib")) {
        if ($Mode -eq "release") {
            cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Release
        } else {
            cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Debug
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
        $LinkerBase $CommonLibs `
        /OUT:Miscible.exe
} else {
    Write-Host "Building libmiscible.dll"
    & $CXX -LD $CXXFLAGS $DEFINES $INCLUDES -DMSCBL_CORE=1 `
        "$ProjectDir/src/miscible.cpp" deps_c.obj deps_cxx.obj `
        $LinkerBase $CommonLibs `
        /OUT:libmiscible.dll /IMPLIB:libmiscible.lib

    Write-Host "Building pages.dll"
    & $CXX -LD $CXXFLAGS $DEFINES $INCLUDES `
        $ProjectDir/src/ui/pages/menu.cpp `
        $ProjectDir/src/ui/pages/preview.cpp `
        $ProjectDir/src/ui/pages/style.cpp `
        $LinkerBase libmiscible.lib `
        /OUT:pages.dll

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
