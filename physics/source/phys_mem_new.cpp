#include "phys_mem_new.h"

void* g_phys_memory_buffer;
int g_phys_memory_buffer_size;
phys_memory_manager* g_phys_memory_manager;

#define allocation_owner 0xFEDCBA98

void* phys_slot_pool::allocate_slot()
{
	return nullptr;
}

unsigned int phys_slot_pool::encode_size_alignment(unsigned int size, unsigned int alignment)
{
	tlAssert(size <= 0xFFFF);
	tlAssert(alignment <= 0xFFFF);

	return size | (alignment << 16);
}

void phys_slot_pool::extra_info_allocate(void* slot)
{
	extra_info* ei = (extra_info*)((char*)slot + m_map_key - 8);

	tlMemoryFence();
	tlAssert(slot);
	tlAssert(ei->m_slot_pool_owner == this);

	if ((phys_slot_pool*)InterlockedCompareExchange((volatile LONG*)ei->m_slot_pool_owner->m_first_free_slot.m_tag, 0xFEDCBA98, (LONG)this) != this) 
	{
		tlAssert(false);
	}
	InterlockedExchangeAdd((volatile LONG*)&m_allocated_slot_count, 1);
	tlAssert(m_allocated_slot_count <= m_total_slot_count);
	InterlockedExchange((volatile LONG*)&slot, 0);
}

void phys_slot_pool::extra_info_free(void* slot)
{
	extra_info* ei = (extra_info*)((char*)slot + m_map_key - 8);

	tlMemoryFence();
	tlAssert(slot);
	memset(slot, 0xFF, m_map_key - 8);
	tlAssert(ei->m_slot_pool_owner == this);

	if (InterlockedCompareExchange((volatile LONG*)ei->m_slot_pool_owner->m_first_free_slot.m_tag, (LONG)this, 0xFEDCBA98) != 0xFEDCBA98)
	{
		tlAssert(false);
	}

	InterlockedExchangeAdd((volatile LONG*)&m_allocated_slot_count, INFINITE);
	tlAssert(m_allocated_slot_count <= m_total_slot_count);
	tlAssert(m_allocated_slot_count >= 0);
	InterlockedExchange((volatile LONG*)&slot, 0);
}

void phys_slot_pool::extra_info_init(void* slot)
{
}

void phys_slot_pool::free_slot(void* slot)
{
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
	ei = (extra_info*)((char*)slot + m_map_key - 8);
	tlAssert(ei->m_slot_pool_owner == this);
	tlAssert(GetStuff32(&ei->m_allocation_owner) == allocation_owner);
}

int phys_memory_manager::allocate(unsigned int size, unsigned int alignment)
{
	return 0;
}

phys_slot_pool* phys_memory_manager::allocate_slot_pool()
{
	return nullptr;
}

phys_slot_pool* phys_memory_manager::get_slot_pool(unsigned int slot_size, unsigned int slot_alignment)
{
	unsigned int sizeAlignment;
	phys_slot_pool* i;
	phys_slot_pool* j;

	tlAssert(slot_alignment >= 4);
	tlAssert(slot_size % slot_alignment == 0);
	sizeAlignment = phys_slot_pool::encode_size_alignment(slot_size + 8, slot_alignment);

	// TODO
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
