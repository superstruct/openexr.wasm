/*
 * OpenEXR WASM Test Utilities
 * Copyright 2025 superstruct ltd, New Zealand
 * Licensed under the OpenEXR license (Apache 2.0)
 */

#include "test_utils.h"
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfPixelType.h>
#include <OpenEXR/ImfCompression.h>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>

using namespace Imf;
using namespace Imath;

namespace OpenEXRTest {

TestResult::TestResult(const std::string& name) 
    : test_name(name), passed(false), duration_ms(0) {}

void TestResult::pass(double duration) {
    passed = true;
    duration_ms = duration;
}

void TestResult::fail(const std::string& error, double duration) {
    passed = false;
    error_message = error;
    duration_ms = duration;
}

void TestResult::print() const {
    const char* status = passed ? "PASS" : "FAIL";
    std::cout << "[" << status << "] " << test_name 
              << " (" << duration_ms << "ms)";
    if (!passed && !error_message.empty()) {
        std::cout << " - " << error_message;
    }
    std::cout << std::endl;
}

TestSuite::TestSuite(const std::string& name) : suite_name(name) {}

void TestSuite::add_result(const TestResult& result) {
    results.push_back(result);
}

void TestSuite::print_summary() const {
    int passed = 0, failed = 0;
    double total_time = 0.0;
    
    for (const auto& result : results) {
        if (result.passed) passed++;
        else failed++;
        total_time += result.duration_ms;
    }
    
    std::cout << "\n=== Test Suite: " << suite_name << " ===" << std::endl;
    std::cout << "Tests: " << (passed + failed) << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Total time: " << total_time << "ms" << std::endl;
    
    if (failed > 0) {
        std::cout << "\nFailures:" << std::endl;
        for (const auto& result : results) {
            if (!result.passed) {
                result.print();
            }
        }
    }
}

bool TestSuite::all_passed() const {
    for (const auto& result : results) {
        if (!result.passed) return false;
    }
    return true;
}

Timer::Timer() {
    reset();
}

void Timer::reset() {
    #ifdef __EMSCRIPTEN__
    start_time = emscripten_get_now();
    #else
    start_time = std::chrono::high_resolution_clock::now();
    #endif
}

double Timer::elapsed_ms() const {
    #ifdef __EMSCRIPTEN__
    return emscripten_get_now() - start_time;
    #else
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time);
    return duration.count() / 1000.0;
    #endif
}

// Test image generation
void generate_rgba_test_image(const std::string& filename, int width, int height) {
    RgbaOutputFile file(filename.c_str(), width, height, WRITE_RGBA);
    
    Array2D<Rgba> pixels(height, width);
    
    // Generate gradient pattern with HDR values
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float fx = float(x) / width;
            float fy = float(y) / height;
            
            // HDR gradient with values > 1.0
            pixels[y][x].r = fx * 2.0f;  // Red channel: 0-2
            pixels[y][x].g = fy * 3.0f;  // Green channel: 0-3
            pixels[y][x].b = (fx + fy) * 0.5f * 4.0f;  // Blue channel: 0-4
            pixels[y][x].a = 1.0f;       // Alpha: opaque
        }
    }
    
    file.setFrameBuffer(&pixels[0][0] - width * 0, 1, width);
    file.writePixels(height);
}

void generate_multichannel_test_image(const std::string& filename, int width, int height) {
    Header header(width, height);
    
    // Add custom channels
    header.channels().insert("R", Channel(FLOAT));
    header.channels().insert("G", Channel(FLOAT));
    header.channels().insert("B", Channel(FLOAT));
    header.channels().insert("A", Channel(FLOAT));
    header.channels().insert("Z", Channel(FLOAT));  // Depth
    header.channels().insert("Normal.X", Channel(HALF));
    header.channels().insert("Normal.Y", Channel(HALF));
    header.channels().insert("Normal.Z", Channel(HALF));
    
    OutputFile file(filename.c_str(), header);
    
    // Allocate pixel data
    Array2D<float> r_pixels(height, width);
    Array2D<float> g_pixels(height, width);
    Array2D<float> b_pixels(height, width);
    Array2D<float> a_pixels(height, width);
    Array2D<float> z_pixels(height, width);
    Array2D<half> nx_pixels(height, width);
    Array2D<half> ny_pixels(height, width);
    Array2D<half> nz_pixels(height, width);
    
    // Generate test pattern
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float fx = float(x) / width;
            float fy = float(y) / height;
            
            r_pixels[y][x] = sin(fx * M_PI * 4) * 2.0f + 1.0f;
            g_pixels[y][x] = cos(fy * M_PI * 4) * 2.0f + 1.0f;
            b_pixels[y][x] = sin((fx + fy) * M_PI * 2) * 3.0f + 2.0f;
            a_pixels[y][x] = 1.0f;
            z_pixels[y][x] = fx + fy;  // Depth gradient
            
            // Normal vectors pointing roughly upward
            nx_pixels[y][x] = half((fx - 0.5f) * 0.2f);
            ny_pixels[y][x] = half((fy - 0.5f) * 0.2f);
            nz_pixels[y][x] = half(0.9f);
        }
    }
    
    // Set frame buffer
    FrameBuffer framebuffer;
    framebuffer.insert("R", Slice(FLOAT, (char*)&r_pixels[0][0], 
                                  sizeof(r_pixels[0][0]), sizeof(r_pixels[0][0]) * width));
    framebuffer.insert("G", Slice(FLOAT, (char*)&g_pixels[0][0], 
                                  sizeof(g_pixels[0][0]), sizeof(g_pixels[0][0]) * width));
    framebuffer.insert("B", Slice(FLOAT, (char*)&b_pixels[0][0], 
                                  sizeof(b_pixels[0][0]), sizeof(b_pixels[0][0]) * width));
    framebuffer.insert("A", Slice(FLOAT, (char*)&a_pixels[0][0], 
                                  sizeof(a_pixels[0][0]), sizeof(a_pixels[0][0]) * width));
    framebuffer.insert("Z", Slice(FLOAT, (char*)&z_pixels[0][0], 
                                  sizeof(z_pixels[0][0]), sizeof(z_pixels[0][0]) * width));
    framebuffer.insert("Normal.X", Slice(HALF, (char*)&nx_pixels[0][0], 
                                         sizeof(nx_pixels[0][0]), sizeof(nx_pixels[0][0]) * width));
    framebuffer.insert("Normal.Y", Slice(HALF, (char*)&ny_pixels[0][0], 
                                         sizeof(ny_pixels[0][0]), sizeof(ny_pixels[0][0]) * width));
    framebuffer.insert("Normal.Z", Slice(HALF, (char*)&nz_pixels[0][0], 
                                         sizeof(nz_pixels[0][0]), sizeof(nz_pixels[0][0]) * width));
    
    file.setFrameBuffer(framebuffer);
    file.writePixels(height);
}

bool verify_rgba_image(const std::string& filename, int expected_width, int expected_height, 
                       float tolerance) {
    try {
        RgbaInputFile file(filename.c_str());
        
        Box2i data_window = file.dataWindow();
        int width = data_window.max.x - data_window.min.x + 1;
        int height = data_window.max.y - data_window.min.y + 1;
        
        if (width != expected_width || height != expected_height) {
            std::cerr << "Image dimensions mismatch: expected " 
                      << expected_width << "x" << expected_height
                      << ", got " << width << "x" << height << std::endl;
            return false;
        }
        
        Array2D<Rgba> pixels(height, width);
        file.setFrameBuffer(&pixels[0][0] - data_window.min.y * width - data_window.min.x, 1, width);
        file.readPixels(data_window.min.y, data_window.max.y);
        
        // Basic sanity check - verify some pixels are non-zero
        bool has_data = false;
        for (int y = 0; y < height && !has_data; ++y) {
            for (int x = 0; x < width && !has_data; ++x) {
                if (pixels[y][x].r > tolerance || pixels[y][x].g > tolerance || pixels[y][x].b > tolerance) {
                    has_data = true;
                }
            }
        }
        
        return has_data;
    } catch (const std::exception& e) {
        std::cerr << "Error verifying image: " << e.what() << std::endl;
        return false;
    }
}

bool images_match(const std::string& file1, const std::string& file2, float tolerance) {
    try {
        RgbaInputFile input1(file1.c_str());
        RgbaInputFile input2(file2.c_str());
        
        Box2i dw1 = input1.dataWindow();
        Box2i dw2 = input2.dataWindow();
        
        if (dw1.min.x != dw2.min.x || dw1.min.y != dw2.min.y ||
            dw1.max.x != dw2.max.x || dw1.max.y != dw2.max.y) {
            return false;
        }
        
        int width = dw1.max.x - dw1.min.x + 1;
        int height = dw1.max.y - dw1.min.y + 1;
        
        Array2D<Rgba> pixels1(height, width);
        Array2D<Rgba> pixels2(height, width);
        
        input1.setFrameBuffer(&pixels1[0][0] - dw1.min.y * width - dw1.min.x, 1, width);
        input2.setFrameBuffer(&pixels2[0][0] - dw2.min.y * width - dw2.min.x, 1, width);
        
        input1.readPixels(dw1.min.y, dw1.max.y);
        input2.readPixels(dw2.min.y, dw2.max.y);
        
        // Compare pixels
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const Rgba& p1 = pixels1[y][x];
                const Rgba& p2 = pixels2[y][x];
                
                if (std::abs(p1.r - p2.r) > tolerance ||
                    std::abs(p1.g - p2.g) > tolerance ||
                    std::abs(p1.b - p2.b) > tolerance ||
                    std::abs(p1.a - p2.a) > tolerance) {
                    return false;
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

std::string get_test_data_dir() {
    const char* env_dir = std::getenv("TEST_DATA_DIR");
    return env_dir ? std::string(env_dir) : "./data";
}

std::string get_test_output_dir() {
    const char* env_dir = std::getenv("TEST_OUTPUT_DIR");
    return env_dir ? std::string(env_dir) : "./output";
}

} // namespace OpenEXRTest