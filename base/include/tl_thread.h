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

tlThreadId tlGetCurrentThreadId();

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

static int tlAtomicIncrement(volatile int* var)
{
    return InterlockedExchangeAdd((volatile LONG*)var, 1);
}

static int tlAtomicDecrement(volatile int* var)
{
    return InterlockedExchangeAdd((volatile LONG*)var, -1);
}

static bool tlAtomicCompareAndSwap(volatile unsigned int* var, unsigned int exchange, unsigned int comperand)
{
    return InterlockedCompareExchange((volatile LONG*)var, exchange, comperand) == comperand;
}

static bool tlAtomicCompareAndSwap(volatile u64* var, u64 newvalue, u64 compare)
{
    return InterlockedCompareExchange64((volatile LONGLONG*)var, compare, newvalue) == compare;
}

static unsigned int tlAtomicAdd(volatile unsigned int* var, unsigned int value)
{
    volatile unsigned int i;

    for (i = *var; InterlockedCompareExchange((volatile LONG*)var, *var + value, *var) != i; i = *var)
    {
        Sleep(0);
    }
    return i + value;
}

static unsigned __int64 tlAtomicAnd(volatile unsigned __int64* var, unsigned __int64 value)
{
    signed __int64 v;

    for (v = *var; InterlockedCompareExchange64((volatile __int64*)var, value & v, v) != v; v = *var)
    {
        Sleep(0);
    }
    return value & v;
}

static unsigned __int64 tlAtomicOr(volatile unsigned __int64* var, unsigned __int64 value)
{
    signed __int64 v;

    for (v = *var; InterlockedCompareExchange64((volatile __int64*)var, value | v, v) != v; v = *var)
    {
        Sleep(0);
    }
    return value | v;
}

static tlThreadId tlGetCurrentThreadId()
{
    return GetCurrentThreadId();
}

static void tlYield()
{
    SwitchToThread();
}

class tlAtomicReadWriteMutex
{
private:
    volatile u64 WriteThreadId;
    volatile int ReadLockCount;
    volatile int WriteLockCount;
    tlAtomicReadWriteMutex* ThisPtr;

public:
    void WriteLock()
    {
        u64 CurThread;

        CurThread = tlGetCurrentThreadId();
        if (tlAtomicCompareAndSwap(&this->ThisPtr->WriteThreadId, CurThread, CurThread))
        {
            tlAtomicIncrement(&this->ThisPtr->WriteLockCount);
            return;
        }
        while (1)
        {
            if (!tlAtomicCompareAndSwap(&this->ThisPtr->WriteThreadId, CurThread, 0LL))
            {
                tlYield();
                continue;
            }
            if (tlAtomicCompareAndSwap((volatile u32*)&this->ThisPtr->ReadLockCount, 0, 0))
                break;
            while (!tlAtomicCompareAndSwap(&this->ThisPtr->WriteThreadId, 0LL, CurThread));
            tlYield();
        }
        tlAtomicIncrement(&this->ThisPtr->WriteLockCount);
        tlMemoryFence();
    }

    void ReadLock()
    {
        u64 CurThread;

        CurThread = tlGetCurrentThreadId();
        if (tlAtomicCompareAndSwap(&this->ThisPtr->WriteThreadId, CurThread, CurThread))
        {
            tlAtomicIncrement(&this->ThisPtr->ReadLockCount);
        }
        else
        {
            while (!tlAtomicCompareAndSwap(&this->ThisPtr->WriteThreadId, CurThread, 0LL))
                tlYield();
            tlAtomicIncrement(&this->ThisPtr->ReadLockCount);
            while (!tlAtomicCompareAndSwap(&this->ThisPtr->WriteThreadId, 0LL, CurThread));
        }
        tlMemoryFence();
    }

    void WriteUnlock()
    {
        u64 CurThread;

        CurThread = tlGetCurrentThreadId();
        if (!tlAtomicDecrement(&this->ThisPtr->WriteLockCount))
        {
            tlMemoryFence();
            while (!tlAtomicCompareAndSwap(&this->ThisPtr->WriteThreadId, 0LL, CurThread));
        }
    }

    void ReadUnlock()
    {
        tlAtomicDecrement(&this->ThisPtr->ReadLockCount);
    }

};

class tlAtomicMutex
{
public:
    u64 ThreadId;
    int LockCount;
    tlAtomicMutex* ThisPtr;

    ~tlAtomicMutex()
    {
        this->ThreadId = 0;
        this->ThisPtr = NULL;
    }

    void Lock()
    {
        u64 CurThread;

        CurThread = tlGetCurrentThreadId();
        if (this->ThreadId == CurThread)
        {
            ++this->LockCount;
        }
        else
        {
            while (!tlAtomicCompareAndSwap(&this->ThisPtr->ThreadId, CurThread, 0LL))
                tlYield();
            tlMemoryFence();
            this->LockCount = 1;
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
        u64 CurThread;

        CurThread = tlGetCurrentThreadId();
        if (this->ThreadId == CurThread)
        {
            ++this->LockCount;
            return true;
        }
        else
        {
            if (tlAtomicCompareAndSwap(&this->ThisPtr->ThreadId, CurThread, 0LL))
            {
                tlMemoryFence();
                this->LockCount = 1;
                return true;
            }
        }
        return false;
    }

    void Create()
    {
        ThisPtr = this;
        ThreadId = 0;
        LockCount = 0;
    }

    void Destroy()
    {
        ThreadId = 0;
        ThisPtr = 0;
    }
};

class tlSharedAtomicMutex
{
public:
    volatile u64 ThreadId;
    volatile int LockCount;
    tlSharedAtomicMutex* ThisPtr;

    void Lock()
    {
        u64 CurThread;

        CurThread = tlGetCurrentThreadId();
        if (this->ThreadId == CurThread)
        {
            ++this->LockCount;
        }
        else
        {
            while (!tlAtomicCompareAndSwap(&this->ThisPtr->ThreadId, CurThread, 0LL))
                tlYield();
            tlMemoryFence();
            this->LockCount = 1;
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
