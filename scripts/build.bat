@echo off

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%.."
echo cwd: %cd%
set "PROJECT_DIR=%cd%"

if not exist build mkdir build
pushd build

rem warning C4244: '=': conversion from 'float' to 'int', possible loss of data
rem warning C4477: 'printf' : format string '%.*s' requires an argument of type 
set CommonCompilerFlags=-std:c++17 -Od -nologo -FC -WX -W4 -wd4201 -wd4100 -wd4505 -wd4189 -wd4457 -wd4456 -wd4819 -wd5287 -wd4244 -wd4477 -MTd -Oi -GR- -Gm- -EHa- -Zi
set CommonLinkerFlags=-incremental:no -opt:ref Rpcrt4.lib
rem Libs user32.lib gdi32.lib winmm.lib

set Definitions=
set Definitions=%Definitions% -DDEBUG=1
set Definitions=%Definitions% -DROOT_DIR=\"%PROJECT_DIR:\=/%\"
set Definitions=%Definitions% -D_CRT_SECURE_NO_WARNINGS=1

set Includes=
set Includes=%Includes% -I..\deps\sqlite
set Includes=%Includes% -I..\deps\stb

rem 32-bit build
rem cl %CommonCompilerFlags% w:\handmade-hero\code\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

rem 64-bit build
del *.pdb 2> NUL
REM Optimization switches: /O2 /Oi /fp:fastZ

REM Compile SQLite source files
cl %CommonCompilerFlags% ..\deps\sqlite\sqlite3.c ..\deps\sqlite\shell.c /c
cl /Fe:Pics.exe %CommonCompilerFlags% %Definitions% %Includes% -Fmpics.map ..\src\main.cpp sqlite3.obj /link %CommonLinkerFlags%

popd
popd