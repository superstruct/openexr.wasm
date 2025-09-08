# OpenEXR WASM Test Suite
cmake_minimum_required(VERSION 3.24)

# Test configuration
set(TEST_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
set(TEST_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/output)

# Create output directory
file(MAKE_DIRECTORY ${TEST_OUTPUT_DIR})

# Base test flags
set(WASM_TEST_FLAGS
    ${WASM_BASE_FLAGS}
    -s INITIAL_MEMORY=128MB
    -s MAXIMUM_MEMORY=512MB
    -s NODERAWFS=1
    --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/test-setup.js
)

# Core OpenEXR functionality tests
set(CORE_TESTS
    test_basic_io
    test_compression_codecs
    test_rgba_interface
    test_deep_images
    test_multipart_files
    test_attributes
    test_channels
    test_pixel_types
)

# SIMD-specific tests (conditional)
if(ENABLE_SIMD)
    set(SIMD_TESTS
        test_simd_compression
        test_simd_tone_mapping
        test_simd_conversion
    )
endif()

# Threading tests (conditional)
if(ENABLE_THREADING)
    set(THREADING_TESTS
        test_threaded_io
        test_parallel_compression
        test_concurrent_access
    )
endif()

# WebGPU tests (conditional)
if(ENABLE_WEBGPU)
    set(WEBGPU_TESTS
        test_webgpu_compute
        test_webgpu_tone_mapping
        test_webgpu_filtering
    )
endif()

# Create test executables
foreach(test_name ${CORE_TESTS})
    add_executable(${test_name} ${test_name}.cpp common/test_utils.cpp)
    target_link_libraries(${test_name} openexr-wasm)
    target_include_directories(${test_name} PRIVATE common)
    target_compile_options(${test_name} PRIVATE ${WASM_TEST_FLAGS})
    target_link_options(${test_name} PRIVATE ${WASM_TEST_FLAGS})
    
    # Node.js test wrapper
    add_test(NAME ${test_name}
        COMMAND node ${CMAKE_CURRENT_BINARY_DIR}/${test_name}.js
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT "TEST_DATA_DIR=${TEST_DATA_DIR};TEST_OUTPUT_DIR=${TEST_OUTPUT_DIR}"
        TIMEOUT 120
    )
endforeach()

# SIMD tests
if(ENABLE_SIMD)
    foreach(test_name ${SIMD_TESTS})
        add_executable(${test_name} ${test_name}.cpp common/test_utils.cpp)
        target_link_libraries(${test_name} openexr-wasm)
        target_include_directories(${test_name} PRIVATE common)
        target_compile_options(${test_name} PRIVATE ${WASM_TEST_FLAGS} -msimd128)
        target_link_options(${test_name} PRIVATE ${WASM_TEST_FLAGS} -msimd128)
        
        add_test(NAME ${test_name}
            COMMAND node --experimental-wasm-simd ${CMAKE_CURRENT_BINARY_DIR}/${test_name}.js
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT "TEST_DATA_DIR=${TEST_DATA_DIR};TEST_OUTPUT_DIR=${TEST_OUTPUT_DIR}"
            TIMEOUT 180
        )
    endforeach()
endif()

# Threading tests
if(ENABLE_THREADING)
    foreach(test_name ${THREADING_TESTS})
        add_executable(${test_name} ${test_name}.cpp common/test_utils.cpp)
        target_link_libraries(${test_name} openexr-wasm)
        target_include_directories(${test_name} PRIVATE common)
        target_compile_options(${test_name} PRIVATE ${WASM_TEST_FLAGS} -pthread)
        target_link_options(${test_name} PRIVATE ${WASM_TEST_FLAGS} -pthread)
        
        add_test(NAME ${test_name}
            COMMAND node --experimental-wasm-threads ${CMAKE_CURRENT_BINARY_DIR}/${test_name}.js
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT "TEST_DATA_DIR=${TEST_DATA_DIR};TEST_OUTPUT_DIR=${TEST_OUTPUT_DIR}"
            TIMEOUT 240
        )
    endforeach()
endif()

# WebGPU tests
if(ENABLE_WEBGPU)
    foreach(test_name ${WEBGPU_TESTS})
        add_executable(${test_name} ${test_name}.cpp common/test_utils.cpp)
        target_link_libraries(${test_name} openexr-wasm)
        target_include_directories(${test_name} PRIVATE common)
        target_compile_options(${test_name} PRIVATE ${WASM_TEST_FLAGS})
        target_link_options(${test_name} PRIVATE ${WASM_TEST_FLAGS})
        
        add_test(NAME ${test_name}
            COMMAND node --experimental-webgpu ${CMAKE_CURRENT_BINARY_DIR}/${test_name}.js
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT "TEST_DATA_DIR=${TEST_DATA_DIR};TEST_OUTPUT_DIR=${TEST_OUTPUT_DIR};WEBGPU_REQUIRED=1"
            TIMEOUT 300
        )
    endforeach()
endif()

# Integration tests
set(INTEGRATION_TESTS
    test_ecosystem_integration
    test_filesystem_patterns
    test_memory_management
    test_error_handling
)

foreach(test_name ${INTEGRATION_TESTS})
    add_executable(${test_name} integration/${test_name}.cpp common/test_utils.cpp)
    target_link_libraries(${test_name} openexr-wasm)
    target_include_directories(${test_name} PRIVATE common)
    target_compile_options(${test_name} PRIVATE ${WASM_TEST_FLAGS})
    target_link_options(${test_name} PRIVATE ${WASM_TEST_FLAGS})
    
    add_test(NAME ${test_name}
        COMMAND node ${CMAKE_CURRENT_BINARY_DIR}/${test_name}.js
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT "TEST_DATA_DIR=${TEST_DATA_DIR};TEST_OUTPUT_DIR=${TEST_OUTPUT_DIR}"
        TIMEOUT 180
    )
endforeach()

# Test data generation
add_custom_target(generate_test_data
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_test_images.py ${TEST_DATA_DIR}
    COMMENT "Generating test image data"
)

# Custom test target for all variants
add_custom_target(test_all_variants
    COMMENT "Running tests for all build variants"
)

# Cleanup target
add_custom_target(clean_test_output
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_OUTPUT_DIR}
    COMMENT "Cleaning test output directory"
)