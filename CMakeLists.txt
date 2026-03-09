cmake_minimum_required(VERSION 3.10)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

include(CMakeDependentOption)

option(DTV "Enable video output" OFF)
option(PIRANHA_ENABLED "Enable scripting input" ON)
cmake_dependent_option(DISCORD_ENABLED "Enable Discord Rich Presence" ON WIN32 OFF)
option(ENGINE_SIM_BUILD_TESTS "Build unit tests" ON)

# Default to compiling as Release in single-configuration generators
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

if (DTV)
    add_compile_definitions(ATG_ENGINE_SIM_VIDEO_CAPTURE)
endif (DTV)

if (PIRANHA_ENABLED)
    add_compile_definitions(ATG_ENGINE_SIM_PIRANHA_ENABLED)
endif (PIRANHA_ENABLED)

if (DISCORD_ENABLED)
    add_compile_definitions(ATG_ENGINE_SIM_DISCORD_ENABLED)
endif (DISCORD_ENABLED)

# Enable group projects in folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

project(engine-sim VERSION 0.1.12)

set(CMAKE_CXX_STANDARD 17)

include(cmake/engine_sim_sources.cmake)

if (ENGINE_SIM_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL
        https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    set_property(TARGET gmock PROPERTY FOLDER "gtest")
    set_property(TARGET gmock_main PROPERTY FOLDER "gtest")
    set_property(TARGET gtest PROPERTY FOLDER "gtest")
    set_property(TARGET gtest_main PROPERTY FOLDER "gtest")
endif()

add_subdirectory(dependencies)

include(cmake/engine_sim_targets.cmake)

# ========================================================
# Generated build info

# Fallback to '..' if not provided
if (NOT DEFINED ENGINE_SIM_DATA_ROOT)
    set(ENGINE_SIM_DATA_ROOT ".." CACHE STRING "Where to look for data")
endif()
message(STATUS "Data root set to '${ENGINE_SIM_DATA_ROOT}'")

configure_file(include/build_info.h.in
    generated/build_info.h @ONLY)
target_include_directories(engine-sim-app
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

include(cmake/engine_sim_tests.cmake)
