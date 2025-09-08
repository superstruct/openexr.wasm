/*
 * OpenEXR WASM Benchmark Utilities
 * Copyright 2025 superstruct ltd, New Zealand
 * Licensed under the OpenEXR license (Apache 2.0)
 */

#ifndef OPENEXR_BENCHMARK_UTILS_H
#define OPENEXR_BENCHMARK_UTILS_H

#include <string>
#include <vector>
#include <chrono>
#include <iostream>
#include <map>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace OpenEXRBenchmark {

// Performance measurement
class BenchmarkTimer {
private:
    #ifdef __EMSCRIPTEN__
    double start_time;
    #else
    std::chrono::high_resolution_clock::time_point start_time;
    #endif

public:
    BenchmarkTimer();
    void reset();
    double elapsed_ms() const;
    double elapsed_us() const;
};

// Benchmark result
struct BenchmarkResult {
    std::string name;
    std::string variant;
    double min_time_ms;
    double max_time_ms;
    double avg_time_ms;
    double median_time_ms;
    double std_dev_ms;
    size_t iterations;
    size_t data_size_bytes;
    double throughput_mbps;
    std::map<std::string, double> custom_metrics;
    
    BenchmarkResult(const std::string& name, const std::string& variant = "default");
    void add_timing(double time_ms);
    void set_data_size(size_t bytes);
    void add_custom_metric(const std::string& key, double value);
    void finalize();
    void print_summary() const;
    std::string to_json() const;
};

// Benchmark suite
class BenchmarkSuite {
public:
    std::string suite_name;
    std::vector<BenchmarkResult> results;
    
    BenchmarkSuite(const std::string& name);
    void add_result(const BenchmarkResult& result);
    void print_summary() const;
    void export_json(const std::string& filename) const;
    void export_csv(const std::string& filename) const;
};

// System information
struct SystemInfo {
    std::string platform;
    std::string browser;
    std::string wasm_version;
    bool simd_support;
    bool threading_support;
    bool webgpu_support;
    size_t memory_limit_mb;
    
    SystemInfo();
    void detect();
    std::string to_json() const;
};

// Benchmark runner macros
#define BENCHMARK_ITERATIONS 10
#define WARMUP_ITERATIONS 3

#define BENCHMARK_START(suite, name, variant) \
    do { \
        std::cout << "Benchmarking " << name << " (" << variant << ")..." << std::endl; \
        BenchmarkResult result(name, variant); \
        BenchmarkTimer timer; \
        \
        /* Warmup */ \
        for (int _i = 0; _i < WARMUP_ITERATIONS; ++_i) { \
            timer.reset();

#define BENCHMARK_END(suite) \
            /* Warmup - ignore timing */ \
        } \
        \
        /* Actual benchmarking */ \
        for (int _i = 0; _i < BENCHMARK_ITERATIONS; ++_i) { \
            timer.reset();

#define BENCHMARK_FINALIZE(suite) \
            result.add_timing(timer.elapsed_ms()); \
        } \
        \
        result.finalize(); \
        suite.add_result(result); \
        result.print_summary(); \
    } while(0)

#define BENCHMARK_SET_DATA_SIZE(size_bytes) \
    result.set_data_size(size_bytes);

#define BENCHMARK_ADD_METRIC(key, value) \
    result.add_custom_metric(key, value);

// Memory measurement utilities
size_t get_wasm_memory_usage();
size_t get_wasm_heap_size();

// Performance utilities
void prevent_optimization_elimination(void* ptr);
void flush_cpu_caches();

// Data generation for benchmarks
void generate_rgba_benchmark_data(float* data, size_t width, size_t height, int pattern = 0);
void generate_hdr_benchmark_data(float* data, size_t width, size_t height, float max_luminance = 10.0f);

// WASM-specific benchmarking
#ifdef __EMSCRIPTEN__
inline void setup_benchmark_environment() {
    // Mount MEMFS for benchmark files
    EM_ASM({
        try {
            FS.mkdir('/benchmark-data');
            FS.mkdir('/benchmark-output');
        } catch (e) {
            // Directories may already exist
        }
    });
}

inline double get_precise_time() {
    return emscripten_get_now();
}

inline size_t get_wasm_memory_size() {
    return EM_ASM_INT({
        return wasmMemory.buffer.byteLength;
    });
}

inline bool is_simd_enabled() {
    return EM_ASM_INT({
        return typeof WebAssembly.SIMD !== 'undefined' ? 1 : 0;
    }) == 1;
}

inline bool is_threading_enabled() {
    return EM_ASM_INT({
        return typeof SharedArrayBuffer !== 'undefined' ? 1 : 0;
    }) == 1;
}

inline bool is_webgpu_available() {
    return EM_ASM_INT({
        return typeof navigator !== 'undefined' && 
               typeof navigator.gpu !== 'undefined' ? 1 : 0;
    }) == 1;
}
#endif

} // namespace OpenEXRBenchmark

#endif // OPENEXR_BENCHMARK_UTILS_H