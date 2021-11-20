#pragma once

#ifdef _WINDOWS

#include <Windows.h>

#include <stdio.h>
#include <stdarg.h>
#include <Psapi.h>

#endif

#define UNIMPLEMENTED(x) { static bool inited = false; if (!inited) { OutputDebugStringA("----- "); OutputDebugStringA(x); OutputDebugStringA(" not implemented.\n"); inited = true; }};
#define assert(cond) if (!(cond) && !_tlAssert(__FILE__, __LINE__, "%s", #cond)) { __debugbreak(); }

typedef unsigned int u32;

struct tlFileBuf {
	char* Buf;
	unsigned int Size;
	unsigned int UserData;
};

struct tlSystemCallbacks
{
	bool(__cdecl* ReadFile)(const char*, tlFileBuf*, unsigned int, unsigned int);
	void(__cdecl* ReleaseFile)(tlFileBuf*);
	void(__cdecl* CriticalError)(const char*);
	void(__cdecl* Warning)(const char*);
	void(__cdecl* DebugPrint)(const char*);
	void* (__cdecl* MemAlloc)(unsigned int, unsigned int, unsigned int);
	void* (__cdecl* MemRealloc)(void*, unsigned int, unsigned int, unsigned int);
	void(__cdecl* MemFree)(void*);
};