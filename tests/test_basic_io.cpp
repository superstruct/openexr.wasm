/*
 * OpenEXR WASM Basic I/O Tests
 * Copyright 2025 superstruct ltd, New Zealand
 * Licensed under the OpenEXR license (Apache 2.0)
 */

#include "common/test_utils.h"
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfPixelType.h>
#include <OpenEXR/ImfArray.h>
#include <iostream>
#include <filesystem>

using namespace Imf;
using namespace Imath;
using namespace OpenEXRTest;

void test_rgba_write_read() {
    const int width = 256;
    const int height = 128;
    const std::string output_dir = get_test_output_dir();
    const std::string filename = output_dir + "/test_rgba_basic.exr";
    
    // Write test image
    generate_rgba_test_image(filename, width, height);
    
    // Verify file exists
    ASSERT_TRUE(std::filesystem::exists(filename));
    
    // Read back and verify
    ASSERT_TRUE(verify_rgba_image(filename, width, height));
}

void test_multichannel_write_read() {
    const int width = 128;
    const int height = 64;
    const std::string output_dir = get_test_output_dir();
    const std::string filename = output_dir + "/test_multichannel.exr";
    
    generate_multichannel_test_image(filename, width, height);
    
    // Read back and verify structure
    InputFile file(filename.c_str());
    const Header& header = file.header();
    
    ASSERT_EQ(width, header.dataWindow().max.x - header.dataWindow().min.x + 1);
    ASSERT_EQ(height, header.dataWindow().max.y - header.dataWindow().min.y + 1);
    
    // Verify channels exist
    const ChannelList& channels = header.channels();
    ASSERT_TRUE(channels.findChannel("R") != nullptr);
    ASSERT_TRUE(channels.findChannel("G") != nullptr);
    ASSERT_TRUE(channels.findChannel("B") != nullptr);
    ASSERT_TRUE(channels.findChannel("A") != nullptr);
    ASSERT_TRUE(channels.findChannel("Z") != nullptr);
    ASSERT_TRUE(channels.findChannel("Normal.X") != nullptr);
    ASSERT_TRUE(channels.findChannel("Normal.Y") != nullptr);
    ASSERT_TRUE(channels.findChannel("Normal.Z") != nullptr);
}

void test_header_attributes() {
    const std::string output_dir = get_test_output_dir();
    const std::string filename = output_dir + "/test_attributes.exr";
    
    // Create header with custom attributes
    Header header(256, 128);
    header.insert("description", StringAttribute("Test image with custom attributes"));
    header.insert("pixelAspectRatio", FloatAttribute(1.5f));
    header.insert("screenWindowCenter", V2fAttribute(V2f(0.5f, 0.3f)));
    header.insert("compression", CompressionAttribute(ZIP_COMPRESSION));
    
    header.channels().insert("R", Channel(HALF));
    header.channels().insert("G", Channel(HALF));
    header.channels().insert("B", Channel(HALF));
    
    // Write file with attributes
    {
        OutputFile file(filename.c_str(), header);
        
        const Box2i& dataWindow = header.dataWindow();
        int width = dataWindow.max.x - dataWindow.min.x + 1;
        int height = dataWindow.max.y - dataWindow.min.y + 1;
        
        Array2D<half> r_pixels(height, width);
        Array2D<half> g_pixels(height, width);
        Array2D<half> b_pixels(height, width);
        
        // Fill with test data
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                r_pixels[y][x] = half(x / float(width));
                g_pixels[y][x] = half(y / float(height));
                b_pixels[y][x] = half(0.5f);
            }
        }
        
        FrameBuffer frameBuffer;
        frameBuffer.insert("R", Slice(HALF, (char*)&r_pixels[0][0], 
                                      sizeof(half), sizeof(half) * width));
        frameBuffer.insert("G", Slice(HALF, (char*)&g_pixels[0][0], 
                                      sizeof(half), sizeof(half) * width));
        frameBuffer.insert("B", Slice(HALF, (char*)&b_pixels[0][0], 
                                      sizeof(half), sizeof(half) * width));
        
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
    }
    
    // Read back and verify attributes
    InputFile file(filename.c_str());
    const Header& read_header = file.header();
    
    const StringAttribute* desc = read_header.findTypedAttribute<StringAttribute>("description");
    ASSERT_TRUE(desc != nullptr);
    ASSERT_EQ("Test image with custom attributes", desc->value());
    
    const FloatAttribute* aspect = read_header.findTypedAttribute<FloatAttribute>("pixelAspectRatio");
    ASSERT_TRUE(aspect != nullptr);
    ASSERT_NEAR(1.5f, aspect->value(), 1e-6f);
    
    const CompressionAttribute* comp = read_header.findTypedAttribute<CompressionAttribute>("compression");
    ASSERT_TRUE(comp != nullptr);
    ASSERT_EQ(ZIP_COMPRESSION, comp->value());
}

void test_error_handling() {
    const std::string output_dir = get_test_output_dir();
    
    // Test reading non-existent file
    bool exception_thrown = false;
    try {
        RgbaInputFile file((output_dir + "/nonexistent.exr").c_str());
    } catch (const IEX_NAMESPACE::IoExc& e) {
        exception_thrown = true;
    }
    ASSERT_TRUE(exception_thrown);
    
    // Test invalid header
    exception_thrown = false;
    try {
        Header invalid_header(-1, -1);  // Invalid dimensions
        OutputFile file((output_dir + "/invalid.exr").c_str(), invalid_header);
    } catch (const IEX_NAMESPACE::ArgExc& e) {
        exception_thrown = true;
    }
    ASSERT_TRUE(exception_thrown);
}

void test_wasm_filesystem_patterns() {
    #ifdef __EMSCRIPTEN__
    setup_wasm_filesystem();
    
    const std::string test_file = "/test-output/wasm_fs_test.exr";
    
    // Test MEMFS write/read
    generate_rgba_test_image(test_file, 64, 32);
    ASSERT_TRUE(verify_rgba_image(test_file, 64, 32));
    
    // Test file size via FS API
    int file_size = EM_ASM_INT({
        try {
            var stat = FS.stat(UTF8ToString($0));
            return stat.size;
        } catch (e) {
            return -1;
        }
    }, test_file.c_str());
    
    ASSERT_TRUE(file_size > 0);
    #endif
}

int main() {
    #ifdef __EMSCRIPTEN__
    setup_wasm_filesystem();
    #endif
    
    TestSuite suite("Basic I/O Tests");
    
    TEST_START(suite, "RGBA Write/Read")
        test_rgba_write_read();
    TEST_END(suite)
    
    TEST_START(suite, "Multi-channel Write/Read")
        test_multichannel_write_read();
    TEST_END(suite)
    
    TEST_START(suite, "Header Attributes")
        test_header_attributes();
    TEST_END(suite)
    
    TEST_START(suite, "Error Handling")
        test_error_handling();
    TEST_END(suite)
    
    #ifdef __EMSCRIPTEN__
    TEST_START(suite, "WASM Filesystem Patterns")
        test_wasm_filesystem_patterns();
    TEST_END(suite)
    #endif
    
    suite.print_summary();
    return suite.all_passed() ? 0 : 1;
}