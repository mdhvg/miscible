$ScriptDir = $PSScriptRoot
pushd (Join-Path $ScriptDir "../..")
$ProjectDir = $PWD.Path

$INCLUDES = @(
    "-I$ProjectDir/src"
    "-I$ProjectDir/deps/glfw/include"
    "-I$ProjectDir/deps/glfw/src"
    "-I$ProjectDir/deps/glad/include"
    "-I$ProjectDir/deps/glad/src"
    "-I$ProjectDir/deps/imgui"
    "-I$ProjectDir/deps/icons"
    "-I$ProjectDir/deps/stb"
    "-I$ProjectDir/deps/sha2"
    "-I$ProjectDir/deps/tinyfiledialogs"
    "-I$ProjectDir/deps/easy-args"
    "-I$ProjectDir/deps/sqlite"
    "-I$ProjectDir/deps/ggml/src"
    "-I$ProjectDir/deps/ggml/src/ggml-cpu"
    "-I$ProjectDir/deps/ggml/include"
    "-I$ProjectDir/deps/libfyaml/include"
    "-I$ProjectDir/deps/usearch/include"
    "-I$ProjectDir/deps/usearch/fp16/include"
    "-I$ProjectDir/deps/usearch/stringzilla/include"
    "-I$ProjectDir/deps/usearch/sqlite"
    "-I$ProjectDir/deps/doctest"
    "-I$ProjectDir/deps/curl/include"
)

$CommonLibs = @(
    "user32.lib",
    "gdi32.lib",
    "opengl32.lib",
    "shell32.lib",
    "advapi32.lib",
    "Rpcrt4.lib",
    "Ole32.lib",
    "Comdlg32.lib",
    "ws2_32.lib",
    "crypt32.lib",
    "normaliz.lib",
    "wldap32.lib",
    "bcrypt.lib",
    "iphlpapi.lib",
    "secur32.lib"
)

$ThirdpartyLibs = @(
    "fyaml.lib",
    "glfw3.lib",
    "ggml.lib",
    "ggml-cpu.lib",
    "ggml-base.lib",
    "libcurl-d.lib"
)

$LinkerBase = @("/link", "/LIBPATH:.", "/DEBUG", "-incremental:no")
$CFLAGS = @()
$DEFINES = @("-D_CRT_SECURE_NO_WARNINGS=1", "-DSQLITE_CORE=1", "-DCURL_STATICLIB")

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
if ($compiler -eq "msvc") {
    $CFLAGS += @("/nologo", "/Oi", "/GR", "/EHa", "/Zi", "/FC", "/W4", "/Zc:preprocessor"
                "/wd4244", "/wd4201", "/wd4100", "/wd4505", "/wd4189", "/wd4457",
                "/wd4456", "/wd4819", "/wd5287", "/wd4458", "/wd4267", "/wd4702",
                "/wd4245", "/wd4324", "/wd4068", "/wd4477", "/wd4996", "/wd4701",
                "/wd4127", "/wd4305", "/wd4005"
    )
    if ($Mode -eq "release") {
        $CFLAGS += @("/O2", "/MD")
    }
    else {
        $CFLAGS += @("/Od", "/MDd")
    }
}
elseif ($compiler -eq "clang-cl") {
    $CFLAGS += @("/nologo", "/Oi", "/GR", "/EHa", "/Zi", "/FC", "/W4",
        "-Wno-unused-function", "-Wno-missing-field-initializers",
        "-Wno-cast-function-type-mismatch", "-Wno-unused-parameter",
        "-Wno-unused-but-set-variable", "-Wno-format", "-Wno-missing-braces",
        "-Wno-sign-compare", "-Wno-unused-variable", "-Wno-string-plus-int",
        "-Wno-c99-designator", "-Wno-null-pointer-arithmetic",
        "-Wno-inconsistent-dllimport"
    )
    if ($Mode -eq "release") {
        $CFLAGS += @("/O2", "/MD")
    }
    else {
        $CFLAGS += @("/Od", "/MDd")
    }
}
elseif ($compiler -eq "gcc") {
    $CFLAGS += @("-Wno-unused-function", "-Wno-missing-field-initializers",
        "-Wno-cast-function-type-mismatch", "-Wno-unused-parameter",
        "-Wno-unused-but-set-variable", "-Wno-format", "-Wno-missing-braces",
        "-Wno-sign-compare", "-Wno-unused-variable", "-Wno-string-plus-int"
    )
}
else {
    Write-Host "Unknown compiler: $compiler" -ForegroundColor Red
    exit 1
}

if ($Mode -eq "debug") {
    $DEFINES += @("-DDBG=1", "-DROOT_DIR=`\""$($ProjectDir -replace '\\','/')`\""")
}

$CXXFLAGS = $CFLAGS + @("/std:c++20")
$DEFINES += '-DIMGUI_USER_CONFIG=\"mscbl_imconfig.h\"'
