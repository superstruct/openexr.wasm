/*
 * OpenEXR WASM Test Utilities
 * Copyright 2025 superstruct ltd, New Zealand
 * Licensed under the OpenEXR license (Apache 2.0)
 */

#ifndef OPENEXR_TEST_UTILS_H
#define OPENEXR_TEST_UTILS_H

#include <string>
#include <vector>
#include <chrono>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace OpenEXRTest {

// Test result tracking
struct TestResult {
    std::string test_name;
    bool passed;
    std::string error_message;
    double duration_ms;
    
    TestResult(const std::string& name);
    void pass(double duration);
    void fail(const std::string& error, double duration);
    void print() const;
};

// Test suite management
class TestSuite {
public:
    std::string suite_name;
    std::vector<TestResult> results;
    
    TestSuite(const std::string& name);
    void add_result(const TestResult& result);
    void print_summary() const;
    bool all_passed() const;
};

// High-resolution timer
class Timer {
private:
    #ifdef __EMSCRIPTEN__
    double start_time;
    #else
    std::chrono::high_resolution_clock::time_point start_time;
    #endif

public:
    Timer();
    void reset();
    double elapsed_ms() const;
};

// Test helper macros
#define TEST_START(suite, name) \
    do { \
        std::cout << "Running " << name << "..." << std::endl; \
        Timer timer; \
        TestResult result(name); \
        try {

#define TEST_END(suite) \
        result.pass(timer.elapsed_ms()); \
        } catch (const std::exception& e) { \
            result.fail(e.what(), timer.elapsed_ms()); \
        } catch (...) { \
            result.fail("Unknown exception", timer.elapsed_ms()); \
        } \
        suite.add_result(result); \
        result.print(); \
    } while(0)

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error("Assertion failed: " #condition); \
    }

#define ASSERT_FALSE(condition) \
    if (condition) { \
        throw std::runtime_error("Assertion failed: !(" #condition ")"); \
    }

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        throw std::runtime_error("Assertion failed: " #expected " == " #actual); \
    }

#define ASSERT_NEAR(expected, actual, tolerance) \
    if (std::abs((expected) - (actual)) > (tolerance)) { \
        throw std::runtime_error("Assertion failed: " #expected " ≈ " #actual " (tolerance: " #tolerance ")"); \
    }

// Image generation and verification utilities
void generate_rgba_test_image(const std::string& filename, int width, int height);
void generate_multichannel_test_image(const std::string& filename, int width, int height);
bool verify_rgba_image(const std::string& filename, int expected_width, int expected_height, float tolerance = 1e-6f);
bool images_match(const std::string& file1, const std::string& file2, float tolerance = 1e-6f);

// Environment utilities
std::string get_test_data_dir();
std::string get_test_output_dir();

// WASM-specific test utilities
#ifdef __EMSCRIPTEN__
inline void setup_wasm_filesystem() {
    // Mount MEMFS for test files
    EM_ASM({
        try {
            FS.mkdir('/test-data');
            FS.mkdir('/test-output');
        } catch (e) {
            // Directories may already exist
        }
    });
}

inline bool check_simd_support() {
    return EM_ASM_INT({
        return typeof WebAssembly.SIMD !== 'undefined' ? 1 : 0;
    }) == 1;
}

inline bool check_threading_support() {
    return EM_ASM_INT({
        return typeof SharedArrayBuffer !== 'undefined' ? 1 : 0;
    }) == 1;
}

inline bool check_webgpu_support() {
    return EM_ASM_INT({
        return typeof navigator !== 'undefined' && 
               typeof navigator.gpu !== 'undefined' ? 1 : 0;
    }) == 1;
}
#endif

} // namespace OpenEXRTest

#endif // OPENEXR_TEST_UTILS_H