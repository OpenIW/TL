#pragma once

#include "tl_defs.h"

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    unsigned int dwType;
    const char* szName;
    unsigned int dwThreadID;
    unsigned int dwFlags;
} THREADNAME_INFO;
#pragma pack(pop)

struct _SCOPETABLE_ENTRY
{
    unsigned int enclosing_level;
    unsigned int filter;
    unsigned int specific_handler;
};

typedef struct _SCOPETABLE_ENTRY* PSCOPETABLE_ENTRY;

typedef struct _EH3_EXCEPTION_REGISTRATION
{
    struct _EH3_EXCEPTION_REGISTRATION* Next;
    PVOID ExceptionHandler;
    PSCOPETABLE_ENTRY ScopeTable;
    DWORD TryLevel;
} _EH3_EXCEPTION_REGISTRATION;

typedef struct CPPEH_RECORD
{
    DWORD old_esp;
    EXCEPTION_POINTERS* exc_ptr;
    struct _EH3_EXCEPTION_REGISTRATION registration;
} CPPEH_RECORD;

const DWORD MS_VC_EXCEPTION = 0x406D1388;

FORCEINLINE void tlMemoryFence()
{
    LONG Fence = 0;
    InterlockedExchange(&Fence, 0);
}

static void SetThreadName(unsigned int dwThreadID, const char* szThreadName)
{
    THREADNAME_INFO info;
    CPPEH_RECORD ms_exc;

    info.dwType = 4096;
    info.szName = szThreadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;
    ms_exc.registration.TryLevel = 0;

#pragma warning(push)
#pragma warning(disable: 6320 6322)
    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
#pragma warning(pop)
}

class tlAtomicReadWriteMutex
{
private:
    volatile unsigned __int64 WriteThreadId;
    volatile int ReadLockCount;
    volatile int WriteLockCount;
    tlAtomicReadWriteMutex* ThisPtr;

public:
    void WriteLock()
    {
        unsigned __int64 CurThread;

        CurThread = GetCurrentThreadId();
        if (_InterlockedCompareExchange64((volatile __int64*)ThisPtr, CurThread, CurThread) == CurThread)
        {
            _InterlockedExchangeAdd((volatile LONG*)&ThisPtr->WriteLockCount, 1);
            return;
        }

        while (1)
        {
            if (_InterlockedCompareExchange64((volatile __int64*)ThisPtr, CurThread, 0) == 0)
            {
                if (_InterlockedCompareExchange((volatile LONG*)&ThisPtr->ReadLockCount, 0, 0) == 0)
                {
                    break;
                }

                while (_InterlockedCompareExchange64((volatile __int64*)ThisPtr, 0, CurThread) != CurThread) {}
            }

            SwitchToThread();
        }

        _InterlockedExchangeAdd((volatile LONG*)&ThisPtr->WriteLockCount, 1);
        tlMemoryFence();
    }

    void ReadLock()
    {
        unsigned __int64 CurThread;

        CurThread = GetCurrentThreadId();
        if (_InterlockedCompareExchange64((volatile __int64*)ThisPtr, CurThread, CurThread) == CurThread)
        {
            _InterlockedExchangeAdd((volatile LONG*)&ThisPtr->ReadLockCount, 1);
        }
        else
        {
            while (1)
            {
                if (_InterlockedCompareExchange64((volatile __int64*)ThisPtr, CurThread, 0) == 0)
                {
                    break;
                }
                SwitchToThread();
            }

            _InterlockedExchangeAdd((volatile LONG*)&ThisPtr->ReadLockCount, 1);

            while (_InterlockedCompareExchange64((volatile __int64*)ThisPtr, 0, CurThread) != CurThread) {}
        }
        tlMemoryFence();
    }

    void WriteUnlock()
    {
        unsigned __int64 CurThread;

        CurThread = GetCurrentThreadId();
        if (!/*Sys_*/InterlockedDecrement((volatile LONG*)&ThisPtr->WriteLockCount))
        {
            tlMemoryFence();
            while (_InterlockedCompareExchange64((volatile signed __int64*)ThisPtr, 0, CurThread) != CurThread) {}
        }
    }

};

class tlAtomicMutex
{
public:
    unsigned __int64 ThreadId;
    int LockCount;
    tlAtomicMutex* ThisPtr;

    ~tlAtomicMutex()
    {
        this->ThreadId = 0;
        this->ThisPtr = NULL;
    }

    void Lock()
    {
        unsigned __int64 CurThread;

        CurThread = GetCurrentThreadId();
        if (ThreadId == CurThread)
        {
            ++LockCount;
        }
        else
        {
            while (1)
            {
                if (_InterlockedCompareExchange64((volatile __int64*)ThisPtr, CurThread, 0) == 0)
                {
                    break;
                }
                SwitchToThread();
            }

            tlMemoryFence();
            LockCount = 1;
        }
    }

    void Unlock()
    {
        if (LockCount-- == 1)
        {
            tlMemoryFence();
            ThreadId = 0;
        }
    }

    bool TryLock()
    {
        unsigned __int64 CurThread;

        CurThread = GetCurrentThreadId();
        if (ThreadId == CurThread)
        {
            ++LockCount;
            return 1;
        }
        else
        {
            if (_InterlockedCompareExchange64((volatile __int64*)ThisPtr, CurThread, 0) == 0)
            {
                tlMemoryFence();
                LockCount = 1;

                return 1;
            }
        }
        return 0;
    }
};

class tlSharedAtomicMutex
{
public:
    volatile unsigned __int64 ThreadId;
    volatile int LockCount;
    tlSharedAtomicMutex* ThisPtr;

    void Lock()
    {
        unsigned __int64 CurThread;

        CurThread = GetCurrentThreadId();
        if (ThisPtr->ThreadId == CurThread)
        {
            ++ThisPtr->LockCount;
        }
        else
        {
            while (_InterlockedCompareExchange64((volatile __int64*)ThisPtr, CurThread, 0))
            {
                SwitchToThread();
            }
            tlMemoryFence();
            ThisPtr->LockCount = 1;
        }
    }
    void Unlock()
    {
        if (ThisPtr->LockCount-- == 1)
        {
            tlMemoryFence();
            ThisPtr->ThreadId = 0;
        }
    }
};

static int tlAtomicIncrement(volatile int* var)
{
    return _InterlockedExchangeAdd((volatile LONG*)var, 1);
}

static int tlAtomicDecrement(volatile int* var)
{
    return _InterlockedExchangeAdd((volatile LONG*)var, -1);
}

static bool tlAtomicCompareAndSwap(volatile int* var, unsigned int exchange, unsigned int comperand)
{
    return _InterlockedCompareExchange((volatile LONG*)var, exchange, comperand) == comperand;
}

static unsigned int tlAtomicAdd(volatile unsigned int* var, unsigned int value)
{
    volatile unsigned int i;

    for (i = *var; _InterlockedCompareExchange((volatile LONG*)var, *var + value, *var) != i; i = *var)
    {
        Sleep(0);
    }
    return i + value;
}

static unsigned __int64 tlAtomicAnd(volatile unsigned __int64* var, unsigned __int64 value)
{
    signed __int64 v;

    for (v = *var; _InterlockedCompareExchange64((volatile __int64*)var, value & v, v) != v; v = *var)
    {
        Sleep(0);
    }
    return value & v;
}

static unsigned __int64 tlAtomicOr(volatile unsigned __int64* var, unsigned __int64 value)
{
    signed __int64 v;

    for (v = *var; _InterlockedCompareExchange64((volatile __int64*)var, value | v, v) != v; v = *var)
    {
        Sleep(0);
    }
    return value | v;
}

static unsigned int tlGetCurrentThreadId()
{
    return GetCurrentThreadId();
}

static void tlYield()
{
    SwitchToThread();
}