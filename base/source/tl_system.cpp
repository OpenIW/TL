#include <tl_system.h>
#include <tl_thread.h>

tlSystemCallbacks tlCurSystemCallbacks;
int tlMemAllocCounter;
int tlScratchPadRefCount;
bool tlScratchpadLocked;
void *tlScratchPadPtr;
char tlHostPrefix[256];

void *tlStackBegin;
void *tlStackEnd;

void tlSetSystemCallbacks(tlSystemCallbacks* Callbacks)
{
    memcpy(&tlCurSystemCallbacks, Callbacks, sizeof(tlCurSystemCallbacks));
}

void tlSetFileServerRootPC(const char *Path)
{
  strcpy(tlHostPrefix, Path);
}

void tlMemFree(void* Ptr)
{
    --tlMemAllocCounter;
    if ( tlCurSystemCallbacks.MemFree )
    {
        tlCurSystemCallbacks.MemFree(Ptr);
    }
    else
    {
        _aligned_free(Ptr);
    }
}

void tlReleaseFile(tlFileBuf* File)
{
    char* buffer;

    if ( tlCurSystemCallbacks.ReleaseFile )
    {
        tlCurSystemCallbacks.ReleaseFile(File);
    }
    else
    {
        buffer = File->Buf;
        tlMemFree(buffer);
        File->Buf = NULL;
        File->Size = 0;
        File->UserData = 0;
    }
}

LARGE_INTEGER tlPcGetTick()
{
    LARGE_INTEGER li;

    QueryPerformanceCounter(&li);
    return li;
}

void tlPrint(const char* txt)
{
    OutputDebugString(txt);
}

int tlGetVersion()
{
    return 66560;
}

void tlStackRangeInit()
{
    tlStackBegin = NULL;
    tlStackEnd = NULL;
}

char tlFatalHandler(const char* Msg)
{
    if ( tlCurSystemCallbacks.CriticalError )
    {
        tlCurSystemCallbacks.CriticalError(Msg);
        return 0;
    }
    else
    {
        OutputDebugString("TL Fatal Error: ");
        OutputDebugString(Msg);
        OutputDebugString("\n");
        return 1;
    }
}

void tlDebugPrint(const char* txt)
{
    if ( tlCurSystemCallbacks.DebugPrint )
    {
        tlCurSystemCallbacks.DebugPrint(txt);
    }
    else
    {
        OutputDebugString(txt);
    }
}

void tlVPrintf(const char* Format, char* args)
{
    char Work[512];

    vsprintf(Work, Format, args);
    tlDebugPrint(Work);
}

void tlPrintf(const char* Format, ...)
{
    char Work[512];
    va_list ap;

    va_start(ap, Format);
    tlVPrintf(Format, ap);
}

char _tlAssert(const char *file, int line, const char *expr, const char *desc)
{
    char Buf[256];

    _snprintf(Buf, 256, "tlAssert in %s(%d):\n\"%s\" - %s", file, line, expr, desc);
    Buf[255] = 0;

    return tlFatalHandler(Buf);
}

void tlFatal(const char* Format, ...)
{
    char Work[512];
    va_list ap;

    va_start(ap, Format);
    vsprintf(Work, Format, ap);
    if ( tlCurSystemCallbacks.CriticalError )
    {
        tlCurSystemCallbacks.CriticalError(Work);
    }
    else
    {
        OutputDebugString("TL Fatal Error: ");
        OutputDebugString(Work);
        OutputDebugString("\n");
        DebugBreak();
    }
}

void* tlMemAlloc(unsigned int Size, unsigned int Align, unsigned int Flags)
{
    void* alloc;

    if ( !Align && (Size & 0xF) == 0 )
    {
        Align = 0;
    }
    ++tlMemAllocCounter;

    if ( tlCurSystemCallbacks.MemAlloc )
    {
        alloc = tlCurSystemCallbacks.MemAlloc(Size, Align, Flags);
    }
    else
    {
        alloc = _aligned_malloc(Size, Align);
    }

    if ( (Flags & 2) == 0 && !alloc && Size )
    {
        tlFatal("Memory allocation failed. %d bytes, %d align", Size, Align);
    }
    return alloc;
}

void* tlMemRealloc(void *Ptr, unsigned int Size, unsigned int Align, unsigned int Flags)
{
    void* realloc;

    if ( !Align && (Size & 0xF) == 0 )
    {
        Align = 0;
    }

    if ( tlCurSystemCallbacks.MemRealloc )
    {
        realloc = tlCurSystemCallbacks.MemRealloc(Ptr, Size, Align, Flags);
    }
    else
    {
        realloc = _aligned_realloc(Ptr, Size, Align);
    }

    if ( (Flags & 2) == 0 && !realloc && Size )
    {
        tlFatal("Memory reallocation failed.");
    }
    return realloc;
}

unsigned int tlGetFreeMemory()
{
    PROCESS_MEMORY_COUNTERS pmc;
    MEMORYSTATUS stat;

    GlobalMemoryStatus(&stat);
    HANDLE newProcess = OpenProcess(1040, 0, GetCurrentProcessId());
    if ( GetProcessMemoryInfo(newProcess, &pmc, 40) )
    {
        CloseHandle(newProcess);
        return stat.dwTotalPhys - pmc.PeakWorkingSetSize;
    }
    CloseHandle(newProcess);
    return 0;
}

void* tlScratchPadInit()
{
    if ( tlScratchPadRefCount++ )
    {
        return tlScratchPadPtr;
    }
    return tlMemAlloc(16384, 16, 0);
}

void tlScratchPadReset()
{
    tlAssert(tlScratchPadRefCount >= 1);

    if ( !--tlScratchPadRefCount )
    {
        tlMemFree(tlScratchPadPtr);
        tlScratchPadPtr = NULL;
    }
}

void tlWarning(const char* Format, ...)
{
    char Work[512];
    va_list ap;

    va_start(ap, Format);
    vsprintf(Work, Format, ap);
    if ( tlCurSystemCallbacks.Warning )
    {
        tlCurSystemCallbacks.Warning(Work);
    }
    else
    {
        tlPrintf("%s", Work);
    }
}

bool tlReadFile(const char *FileName, tlFileBuf *File, unsigned int Align, unsigned int Flags)
{
    UNIMPLEMENTED(__FUNCTION__);
    return 0;
}