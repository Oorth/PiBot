// Module Test: Capture ROI and Save to Disk at ~30 FPS
// ----------------------------------------------------

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>   // Requires libjpeg (IJG)
#include <string>

#pragma comment(lib, "jpeg.lib")

// Configuration
static const int ROI_W        = 200;
static const int ROI_H        = 200;
static const int FPS_DELAY_MS = 33;       // ~30 FPS
static const char* OUTPUT_DIR = "captures";  // Ensure this folder exists

// JPEG compression function
unsigned char* compress_jpeg(unsigned char* raw, int width, int height, unsigned long &outSize) {
    jpeg_compress_struct cinfo;
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    unsigned char* outbuf = nullptr;
    jpeg_mem_dest(&cinfo, &outbuf, &outSize);

    cinfo.image_width      = width;
    cinfo.image_height     = height;
    cinfo.input_components = 3;
    cinfo.in_color_space   = JCS_EXT_BGR;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 75, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < height) {
        JSAMPROW rowPtr = (JSAMPROW)(raw + cinfo.next_scanline * width * 3);
        jpeg_write_scanlines(&cinfo, &rowPtr, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return outbuf;
}

int main() {
    // Create output directory
    CreateDirectoryA(OUTPUT_DIR, NULL);

    // Compute ROI centered on screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x0 = (screenW - ROI_W) / 2;
    int y0 = (screenH - ROI_H) / 2;

    // Prepare DCs
    HDC hScreen = GetDC(NULL);
    HDC hMem    = CreateCompatibleDC(hScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, ROI_W, ROI_H);
    SelectObject(hMem, hBmp);

    // Allocate raw buffer
    int rawSize = ROI_W * ROI_H * 3;
    unsigned char* rawBuf = (unsigned char*)malloc(rawSize);
    if (!rawBuf) { fprintf(stderr, "Malloc failed\n"); return 1; }

    int frameCount = 0;
    while (frameCount < 300) { // capture 300 frames (~10 seconds)
        // Capture ROI
        BitBlt(hMem, 0, 0, ROI_W, ROI_H, hScreen, x0, y0, SRCCOPY);
        BITMAPINFOHEADER bi = {};
        bi.biSize        = sizeof(bi);
        bi.biWidth       = ROI_W;
        bi.biHeight      = -ROI_H; // top-down
        bi.biPlanes      = 1;
        bi.biBitCount    = 24;
        bi.biCompression = BI_RGB;
        GetDIBits(hMem, hBmp, 0, ROI_H, rawBuf, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

        // Compress to JPEG
        unsigned long jpgSize = 0;
        unsigned char* jpgBuf = compress_jpeg(rawBuf, ROI_W, ROI_H, jpgSize);

        // Write to disk
        char filename[MAX_PATH];
        sprintf_s(filename, "%s\\frame_%05d.jpg", OUTPUT_DIR, frameCount);
        FILE* fp = fopen(filename, "wb");
        if (fp) {
            fwrite(jpgBuf, 1, jpgSize, fp);
            fclose(fp);
        }
        free(jpgBuf);

        frameCount++;
        Sleep(FPS_DELAY_MS);
    }

    // Cleanup
    free(rawBuf);
    DeleteObject(hBmp);
    DeleteDC(hMem);
    ReleaseDC(NULL, hScreen);
    return 0;
}

/*
 * Build Instructions:
 * 1. Ensure libjpeg headers/lib are installed and linked (jpeg.lib).
 * 2. Compile with: cl /EHsc capture_to_disk.cpp jpeg.lib
 * 3. Create a folder named "captures" next to the executable.
 * 4. Run. It will save ~30 FPS for 10 seconds (300 frames) into captures/.
 */
