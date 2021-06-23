#pragma once
#include <stddef.h>

// Default frame buffer struct. Holds information for any graphics driver
struct Framebuffer{
	void* BaseAddress;
	size_t BufferSize;
	unsigned int Width;
	unsigned int Height;
	unsigned int PixelsPerScanLine;
};
