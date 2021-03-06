#include "BasicRenderer.h"

BasicRenderer::BasicRenderer(Framebuffer* targetFrameBuffer, PSF1_FONT* psf1_Font){
  TargetFrameBuffer = targetFrameBuffer;
  PSF1_Font = psf1_Font;
  Color = 0xffffffff;
  CursorPosition = {0, 0};
}

void BasicRenderer::Print(const char* str){
  char* chr = (char*)str;
  while(*chr != 0){
    PutChar(*chr, CursorPosition.X, CursorPosition.Y);
    CursorPosition.X += 8;
    if(CursorPosition.X + 8 > TargetFrameBuffer->Width){
      CursorPosition.X = 0;
      CursorPosition.Y += 16;
    }
    chr++;
  }
}

void BasicRenderer::PutChar(char chr, unsigned int xOff, unsigned int yOff){
  unsigned int* pixPtr = (unsigned int*)TargetFrameBuffer->BaseAddress;
  char* fontPtr = (char*)PSF1_Font->glyphBuffer + (chr * PSF1_Font->psf1_Header->charsize);
  for(unsigned long y = yOff; y < yOff + 16; y++){        // 16 bits long
    for(unsigned long x = xOff; x < xOff + 8; x++){       // 8 bits wide
      if((*fontPtr & (0b10000000 >> (x - xOff))) > 0){
        *(unsigned int*)(pixPtr + x + (y * TargetFrameBuffer->PixelsPerScanLine)) = Color;
      }
    }
    fontPtr++;
  }
}
