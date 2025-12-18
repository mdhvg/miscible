cmake_minimum_required(VERSION 3.20)
project(deps_build LANGUAGES C CXX)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GGML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Thirdparty backend
# set(GGML_CUDA ON CACHE BOOL "" FORCE)

set(OUT_DIR ${CMAKE_SOURCE_DIR}/build)

add_subdirectory(deps/ggml)

string(TOUPPER ${CMAKE_BUILD_TYPE} build_type)
message(STATUS "ARCHIVE_OUTPUT_DIRECTORY_${build_type} = ${OUT_DIR}")
message(STATUS "LIBRARY_OUTPUT_DIRECTORY_${build_type} = ${OUT_DIR}")

foreach(tgt ggml ggml-base ggml-cpu)
    if (TARGET ${tgt})
        set_target_properties(${tgt} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_${build_type} ${OUT_DIR}
            LIBRARY_OUTPUT_DIRECTORY_${build_type} ${OUT_DIR}
        )
    endif()
endforeach()