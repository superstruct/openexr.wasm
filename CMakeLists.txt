cmake_minimum_required(VERSION 3.24)
project(openexr-wasm VERSION 3.4.0 LANGUAGES C CXX)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Validate Emscripten toolchain
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This project requires Emscripten toolchain. Use: emcmake cmake ..")
endif()

# Configure build options
option(ENABLE_SIMD "Enable WASM SIMD optimizations" ON)
option(ENABLE_THREADING "Enable pthread support" OFF)
option(ENABLE_WEBGPU "Enable WebGPU compute acceleration for image processing" OFF)
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)
option(BUILD_EXAMPLES "Build usage examples" ON)

# Set Emscripten-specific configuration
set(CMAKE_EXECUTABLE_SUFFIX ".js")

# Base flags for all builds
set(WASM_BASE_FLAGS
    -s WASM=1
    -s MODULARIZE=1
    -s EXPORT_ES6=1
    -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPU8','addFunction','removeFunction','FS']
    -s INITIAL_MEMORY=256MB
    -s MAXIMUM_MEMORY=2GB
    -s ALLOW_MEMORY_GROWTH=1
    -s STACK_SIZE=5MB
    -s ASSERTIONS=0
    -s FORCE_FILESYSTEM=1
    -s DISABLE_EXCEPTION_CATCHING=0
    -lidbfs.js
)

# Performance flags
set(WASM_PERF_FLAGS
    -O3
    -flto
    --closure 1
    -s NO_EXIT_RUNTIME=1
)

# SIMD support
if(ENABLE_SIMD)
    list(APPEND WASM_BASE_FLAGS -msimd128)
    add_compile_definitions(
        OPENEXR_WASM_SIMD_ENABLED=1
        OPENEXR_ENABLE_X86_SIMD_CHECK=1
    )
    # Enable SSE intrinsics that map to WASM SIMD
    list(APPEND WASM_BASE_FLAGS -msse2 -msse3 -mssse3 -msse4.1)
endif()

# Threading support
if(ENABLE_THREADING)
    list(APPEND WASM_BASE_FLAGS 
        -pthread
        -s USE_PTHREADS=1
        -s PTHREAD_POOL_SIZE=4
        -s SHARED_MEMORY=1
        -s PROXY_TO_PTHREAD=1
        -s MALLOC=mimalloc  # Better multi-threaded performance
    )
    add_compile_definitions(OPENEXR_ENABLE_THREADING=1)
else()
    add_compile_definitions(OPENEXR_ENABLE_THREADING=0)
endif()

# WebGPU support (experimental)
if(ENABLE_WEBGPU)
    add_compile_definitions(OPENEXR_WEBGPU_ENABLED=1)
    # WebGPU will be used for compute shaders in image processing
endif()

# Apply flags
add_compile_options(${WASM_BASE_FLAGS} ${WASM_PERF_FLAGS})
add_link_options(${WASM_BASE_FLAGS} ${WASM_PERF_FLAGS})

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib
    ${CMAKE_CURRENT_BINARY_DIR}/config
)

# Configure config.h for WASM
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/config/config.h
    @ONLY
)

# Dependencies - use ecosystem libs when available
find_library(IMATH_LIBRARY NAMES Imath PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../Imath.wasm/lib)
find_library(ZLIB_LIBRARY NAMES z PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../zlib.wasm/lib)
find_library(DEFLATE_LIBRARY NAMES deflate PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../libdeflate.wasm/lib)

if(IMATH_LIBRARY)
    message(STATUS "Using ecosystem Imath.wasm: ${IMATH_LIBRARY}")
    set(HAVE_IMATH 1)
    add_library(Imath::Imath INTERFACE IMPORTED)
    target_link_libraries(Imath::Imath INTERFACE ${IMATH_LIBRARY})
else()
    message(STATUS "Imath not found - using internal fetch")
    set(HAVE_IMATH 0)
    include(FetchContent)
    FetchContent_Declare(Imath
        GIT_REPOSITORY "https://github.com/AcademySoftwareFoundation/Imath.git"
        GIT_TAG "main"
    )
    FetchContent_MakeAvailable(Imath)
endif()

if(ZLIB_LIBRARY)
    message(STATUS "Using ecosystem zlib.wasm: ${ZLIB_LIBRARY}")
    set(HAVE_ZLIB 1)
else()
    message(STATUS "zlib not found - using internal")
    set(HAVE_ZLIB 0)
endif()

if(DEFLATE_LIBRARY)
    message(STATUS "Using ecosystem libdeflate.wasm: ${DEFLATE_LIBRARY}")
    set(HAVE_DEFLATE 1)
else()
    message(STATUS "libdeflate not found - using internal")
    set(HAVE_DEFLATE 0)
endif()

# OpenJPH - always use internal for now
set(HAVE_OPENJPH 0)
include(FetchContent)
FetchContent_Declare(openjph
    GIT_REPOSITORY "https://github.com/aous72/OpenJPH.git"
    GIT_TAG "master"
)
set(OJPH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OJPH_ENABLE_TIFF_SUPPORT OFF CACHE BOOL "" FORCE)
set(OJPH_BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(openjph)

# Core OpenEXR sources - modular approach
set(OPENEXR_CORE_SOURCES
    # OpenEXRCore (C API)
    src/lib/OpenEXRCore/openexr_attr.c
    src/lib/OpenEXRCore/openexr_base.c
    src/lib/OpenEXRCore/openexr_channel_list.c
    src/lib/OpenEXRCore/openexr_chunk.c
    src/lib/OpenEXRCore/openexr_coding.c
    src/lib/OpenEXRCore/openexr_compression.c
    src/lib/OpenEXRCore/openexr_context.c
    src/lib/OpenEXRCore/openexr_decode.c
    src/lib/OpenEXRCore/openexr_encode.c
    src/lib/OpenEXRCore/openexr_memory.c
    src/lib/OpenEXRCore/openexr_parse_header.c
    src/lib/OpenEXRCore/openexr_part.c
    src/lib/OpenEXRCore/openexr_part_type.c
    src/lib/OpenEXRCore/openexr_string.c
    src/lib/OpenEXRCore/openexr_string_vector.c
    src/lib/OpenEXRCore/openexr_validation.c
    src/lib/OpenEXRCore/openexr_write_header.c
)

# Compression codecs
set(OPENEXR_COMPRESSION_SOURCES
    src/lib/OpenEXRCore/internal_b44.c
    src/lib/OpenEXRCore/internal_b44_table.c
    src/lib/OpenEXRCore/internal_dwa.c
    src/lib/OpenEXRCore/internal_huf.c
    src/lib/OpenEXRCore/internal_piz.c
    src/lib/OpenEXRCore/internal_pxr24.c
    src/lib/OpenEXRCore/internal_rle.c
    src/lib/OpenEXRCore/internal_zip.c
)

# SIMD-optimized compression (conditional)
if(ENABLE_SIMD)
    set(OPENEXR_SIMD_SOURCES
        src/lib/OpenEXRCore/internal_dwa_simd.c
        src/lib/OpenEXRCore/internal_huf_simd.c
    )
endif()

# C++ API
set(OPENEXR_CPP_SOURCES
    src/lib/OpenEXR/ImfAcesFile.cpp
    src/lib/OpenEXR/ImfAttribute.cpp
    src/lib/OpenEXR/ImfBoxAttribute.cpp
    src/lib/OpenEXR/ImfChannelList.cpp
    src/lib/OpenEXR/ImfChannelListAttribute.cpp
    src/lib/OpenEXR/ImfChromaticities.cpp
    src/lib/OpenEXR/ImfChromaticitiesAttribute.cpp
    src/lib/OpenEXR/ImfCompositeDeepScanLine.cpp
    src/lib/OpenEXR/ImfCompressionAttribute.cpp
    src/lib/OpenEXR/ImfDeepCompositing.cpp
    src/lib/OpenEXR/ImfDeepFrameBuffer.cpp
    src/lib/OpenEXR/ImfDeepImageState.cpp
    src/lib/OpenEXR/ImfDeepImageStateAttribute.cpp
    src/lib/OpenEXR/ImfDeepScanLineInputFile.cpp
    src/lib/OpenEXR/ImfDeepScanLineOutputFile.cpp
    src/lib/OpenEXR/ImfDeepTiledInputFile.cpp
    src/lib/OpenEXR/ImfDeepTiledOutputFile.cpp
    src/lib/OpenEXR/ImfDoubleAttribute.cpp
    src/lib/OpenEXR/ImfEnvmap.cpp
    src/lib/OpenEXR/ImfEnvmapAttribute.cpp
    src/lib/OpenEXR/ImfFlatImageChannel.cpp
    src/lib/OpenEXR/ImfFlatImageLevel.cpp
    src/lib/OpenEXR/ImfFloatAttribute.cpp
    src/lib/OpenEXR/ImfFloatVectorAttribute.cpp
    src/lib/OpenEXR/ImfFrameBuffer.cpp
    src/lib/OpenEXR/ImfFramesPerSecond.cpp
    src/lib/OpenEXR/ImfGenericInputFile.cpp
    src/lib/OpenEXR/ImfGenericOutputFile.cpp
    src/lib/OpenEXR/ImfHeader.cpp
    src/lib/OpenEXR/ImfHuf.cpp
    src/lib/OpenEXR/ImfIO.cpp
    src/lib/OpenEXR/ImfInputFile.cpp
    src/lib/OpenEXR/ImfInputPart.cpp
    src/lib/OpenEXR/ImfInputPartData.cpp
    src/lib/OpenEXR/ImfIntAttribute.cpp
    src/lib/OpenEXR/ImfKeyCode.cpp
    src/lib/OpenEXR/ImfKeyCodeAttribute.cpp
    src/lib/OpenEXR/ImfLineOrderAttribute.cpp
    src/lib/OpenEXR/ImfLut.cpp
    src/lib/OpenEXR/ImfMatrixAttribute.cpp
    src/lib/OpenEXR/ImfMisc.cpp
    src/lib/OpenEXR/ImfMultiPartInputFile.cpp
    src/lib/OpenEXR/ImfMultiPartOutputFile.cpp
    src/lib/OpenEXR/ImfMultiView.cpp
    src/lib/OpenEXR/ImfOpaqueAttribute.cpp
    src/lib/OpenEXR/ImfOutputFile.cpp
    src/lib/OpenEXR/ImfOutputPart.cpp
    src/lib/OpenEXR/ImfOutputPartData.cpp
    src/lib/OpenEXR/ImfPartType.cpp
    src/lib/OpenEXR/ImfPixelType.cpp
    src/lib/OpenEXR/ImfPreviewImage.cpp
    src/lib/OpenEXR/ImfPreviewImageAttribute.cpp
    src/lib/OpenEXR/ImfRational.cpp
    src/lib/OpenEXR/ImfRationalAttribute.cpp
    src/lib/OpenEXR/ImfRgbaFile.cpp
    src/lib/OpenEXR/ImfRgbaYca.cpp
    src/lib/OpenEXR/ImfScanLineInputFile.cpp
    src/lib/OpenEXR/ImfStandardAttributes.cpp
    src/lib/OpenEXR/ImfStdIO.cpp
    src/lib/OpenEXR/ImfStringAttribute.cpp
    src/lib/OpenEXR/ImfStringVectorAttribute.cpp
    src/lib/OpenEXR/ImfTestFile.cpp
    src/lib/OpenEXR/ImfThreading.cpp
    src/lib/OpenEXR/ImfTileDescription.cpp
    src/lib/OpenEXR/ImfTileDescriptionAttribute.cpp
    src/lib/OpenEXR/ImfTiledInputFile.cpp
    src/lib/OpenEXR/ImfTiledMisc.cpp
    src/lib/OpenEXR/ImfTiledOutputFile.cpp
    src/lib/OpenEXR/ImfTiledRgbaFile.cpp
    src/lib/OpenEXR/ImfTimeCode.cpp
    src/lib/OpenEXR/ImfTimeCodeAttribute.cpp
    src/lib/OpenEXR/ImfVecAttribute.cpp
    src/lib/OpenEXR/ImfVersion.cpp
    src/lib/OpenEXR/ImfWav.cpp
)

# Exception handling library
set(IEX_SOURCES
    src/lib/Iex/IexBaseExc.cpp
    src/lib/Iex/IexThrowErrnoExc.cpp
)

# Threading library (conditional)
if(ENABLE_THREADING)
    set(ILMTHREAD_SOURCES
        src/lib/IlmThread/IlmThread.cpp
        src/lib/IlmThread/IlmThreadMutex.cpp
        src/lib/IlmThread/IlmThreadPool.cpp
        src/lib/IlmThread/IlmThreadSemaphore.cpp
    )
endif()

# Create core libraries
add_library(openexr-core STATIC
    ${OPENEXR_CORE_SOURCES}
    ${OPENEXR_COMPRESSION_SOURCES}
    ${OPENEXR_SIMD_SOURCES}
)

add_library(iex STATIC ${IEX_SOURCES})

if(ENABLE_THREADING)
    add_library(ilmthread STATIC ${ILMTHREAD_SOURCES})
endif()

add_library(openexr-cpp STATIC ${OPENEXR_CPP_SOURCES})

# Main OpenEXR library
add_library(openexr-wasm STATIC)
target_link_libraries(openexr-wasm 
    openexr-core 
    openexr-cpp 
    iex
)

if(ENABLE_THREADING)
    target_link_libraries(openexr-wasm ilmthread)
endif()

# Link dependencies
target_link_libraries(openexr-wasm Imath::Imath)

if(ZLIB_LIBRARY)
    target_link_libraries(openexr-wasm ${ZLIB_LIBRARY})
endif()

if(DEFLATE_LIBRARY)
    target_link_libraries(openexr-wasm ${DEFLATE_LIBRARY})
endif()

target_link_libraries(openexr-wasm openjph)

# Include directories for all targets
target_include_directories(openexr-wasm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lib/OpenEXR>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lib/OpenEXRCore>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lib/Iex>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lib/IlmThread>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/config>
)

# WASM module with JavaScript interface
add_executable(openexr-module
    wasm/openexr_wasm_wrapper.cpp
)

target_link_libraries(openexr-module openexr-wasm)

# Set output names
set_target_properties(openexr-module PROPERTIES
    OUTPUT_NAME "openexr"
    SUFFIX ".js"
)

# Install targets
install(TARGETS openexr-wasm DESTINATION lib)
install(TARGETS openexr-module DESTINATION dist)

# Install headers
install(DIRECTORY src/lib/OpenEXR/ DESTINATION include/OpenEXR
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY src/lib/OpenEXRCore/ DESTINATION include/OpenEXRCore
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY src/lib/Iex/ DESTINATION include/Iex
    FILES_MATCHING PATTERN "*.h")

if(ENABLE_THREADING)
    install(DIRECTORY src/lib/IlmThread/ DESTINATION include/IlmThread
        FILES_MATCHING PATTERN "*.h")
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Generate package config
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/openexr.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/openexr.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/openexr.pc
    DESTINATION lib/pkgconfig)