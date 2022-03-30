#include "phys_mem_new.h"

void* g_phys_memory_buffer;
int g_phys_memory_buffer_size;
phys_memory_manager* g_phys_memory_manager;

#define allocation_owner 0xFEDCBA98

phys_slot_pool* phys_slot_pool::get_hash_next()
{
	return m_hash_next;
}

void phys_slot_pool::set_hash_next(phys_slot_pool* hash)
{
	m_hash_next = hash;
}

int phys_slot_pool::get_count()
{
	return m_map_key;
}

unsigned __int16 phys_slot_pool::get_slot_size(int count)
{
	return (unsigned __int16)count;
}

phys_slot_pool::extra_info* phys_slot_pool::get_ei(void* slot, int count)
{
	return (phys_slot_pool::extra_info*)((int)slot + get_slot_size(count) - 8);
}

void* phys_slot_pool::allocate_slot()
{
	return NULL;
}

unsigned int phys_slot_pool::encode_size_alignment(unsigned int size, unsigned int alignment)
{
	tlAssert(size <= 0xFFFF);
	tlAssert(alignment <= 0xFFFF);

	return size | (alignment << 16);
}

void phys_slot_pool::extra_info_allocate(void* slot)
{
	extra_info* ei;

	tlMemoryFence();
	ei = get_ei(slot, get_count());
	tlAssert(slot);
	tlAssert(ei->m_slot_pool_owner == this);

	tlAssert(!tlAtomicCompareAndSwap((volatile u32*)&ei->m_slot_pool_owner->m_first_free_slot.m_tag, 0xFEDCBA98, (LONG)this));
	tlAtomicIncrement(&m_allocated_slot_count);
	tlAssert(m_allocated_slot_count <= m_total_slot_count);
	tlMemoryFence();
}

void phys_slot_pool::extra_info_free(void* slot)
{
	extra_info* ei;

	tlMemoryFence();
	ei = get_ei(slot, get_count());
	tlAssert(slot);
	memset(slot, 0xFF, m_map_key - 8);
	tlAssert(ei->m_slot_pool_owner == this);

	tlAssert(tlAtomicCompareAndSwap((volatile u32*)&ei->m_slot_pool_owner->m_first_free_slot.m_tag, (LONG)this, 0xFEDCBA98));

	tlAtomicDecrement(&m_allocated_slot_count);
	tlAssert(m_allocated_slot_count <= m_total_slot_count);
	tlAssert(m_allocated_slot_count >= 0);
	tlMemoryFence();
}

void phys_slot_pool::extra_info_init(void* slot)
{
	extra_info* ei;
	if (slot)
	{
		ei = get_ei(slot, get_count());
		ei->m_slot_pool_owner = this;
		ei->m_allocation_owner = (void*)allocation_owner;
		tlAtomicIncrement(&m_total_slot_count);
		tlAtomicIncrement(&m_allocated_slot_count);
		tlAssert(m_allocated_slot_count <= m_total_slot_count);
		tlMemoryFence();
	}
}

void phys_slot_pool::free_slot(void* _slot)
{
	u64 next_slot;
	u32 slot;
	tagged_void_pointer_t first_slot;

	slot = (u32)_slot - (u32)g_phys_memory_manager->m_buffer_start;
	tlAssert(_slot);
	extra_info_free(_slot);
	do
	{
		first_slot.set(&this->m_first_free_slot);
		*(u32*)&g_phys_memory_manager->m_buffer_start[slot] = first_slot.m_ptr;
		tlMemoryFence();
		(*(tagged_void_pointer_t*)&next_slot).m_ptr = slot;
		(*(tagged_void_pointer_t*)&next_slot).m_tag = first_slot.m_tag + 1;

	} while (!tlAtomicCompareAndSwap((volatile u64*)&this->m_first_free_slot, next_slot, *(u64*)&first_slot));
}

void phys_slot_pool::init(unsigned int slot_size, unsigned int slot_alignment)
{
	m_first_free_slot.m_ptr = 0;
	m_first_free_slot.m_tag = 0;
	m_map_key = encode_size_alignment(slot_size, slot_alignment);
	m_total_slot_count = 0;
	m_allocated_slot_count = 0;
}

void phys_slot_pool::validate_slot(void* slot)
{
	extra_info* ei;

	tlMemoryFence();
	ei = get_ei(slot, get_count());
	tlAssert(ei->m_slot_pool_owner == this);
	tlAssert(GetStuff32(&ei->m_allocation_owner) == allocation_owner);
}

void* phys_memory_manager::allocate(unsigned int size, unsigned int alignment)
{
	char* alignedPos;

	while (1)
	{
		alignedPos = PHYS_ALIGN(m_buffer_cur, alignment);
		if (alignedPos + size > m_buffer_end)
		{
			return 0;
		}
		if (InterlockedCompareExchange((volatile LONG*)m_buffer_cur, (int)(alignedPos + size), *m_buffer_cur) == *m_buffer_cur)
		{
			return alignedPos;
		}
	}
}

phys_slot_pool* phys_memory_manager::allocate_slot_pool()
{
	phys_slot_pool* slotPool;

	m_slot_pool_allocate_mutex.Lock();
	if (m_list_preallocated_slot_pools_count >= 28)
	{
		slotPool = (phys_slot_pool*)allocate(sizeof(phys_slot_pool), 8);
	}
	else
	{
		slotPool = &m_list_preallocated_slot_pools[m_list_preallocated_slot_pools_count++];
	}
	++m_list_slot_pool_count;
	m_slot_pool_allocate_mutex.Unlock();
	return slotPool;
}

phys_slot_pool* phys_memory_manager::get_slot_pool(unsigned int slot_size, unsigned int slot_alignment)
{
	unsigned int key;
	phys_slot_pool* slotPool;

	tlAssert(slot_alignment >= 4);
	tlAssert(slot_size % slot_alignment == 0);

	key = phys_slot_pool::encode_size_alignment(slot_size + 8, slot_alignment);
	m_slot_pool_map_mutex.ReadLock();
	slotPool = m_slot_pool_map.find(key);
	m_slot_pool_map_mutex.ReadUnlock();

	if (!slotPool)
	{
		m_slot_pool_map_mutex.WriteLock();
		slotPool = m_slot_pool_map.find(key);
		if (!slotPool)
		{
			slotPool = allocate_slot_pool();
			slotPool->init(slot_size + 8, slot_alignment);
			m_slot_pool_map.add(key, slotPool);
		}
		m_slot_pool_map_mutex.WriteUnlock();
	}
	return slotPool;
}

phys_memory_manager::phys_memory_manager(void* memory_buffer, int memory_buffer_size)
{
	m_slot_pool_map_mutex.m_count = 1;
	memset(&m_slot_pool_map, 0, 256);
	m_slot_pool_map.m_mod = 1;
	m_slot_pool_map.m_highest_collision = 0;
	m_slot_pool_map.m_total_collisions = 0;
	m_slot_pool_allocate_mutex.m_token = 0;
	m_buffer_start = (char*)memory_buffer;
	m_buffer_cur = (char*)memory_buffer;
	m_buffer_end = (char*)memory_buffer + memory_buffer_size;
	m_list_slot_pool_count = 0;
	memset(&m_slot_pool_map, 0, 256);
	m_slot_pool_map.m_highest_collision = 0;
	m_slot_pool_map.m_total_collisions = 0;
	m_slot_pool_map.m_mod = 1;
	m_list_preallocated_slot_pools_count = 0;
}

char* PHYS_ALIGN(char* pos, int alignment)
{
	return (char*)tl_align((int)pos, alignment);
}

phys_slot_pool* GET_PHYS_SLOT_POOL(unsigned int size, unsigned int alignment)
{
	tlAssertMsg(size >= sizeof(uintptr_t), "Allocations smaller than pointer size are not supported.");
	return g_phys_memory_manager->get_slot_pool(size, alignment);
}

void phys_memory_manager_init(void* memory_buffer, const int memory_buffer_size)
{
	phys_memory_manager* memManager;

	tlAssert(g_phys_memory_buffer == NULL);
	tlAssert(g_phys_memory_buffer_size == 0);
	tlAssert(g_phys_memory_manager == NULL);

	memManager = (phys_memory_manager*)(((unsigned int)memory_buffer + 7) & 0xFFFFFFF8);
	g_phys_memory_buffer = memory_buffer;
	g_phys_memory_buffer_size = memory_buffer_size;
	g_phys_memory_manager = memManager;
	if (memManager)
	{
		memManager = &phys_memory_manager(memory_buffer, memory_buffer_size);
	}
}

void phys_memory_manager_term()
{
	g_phys_memory_buffer = 0;
	g_phys_memory_buffer_size = 0;
	g_phys_memory_manager = 0;
}

void* PMM_PERM_ALLOCATE(const size_t size, const u32 alignment)
{
	void* ptr = g_phys_memory_manager->allocate(size, alignment);
	tlAssertMsg(ptr, "physics memory manager error: out of memory.");
	return ptr;
}

void* PMM_ALLOC(const size_t size, const u32 alignment)
{
	phys_slot_pool* psp;

	tlAssert(size);
	psp = g_phys_memory_manager->get_slot_pool(size, alignment);
	return psp->allocate_slot();
}

void PMM_FREE(void* ptr, const size_t size, const u32 alignment)
{
	phys_slot_pool* slot_pool = g_phys_memory_manager->get_slot_pool(size, alignment);
	slot_pool->free_slot(ptr);
}

void* PSP_ALLOC(void* slot_pool)
{
	return reinterpret_cast<phys_slot_pool*>(slot_pool)->allocate_slot();
}

void PMM_VALIDATE(void* ptr, const size_t size, const u32 alignment)
{
	phys_slot_pool* slot_pool = g_phys_memory_manager->get_slot_pool(size, alignment);
	slot_pool->validate_slot(ptr);
}
