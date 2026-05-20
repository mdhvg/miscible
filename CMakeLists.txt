cmake_minimum_required(VERSION 3.20)
project(deps_build LANGUAGES C CXX)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Force static libraries" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable testing globally" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Enable PIC" FORCE)

set(CMAKE_EXPORT_COMPILE_COMMANDS OFF CACHE BOOL "" FORCE) # Useless. CMake WILL ALWAYS generate compile commands

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

# CURL settings
set(BUILD_CURL_EXE OFF CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE INTERNAL "")
set(BUILD_STATIC_LIBS ON CACHE INTERNAL "")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(CURL_DISABLE_INSTALL ON CACHE INTERNAL "")
set(CURL_ENABLE_SSL ON CACHE INTERNAL "")
set(CURL_USE_LIBPSL OFF CACHE INTERNAL "")
set(CURL_WERROR OFF CACHE INTERNAL "")
set(PICKY_COMPILER OFF CACHE INTERNAL "")
set(BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(CURL_USE_SCHANNEL ON CACHE INTERNAL "")

# TODO: Make this enable on linux only
# find_package(OpenSSL QUIET)
# if(OPENSSL_FOUND)
#     message(STATUS "Linux: Found OpenSSL (Static). Bundling into app.")
#     set(OPENSSL_USE_STATIC_LIBS TRUE)
#     set(CURL_USE_OPENSSL ON CACHE INTERNAL "")
# else()
#     message(WARNING "Linux: OpenSSL not found. Curl might build without HTTPS!")
# endif()

add_subdirectory(deps/ggml)
add_subdirectory(deps/glfw)
add_subdirectory(deps/curl)
add_subdirectory(deps/libfyaml)

function(print_all_targets dir)
    get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target ${targets})
        message(STATUS "Active Target: ${target}")
    endforeach()

    get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir ${subdirs})
        print_all_targets(${subdir})
    endforeach()
endfunction()
print_all_targets(${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS)

set(OUT_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXPECTED_TARGETS ggml ggml-base ggml-cpu glfw fyaml libcurl_static)
message(STATUS "Build dir: ${OUT_DIR}")
# string(TOUPPER ${CMAKE_BUILD_TYPE} build_type)
# message(STATUS "ARCHIVE_OUTPUT_DIRECTORY_${build_type} = ${OUT_DIR}")
# message(STATUS "LIBRARY_OUTPUT_DIRECTORY_${build_type} = ${OUT_DIR}")

foreach(tgt ${EXPECTED_TARGETS})
    if(TARGET ${tgt})
        message(STATUS "Applying output paths to target: ${tgt}")
        set_target_properties(${tgt} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY "${OUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${OUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY "${OUT_DIR}"
            ARCHIVE_OUTPUT_DIRECTORY_${build_type} "${OUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_${build_type} "${OUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_${build_type} "${OUT_DIR}"
        )
    endif()
endforeach()
message(STATUS)
