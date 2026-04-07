cmake_minimum_required(VERSION 3.14)
project(wrk3 C CXX ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ==============================================================================
# Dependencies (fetched automatically, like Cargo deps in Rust)
# ==============================================================================

# --- AWS-LC (libcrypto - replaces OpenSSL) ---
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(SEARCH_LIBCRYPTO OFF CACHE BOOL "" FORCE)

FetchContent_Declare(aws_lc
    GIT_REPOSITORY https://github.com/aws/aws-lc.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
message(STATUS "Fetching AWS-LC...")
FetchContent_MakeAvailable(aws_lc)

# --- s2n-tls (TLS implementation, uses AWS-LC's crypto target) ---
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(UNSAFE_TREAT_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(s2n_tls
    GIT_REPOSITORY https://github.com/aws/s2n-tls.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
message(STATUS "Fetching s2n-tls...")
FetchContent_MakeAvailable(s2n_tls)

# --- LuaJIT (scripting engine - uses its own Makefile) ---
FetchContent_Declare(luajit
    GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
    GIT_TAG        v2.1
    GIT_SHALLOW    TRUE
)
# Only populate, don't add_subdirectory (LuaJIT uses Make, not CMake)
FetchContent_GetProperties(luajit)
if(NOT luajit_POPULATED)
    message(STATUS "Fetching LuaJIT...")
    FetchContent_Populate(luajit)
endif()

# Build LuaJIT via Make
set(LUAJIT_DIR ${luajit_SOURCE_DIR}/src)
if(APPLE)
    execute_process(COMMAND sw_vers -productVersion OUTPUT_VARIABLE MACOS_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(LUAJIT_EXTRA_ENV "MACOSX_DEPLOYMENT_TARGET=${MACOS_VERSION}")
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
    m
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
