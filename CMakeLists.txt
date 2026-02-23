cmake_minimum_required(VERSION 3.20)
project(deps_build LANGUAGES C CXX)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

set(CMAKE_EXPORT_COMPILE_COMMANDS OFF CACHE BOOL "" FORCE) # Useless. CMake WILL ALWAYS generate compile commands
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Enable PIC" FORCE)

# GGML settings
set(GGML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)

# Thirdparty backend
# set(GGML_CUDA ON CACHE BOOL "" FORCE)

# GLFW settings
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

set(OUT_DIR ${CMAKE_SOURCE_DIR}/build)

add_subdirectory(deps/ggml)
add_subdirectory(deps/glfw)
add_subdirectory(deps/libfyaml)

string(TOUPPER ${CMAKE_BUILD_TYPE} build_type)
message(STATUS "ARCHIVE_OUTPUT_DIRECTORY_${build_type} = ${OUT_DIR}")
message(STATUS "LIBRARY_OUTPUT_DIRECTORY_${build_type} = ${OUT_DIR}")

foreach(tgt ggml ggml-base ggml-cpu glfw fyaml)
    if (TARGET ${tgt})
        set_target_properties(${tgt} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_${build_type} ${OUT_DIR}
            LIBRARY_OUTPUT_DIRECTORY_${build_type} ${OUT_DIR}
        )
    endif()
        set_target_properties(${tgt} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR}
            LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}
            RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}
        )
endforeach()
