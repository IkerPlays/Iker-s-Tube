#pragma once
#include "3ds.h"
typedef int GPU_TEXCOLOR;
#define GPU_RGBA8 0
#define GPU_RGB565 1
typedef struct { int dummy; } Tex3DS_SubTexture;
typedef struct { void* tex; const Tex3DS_SubTexture* subtex; } C2D_Image;
void C2D_DrawCircleSolid(float,float,float,float,u32);
void C2D_DrawRectSolid(float,float,float,float,float,u32);
void C2D_DrawTriangle(float,float,u32,float,float,u32,float,float,u32,float);
