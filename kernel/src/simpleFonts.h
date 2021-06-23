#pragma once

struct PSF1_HEADER{
	unsigned char magic[2];			// bytes that header stores to identify file as psf file
	unsigned char mode;					// mode psf font is in
	unsigned char charsize;			// how large the char size is in bytes
};

struct PSF1_FONT{
	PSF1_HEADER* psf1_Header;
	void* glyphBuffer;
};
