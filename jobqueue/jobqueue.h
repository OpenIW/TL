#pragma once

#include <tl_defs.h>
#include <tl_system.h>
#include <tl_thread.h>

typedef bool(__cdecl* jqDoneAssistingWithBatchesFn)(void*);
typedef void(__cdecl* jqWorkerInitFnType)(int);
typedef int(__cdecl* jqModuleCallback)(class jqBatch*);
typedef int jqBoolean;

enum jqWorkerType
{
	JQ_WORKER_GENERIC = 0x0,
	JQ_WORKER_MAX = 0x1,
	JQ_WORKER_DEFAULT = 0x0,
};

enum jqProcessor
{
	JQ_CORE_0 = 0x1,
	JQ_CORE_1 = 0x2,
	JQ_CORE_2 = 0x4,
	JQ_CORE_3 = 0x8,
	JQ_CORE_4 = 0x10,
	JQ_CORE_5 = 0x20,
	JQ_CORE_6 = 0x40,
	JQ_CORE_7 = 0x80,
	JQ_CORE_ALL = 0xFF,
};

class jqBatchGroup
{
public:
	union
	{
		struct
		{
			int QueuedBatchCount;
			int ExecutingBatchCount;
		};
		unsigned __int64 BatchCount;
	};

	jqBatchGroup();
};

class jqModule
{
public:
	const char* Name;
	jqWorkerType Type;
	int(__cdecl* Code)(class jqBatch*);
	jqBatchGroup Group;
};

class _jqBatch {};

#pragma pack(push,4)
class jqBatch
{
public:
	void* p3x_info;
	void* Input;
	void* Output;
	jqModule* Module;
	jqBatchGroup* GroupID;
	void* ConditionalAddress;
	unsigned int ConditionalValue;
	unsigned int ParamData[23];
	_jqBatch _Batch;

	jqBatch();
};
#pragma pack(pop)

template <typename T, unsigned int I>
class jqAtomicQueue
{
public:
	class NodeType
	{
	public:
		NodeType* Next;
		jqBatch Data;
	};
	struct NodeBlockEntry
	{
		void* Addr;
		NodeBlockEntry* Next;
	};

	NodeType** FreeListPtr;
	NodeType* _FreeList;
	NodeBlockEntry* NodeBlockListHead;
	NodeType* Head;
	NodeType* Tail;
	tlSharedAtomicMutex FreeLock;
	tlAtomicMutex HeadLock;
	tlAtomicMutex TailLock;
	jqAtomicQueue<T, I>* ThisPtr;

	void AllocateNodeBlock(int Count);
	NodeType* AllocateNode();
	void Init(jqAtomicQueue<T, I>* SharedFreeList);
	void Push(const jqBatch* Data);
	bool Pop(jqBatch* p);
};

class jqAtomicHeap
{
public:
	struct LevelInfo
	{
		unsigned int BlockSize;
		int NBlocks;
		int NCells;
		unsigned __int64* CellAvailable;
		unsigned __int64* CellAllocated;
	};

	jqAtomicHeap* ThisPtr;
	tlAtomicMutex Mutex;
	char* HeapBase;
	unsigned int HeapSize;
	unsigned int BlockSize;
	volatile unsigned int TotalUsed;
	volatile unsigned int TotalBlocks;
	int NLevels;
	LevelInfo Levels[11];
	unsigned char* LevelData;

	inline int BlockCell(int FitSlot)
	{
		return FitSlot / 64;
	}
	inline unsigned __int64 BlockBit(int FitSlot)
	{
		return ~(1i64 << (FitSlot & 0x3F));
	}
	bool GetAvailableBlock(LevelInfo* FitLevel, int* FitSlot);
	bool AllocBlock(LevelInfo** FitLevel, int* FitSlot);
	int SplitBlock(LevelInfo* Level, int Slot, LevelInfo* LevelTo);
	char* AllocLevel(int LevelIdx);
	int FindLevelForSize(unsigned int Size);
	char* Alloc(unsigned int Size, unsigned int Align);
	void FindAllocatedBlock(unsigned int Offset, LevelInfo** FitLevel, int* FitSlot);
	void MergeBlocks(LevelInfo** FitLevel, int* FitSlot);
	void Free(void* Ptr);
	~jqAtomicHeap();
	void Init(void* _HeapBase, unsigned int _HeapSize, unsigned int _BlockSize);
};

class jqMemBaseMarker
{
public:
	void* MemBaseRestore;
};

class jqQueue
{
public:
	jqQueue* ThisPtr;
	jqAtomicQueue<jqBatch, 32> Queue;
	int QueuedBatchCount;
	unsigned int ProcessorsMask;
	~jqQueue();
};

class jqBatchPool
{
public:
	jqBatchPool* ThisPtr;
	jqQueue BaseQueue;
	jqBatchGroup GroupID;
	jqAtomicHeap BatchDataHeap;

	~jqBatchPool();
};

#pragma pack(push,4)
struct _jqWorker
{
	jqWorkerType Type;
	void* Thread;
	unsigned int ThreadId;
	bool Idle;
};
#pragma pack(pop)

class jqWorker : public _jqWorker
{
public:
	jqWorker* ThisPtr;
	int Processor;
	int WorkerID;
	int NumQueues;
	jqQueue WorkerSpecific;
	jqQueue* Queues[8];
	unsigned __int64 WorkTime;
};

#pragma pack(push,8)
struct jqWorkerCmd
{
	jqModule* module;
	unsigned int dataSize;
	volatile int ppu_fence;
	volatile int spu_fence;
	volatile int* spuThreadLimit;
	jqQueue* queue;
	unsigned int string_table;
};
#pragma pack(pop)

void jqAttachQueueToWorkers(jqQueue* Queue, unsigned int ProcessorMask);
void jqEnableWorkers(unsigned int ProcessorsMask);
int jqGetNumWorkers();
unsigned __int64 jqGetCurrentThreadID();
unsigned __int64 jqGetMainThreadID();
jqBatch* jqGetCurrentBatch();
jqWorker* jqGetCurrentWorker();
jqQueue* jqGetWorkerQueue(int worker);
void jqShutdownWorker();
int jqGetQueuedBatchCount(jqBatchGroup* GroupID);
int jqGetExecutingBatchCount(jqBatchGroup* GroupID);
jqWorker* jqFindWorkerForProcessor(jqProcessor Processor);
jqBoolean jqPoll(jqBatchGroup* GroupID);
bool jqAreJobsQueued(jqBatchGroup* GroupID);
void jqSetWorkerInitFunction(void(*fn)(int));
void jqLetWorkersSleep();
char* jqAllocBatchData(unsigned int Size);
void jqFreeBatchData(void* Ptr);
unsigned int jqGetBatchDataAvailable();
int jqExecuteBatch(jqWorker* Worker, jqBatch* Batch);
bool jqCanBatchExecute();
jqBoolean jqWorkerSleep();
void jqSetCheckContext(const char* desc);
void jqCheckDMALS(const void* addr);
void jqCheckDMAMain(const void* addr);
void jqCheckDMASize(unsigned int size);
void jqCheckDMATag(int tag);
void jqCheckRange(int val, int mn, int mx);
void jqCheckStack();
void* jqFetch(void* dest, const void* src, unsigned int size);
void jqStore(void* dest, const void* src, unsigned int size);
void* jqFetchAsync(void* dest, const void* src, unsigned int size);
void jqStoreAsync(void* dest, const void* src, unsigned int size);
void jqWait();
void jqWaitMultiple();
void jqSetMemBase();
void jqSetStackSize();
int jqGetMemAvailable();
void* jqAlloc();
void* jqGetMemBase();

void _jqInit();
void _jqShutdown();
void _jqStart();
void _jqStop();
void _jqAddBatch();

void jqAlertWorkers();
void jqUnlockBatchPoolInternal();
void jqKeepWorkersAwake();
void jqUnlockBatchPool();
void jqSetBatchDataHeapSize(unsigned int Size, unsigned int BlockSize);
void jqInit();
void jqInitQueue(jqQueue* Queue);
void jqInitWorker(jqWorker* Worker);
void jqAddBatchToQueue(const jqBatch* Batch, jqQueue* Queue);
void jqAddBatch(const jqBatch* Batch, jqQueue* Queue);
void jqAddBatch(const jqModule* Module, void* Input, void* Output, jqBatchGroup* GroupID, jqQueue* Queue, void* ParamData, int ParamSize);
void jqSkipBatch();
bool jqPopNextBatchFromQueue(jqWorker* Worker, jqQueue* Queue, jqBatchGroup* GroupID);
bool jqPopNextBatch(jqWorker* Worker, bool* doHighPriority, jqBatchGroup* GroupID, jqBatch* PoppedBatch);
void jqWorkerLoop(jqWorker* Worker, jqBatchGroup* GroupID, bool BreakWhenEmpty, unsigned __int64* batchCount);
void jqTempWorkerLoop(jqWorker* Worker, jqBatchGroup* GroupID, bool(__cdecl* callback)(void*), void* context);
unsigned int jqWorkerThread(void* _this);
void jqFlush(jqBatchGroup* GroupID, unsigned __int64 batchCount);
void jqStop();
void jqAssistWithBatches(bool(__cdecl* callback)(void*), void* context, jqBatchGroup* GroupID);
void jqShutdown();
void jqStart();

inline unsigned __int64 jqGet(unsigned __int64* Cell)
{
	// TODO
	unsigned __int64 result;

	*(unsigned __int64*)((char*)&result + 4) = *Cell;
	return result;
}

template<typename T, unsigned int I>
inline void jqAtomicQueue<T, I>::AllocateNodeBlock(int Count)
{
	UNIMPLEMENTED(__FUNCTION__);
	return;

	NodeType* allocation;
	int i;

	allocation = (NodeType*)tlMemAlloc(sizeof(NodeType) * Count, 8, 0);
	for (i = 0; i < Count - 1; ++i)
	{
		allocation->Next = &allocation[i];
	}
}

template<typename T, unsigned int I>
inline jqAtomicQueue<T, I>::NodeType* jqAtomicQueue<T, I>::AllocateNode()
{
	UNIMPLEMENTED(__FUNCTION__);
	return nullptr;
}

template<typename T, unsigned int I>
inline void jqAtomicQueue<T, I>::Init(jqAtomicQueue<T, I>* SharedFreeList)
{
	UNIMPLEMENTED(__FUNCTION__);
}

template<typename T, unsigned int I>
inline void jqAtomicQueue<T, I>::Push(const jqBatch* Data)
{
	UNIMPLEMENTED(__FUNCTION__);
}

template<typename T, unsigned int I>
inline bool jqAtomicQueue<T, I>::Pop(jqBatch* p)
{
	UNIMPLEMENTED(__FUNCTION__);
	return false;
}
