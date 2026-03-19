cmake_minimum_required(VERSION 3.16)
project(Magnaundasoni VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(MAGNAUNDASONI_BUILD_TESTS   "Build unit tests"                   OFF)
option(MAGNAUNDASONI_ENABLE_RT     "Enable hardware ray-tracing backends" OFF)
option(MAGNAUNDASONI_ENABLE_SIMD   "Enable SIMD optimisations"           ON)

# ---------------------------------------------------------------------------
# Compiler requirements
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Fall back to C++17 when the compiler lacks C++20 support
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++20" HAS_CXX20)
if(NOT HAS_CXX20)
    set(CMAKE_CXX_STANDARD 17)
endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
set(MAG_PUBLIC_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Magnaundasoni.h
)

set(MAG_CORE_SOURCES
    src/core/Scene.cpp
    src/core/MaterialPresets.cpp
    src/core/BVH.cpp
    src/core/SpatialGrid.cpp
    src/core/ChunkManager.cpp
    src/core/EdgeExtractor.cpp
    src/core/ThreadPool.cpp
)

set(MAG_CORE_HEADERS
    src/core/Types.h
    src/core/Scene.h
    src/core/MaterialPresets.h
    src/core/BVH.h
    src/core/SpatialGrid.h
    src/core/ChunkManager.h
    src/core/EdgeExtractor.h
    src/core/ThreadPool.h
)

set(MAG_RENDER_SOURCES
    src/render/BandProcessor.cpp
    src/render/DirectPathSolver.cpp
    src/render/ReflectionSolver.cpp
    src/render/DiffractionSolver.cpp
    src/render/ReverbEstimator.cpp
    src/render/TemporalSmoother.cpp
    src/render/OutputMixer.cpp
    src/render/AcousticRenderer.cpp
)

set(MAG_RENDER_HEADERS
    src/render/BandProcessor.h
    src/render/DirectPathSolver.h
    src/render/ReflectionSolver.h
    src/render/DiffractionSolver.h
    src/render/ReverbEstimator.h
    src/render/TemporalSmoother.h
    src/render/OutputMixer.h
    src/render/AcousticRenderer.h
)

# Engine entry-point that implements the C ABI
set(MAG_ENGINE_SOURCES
    src/Engine.cpp
)

# ---------------------------------------------------------------------------
# Shared library target
# ---------------------------------------------------------------------------
add_library(magnaundasoni SHARED
    ${MAG_PUBLIC_HEADER}
    ${MAG_CORE_HEADERS}
    ${MAG_CORE_SOURCES}
    ${MAG_RENDER_HEADERS}
    ${MAG_RENDER_SOURCES}
    ${MAG_ENGINE_SOURCES}
)

target_compile_definitions(magnaundasoni PRIVATE MAGNAUNDASONI_EXPORTS)

target_include_directories(magnaundasoni
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ---------------------------------------------------------------------------
# Platform-specific settings
# ---------------------------------------------------------------------------
if(WIN32)
    target_compile_definitions(magnaundasoni PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(APPLE)
    set_target_properties(magnaundasoni PROPERTIES
        INSTALL_RPATH "@loader_path"
    )
else()
    target_link_libraries(magnaundasoni PRIVATE pthread)
    set_target_properties(magnaundasoni PROPERTIES
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# Math library (Linux/macOS)
if(UNIX)
    target_link_libraries(magnaundasoni PRIVATE m)
endif()

# ---------------------------------------------------------------------------
# SIMD
# ---------------------------------------------------------------------------
if(MAGNAUNDASONI_ENABLE_SIMD)
    target_compile_definitions(magnaundasoni PRIVATE MAGNAUNDASONI_SIMD=1)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(magnaundasoni PRIVATE -msse4.1)
    endif()
endif()

# ---------------------------------------------------------------------------
# Hardware RT backends (stub – actual GPU code lives in backends/)
# ---------------------------------------------------------------------------
if(MAGNAUNDASONI_ENABLE_RT)
    target_compile_definitions(magnaundasoni PRIVATE MAGNAUNDASONI_RT=1)
endif()

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(magnaundasoni PRIVATE
        -Wall -Wextra -Wpedantic -Wno-unused-parameter
    )
elseif(MSVC)
    target_compile_options(magnaundasoni PRIVATE /W4 /wd4100)
endif()

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
install(TARGETS magnaundasoni
    LIBRARY  DESTINATION lib
    ARCHIVE  DESTINATION lib
    RUNTIME  DESTINATION bin
)
install(FILES ${MAG_PUBLIC_HEADER} DESTINATION include)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(MAGNAUNDASONI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit
                     ${CMAKE_CURRENT_BINARY_DIR}/tests/unit)
endif()
