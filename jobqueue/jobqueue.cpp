#include "jobqueue.h"

#define JQ_MAX_TEMP_WORKERS 16

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
    return false;
}

int jqAtomicHeap::SplitBlock(LevelInfo* Level, int Slot, LevelInfo* LevelTo)
{
    return 0;
}

char* jqAtomicHeap::AllocLevel(int LevelIdx)
{
    return nullptr;
}

int jqAtomicHeap::FindLevelForSize(unsigned int size)
{
    return 0;
}

char* jqAtomicHeap::Alloc(unsigned int Size, unsigned int Align)
{
    return nullptr;
}

void jqAtomicHeap::FindAllocatedBlock(unsigned int Offset, LevelInfo** FitLevel, int* FitSlot)
{
}

void jqAtomicHeap::MergeBlocks(LevelInfo** FitLevel, int* FitSlot)
{
}

void jqAtomicHeap::Free(void* Ptr)
{
    int FitSlot = 0;
    LevelInfo* FitLevel = (LevelInfo*)Ptr;

    Mutex.Lock();
    FindAllocatedBlock((char*)Ptr - HeapBase, &FitLevel, &FitSlot);
    tlAtomicAnd(&FitLevel->CellAllocated[FitSlot / 64], ~(1i64 << (FitSlot & 0x3F)));
    tlAtomicAdd(&ThisPtr->TotalBlocks, 0xFFFFFFFF);
    tlAtomicAdd(&ThisPtr->TotalUsed, -FitLevel->BlockSize);
    MergeBlocks(&FitLevel, &FitSlot);

    // todo
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

void jqAttachQueueToWorkers(jqQueue* Queue, unsigned int ProcessorMask)
{
}

unsigned int jqProcessorsMask = 255;
int jqNWorkers;
unsigned __int64 jqMainThreadID;

jqWorker* jqWorkers;
jqWorker* jqTempWorkers;
jqBatchPool jqPool;
jqQueue jqGlobalQueue;
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
    assert(((unsigned int)BatchCount & 0x7) == 0);
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
    assert(jqKeepWorkersAwakeCount > 0);
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
            assert(thr != 0);
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
    assert(jqBatchPoolExternallyLockedCount > 0);
    InterlockedExchangeAdd(&jqBatchPoolExternallyLockedCount, 0xFFFFFFFF);
    if (!jqBatchPoolExternallyLockedCount)
        PulseEvent(jqNewJobAdded);
    jqUnlockBatchPoolInternal();
}

void jqSetBatchDataHeapSize(unsigned int Size, unsigned int BlockSize)
{
    void* alloc;

    assert(!jqNWorkers);
    assert(Size > 0);

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
    assert(Batch->Module != NULL);
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
    assert(Batch->Module != NULL);
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

    assert(ParamSize >= 0 && ParamSize <= (int)sizeof(Batch.ParamData));

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
    return false;
}

bool jqPopNextBatch(jqWorker* Worker, bool* doHighPriority, jqBatchGroup* GroupID, jqBatch* PoppedBatch)
{
    return false;
}

void jqWorkerLoop(jqWorker* Worker, jqBatchGroup* GroupID, bool BreakWhenEmpty, unsigned __int64* batchCount)
{
}

void jqTempWorkerLoop(jqWorker* Worker, jqBatchGroup* GroupID, bool(__cdecl* callback)(void*), void* context)
{
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

    assert(((u32)BatchCount & 7)==0);
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
    assert(jqGetCurrentThreadID() == jqGetMainThreadID());
    if (jqWorkers)
    {
        jqFlush(0, 0);
        assert(jqPool.GroupID.QueuedBatchCount == 0);
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
        assert(callback);
        assert(jqNextAvailTempWorker < JQ_MAX_TEMP_WORKERS);
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

    assert(jqGetCurrentThreadID() == jqGetMainThreadID());
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
    int i, processorsMask;

    assert(jqGetCurrentThreadID() == jqGetMainThreadID());
    jqStop();

    processorsMask = jqProcessorsMask | 1;
    jqProcessorsMask |= 1;
    for (i = 0, processorsMask = jqProcessorsMask | 1; processorsMask; ++i)
    {
        processorsMask &= processorsMask - 1;
    }
    jqNWorkers = i;
    jqWorkers = (jqWorker*)tlMemAlloc(sizeof(jqWorker) * i, 8, 0);
    memset(jqWorkers, 0, sizeof(jqWorker) * jqNWorkers);
    // todo
}
