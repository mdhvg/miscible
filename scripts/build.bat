@echo off

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%.."
echo cwd: %cd%
set "PROJECT_DIR=%cd%"

if not exist build mkdir build
pushd build
if not exist intermediate mkdir intermediate

rem warning C4244: '=': conversion from 'float' to 'int', possible loss of data
rem warning C4477: 'printf' : format string '%.*s' requires an argument of type 
rem warning C4996: 'strcpy': This function or variable may be unsafe. Consider using strcpy_s instead.
rem warning C4034: sizeof returns 0
rem warning C4068: unknown pragma 'GCC'
rem warning C4458: declaration of 'scalar_t' hides class member
rem warning C4267: 'initializing': conversion from 'size_t' to 'int32_t', possible loss of data
rem warning C4702: unreachable code
rem warning C4245: 'initializing': conversion from 'int' to 'size_t', signed/unsigned mismatch
rem warning C4701: potentially uninitialized local variable 'new_size' used
set CCompilerFlags=-nologo ^
                  -Od ^
                  -FC ^
                  -W4 ^
                  -MDd ^
                  -Oi ^
                  -GR ^
                  -EHa ^
                  -Zi ^
                  ^
                  -wd4244 ^
                  -wd4201 ^
                  -wd4100 ^
                  -wd4505 ^
                  -wd4189 ^
                  -wd4457 ^
                  -wd4456 ^
                  -wd4819 ^
                  -wd5287 ^
                  -wd4458 ^
                  -wd4267 ^
                  -wd4702 ^
                  -wd4245 ^
                  -wd4324 ^
                  -wd4068 ^
                  -wd4477 ^
                  -wd4996 ^
                  -wd4701
rem               -Gm

set CppCompilerFlags=%CCompilerFlags% -std:c++17

rem Libs user32.lib gdi32.lib winmm.lib
set CommonLinkerFlags=-incremental:no ^
                      -opt:ref ^
                      Rpcrt4.lib ^
                      user32.lib ^
                      gdi32.lib ^
                      opengl32.lib ^
                      shell32.lib ^
                      advapi32.lib ^
                      ^
                      ggml.lib ^
                      ggml-cpu.lib ^
                      ggml-base.lib

set Definitions=-DDEBUG=1 ^
                -DROOT_DIR=\"%PROJECT_DIR:\=/%\" ^
                -D_CRT_SECURE_NO_WARNINGS=1 ^
                -D_GLFW_WIN32=1 ^
                -DSQLITE_CORE=1

set Includes=-I../src ^
             ^
             -I../deps/sqlite ^
             ^
             -I../deps/stb ^
             ^
             -I../deps/glfw/include ^
             ^
             -I../deps/glad/include ^
             ^
             -I../deps/imgui ^
             ^
             -I../deps/icons ^
             ^
             -I../deps/ggml/src ^
             -I../deps/ggml/src/ggml-cpu ^
             -I../deps/ggml/include ^
             ^
             -I../deps/usearch/include ^
             -I../deps/usearch/fp16/include ^
             -I../deps/usearch/stringzilla/include

rem 64-bit build
del /q *.pdb 2> NUL
del /q *.obj 2> NUL
del /q *.exe 2> NUL
del /q   .vs 2> NUL
REM Optimization switches: /O2 /Oi /fp:fastZ

REM Compile SQLite and usearch sqlite extension source files
echo:
echo Building sqlite usearch
cl %CppCompilerFlags% %Definitions% %Includes% /c ../deps/sqlite/sqlite3.c ../deps/usearch/sqlite/lib.cpp

REM Compile GLFW source files
echo:
echo Building glfw
set GLFW_SRC=../deps/glfw/src
cl %CCompilerFlags% %Definitions% %Includes% /c ^
   %GLFW_SRC%/context.c ^
   %GLFW_SRC%/init.c ^
   %GLFW_SRC%/input.c ^
   %GLFW_SRC%/monitor.c ^
   %GLFW_SRC%/vulkan.c ^
   %GLFW_SRC%/window.c ^
   %GLFW_SRC%/platform.c ^
   %GLFW_SRC%/null_init.c ^
   %GLFW_SRC%/null_monitor.c ^
   %GLFW_SRC%/null_window.c ^
   %GLFW_SRC%/null_joystick.c ^
   ^
   %GLFW_SRC%/win32_init.c ^
   %GLFW_SRC%/win32_joystick.c ^
   %GLFW_SRC%/win32_monitor.c ^
   %GLFW_SRC%/win32_time.c ^
   %GLFW_SRC%/win32_thread.c ^
   %GLFW_SRC%/win32_window.c ^
   %GLFW_SRC%/win32_module.c ^
   ^
   %GLFW_SRC%/wgl_context.c ^
   %GLFW_SRC%/egl_context.c ^
   %GLFW_SRC%/osmesa_context.c

@REM REM Compile ImGUI source files
@REM echo:
@REM echo Building ImGUI
@REM set IMGUI_SRC=../deps/imgui
@REM cl %CommonCompilerFlags% %Definitions% %Includes% /c ^
@REM    %IMGUI_SRC%/imgui.cpp ^
@REM    %IMGUI_SRC%/imgui_demo.cpp ^
@REM    %IMGUI_SRC%/imgui_draw.cpp ^
@REM    %IMGUI_SRC%/imgui_tables.cpp ^
@REM    %IMGUI_SRC%/imgui_widgets.cpp ^
@REM    %IMGUI_SRC%/backends/imgui_impl_glfw.cpp ^
@REM    %IMGUI_SRC%/backends/imgui_impl_opengl3.cpp

REM Compile glad source files
echo:
echo Building glad
cl %CCompilerFlags% %Includes% /c ../deps/glad/src/*.c

REM Build Miscible
echo:
echo Building Miscible
REM Intermediate
REM cl %CppCompilerFlags% %Definitions% %Includes% ../src/miscible.cpp /P /Fiintermediate/Miscible.i
REM Miscible lib
cl /c %CppCompilerFlags% %Definitions% %Includes% ../src/miscible.cpp /Fo:miscible.obj /link %CommonLinkerFlags%
lib /OUT:miscible.lib *.obj
REM cl %CppCompilerFlags% %Definitions% %Includes% ../src/miscible.cpp *.obj /LD /Fe:miscible.lib /link %CommonLinkerFlags%

REM Hot reloaded components
REM - UI Menu
cl %CppCompilerFlags% %Definitions% %Includes% ../src/ui/pages/menu.cpp /LD /Fe:menu.dll /link miscible.lib
copy menu.dll menu.tmp.dll

REM Executable
cl %CppCompilerFlags% %Definitions% %Includes% ../src/main.cpp /Fe:Miscible.exe -FmMiscible.map /link miscible.lib

popd
popd
