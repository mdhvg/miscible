#!/bin/bash

MODE="${1:-debug}"
if [[ "$MODE" != "debug" && "$MODE" != "release" ]]; then
    echo "Usage: $0 [debug|release]"
    exit 1
fi

printf "Building in %s mode\n" $MODE
SCRIPT_DIR="$(dirname $0)"
pushd "$(pwd)/$SCRIPT_DIR/.." > /dev/null
printf "CWD: %s\n" $(pwd)
PROJECT_DIR=$(pwd)

mkdir -p build
pushd build > /dev/null
# mkdir -p intermediate

if [ ! -e "libggml.a" ]; then
    if [[ $MODE == "release" ]]; then
        cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release
    else
        cmake -S .. -B . -DCMAKE_BUILD_TYPE=Debug
    fi
    cmake --build . -j &
fi

if [ -e "compile_commands.json" ]; then
    rm "compile_commands.json"
fi

CC="gcc"
CXX="g++"

if [[ $MODE == "release" ]]; then
    CFLAGS="-O2 -march=native -ldl -lm -lpthread"
    CXXFLAGS="${CFLAGS} -std=c++17"
    DEFINES=(-DSQLITE_CORE=1 "-DROOT_DIR=\"${PROJECT_DIR}\"")
    LINK_MODE="static"
else
    CFLAGS="-O0 -g -ggdb -ldl -lm -lpthread -fPIC"
    CXXFLAGS="${CFLAGS} -std=c++17"
    DEFINES=(-DDEBUG=1 -DSQLITE_CORE=1 "-DROOT_DIR=\"${PROJECT_DIR}\"")
    LINK_MODE="hotreload"
fi

DEFINES+=(-DIMGUI_USER_CONFIG=\"mscbl_imconfig.h\")

INCLUDES=(
    -I"${PROJECT_DIR}/src"

    -I"${PROJECT_DIR}/deps/glfw/include"
    -I"${PROJECT_DIR}/deps/glfw/src"

    -I"${PROJECT_DIR}/deps/glad/include"
    -I"${PROJECT_DIR}/deps/glad/src"

    -I"${PROJECT_DIR}/deps/imgui"

    -I"${PROJECT_DIR}/deps/icons"

    -I"${PROJECT_DIR}/deps/stb"

    -I"${PROJECT_DIR}/deps/tinyfiledialogs"

    -I"${PROJECT_DIR}/deps/easy-args"

    -I"${PROJECT_DIR}/deps/sqlite"

    -I"${PROJECT_DIR}/deps/ggml/src"
    -I"${PROJECT_DIR}/deps/ggml/src/ggml-cpu"
    -I"${PROJECT_DIR}/deps/ggml/include"

    -I"${PROJECT_DIR}/deps/usearch/include"
    -I"${PROJECT_DIR}/deps/usearch/fp16/include"
    -I"${PROJECT_DIR}/deps/usearch/stringzilla/include"
    -I"${PROJECT_DIR}/deps/usearch/sqlite"
)

if [ ! -e "deps_c.o" ]; then
    printf "Building CXX deps\n"
    ${CXX} ${CXXFLAGS} "${DEFINES[@]}" -c "${INCLUDES[@]}" "${PROJECT_DIR}/src/deps_unity.cpp" -o deps_cxx.o &
    printf "Building C deps\n"
    ${CC} ${CFLAGS} "${DEFINES[@]}" -c "${INCLUDES[@]}" "${PROJECT_DIR}/src/deps_unity.c" -o deps_c.o
fi

if [[ $MODE == "release" ]]; then
    printf "Building Miscible\n"
    ${CXX} ${CXXFLAGS} "${DEFINES[@]}" "${INCLUDES[@]}" \
        "${PROJECT_DIR}/src/main.cpp" deps_c.o deps_cxx.o \
        -L. -l:libglfw3.a -l:libggml.a -l:libggml-cpu.a -l:libggml-base.a \
        -o Miscible
else
    printf "Building pages.so\n"
    ${CXX} -shared ${CXXFLAGS} "${DEFINES[@]}" "${INCLUDES[@]}" \
        ${PROJECT_DIR}/src/ui/pages/*.cpp deps_c.o deps_cxx.o \
        -L. -l:libglfw3.a -l:libggml.a -l:libggml-cpu.a -l:libggml-base.a \
        -o pages.so &

    printf "Building libmiscible.so\n"
    ${CXX} -shared ${CXXFLAGS} "${DEFINES[@]}" "${INCLUDES[@]}" \
        "${PROJECT_DIR}/src/miscible.cpp" deps_c.o deps_cxx.o \
        -L. -l:libglfw3.a -l:libggml.a -l:libggml-cpu.a -l:libggml-base.a \
        -o libmiscible.so

    printf "Building Miscible\n"
    ${CXX} ${CXXFLAGS} "${DEFINES[@]}" "${INCLUDES[@]}" \
        "${PROJECT_DIR}/src/main.cpp" \
        -L. -l:libmiscible.so -Wl,-rpath,'$ORIGIN' \
        -o Miscible
fi

popd
popd

exit 0
