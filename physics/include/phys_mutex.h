#pragma once

#include <tl_system.h>
#include <tl_thread.h>

class minspec_mutex
{
public:
	volatile unsigned int m_token;

	void Lock()
	{
		while (InterlockedCompareExchange((volatile LONG*)m_token, 1, 0));
		tlMemoryFence();
	}
	void Unlock()
	{
		tlMemoryFence();
		tlAssert(m_token == 1);
		if (InterlockedCompareExchange((volatile LONG*)m_token, 0, 1) != 1)
		{
			tlAssert(false);
		}
	}
};

class minspec_read_write_mutex
{
public:
	volatile unsigned int m_count;

	void ReadLock()
	{
		LONG Target[3];
		unsigned int count;

		do
		{
			do {
				count = m_count;
			} while (!count);
			Target[1] = count;
			Target[2] = count + 1;
		}
		while (InterlockedCompareExchange((volatile LONG*)m_count, count + 1, count) != count);
		Target[0] = 0;
		InterlockedExchange(Target, 0);
	}
	void ReadUnlock()
	{
		LONG count;
		do
		{
			count = m_count;
			tlAssert(count > 1);
		}
		while (InterlockedCompareExchange((volatile LONG*)m_count, count - 1, count) != count);
	}
	void WriteLock()
	{
		LONG Target; // [esp+4h] [ebp-4h] BYREF

		while (InterlockedCompareExchange((volatile LONG*)m_count, 0, 1) != 1);
		tlMemoryFence();
	}
	void WriteUnlock()
	{
		tlMemoryFence();
		tlAssert(m_count == 0);
		if (InterlockedCompareExchange((volatile LONG*)m_count, 1, 0) != 0)
		{
			tlAssert(false);
		}
	}
};
