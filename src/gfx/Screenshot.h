#pragma once
// ---------------------------------------------------------------------------
// Reads the finished frame back from the GPU and writes it to a TGA file.
//
// glReadPixels stalls the pipeline (the CPU has to wait for the GPU to catch
// up), which is exactly why you do it on a keypress and never per frame.
// ---------------------------------------------------------------------------
bool saveScreenshot(const char* path, int pixelWidth, int pixelHeight);
