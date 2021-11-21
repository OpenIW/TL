#include "jobqueue.h"

#define JQ_MAX_TEMP_WORKERS 16
#define JQ_ATOMIC_HEAP_MAX_LEVELS 11

jqBatchGroup::jqBatchGroup()
{
    BatchCount = 0;
}

jqBatch::jqBatch()
{
    p3x_info = 0;
    Input = 0;
    Output = 0;
    Module = 0;
    ConditionalAddress = 0;
    ConditionalValue = 0;
    GroupID = 0;
    memset(ParamData, 0, sizeof(ParamData));
}

bool jqAtomicHeap::GetAvailableBlock(LevelInfo* FitLevel, int* FitSlot)
{
    return false;
}

bool jqAtomicHeap::AllocBlock(LevelInfo** FitLevel, int* FitSlot)
{
    if (*FitLevel >= &Levels[NLevels])
    {
        return false;
    }
    
    while (!GetAvailableBlock(*FitLevel, FitSlot))
    {
        if (++ * FitLevel >= &Levels[NLevels])
        {
            return false;
        }
    }
    return true;
}

int jqAtomicHeap::SplitBlock(LevelInfo* Level, int Slot, LevelInfo* LevelTo)
{
    jqAtomicHeap::LevelInfo* i;

    for (i = Level; i > LevelTo; tlAtomicOr(&i->CellAvailable[(Slot + 1) / 64], 1i64 << ((Slot + 1) & 0x3F)))
    {
        Slot *= 2;
        --i;
    }
    tlAtomicOr(&i->CellAllocated[Slot / 64], 1i64 << (Slot & 0x3F));
    return Slot;
}

char* jqAtomicHeap::AllocLevel(int LevelIdx)
{
    jqAtomicHeap::LevelInfo* LevelTo;
    jqAtomicHeap::LevelInfo* FitLevel;
    int blockPos;
    
    LevelTo = &Levels[LevelIdx];
    FitLevel = LevelTo;
    LevelIdx = 0;
    if (!AllocBlock(&FitLevel, &LevelIdx))
    {
        return 0;
    }
    blockPos = SplitBlock(FitLevel, LevelIdx, LevelTo);
    tlAtomicAdd(&ThisPtr->TotalBlocks, 1);
    tlAtomicAdd(&ThisPtr->TotalUsed, LevelTo->BlockSize);
    return &HeapBase[blockPos * LevelTo->BlockSize];
}

int jqAtomicHeap::FindLevelForSize(unsigned int Size)
{
    return (BlockSize < Size)
        + (2 * BlockSize < Size)
        + (4 * BlockSize < Size)
        + (8 * BlockSize < Size)
        + (16 * BlockSize < Size)
        + (32 * BlockSize < Size)
        + (BlockSize << 6 < Size)
        + (BlockSize << 7 < Size)
        + (BlockSize << 8 < Size)
        + (BlockSize << 9 < Size)
        + (BlockSize << 10 < Size);
}

char* jqAtomicHeap::Alloc(unsigned int Size, unsigned int Align)
{
    int LevelForSize;
    char* alloc;

    Mutex.Lock();
    Size = (Size < Align) ? Align : Size;
    if (Size <= HeapSize)
    {
        LevelForSize = FindLevelForSize(Size);
        alloc = AllocLevel(LevelForSize);
        Mutex.Unlock();
        return alloc;
    }
    else
    {
        tlPrintf("Size (%d) > HeapSize (%d), return NULL\n", Size, HeapSize);
        return 0;
    }
}

void jqAtomicHeap::FindAllocatedBlock(unsigned int Offset, LevelInfo** FitLevel, int* FitSlot)
{
    jqAtomicHeap::LevelInfo* i;

    *FitLevel = Levels;

    if (*FitLevel < &Levels[NLevels])
    {
        for (i = *FitLevel; i < &Levels[NLevels]; ++i)
        {
            *FitSlot = Offset / (*FitLevel)->BlockSize;
            if (((*FitLevel)->CellAllocated[*FitSlot / 64] & (1i64 << (*FitSlot & 0x3F))) != 0)
            {
                break;
            }
        }
    }
    tlAssert(*FitLevel < &Levels[NLevels]);
}

void jqAtomicHeap::MergeBlocks(LevelInfo** FitLevel, int* FitSlot)
{
    UNIMPLEMENTED(__FUNCTION__);
}

void jqAtomicHeap::Free(void* Ptr)
{
    int FitSlot = 0;
    LevelInfo* FitLevel = (LevelInfo*)Ptr;

    Mutex.Lock();
    FindAllocatedBlock((char*)Ptr - HeapBase, &FitLevel, &FitSlot);
    tlAtomicAnd(&FitLevel->CellAllocated[BlockCell(FitSlot)], BlockBit(FitSlot));
    tlAtomicAdd(&ThisPtr->TotalBlocks, 0xFFFFFFFF);
    tlAtomicAdd(&ThisPtr->TotalUsed, -FitLevel->BlockSize);
    MergeBlocks(&FitLevel, &FitSlot);
    
    tlAssert((jqGet(&FitLevel->CellAvailable[BlockCell(FitSlot)]) & BlockBit(FitSlot)) == 0);
    tlAtomicOr(&FitLevel->CellAvailable[BlockCell(FitSlot)], BlockBit(FitSlot));
    Mutex.Unlock();
}

jqAtomicHeap::~jqAtomicHeap()
{
    if (LevelData)
    {
        tlMemFree(LevelData);
    }
    Mutex.ThreadId = 0;
    Mutex.ThisPtr = NULL;
}

void jqAtomicHeap::Init(void* _HeapBase, unsigned int _HeapSize, unsigned int _BlockSize)
{
    int i, j, k;
    unsigned char* v6;
    unsigned __int64* nextCell;
    int align;

    HeapBase = reinterpret_cast<char*>(_HeapBase);
    HeapSize = _HeapSize;
    BlockSize = _BlockSize;
    ThisPtr = this;
    TotalUsed = 0;
    TotalBlocks = 0;
    for (NLevels = 1; BlockSize < HeapSize; BlockSize *= 2)
    {
        ++NLevels;
    }
    tlAssert((BlockSize << (NLevels - 1)) == HeapSize);
    tlAssert(NLevels <= JQ_ATOMIC_HEAP_MAX_LEVELS);
    i = 0;
    for (j = 0; j < NLevels; ++j)
    {
        Levels[j].BlockSize = BlockSize << j;
        Levels[j].NBlocks = 1 << (NLevels - 1 - j);
        Levels[j].NCells = tlCeilDiv(Levels[j].NBlocks, 64);
        i += tl_align(Levels[j].NBlocks, 1024) / 8;
    }
    LevelData = (unsigned char*)tlMemAlloc(2 * i, 128, 0);
    memset(LevelData, 0, 2 * i);
    v6 = LevelData;
    nextCell = (unsigned __int64*)&LevelData[i];
    for (k = 0; k < NLevels; ++k)
    {
        align = tl_align(Levels[k].NBlocks, 1024) / 8;
        Levels[k].CellAvailable = (unsigned __int64*)LevelData;
        Levels[k].CellAllocated = nextCell;
        nextCell += align;
        LevelData += align;
    }
}

jqQueue::~jqQueue()
{
    Queue.TailLock.ThreadId = 0;
    Queue.TailLock.ThisPtr = 0;
    Queue.HeadLock.ThreadId = 0;
    Queue.HeadLock.ThisPtr = 0;
}

jqBatchPool::~jqBatchPool()
{
    BatchDataHeap.~jqAtomicHeap();
    BaseQueue.Queue.TailLock.ThreadId = 0;
    BaseQueue.Queue.TailLock.ThisPtr = NULL;
    BaseQueue.Queue.HeadLock.ThreadId = 0;
    BaseQueue.Queue.HeadLock.ThisPtr = 0;
}

unsigned int jqProcessorsMask = 255;
int jqNWorkers;
unsigned __int64 jqMainThreadID;

jqWorker* jqWorkers;
jqWorker* jqTempWorkers;
jqBatchPool jqPool;
jqQueue jqGlobalQueue;
jqQueue jqHighPriorityQueue;
void(__cdecl* jqWorkerInitFn)(int);
LONG jqKeepWorkersAwakeCount;
LONG jqSleepingWorkersCount;
LONG jqPoolLock;
LONG jqBatchPoolExternallyLockedCount;
bool jqStopSignal;
HANDLE jqNewJobAdded;
const char* jqCheckContext;
LONG jqNextAvailTempWorker;

__declspec(thread) jqQueue* jqCurQueue;
__declspec(thread) jqWorker* jqCurWorker;
__declspec(thread) jqBatch* jqCurBatch;

void jqAttachQueueToWorkers(jqQueue* Queue, unsigned int ProcessorMask)
{
    UNIMPLEMENTED(__FUNCTION__);
}

void jqEnableWorkers(unsigned int ProcessorsMask)
{
    jqProcessorsMask = ProcessorsMask;
}

int jqGetNumWorkers()
{
    return jqNWorkers;
}

unsigned __int64 jqGetCurrentThreadID()
{
    return GetCurrentThreadId();
}

unsigned __int64 jqGetMainThreadID()
{
    return jqMainThreadID;
}

jqBatch* jqGetCurrentBatch()
{
    return jqCurBatch;
}

jqWorker* jqGetCurrentWorker()
{
    return jqCurWorker;
}

jqQueue* jqGetWorkerQueue(int worker)
{
    return 0;
}

void jqShutdownWorker()
{
    ;
}

int jqGetQueuedBatchCount(jqBatchGroup* GroupID)
{
    if (GroupID)
    {
        return GroupID->QueuedBatchCount;
    }
    else
    {
        return jqPool.GroupID.QueuedBatchCount;
    }
}

int jqGetExecutingBatchCount(jqBatchGroup* GroupID)
{
    if (GroupID)
    {
        return GroupID->ExecutingBatchCount;
    }
    else
    {
        return jqPool.GroupID.ExecutingBatchCount;
    }
}

jqWorker* jqFindWorkerForProcessor(jqProcessor Processor)
{
    int i;

    if (jqNWorkers <= 0)
    {
        return 0;
    }
    for (i = 0; ++i >= jqNWorkers; )
    {
        if (jqWorkers[i].Processor == Processor)
        {
            return &jqWorkers[i];
        }
    }
    return 0;
}

jqBoolean jqPoll(jqBatchGroup* GroupID)
{
    unsigned __int64 BatchCount;

    if (GroupID)
    {
        BatchCount = GroupID->BatchCount;
    }
    else
    {
        BatchCount = jqPool.GroupID.BatchCount;
    }
    tlAssert(((unsigned int)BatchCount & 0x7) == 0);
    return BatchCount != 0;
}

bool jqAreJobsQueued(jqBatchGroup* GroupID)
{
    if (GroupID)
    {
        return GroupID->QueuedBatchCount != 0;
    }
    else
    {
        return jqPool.GroupID.QueuedBatchCount != 0;
    }
}

void jqSetWorkerInitFunction(void(*fn)(int))
{
    jqWorkerInitFn = fn;
}

void jqLetWorkersSleep()
{
    tlAssert(jqKeepWorkersAwakeCount > 0);
    _InterlockedExchangeAdd(&jqKeepWorkersAwakeCount, 0xFFFFFFFF);
}

char* jqAllocBatchData(unsigned int Size)
{
    return jqPool.BatchDataHeap.Alloc(Size, 16);
}

void jqFreeBatchData(void* Ptr)
{
    jqPool.BatchDataHeap.Free(Ptr);
}

unsigned int jqGetBatchDataAvailable()
{
    return jqPool.BatchDataHeap.HeapSize - jqPool.BatchDataHeap.TotalUsed;
}

int jqExecuteBatch(jqWorker* Worker, jqBatch* Batch)
{
    return Batch->Module->Code(Batch);
}

bool jqCanBatchExecute()
{
    return true;
}

jqBoolean jqWorkerSleep()
{
    LONG Target;

    while (!jqPool.GroupID.QueuedBatchCount)
    {
        if (jqStopSignal)
        {
            break;
        }
        if (jqKeepWorkersAwakeCount)
        {
            break;
        }
        _InterlockedExchangeAdd(&jqSleepingWorkersCount, 1u);
        WaitForSingleObject(jqNewJobAdded, 1u);
        _InterlockedExchangeAdd(&jqSleepingWorkersCount, 0xFFFFFFFF);
    }
    SwitchToThread();
    Target = 0;
    InterlockedExchange(&Target, 0);
    return !jqStopSignal;
}

void jqSetCheckContext(const char* desc)
{
    jqCheckContext = desc;
}

void jqCheckDMALS(const void* addr)
{
    if (!addr)
    {
        tlFatal("%s (LS) is NULL.", jqCheckContext);
    }
    if (((unsigned __int8)addr & 0xF) != 0)
    {
        tlFatal("%s 0x%x (LS) not 16byte aligned.", jqCheckContext, addr);
    }
    if ((unsigned int)addr > 0x40000)
    {
        tlFatal("%s 0x%x (LS) is > 256k.", jqCheckContext, addr);
    }
    if ((unsigned int)addr < 0x4000)
    {
        tlFatal("%s 0x%x (LS) is in kernel memory.", jqCheckContext, addr);
    }
}

void jqCheckDMAMain(const void* addr)
{
    if (!addr)
    {
        tlFatal("%s (Main) is NULL.", jqCheckContext);
    }
    if (((unsigned __int8)addr & 0xF) != 0)
    {
        tlFatal("%s 0x%x (Main) not 16byte aligned.", jqCheckContext, addr);
    }
    if ((unsigned int)addr < 0x40000)
    {
        tlFatal("%s 0x%x (Main) is < 256k.", jqCheckContext, addr);
    }
}

void jqCheckDMASize(unsigned int size)
{
    if ((size & 0xF) != 0)
    {
        tlFatal("%s size %d not 16byte aligned.", jqCheckContext, size);
    }
}

void jqCheckDMATag(int tag)
{
    if (tag < 0 || tag >= 31)
    {
        tlFatal("%s invalid DMA tag %d.", jqCheckContext, tag);
    }
}

void jqCheckRange(int val, int mn, int mx)
{
    if (val < mn || val > mx)
    {
        tlFatal("%s %d out of range [%d-%d].", jqCheckContext, val, mn, mx);
    }
}

void jqCheckStack()
{
    ;
}

void* jqFetch(void* dest, const void* src, unsigned int size)
{
    if (size)
    {
        memcpy(dest, (char*)src, size);
    }
    return dest;
}

void jqStore(void* dest, const void* src, unsigned int size)
{
    if (size)
    {
        memcpy(dest, src, size);
    }
}

void* jqFetchAsync(void* dest, const void* src, unsigned int size)
{
    if (size)
    {
        memcpy(dest, src, size);
    }
    return dest;
}

void jqStoreAsync(void* dest, const void* src, unsigned int size)
{
    if (size)
    {
        memcpy(dest, src, size);
    }
}

void jqWait()
{
    ;
}

void jqWaitMultiple()
{
    ;
}

void jqSetMemBase()
{
    ;
}

void jqSetStackSize()
{
    ;
}

int jqGetMemAvailable()
{
    return 0;
}

void* jqAlloc()
{
    return 0;
}

void* jqGetMemBase()
{
    return 0;
}

void _jqInit()
{
    SYSTEM_INFO SystemInfo;

    jqNewJobAdded = CreateEventA(0, 1, 0, 0);
    jqProcessorsMask = 0;
    GetSystemInfo(&SystemInfo);

    for (int i = 0; i < SystemInfo.dwNumberOfProcessors; ++i)
    {
        jqProcessorsMask |= 1 << i;
    }
}

void _jqShutdown()
{
    CloseHandle(jqNewJobAdded);
}

void _jqStart()
{
    HANDLE thr;
    DWORD ThreadId;

    for (int i = 0; i < jqNWorkers; ++i)
    {
        jqWorkers[i].Type = JQ_WORKER_GENERIC;
        if ( !jqWorkers[i].Processor )
        {
            thr = CreateThread(0, 0x10000, (LPTHREAD_START_ROUTINE)jqWorkerThread, &jqWorkers[i], 4, &ThreadId);
            tlAssert(thr != 0);
            jqWorkers[i].Thread = thr;
            ResumeThread(thr);
        }
    }
}

void _jqStop()
{
    jqStopSignal = true;
    SetEvent(jqNewJobAdded);

    if ( jqNWorkers )
    {
        for (int i = 0; i < jqNWorkers; ++i)
        {
            if (jqWorkers[i].Thread)
            {
                WaitForSingleObject(jqWorkers[i].Thread, INFINITE);
                CloseHandle(jqWorkers[i].Thread);
            }
        }
    }

    jqStopSignal = false;
    ResetEvent(jqNewJobAdded);
}

void _jqAddBatch()
{
    ;
}

void jqAlertWorkers()
{
    PulseEvent(jqNewJobAdded);
}

void jqUnlockBatchPoolInternal()
{
    LONG Target;

    Target = 0;
    InterlockedExchange(&Target, 0);
    while (InterlockedCompareExchange(&jqPoolLock, 0, 1) != 1);
}

void jqKeepWorkersAwake()
{
    InterlockedExchangeAdd(&jqKeepWorkersAwakeCount, 1u);
    if (jqKeepWorkersAwakeCount > 0)
    {
        PulseEvent(jqNewJobAdded);
    }
}

void jqUnlockBatchPool()
{
    tlAssert(jqBatchPoolExternallyLockedCount > 0);
    InterlockedExchangeAdd(&jqBatchPoolExternallyLockedCount, 0xFFFFFFFF);
    if (!jqBatchPoolExternallyLockedCount)
        PulseEvent(jqNewJobAdded);
    jqUnlockBatchPoolInternal();
}

void jqSetBatchDataHeapSize(unsigned int Size, unsigned int BlockSize)
{
    void* alloc;

    tlAssert(!jqNWorkers);
    tlAssert(Size > 0);

    if (jqPool.BatchDataHeap.LevelData)
    {
        tlMemFree(jqPool.BatchDataHeap.LevelData);
    }
    alloc = tlMemAlloc(Size, 0x80u, 0);
    jqPool.BatchDataHeap.Init(alloc, Size, BlockSize);
}

void jqInit()
{
    jqAtomicQueue<jqBatch, 32>::NodeType* Node;

    jqPool.ThisPtr = &jqPool;
    jqPool.BaseQueue.Queue.ThisPtr = &jqPool.BaseQueue.Queue;
    jqPool.BaseQueue.Queue._FreeList = 0;
    jqPool.BaseQueue.Queue.FreeListPtr = &jqPool.BaseQueue.Queue._FreeList;
    jqPool.BaseQueue.Queue.FreeLock.ThisPtr = &jqPool.BaseQueue.Queue.FreeLock;
    jqPool.BaseQueue.Queue.FreeLock.ThreadId = 0;
    jqPool.BaseQueue.Queue.FreeLock.LockCount = 0;
    jqPool.BaseQueue.Queue.NodeBlockListHead = 0;
    jqPool.BaseQueue.Queue.HeadLock.ThisPtr = &jqPool.BaseQueue.Queue.HeadLock;
    jqPool.BaseQueue.Queue.HeadLock.ThreadId = 0;
    jqPool.BaseQueue.Queue.HeadLock.LockCount = 0;
    jqPool.BaseQueue.Queue.TailLock.ThisPtr = &jqPool.BaseQueue.Queue.TailLock;
    jqPool.BaseQueue.Queue.TailLock.ThreadId = 0;
    jqPool.BaseQueue.Queue.TailLock.LockCount = 0;
    Node = jqPool.BaseQueue.Queue.AllocateNode();
    Node->Next = NULL;
    jqPool.BaseQueue.Queue.Tail = Node;
    jqPool.BaseQueue.Queue.Head = Node;
    jqPool.BaseQueue.QueuedBatchCount = 0;
    jqPool.BaseQueue.Queue.AllocateNodeBlock(128);
    jqSleepingWorkersCount = 0;
    jqMainThreadID = GetCurrentThreadId();
    _jqInit();
}

void jqInitQueue(jqQueue* Queue)
{
    Queue->ThisPtr = Queue;
    Queue->QueuedBatchCount = 0;
    Queue->Queue.Init(&jqPool.BaseQueue.Queue);
    Queue->ProcessorsMask = 0;
}

void jqInitWorker(jqWorker* Worker)
{
    Worker->ThisPtr = Worker;
    Worker->WorkerSpecific.ThisPtr = &Worker->WorkerSpecific;
    Worker->WorkerSpecific.QueuedBatchCount = 0;
    Worker->WorkerSpecific.Queue.Init(&jqPool.BaseQueue.Queue);
    Worker->WorkerSpecific.ProcessorsMask = 0;
    Worker->NumQueues = 0;
    Worker->Queues[0] = 0;
    Worker->Queues[1] = 0;
    Worker->Queues[2] = 0;
    Worker->Queues[3] = 0;
    Worker->Queues[4] = 0;
    Worker->Queues[5] = 0;
    Worker->Queues[6] = 0;
    Worker->Queues[7] = 0;
}

void jqAddBatchToQueue(const jqBatch* Batch, jqQueue* Queue)
{
    tlAssert(Batch->Module != NULL);
    if (Batch->GroupID)
    {
        _InterlockedExchangeAdd(&Batch->GroupID->QueuedBatchCount, 1u);
    }
    _InterlockedExchangeAdd(&Batch->Module->Group.QueuedBatchCount, 1u);
    _InterlockedExchangeAdd(&jqPool.GroupID.QueuedBatchCount, 1u);
    _InterlockedExchangeAdd(&Queue->QueuedBatchCount, 1u);
    Queue->Queue.Push(Batch);
    PulseEvent(jqNewJobAdded);
}

void jqAddBatch(const jqBatch* Batch, jqQueue* Queue)
{
    if (!Queue)
    {
        Queue = &jqGlobalQueue;
    }
    tlAssert(Batch->Module != NULL);
    if (Batch->GroupID)
    {

        _InterlockedExchangeAdd(&Batch->GroupID->QueuedBatchCount, 1u);
    }
    _InterlockedExchangeAdd(&Batch->Module->Group.QueuedBatchCount, 1u);
    _InterlockedExchangeAdd(&jqPool.GroupID.QueuedBatchCount, 1u);
    _InterlockedExchangeAdd(&Queue->QueuedBatchCount, 1u);
    Queue->Queue.Push(Batch);
    PulseEvent(jqNewJobAdded);
}

void jqAddBatch(const jqModule* Module, void* Input, void* Output, jqBatchGroup* GroupID, jqQueue* Queue, void* ParamData, int ParamSize)
{
    jqBatch Batch;

    Batch.Module = (jqModule*)Module;
    Batch.p3x_info = 0;
    Batch.ConditionalAddress = 0;
    Batch.ConditionalValue = 0;
    Batch.Input = Input;
    Batch.Output = Output;
    Batch.GroupID = GroupID;

    tlAssert(ParamSize >= 0 && ParamSize <= (int)sizeof(Batch.ParamData));

    if (ParamData && ParamSize)
    {
        memcpy(Batch.ParamData, ParamData, ParamSize);
    }
    memset(Batch.ParamData + ParamSize, 205, 92 - ParamSize);
    jqAddBatch(&Batch, Queue);
}

void jqSkipBatch()
{
    jqAddBatch(jqCurBatch, jqCurQueue);
}

bool jqPopNextBatchFromQueue(jqWorker* Worker, jqQueue* Queue, jqBatchGroup* GroupID)
{
    UNIMPLEMENTED(__FUNCTION__);
    return false;
}

bool jqPopNextBatch(jqWorker* Worker, bool* doHighPriority, jqBatchGroup* GroupID, jqBatch* PoppedBatch)
{
    UNIMPLEMENTED(__FUNCTION__);
    return false;
}

void jqWorkerLoop(jqWorker* Worker, jqBatchGroup* GroupID, bool BreakWhenEmpty, unsigned __int64* batchCount)
{
    UNIMPLEMENTED(__FUNCTION__);
}

void jqTempWorkerLoop(jqWorker* Worker, jqBatchGroup* GroupID, bool(__cdecl* callback)(void*), void* context)
{
    UNIMPLEMENTED(__FUNCTION__);
}

unsigned int jqWorkerThread(void* _this)
{
    jqWorker* worker = (jqWorker*)_this;
    char ThreadName[256];

    sprintf(ThreadName, "JQ Worker %d (Processor 0x%x)", worker->WorkerID, worker->Processor);
    SetThreadName(0xFFFFFFFF, ThreadName);
    worker->ThreadId = GetCurrentThreadId();
    jqWorkerLoop(worker, 0, 0, 0);
    return 0;
}

void jqFlush(jqBatchGroup* GroupID, unsigned __int64 batchCount)
{
    unsigned __int64 BatchCount;
    volatile LONG* ExecutingBatchCount;
    int QueuedBatchCount;
    LONG Target;
    unsigned __int64* workerBatchCount;
    unsigned __int64 zero = 0;

    PulseEvent(jqNewJobAdded);

    if (GroupID)
    {
        BatchCount = GroupID->BatchCount;
        ExecutingBatchCount = &GroupID->ExecutingBatchCount;
    }
    else
    {
        BatchCount = jqPool.GroupID.BatchCount;
        GroupID = &jqPool.GroupID;
        ExecutingBatchCount = &jqPool.GroupID.ExecutingBatchCount;
    }

    tlAssert(((u32)BatchCount & 7)==0);
    workerBatchCount = (batchCount) ? &zero : &BatchCount;
    while (1)
    {
        Target = 0;
        InterlockedExchange(&Target, 0);
        if (!(GroupID->QueuedBatchCount + *ExecutingBatchCount) && GroupID->BatchCount <= batchCount)
        {
            break;
        }
        jqWorkerLoop(jqWorkers, GroupID, 1, workerBatchCount);
    }
}

void jqStop()
{
    tlAssert(jqGetCurrentThreadID() == jqGetMainThreadID());
    if (jqWorkers)
    {
        jqFlush(0, 0);
        tlAssert(jqPool.GroupID.QueuedBatchCount == 0);
        _jqStop();
        tlMemFree(jqTempWorkers);
        jqNWorkers = 0;
        tlMemFree(jqWorkers);
        jqWorkers = NULL;
    }
}

void jqAssistWithBatches(bool(__cdecl* callback)(void*), void* context, jqBatchGroup* GroupID)
{
    jqWorker* tmpWorker;

    if (jqTempWorkers)
    {
        tlAssert(callback);
        tlAssert(jqNextAvailTempWorker < JQ_MAX_TEMP_WORKERS);
        tmpWorker = &jqTempWorkers[InterlockedExchangeAdd(&jqNextAvailTempWorker, 1u)];
        tmpWorker->Processor = -1;
        tmpWorker->NumQueues = 1;
        tmpWorker->Queues[0] = &jqGlobalQueue;
        jqTempWorkerLoop(tmpWorker, GroupID, callback, context);
        InterlockedExchangeAdd(&jqNextAvailTempWorker, INFINITE);
    }
}

void jqShutdown()
{
    void* mem;

    tlAssert(jqGetCurrentThreadID() == jqGetMainThreadID());
    jqStop();
    while (jqPool.BaseQueue.Queue.NodeBlockListHead)
    {
        mem = jqPool.BaseQueue.Queue.NodeBlockListHead;
        jqPool.BaseQueue.Queue.NodeBlockListHead = jqPool.BaseQueue.Queue.NodeBlockListHead->Next;
        tlMemFree(mem);
    }
    CloseHandle(jqNewJobAdded);
    jqSleepingWorkersCount = 0;
}

void jqStart()
{
    int id, j, processor;

    tlAssert(jqGetCurrentThreadID() == jqGetMainThreadID());
    // Stop any running jobqueue
    jqStop();

    // Init needed workers
    jqProcessorsMask |= 1;
    jqNWorkers = tlCountOnes(jqProcessorsMask);
    jqWorkers = (jqWorker*)tlMemAlloc(jqNWorkers << 7, 8, 0);
    memset(jqWorkers, 0, jqNWorkers << 7);
    for (processor = jqProcessorsMask, j = 1, id = 0; (processor & j) == 0; j *= 2)
    {
        jqInitWorker(&jqWorkers[id]);
        jqWorkers[id].Processor = processor;
        jqWorkers[id].WorkerID = id;
        ++id;
    }
    tlAssert(id == jqNWorkers);

    // Init temp workers
    jqTempWorkers = (jqWorker*)tlMemAlloc(sizeof(jqWorker) * JQ_MAX_TEMP_WORKERS, 8, 0);
    memset(jqTempWorkers, 0, sizeof(jqWorker) * JQ_MAX_TEMP_WORKERS);
    for (id = 0; id < JQ_MAX_TEMP_WORKERS; ++id)
    {
        jqInitWorker(&jqTempWorkers[id]);
    }

    // Init global queue and high priority queue
    jqInitQueue(&jqGlobalQueue);
    jqAttachQueueToWorkers(&jqGlobalQueue, 255);
    jqInitQueue(&jqHighPriorityQueue);
    jqAttachQueueToWorkers(&jqHighPriorityQueue, 255);
    _jqStart();
}
