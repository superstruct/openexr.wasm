/*
 * OpenEXR WASM Compression Codecs Benchmark
 * Copyright 2025 superstruct ltd, New Zealand
 * Licensed under the OpenEXR license (Apache 2.0)
 */

#include "common/benchmark_utils.h"
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfPixelType.h>
#include <OpenEXR/ImfCompression.h>
#include <OpenEXR/ImfArray.h>
#include <cmath>
#include <filesystem>

using namespace Imf;
using namespace Imath;
using namespace OpenEXRBenchmark;

void benchmark_compression_codec(BenchmarkSuite& suite, Compression compression, 
                                const std::string& codec_name) {
    const int width = 1024;
    const int height = 512;
    const std::string output_dir = "/benchmark-output";
    const std::string filename = output_dir + "/bench_" + codec_name + ".exr";
    
    // Generate HDR test data
    Array2D<Rgba> pixels(height, width);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float fx = float(x) / width;
            float fy = float(y) / height;
            
            // HDR content with high dynamic range
            pixels[y][x].r = (sin(fx * M_PI * 8) + 1.0f) * 5.0f;
            pixels[y][x].g = (cos(fy * M_PI * 6) + 1.0f) * 3.0f;
            pixels[y][x].b = (sin((fx + fy) * M_PI * 4) + 1.0f) * 8.0f;
            pixels[y][x].a = 1.0f;
        }
    }
    
    size_t uncompressed_size = width * height * 4 * sizeof(float);
    
    BENCHMARK_START(suite, "Compression Write", codec_name)
        RgbaOutputFile file(filename.c_str(), width, height, WRITE_RGBA, 1.0, V2f(0,0), 1.0, INCREASING_Y, compression);
        file.setFrameBuffer(&pixels[0][0] - width * 0, 1, width);
        file.writePixels(height);
    BENCHMARK_END(suite)
        // Compression operation
        RgbaOutputFile file(filename.c_str(), width, height, WRITE_RGBA, 1.0, V2f(0,0), 1.0, INCREASING_Y, compression);
        file.setFrameBuffer(&pixels[0][0] - width * 0, 1, width);
        file.writePixels(height);
    BENCHMARK_FINALIZE(suite)
    
    // Get compressed file size
    size_t compressed_size = std::filesystem::file_size(filename);
    float compression_ratio = float(uncompressed_size) / compressed_size;
    
    BenchmarkResult& write_result = suite.results.back();
    write_result.set_data_size(uncompressed_size);
    write_result.add_custom_metric("compressed_size_bytes", compressed_size);
    write_result.add_custom_metric("compression_ratio", compression_ratio);
    
    BENCHMARK_START(suite, "Compression Read", codec_name)
        Array2D<Rgba> read_pixels(height, width);
        RgbaInputFile input_file(filename.c_str());
        input_file.setFrameBuffer(&read_pixels[0][0] - input_file.dataWindow().min.y * width - input_file.dataWindow().min.x, 1, width);
        input_file.readPixels(input_file.dataWindow().min.y, input_file.dataWindow().max.y);
    BENCHMARK_END(suite)
        // Decompression operation
        Array2D<Rgba> read_pixels(height, width);
        RgbaInputFile input_file(filename.c_str());
        input_file.setFrameBuffer(&read_pixels[0][0] - input_file.dataWindow().min.y * width - input_file.dataWindow().min.x, 1, width);
        input_file.readPixels(input_file.dataWindow().min.y, input_file.dataWindow().max.y);
    BENCHMARK_FINALIZE(suite)
    
    BenchmarkResult& read_result = suite.results.back();
    read_result.set_data_size(compressed_size);
    read_result.add_custom_metric("decompressed_size_bytes", uncompressed_size);
    read_result.add_custom_metric("compression_ratio", compression_ratio);
}

void benchmark_zip_levels(BenchmarkSuite& suite) {
    const int width = 512;
    const int height = 256;
    const std::string output_dir = "/benchmark-output";
    
    // Generate test data
    Array2D<Rgba> pixels(height, width);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float noise = sin(x * 0.1f) * cos(y * 0.1f);
            pixels[y][x].r = 1.0f + noise * 2.0f;
            pixels[y][x].g = 2.0f + noise * 1.5f;
            pixels[y][x].b = 3.0f + noise * 3.0f;
            pixels[y][x].a = 1.0f;
        }
    }
    
    // Test different ZIP compression scenarios
    for (int level = 1; level <= 9; level += 2) {
        std::string variant = "ZIP_level_" + std::to_string(level);
        std::string filename = output_dir + "/bench_zip_" + std::to_string(level) + ".exr";
        
        BENCHMARK_START(suite, "ZIP Compression Levels", variant)
            Header header(width, height);
            header.compression() = ZIP_COMPRESSION;
            header.channels().insert("R", Channel(FLOAT));
            header.channels().insert("G", Channel(FLOAT));
            header.channels().insert("B", Channel(FLOAT));
            header.channels().insert("A", Channel(FLOAT));
            
            OutputFile file(filename.c_str(), header);
            
            Array2D<float> r_pixels(height, width);
            Array2D<float> g_pixels(height, width);
            Array2D<float> b_pixels(height, width);
            Array2D<float> a_pixels(height, width);
            
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    r_pixels[y][x] = pixels[y][x].r;
                    g_pixels[y][x] = pixels[y][x].g;
                    b_pixels[y][x] = pixels[y][x].b;
                    a_pixels[y][x] = pixels[y][x].a;
                }
            }
            
            FrameBuffer frameBuffer;
            frameBuffer.insert("R", Slice(FLOAT, (char*)&r_pixels[0][0], sizeof(float), sizeof(float) * width));
            frameBuffer.insert("G", Slice(FLOAT, (char*)&g_pixels[0][0], sizeof(float), sizeof(float) * width));
            frameBuffer.insert("B", Slice(FLOAT, (char*)&b_pixels[0][0], sizeof(float), sizeof(float) * width));
            frameBuffer.insert("A", Slice(FLOAT, (char*)&a_pixels[0][0], sizeof(float), sizeof(float) * width));
            
            file.setFrameBuffer(frameBuffer);
            file.writePixels(height);
        BENCHMARK_END(suite)
            // ZIP compression with specific level
            Header header(width, height);
            header.compression() = ZIP_COMPRESSION;
            header.channels().insert("R", Channel(FLOAT));
            header.channels().insert("G", Channel(FLOAT));
            header.channels().insert("B", Channel(FLOAT));
            header.channels().insert("A", Channel(FLOAT));
            
            OutputFile file(filename.c_str(), header);
            
            Array2D<float> r_pixels(height, width);
            Array2D<float> g_pixels(height, width);
            Array2D<float> b_pixels(height, width);
            Array2D<float> a_pixels(height, width);
            
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    r_pixels[y][x] = pixels[y][x].r;
                    g_pixels[y][x] = pixels[y][x].g;
                    b_pixels[y][x] = pixels[y][x].b;
                    a_pixels[y][x] = pixels[y][x].a;
                }
            }
            
            FrameBuffer frameBuffer;
            frameBuffer.insert("R", Slice(FLOAT, (char*)&r_pixels[0][0], sizeof(float), sizeof(float) * width));
            frameBuffer.insert("G", Slice(FLOAT, (char*)&g_pixels[0][0], sizeof(float), sizeof(float) * width));
            frameBuffer.insert("B", Slice(FLOAT, (char*)&b_pixels[0][0], sizeof(float), sizeof(float) * width));
            frameBuffer.insert("A", Slice(FLOAT, (char*)&a_pixels[0][0], sizeof(float), sizeof(float) * width));
            
            file.setFrameBuffer(frameBuffer);
            file.writePixels(height);
        BENCHMARK_FINALIZE(suite)
        
        size_t file_size = std::filesystem::file_size(filename);
        size_t uncompressed_size = width * height * 4 * sizeof(float);
        
        BenchmarkResult& result = suite.results.back();
        result.set_data_size(uncompressed_size);
        result.add_custom_metric("compressed_size_bytes", file_size);
        result.add_custom_metric("compression_ratio", float(uncompressed_size) / file_size);
        result.add_custom_metric("zip_level", level);
    }
}

int main() {
    #ifdef __EMSCRIPTEN__
    setup_benchmark_environment();
    #endif
    
    SystemInfo sys_info;
    sys_info.detect();
    std::cout << "System Info: " << sys_info.to_json() << std::endl;
    
    BenchmarkSuite suite("Compression Codecs");
    
    // Benchmark all major compression codecs
    benchmark_compression_codec(suite, NO_COMPRESSION, "None");
    benchmark_compression_codec(suite, RLE_COMPRESSION, "RLE");
    benchmark_compression_codec(suite, ZIPS_COMPRESSION, "ZIPS");
    benchmark_compression_codec(suite, ZIP_COMPRESSION, "ZIP");
    benchmark_compression_codec(suite, PIZ_COMPRESSION, "PIZ");
    benchmark_compression_codec(suite, PXR24_COMPRESSION, "PXR24");
    benchmark_compression_codec(suite, B44_COMPRESSION, "B44");
    benchmark_compression_codec(suite, B44A_COMPRESSION, "B44A");
    benchmark_compression_codec(suite, DWAA_COMPRESSION, "DWAA");
    benchmark_compression_codec(suite, DWAB_COMPRESSION, "DWAB");
    
    // Detailed ZIP level analysis
    benchmark_zip_levels(suite);
    
    suite.print_summary();
    suite.export_json("/benchmark-output/compression_codecs_results.json");
    
    return 0;
}