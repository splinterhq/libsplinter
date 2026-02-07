cmake_minimum_required(VERSION 3.16)
project(Splinter VERSION 0.9.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SPLINTER_VERSION_STRING "0.9.1")
add_definitions(-DSPLINTER_VERSION="${SPLINTER_VERSION_STRING}")

# --- Build Options ---
option(WITH_EMBEDDINGS "Enable Nomic/LLM embedding support" ON)
option(WITH_NUMA       "Enable NUMA memory affinity support" OFF)
option(WITH_LUA        "Enable Lua CLI integration" ON)

# --- Dependency Detection ---
find_package(PkgConfig REQUIRED)

# Detect Lua
if(WITH_LUA)
    pkg_check_modules(LUA lua5.4)
    if(LUA_FOUND)
        add_definitions(-DHAVE_LUA)
    else()
        message(FATAL_ERROR "Lua 5.4 not found. Disable WITH_LUA to skip.")
    endif()
endif()

# Detect NUMA
if(WITH_NUMA)
# Search specifically in the Debian/Ubuntu multi-arch path
    find_library(NUMA_LIB 
        NAMES numa 
        HINTS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib
    )
    find_path(NUMA_INCLUDE_DIR 
        NAMES numa.h 
        HINTS /usr/include
    )
    #find_library(NUMA_LIB names numa)
    #find_path(NUMA_INCLUDE_DIR names numa.h)
    if(NUMA_LIB AND NUMA_INCLUDE_DIR)
        add_definitions(-DSPLINTER_NUMA_AFFINITY)
    else()
        message(FATAL_ERROR "libnuma-dev not found. Disable WITH_NUMA to skip.")
    endif()
endif()

# Detect Valgrind
find_path(VALGRIND_INCLUDE_DIR names valgrind/valgrind.h)
if(VALGRIND_INCLUDE_DIR)
    add_definitions(-DHAVE_VALGRIND_H)
endif()

# --- Compiler Flags ---
# Replicating your -O3 and -fPIC logic 
add_compile_options(-O3 -Wall -Wextra -D_GNU_SOURCE -fPIC)

if(WITH_EMBEDDINGS)
    add_definitions(-DSPLINTER_EMBEDDINGS)
endif()

# --- Targets ---
add_library(splinter_shared SHARED splinter.c)
set_target_properties(splinter_shared PROPERTIES OUTPUT_NAME splinter)

# Link NUMA if enabled
if(WITH_NUMA)
    target_link_libraries(splinter_shared PRIVATE ${NUMA_LIB})
    target_include_directories(splinter_shared PRIVATE ${NUMA_INCLUDE_DIR})
endif()

# CLI Target (Replicating splinter_cli) 
file(GLOB CLI_SOURCES "splinter_cli_*.c")
add_executable(splinter_cli ${CLI_SOURCES} 3rdparty/linenoise.c 3rdparty/libgrawk.c)
target_link_libraries(splinter_cli PRIVATE splinter_shared ${LUA_LDFLAGS})
target_include_directories(splinter_cli PRIVATE ${LUA_INCLUDE_DIRS})

# --- Installation Logic ---
include(GNUInstallDirs)

# Install Libraries
install(TARGETS splinter_shared
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Install Binaries
install(TARGETS splinter_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install Headers
install(FILES splinter.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Replicate the Symlink and ldconfig logic [cite: 10]
install(CODE "
    execute_process(COMMAND ln -sf ${CMAKE_INSTALL_FULL_BINDIR}/splinter_cli ${CMAKE_INSTALL_FULL_BINDIR}/splinterctl)
    message(STATUS \"Created symlink: splinterctl -> splinter_cli\")
")

# Optional: Run ldconfig on Linux systems [cite: 10]
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    install(CODE "execute_process(COMMAND ldconfig)")
endif()

# CLI Target
file(GLOB CLI_SOURCES "splinter_cli_*.c")


# Tell CMake where to find the headers for these files 
target_include_directories(splinter_cli PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
    ${LUA_INCLUDE_DIRS}
)

target_link_libraries(splinter_cli PRIVATE splinter_shared ${LUA_LDFLAGS})

# --- Persistent Core Object ---
add_library(splinter_p_obj OBJECT splinter.c)
target_compile_definitions(splinter_p_obj PRIVATE SPLINTER_PERSISTENT)
target_compile_options(splinter_p_obj PRIVATE -fPIC)

# Persistent Shared Library
add_library(splinter_p_shared SHARED $<TARGET_OBJECTS:splinter_p_obj>)
set_target_properties(splinter_p_shared PROPERTIES OUTPUT_NAME splinter_p)

# Link NUMA to persistent core if enabled
if(WITH_NUMA)
    target_link_libraries(splinter_p_shared PRIVATE ${NUMA_LIB})
endif()

# --- Persistent CLI (splinterp_cli) ---
add_executable(splinterp_cli 
    ${CLI_SOURCES} 
    3rdparty/linenoise.c 
    3rdparty/libgrawk.c
)
target_compile_definitions(splinterp_cli PRIVATE SPLINTER_PERSISTENT)
target_include_directories(splinterp_cli PRIVATE 3rdparty ${LUA_INCLUDE_DIRS})
target_link_libraries(splinterp_cli PRIVATE splinter_p_shared ${LUA_LDFLAGS})

# --- Local Symlinks for Development/Testing ---

# Symlink for splinter_cli -> splinterctl
add_custom_command(
    TARGET splinter_cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
            $<TARGET_FILE_NAME:splinter_cli> 
            ${CMAKE_CURRENT_BINARY_DIR}/splinterctl
    COMMENT "Creating local symlink: splinterctl"
)

# Symlink for splinterp_cli -> splinterpctl
add_custom_command(
    TARGET splinterp_cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
            $<TARGET_FILE_NAME:splinterp_cli> 
            ${CMAKE_CURRENT_BINARY_DIR}/splinterpctl
    COMMENT "Creating local symlink: splinterpctl"
)

# The lua test suite
set(LUA_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/test.lua")
set(LUA_TEST_BINARY "${CMAKE_CURRENT_BINARY_DIR}/test.lua")
add_custom_command(
    OUTPUT "${LUA_TEST_BINARY}"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "${LUA_TEST_SOURCE}" "${LUA_TEST_BINARY}"
    DEPENDS "${LUA_TEST_SOURCE}"
    COMMENT "Symlinking test.lua to build directory"
)
add_custom_target(copy_test_lua ALL DEPENDS "${LUA_TEST_BINARY}")

# --- Test and Stress Tools ---

# splinter_test: Verifies basic functionality and integrity 
add_executable(splinter_test splinter_test.c)
target_link_libraries(splinter_test PRIVATE splinter_shared)

# splinter_stress: The MRSW gauntlet (where you hit 3M+ ops/sec) 
add_executable(splinter_stress splinter_stress.c)
target_link_libraries(splinter_stress PRIVATE splinter_shared)

# --- Persistent Variants ---

# splinterp_test: Verifies persistent-mode specific logic
add_executable(splinterp_test splinter_test.c)
target_compile_definitions(splinterp_test PRIVATE SPLINTER_PERSISTENT)
target_link_libraries(splinterp_test PRIVATE splinter_p_shared)

# splinterp_stress: Stress tests the disk-backed bus
add_executable(splinterp_stress splinter_stress.c)
target_compile_definitions(splinterp_stress PRIVATE SPLINTER_PERSISTENT)
target_link_libraries(splinterp_stress PRIVATE splinter_p_shared)

# --- CTest Integration (The "One-Button" Test) ---
enable_testing()

add_test(NAME "Core_Functionality" COMMAND splinter_test)
add_test(NAME "Persistent_Functionality" COMMAND splinterp_test)
add_test(NAME "CLI_Regression_Suite" COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/splinterctl_tests.sh)

# --- Final Install update ---
install(TARGETS splinter_test splinter_stress splinterp_test splinterp_stress 
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

