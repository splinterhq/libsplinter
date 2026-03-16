cmake_minimum_required(VERSION 3.16)
project(Splinter VERSION 1.1.6 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SPLINTER_VERSION_STRING "1.1.6")
add_definitions(-DSPLINTER_VERSION="${SPLINTER_VERSION_STRING}")

# --- Build Options ---
option(WITH_EMBEDDINGS "Enable Nomic/LLM embedding support" ON)
option(WITH_NUMA       "Enable NUMA memory affinity support" OFF)
option(WITH_LUA        "Enable Lua CLI integration" ON)
option(WITH_LLAMA      "Enable llama.cpp inference sidecar" OFF)
option(WITH_RUST       "Enable Rust bindings via Cargo" OFF)

# --- Dependency Detection ---
find_package(PkgConfig REQUIRED)
find_program(VALGRIND_EXEC NAMES valgrind)

if(WITH_LUA)
    pkg_check_modules(LUA lua5.4)
    if(LUA_FOUND)
        add_definitions(-DHAVE_LUA)
    else()
        message(FATAL_ERROR "Lua 5.4 not found. Disable WITH_LUA to skip.")
    endif()
endif()

if(WITH_NUMA)
    find_library(NUMA_LIB NAMES numa HINTS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib)
    find_path(NUMA_INCLUDE_DIR NAMES numa.h HINTS /usr/include)
    if(NUMA_LIB AND NUMA_INCLUDE_DIR)
        add_definitions(-DSPLINTER_NUMA_AFFINITY)
    else()
        message(FATAL_ERROR "libnuma-dev not found. Disable WITH_NUMA to skip.")
    endif()
endif()

find_path(VALGRIND_INCLUDE_DIR names valgrind/valgrind.h)
if(VALGRIND_INCLUDE_DIR)
    add_definitions(-DHAVE_VALGRIND_H)
endif()

add_compile_options(-O3 -Wall -Wextra -D_GNU_SOURCE -fPIC)

if(WITH_EMBEDDINGS)
    add_definitions(-DSPLINTER_EMBEDDINGS)
endif()

# --- Library Targets ---
add_library(splinter_shared SHARED splinter.c)
set_target_properties(splinter_shared PROPERTIES OUTPUT_NAME splinter)

if(WITH_NUMA)
    target_link_libraries(splinter_shared PRIVATE ${NUMA_LIB})
    target_include_directories(splinter_shared PRIVATE ${NUMA_INCLUDE_DIR})
endif()

# Persistent Core Objects and Library
add_library(splinter_p_obj OBJECT splinter.c)
target_compile_definitions(splinter_p_obj PRIVATE SPLINTER_PERSISTENT)
target_compile_options(splinter_p_obj PRIVATE -fPIC)

add_library(splinter_p_shared SHARED $<TARGET_OBJECTS:splinter_p_obj>)
set_target_properties(splinter_p_shared PROPERTIES OUTPUT_NAME splinter_p)

if(WITH_NUMA)
    target_link_libraries(splinter_p_shared PRIVATE ${NUMA_LIB})
endif()

# --- CLI Targets ---
file(GLOB CLI_SOURCES "splinter_cli_*.c")

# Standard CLI
add_executable(splinter_cli ${CLI_SOURCES} 3rdparty/linenoise.c 3rdparty/libgrawk.c)
target_link_libraries(splinter_cli PRIVATE splinter_shared ${LUA_LDFLAGS})
target_include_directories(splinter_cli PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
    ${LUA_INCLUDE_DIRS}
)

# Persistent CLI
add_executable(splinterp_cli ${CLI_SOURCES} 3rdparty/linenoise.c 3rdparty/libgrawk.c)
target_compile_definitions(splinterp_cli PRIVATE SPLINTER_PERSISTENT)
target_include_directories(splinterp_cli PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty 
    ${LUA_INCLUDE_DIRS}
)
target_link_libraries(splinterp_cli PRIVATE splinter_p_shared ${LUA_LDFLAGS})

# --- llama.cpp Inference Sidecar ---
if(WITH_LLAMA)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    find_library(LLAMA_LIB NAMES llama HINTS /usr/local/lib /usr/lib /opt/llama.cpp/build)
    find_path(LLAMA_INCLUDE_DIR NAMES llama.h HINTS /usr/local/include /usr/include /opt/llama.cpp)

    if(LLAMA_LIB AND LLAMA_INCLUDE_DIR)
        add_executable(splinference splinference.cpp)
        target_include_directories(splinference PRIVATE ${LLAMA_INCLUDE_DIR})
        target_link_libraries(splinference PRIVATE splinter_shared ${LLAMA_LIB})
        
        add_executable(splinferencep splinference.cpp)
        target_compile_definitions(splinferencep PRIVATE SPLINTER_PERSISTENT)
        target_include_directories(splinferencep PRIVATE ${LLAMA_INCLUDE_DIR})
        target_link_libraries(splinferencep PRIVATE splinter_p_shared ${LLAMA_LIB})
    else()
        message(FATAL_ERROR "llama.cpp not found. Disable WITH_LLAMA to skip.")
    endif()
endif()

# --- Installation Logic ---
include(GNUInstallDirs)

# 1. Libraries
install(TARGETS splinter_shared splinter_p_shared
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# 2. Binaries (Standard, Persistent, and Sidecars)
install(TARGETS splinter_cli splinterp_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(WITH_LLAMA AND TARGET splinference AND TARGET splinferencep)
    install(TARGETS splinference splinferencep RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# 3. Headers
install(FILES splinter.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# 4. System Symlinks and ldconfig
install(CODE "
    execute_process(COMMAND ln -sf ${CMAKE_INSTALL_FULL_BINDIR}/splinter_cli ${CMAKE_INSTALL_FULL_BINDIR}/splinterctl)
    message(STATUS \"Created symlink: splinterctl -> splinter_cli\")

    execute_process(COMMAND ln -sf ${CMAKE_INSTALL_FULL_BINDIR}/splinterp_cli ${CMAKE_INSTALL_FULL_BINDIR}/splinterpctl)
    message(STATUS \"Created symlink: splinterpctl -> splinterp_cli\")
")

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    install(CODE "execute_process(COMMAND ldconfig)")
endif()

# --- Local Development Symlinks ---
add_custom_command(TARGET splinter_cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:splinter_cli> ${CMAKE_CURRENT_BINARY_DIR}/splinterctl)

add_custom_command(TARGET splinterp_cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:splinterp_cli> ${CMAKE_CURRENT_BINARY_DIR}/splinterpctl)

# --- Rust Bindings ---
if(WITH_RUST)
    find_program(CARGO_EXEC NAMES cargo)
    if(CARGO_EXEC)
        set(RUST_ENV "CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/rust_target")
        set(RUST_LINK_SEARCH "SPLINTER_LIB_DIR=${CMAKE_CURRENT_BINARY_DIR}")
        add_custom_target(rust_bindings ALL
            COMMAND ${CMAKE_COMMAND} -E env ${RUST_ENV} ${RUST_LINK_SEARCH} ${CARGO_EXEC} build --release
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/rust
            DEPENDS splinter_shared splinter_p_shared
        )
    endif()
endif()

# --- Testing and Documentation (Remain as defined in original) ---
enable_testing()
add_executable(splinter_test splinter_test.c)
target_link_libraries(splinter_test PRIVATE splinter_shared)
add_executable(splinter_stress splinter_stress.c)
target_link_libraries(splinter_stress PRIVATE splinter_shared)
add_executable(splinterp_test splinter_test.c)
target_compile_definitions(splinterp_test PRIVATE SPLINTER_PERSISTENT)
target_link_libraries(splinterp_test PRIVATE splinter_p_shared)
add_executable(splinterp_stress splinter_stress.c)
target_compile_definitions(splinterp_stress PRIVATE SPLINTER_PERSISTENT)
target_link_libraries(splinterp_stress PRIVATE splinter_p_shared)

install(TARGETS splinter_test splinter_stress splinterp_test splinterp_stress 
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES splinterrc_example DESTINATION ${CMAKE_INSTALL_DATADIR}/splinter RENAME splinterrc.example)
