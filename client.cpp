// Module Test: Capture ROI and Save to Disk at ~30 FPS
// cl.exe /EHsc /I"C:\vcpkg\installed\x64-windows\include" .\client.cpp /link /OUT:client.exe /LIBPATH:"C:\vcpkg\installed\x64-windows\lib" user32.lib gdi32.lib jpeg.lib
// ----------------------------------------------------

#define DEBUG 1

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <string>
#include "DbgMacros.h"

static const int ROI_W        = 200;
static const int ROI_H        = 200;
static const int FPS_DELAY_MS = 33;       // ~30 FPS
static const char* OUTPUT_DIR = "captures";


unsigned char* compress_jpeg(unsigned char* raw, int width, int height, unsigned long &outSize)
{
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

    while (cinfo.next_scanline < height)
    {
        JSAMPROW rowPtr = (JSAMPROW)(raw + cinfo.next_scanline * width * 3);
        jpeg_write_scanlines(&cinfo, &rowPtr, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    
    return outbuf;
}

int main()
{
    if (!CreateDirectoryA(OUTPUT_DIR, NULL))
    {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
        {
            fuk("CreateDirectoryA failed: ", error);
            return 1;
        }
    }
    norm(CYAN"Output directory created or already exists.\n");

    // Compute ROI centered on screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x0 = (screenW - ROI_W) / 2;
    int y0 = (screenH - ROI_H) / 2;

    // Prepare DCs
    HDC hScreen = GetDC(NULL);
    if (!hScreen) { fuk("GetDc Failed"); return 1; }
    ok("Got Screen DC\n");

    HDC hMem = CreateCompatibleDC(hScreen);
    if (!hMem) { fuk("CreateCompatibleDC failed\n"); ReleaseDC(NULL, hScreen); return 1; }
    ok("Created compatible DC.\n");

    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, ROI_W, ROI_H);
    if (!hBmp) { fuk("CreateCompatibleBitmap failed\n"); DeleteDC(hMem); ReleaseDC(NULL, hScreen); return 1; }
    ok("Created compatible bitmap.\n");

    SelectObject(hMem, hBmp);
    ok("Selected bitmap.\n");

    // Allocate raw buffer
    int rawSize = ROI_W * ROI_H * 3;
    unsigned char* rawBuf = (unsigned char*)malloc(rawSize);
    if (!rawBuf) { fuk("Malloc failed\n"); return 1; }
    ok("Allocated raw buffer.\n");

    int frameCount = 0;
    while (frameCount < 150)                                                            // capture 150 frames (~5 seconds)
    {
        norm("Capturing frame ", CYAN"", frameCount, RESET"\n");
        
        // Capture ROI
        if (!BitBlt(hMem, 0, 0, ROI_W, ROI_H, hScreen, x0, y0, SRCCOPY))
        {
            DWORD error = GetLastError();
            fuk("BitBlt failed: ", error);norm("\n");
            break;
        }
        
        BITMAPINFOHEADER bi = {};
        bi.biSize        = sizeof(bi);
        bi.biWidth       = ROI_W;
        bi.biHeight      = -ROI_H; // top-down
        bi.biPlanes      = 1;
        bi.biBitCount    = 24;
        bi.biCompression = BI_RGB;
        if (!GetDIBits(hMem, hBmp, 0, ROI_H, rawBuf, (BITMAPINFO*)&bi, DIB_RGB_COLORS))
        {
            DWORD error = GetLastError();
            fuk("GetDIBits failed: ", error);norm("\n");
            break;
        }

        // Compress to JPEG
        unsigned long jpgSize = 0;
        unsigned char* jpgBuf = compress_jpeg(rawBuf, ROI_W, ROI_H, jpgSize);
        ok("Compressed frame.\n");

        // Write to disk
        char filename[MAX_PATH];
        sprintf_s(filename, "%s\\frame_%05d.jpg", OUTPUT_DIR, frameCount);
        FILE* fp = fopen(filename, "wb");
        if (fp)
        {
            fwrite(jpgBuf, 1, jpgSize, fp);
            fclose(fp);
            ok("Saved frame to ", filename, "\n");
        } else fuk("fopen failed"); norm("\n");
        free(jpgBuf);

        frameCount++;
        Sleep(FPS_DELAY_MS);
    }

    // Cleanup
    free(rawBuf);
    DeleteObject(hBmp);
    DeleteDC(hMem);
    ReleaseDC(NULL, hScreen);
    
    norm(CYAN"====================================================================\n");
    ok("Cleanup complete.\n");
    return 0;
}
