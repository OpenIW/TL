#pragma once

#include "tl_defs.h"

void tlSetSystemCallbacks(tlSystemCallbacks* Callbacks);
void tlSetFileServerRootPC(const char* Path);
void tlMemFree(void* Ptr);
void tlReleaseFile(tlFileBuf* File);
LARGE_INTEGER tlPcGetTick();
void tlPrint(const char* txt);
int tlGetVersion();
void tlStackRangeInit();
char tlFatalHandler(const char* Msg);
void tlDebugPrint(const char* txt);
void tlVPrintf(const char* Format, char* args);
void tlPrintf(const char* Format, ...);
char _tlAssert(const char* file, int line, const char* expr, const char* desc);
void tlFatal(const char*Format, ...);
void* tlMemAlloc(unsigned int Size, unsigned int Align, unsigned int Flags);
void* tlMemRealloc(void* Ptr, unsigned int size, unsigned int Align, unsigned int Flags);
unsigned int tlGetFreeMemory();
void* tlScratchPadInit();
void tlScratchPadReset();
void tlWarning(const char* Format, ...);
bool tlReadFile(const char* FileName, tlFileBuf* File, unsigned int Align, unsigned int Flags);
int tlCeilDiv(int Dividend, int Divisor);
int tlCountOnes(int in);

inline void tlDebugBreak()
{
	__asm int 3
}

inline int tl_align(int pos, int align)
{
	return (pos + align - 1) & ~(align - 1);
}

inline int GetStuff32(void* pos)
{
	return (int)pos;
}
