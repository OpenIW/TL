#pragma once

#include <tl_system.h>
#include <tl_thread.h>

class minspec_mutex
{
public:
	volatile unsigned int m_token;

	void Lock()
	{
		while (!tlAtomicCompareAndSwap(&this->m_token, 1u, 0));
		tlMemoryFence();
	}
	void Unlock()
	{
		tlMemoryFence();
		tlAssert(m_token == 1);
		if (!tlAtomicCompareAndSwap(&this->m_token, 0, 1u))
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
		do
		{
			while (!this->m_count);
		} while (!tlAtomicCompareAndSwap(&this->m_count, this->m_count + 1, this->m_count));
		tlMemoryFence();
	}
	void ReadUnlock()
	{
		LONG count;
		do
		{
			count = m_count;
			tlAssert(count > 1);
		}
		while (!tlAtomicCompareAndSwap(&this->m_count, count - 1, count));
	}
	void WriteLock()
	{
		while (!tlAtomicCompareAndSwap(&this->m_count, 0, 1u));
		tlMemoryFence();
	}
	void WriteUnlock()
	{
		tlMemoryFence();
		tlAssert(m_count == 0);
		if (!tlAtomicCompareAndSwap(&this->m_count, 1u, 0))
		{
			tlAssert(false);
		}
	}
};
