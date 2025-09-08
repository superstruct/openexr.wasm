# OpenEXR WASM Benchmarks
cmake_minimum_required(VERSION 3.24)

# Benchmark configuration
set(BENCHMARK_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
set(BENCHMARK_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/results)

# Create output directory
file(MAKE_DIRECTORY ${BENCHMARK_OUTPUT_DIR})

# Base benchmark flags - optimized for performance measurement
set(WASM_BENCHMARK_FLAGS
    ${WASM_BASE_FLAGS}
    ${WASM_PERF_FLAGS}
    -s INITIAL_MEMORY=512MB
    -s MAXIMUM_MEMORY=2GB
    -s PRECISE_F32=1
    --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/benchmark-setup.js
)

# Core benchmarks
set(CORE_BENCHMARKS
    bench_compression_codecs
    bench_io_performance
    bench_rgba_interface
    bench_multichannel_io
    bench_memory_usage
)

# SIMD benchmarks (conditional)
if(ENABLE_SIMD)
    set(SIMD_BENCHMARKS
        bench_simd_compression
        bench_simd_tone_mapping
        bench_simd_pixel_operations
    )
    set(SIMD_BENCHMARK_FLAGS ${WASM_BENCHMARK_FLAGS} -msimd128)
endif()

# Threading benchmarks (conditional)
if(ENABLE_THREADING)
    set(THREADING_BENCHMARKS
        bench_parallel_compression
        bench_threaded_io
        bench_concurrent_processing
    )
    set(THREADING_BENCHMARK_FLAGS ${WASM_BENCHMARK_FLAGS} -pthread)
endif()

# WebGPU benchmarks (conditional)
if(ENABLE_WEBGPU)
    set(WEBGPU_BENCHMARKS
        bench_webgpu_compute
        bench_webgpu_tone_mapping
        bench_webgpu_filtering
    )
endif()

# Create core benchmark executables
foreach(benchmark_name ${CORE_BENCHMARKS})
    add_executable(${benchmark_name} ${benchmark_name}.cpp common/benchmark_utils.cpp)
    target_link_libraries(${benchmark_name} openexr-wasm)
    target_include_directories(${benchmark_name} PRIVATE common)
    target_compile_options(${benchmark_name} PRIVATE ${WASM_BENCHMARK_FLAGS})
    target_link_options(${benchmark_name} PRIVATE ${WASM_BENCHMARK_FLAGS})
    
    # Custom benchmark runner
    add_custom_target(run_${benchmark_name}
        COMMAND node ${CMAKE_CURRENT_BINARY_DIR}/${benchmark_name}.js > ${BENCHMARK_OUTPUT_DIR}/${benchmark_name}.json
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${benchmark_name}
        COMMENT "Running benchmark: ${benchmark_name}"
    )
endforeach()

# SIMD benchmarks
if(ENABLE_SIMD)
    foreach(benchmark_name ${SIMD_BENCHMARKS})
        add_executable(${benchmark_name} ${benchmark_name}.cpp common/benchmark_utils.cpp)
        target_link_libraries(${benchmark_name} openexr-wasm)
        target_include_directories(${benchmark_name} PRIVATE common)
        target_compile_options(${benchmark_name} PRIVATE ${SIMD_BENCHMARK_FLAGS})
        target_link_options(${benchmark_name} PRIVATE ${SIMD_BENCHMARK_FLAGS})
        
        add_custom_target(run_${benchmark_name}
            COMMAND node --experimental-wasm-simd ${CMAKE_CURRENT_BINARY_DIR}/${benchmark_name}.js > ${BENCHMARK_OUTPUT_DIR}/${benchmark_name}.json
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            DEPENDS ${benchmark_name}
            COMMENT "Running SIMD benchmark: ${benchmark_name}"
        )
    endforeach()
endif()

# Threading benchmarks
if(ENABLE_THREADING)
    foreach(benchmark_name ${THREADING_BENCHMARKS})
        add_executable(${benchmark_name} ${benchmark_name}.cpp common/benchmark_utils.cpp)
        target_link_libraries(${benchmark_name} openexr-wasm)
        target_include_directories(${benchmark_name} PRIVATE common)
        target_compile_options(${benchmark_name} PRIVATE ${THREADING_BENCHMARK_FLAGS})
        target_link_options(${benchmark_name} PRIVATE ${THREADING_BENCHMARK_FLAGS})
        
        add_custom_target(run_${benchmark_name}
            COMMAND node --experimental-wasm-threads ${CMAKE_CURRENT_BINARY_DIR}/${benchmark_name}.js > ${BENCHMARK_OUTPUT_DIR}/${benchmark_name}.json
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            DEPENDS ${benchmark_name}
            COMMENT "Running threading benchmark: ${benchmark_name}"
        )
    endforeach()
endif()

# WebGPU benchmarks
if(ENABLE_WEBGPU)
    foreach(benchmark_name ${WEBGPU_BENCHMARKS})
        add_executable(${benchmark_name} ${benchmark_name}.cpp common/benchmark_utils.cpp)
        target_link_libraries(${benchmark_name} openexr-wasm)
        target_include_directories(${benchmark_name} PRIVATE common)
        target_compile_options(${benchmark_name} PRIVATE ${WASM_BENCHMARK_FLAGS})
        target_link_options(${benchmark_name} PRIVATE ${WASM_BENCHMARK_FLAGS})
        
        add_custom_target(run_${benchmark_name}
            COMMAND node --experimental-webgpu ${CMAKE_CURRENT_BINARY_DIR}/${benchmark_name}.js > ${BENCHMARK_OUTPUT_DIR}/${benchmark_name}.json
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            DEPENDS ${benchmark_name}
            COMMENT "Running WebGPU benchmark: ${benchmark_name}"
        )
    endforeach()
endif()

# Comprehensive benchmark suite
add_custom_target(run_all_benchmarks
    COMMENT "Running complete benchmark suite"
)

# Add dependencies for all benchmarks
foreach(benchmark_name ${CORE_BENCHMARKS})
    add_dependencies(run_all_benchmarks run_${benchmark_name})
endforeach()

if(ENABLE_SIMD)
    foreach(benchmark_name ${SIMD_BENCHMARKS})
        add_dependencies(run_all_benchmarks run_${benchmark_name})
    endforeach()
endif()

if(ENABLE_THREADING)
    foreach(benchmark_name ${THREADING_BENCHMARKS})
        add_dependencies(run_all_benchmarks run_${benchmark_name})
    endforeach()
endif()

if(ENABLE_WEBGPU)
    foreach(benchmark_name ${WEBGPU_BENCHMARKS})
        add_dependencies(run_all_benchmarks run_${benchmark_name})
    endforeach()
endif()

# Benchmark data generation
add_custom_target(generate_benchmark_data
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_benchmark_images.py ${BENCHMARK_DATA_DIR}
    COMMENT "Generating benchmark test data"
)

# Results aggregation
add_custom_target(aggregate_benchmark_results
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/aggregate_results.py ${BENCHMARK_OUTPUT_DIR}
    WORKING_DIRECTORY ${BENCHMARK_OUTPUT_DIR}
    COMMENT "Aggregating benchmark results"
)

# Performance comparison
add_custom_target(compare_performance
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/compare_performance.py ${BENCHMARK_OUTPUT_DIR}
    WORKING_DIRECTORY ${BENCHMARK_OUTPUT_DIR}
    COMMENT "Comparing performance across variants"
)

# Cleanup benchmark results
add_custom_target(clean_benchmark_results
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${BENCHMARK_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BENCHMARK_OUTPUT_DIR}
    COMMENT "Cleaning benchmark results"
)