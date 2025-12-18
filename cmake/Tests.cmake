include(CTest)

set(GTEST_DIR "${DEPS_DIR}/googletest")
enable_testing()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "MSVC Debug build")
    message(STATUS "${CMAKE_CXX_FLAGS_DEBUG} /EHsc /W4 /D_DEBUG /MDd /Zi /fsanitize=address")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /EHsc /W4 /D_DEBUG /MDd /Zi /fsanitize=address")
else()
    message(STATUS "MSVC Release build")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
endif()

add_subdirectory("${GTEST_DIR}")

include_directories("${CMAKE_SOURCE_DIR}/include")
file(GLOB_RECURSE TEST_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.cpp")
add_executable(${CMAKE_PROJECT_NAME}Tests ${TEST_SRC} "${CMAKE_SOURCE_DIR}/src/base/arena.cpp" "${CMAKE_SOURCE_DIR}/src/base/core.cpp" "${CMAKE_SOURCE_DIR}/src/base/string.cpp" "${CMAKE_SOURCE_DIR}/src/base/path.cpp" "${CMAKE_SOURCE_DIR}/src/base/array.cpp")

target_link_libraries("${CMAKE_PROJECT_NAME}Tests" PRIVATE gtest gtest_main)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Defining DEBUG macro")
    target_compile_definitions("${CMAKE_PROJECT_NAME}Tests" PUBLIC DEBUG=1)
endif()
target_compile_definitions("${CMAKE_PROJECT_NAME}Tests" PUBLIC APP_NAME="${CMAKE_PROJECT_NAME}")
target_compile_definitions("${CMAKE_PROJECT_NAME}Tests" PUBLIC ROOT_DIR="${CMAKE_SOURCE_DIR}")

add_test(NAME "${CMAKE_PROJECT_NAME}Tests" COMMAND ${CMAKE_PROJECT_NAME}Tests)

message(STATUS "Test source files: ${TEST_SRC}")
message(STATUS "Creating test executable: ${CMAKE_PROJECT_NAME}Tests")