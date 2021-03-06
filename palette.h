#ifndef PALETTE_H
#define PALETTE_H

//Includes
#include "globalstate.h"

//Variables
extern HWND hwndPalette;
extern bool wvisPalette;
extern DWORD paletteBuffer[0x100];
extern DWORD gradientBuffer[0x18];

//Functions
void loadPalette();
void updatePalette();
void updatePaletteW6(bool dark);
//Window
LRESULT CALLBACK WndProc_Palette(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);

#endif

