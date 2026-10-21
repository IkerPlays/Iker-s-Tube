#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32; typedef int64_t s64;
typedef unsigned int uint;
typedef s32 Result;
typedef u32 Handle;
typedef s32 LightLock;
typedef s32 RecursiveLock;
typedef void* Thread;
typedef struct { u64 elapsed; } TickCounter;
#define R_SUCCEEDED(x) ((x)>=0)
#define R_FAILED(x) ((x)<0)
static inline u64 osGetTime(void){ return 1000; }
static inline void LightLock_Init(LightLock*){}
static inline void LightLock_Lock(LightLock*){}
static inline void LightLock_Unlock(LightLock*){}
static inline int LightLock_TryLock(LightLock*){return 0;}
#define KEY_TOUCH 1
typedef struct { u16 px, py; } touchPosition;
