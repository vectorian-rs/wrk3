cmake_minimum_required(VERSION 3.14)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    execute_process(COMMAND sw_vers -productVersion
        OUTPUT_VARIABLE CMAKE_OSX_DEPLOYMENT_TARGET OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

project(wrk3 C CXX ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ==============================================================================
# Dependencies (git submodules under deps/)
# ==============================================================================

# Block system OpenSSL/LibreSSL so nothing accidentally links against them.
# s2n-tls's try_compile feature probes can pick up system OpenSSL 3.x headers
# and enable provider APIs (EVP_MD_fetch etc.) that aws-lc doesn't implement,
# causing link failures.
set(CMAKE_DISABLE_FIND_PACKAGE_OpenSSL TRUE CACHE BOOL "" FORCE)

# --- AWS-LC (libcrypto - replaces OpenSSL) ---
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(SEARCH_LIBCRYPTO OFF CACHE BOOL "" FORCE)
set(BUILD_TOOL OFF CACHE BOOL "" FORCE)
set(BUILD_LIBSSL OFF CACHE BOOL "" FORCE)

set(CMAKE_WARN_DEPRECATED OFF)
add_subdirectory(deps/aws-lc EXCLUDE_FROM_ALL)
set(CMAKE_WARN_DEPRECATED ON)

# Ensure s2n-tls feature probes compile against aws-lc headers, not system
# OpenSSL. try_compile doesn't fully resolve INTERFACE_INCLUDE_DIRECTORIES
# from non-imported targets, so we inject aws-lc's include dirs globally.
get_target_property(_aws_lc_inc crypto INTERFACE_INCLUDE_DIRECTORIES)
if(_aws_lc_inc)
    include_directories(BEFORE SYSTEM ${_aws_lc_inc})
endif()

# --- s2n-tls (TLS implementation, uses AWS-LC's crypto target) ---
set(UNSAFE_TREAT_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

add_subdirectory(deps/s2n-tls EXCLUDE_FROM_ALL)

# --- LuaJIT (scripting engine - uses its own Makefile, not CMake) ---
set(LUAJIT_DIR ${CMAKE_SOURCE_DIR}/deps/luajit/src)
if(APPLE)
    set(LUAJIT_EXTRA_ENV "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

add_custom_command(
    OUTPUT ${LUAJIT_DIR}/libluajit.a ${LUAJIT_DIR}/luajit
    COMMAND ${CMAKE_COMMAND} -E env ${LUAJIT_EXTRA_ENV}
        make -C ${LUAJIT_DIR}
        BUILDMODE=static "XCFLAGS=-DLUAJIT_ENABLE_GC64"
    COMMENT "Building LuaJIT..."
)
add_custom_target(luajit_build DEPENDS ${LUAJIT_DIR}/libluajit.a)

# Create imported library target for LuaJIT
add_library(luajit_lib STATIC IMPORTED GLOBAL)
set_target_properties(luajit_lib PROPERTIES
    IMPORTED_LOCATION ${LUAJIT_DIR}/libluajit.a
    INTERFACE_INCLUDE_DIRECTORIES ${LUAJIT_DIR}
)
add_dependencies(luajit_lib luajit_build)

# Compile Lua script to bytecode (emit .c to avoid ARM64 page alignment issues)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/bytecode.c
    COMMAND ${LUAJIT_DIR}/luajit -b
        ${CMAKE_SOURCE_DIR}/src/wrk.lua
        ${CMAKE_BINARY_DIR}/bytecode.c
    DEPENDS luajit_build ${CMAKE_SOURCE_DIR}/src/wrk.lua
    WORKING_DIRECTORY ${LUAJIT_DIR}
    COMMENT "Compiling Lua bytecode..."
)

# ==============================================================================
# wrk3 executable
# ==============================================================================

set(WRK_SOURCES
    src/wrk.c
    src/net.c
    src/ssl.c
    src/aprintf.c
    src/stats.c
    src/script.c
    src/units.c
    src/ae.c
    src/zmalloc.c
    src/http_parser.c
    src/tinymt64.c
    src/hdr_histogram.c
    ${CMAKE_BINARY_DIR}/bytecode.c
)

add_executable(wrk ${WRK_SOURCES})

target_include_directories(wrk PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(wrk PRIVATE
    s2n
    crypto
    luajit_lib
    pthread
)

target_compile_options(wrk PRIVATE -Wall -O2 -D_REENTRANT)

# --- Platform-specific settings ---
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        target_link_options(wrk PRIVATE -pagezero_size 10000 -image_base 100000000)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(wrk PRIVATE _POSIX_C_SOURCE=200809L _BSD_SOURCE)
    target_link_libraries(wrk PRIVATE dl)
    target_link_options(wrk PRIVATE -Wl,-E)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_compile_definitions(wrk PRIVATE _DECLARE_C99_LDBL_MATH)
    target_link_options(wrk PRIVATE -Wl,-E)
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    target_compile_definitions(wrk PRIVATE _PTHREADS _POSIX_C_SOURCE=200112L)
    target_link_libraries(wrk PRIVATE socket)
endif()
