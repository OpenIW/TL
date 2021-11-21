#pragma once

#include "phys_mutex.h"
#include "phys_hash_table.h"

volatile struct tagged_void_pointer_t
{
	void* m_ptr;
	unsigned int m_tag;
};

class phys_slot_pool
{
public:
	volatile tagged_void_pointer_t m_first_free_slot;
	unsigned int m_map_key;
	phys_slot_pool* m_hash_next;
	int m_total_slot_count;
	int m_allocated_slot_count;

	struct extra_info
	{
		phys_slot_pool* m_slot_pool_owner;
		void* m_allocation_owner;
	};

	int get_count();
	static unsigned __int16 get_slot_size(int count);
	extra_info* get_ei(void* slot, int count);
	void* allocate_slot();
	static unsigned int encode_size_alignment(unsigned int size, unsigned int alignment);
	void extra_info_allocate(void* slot);
	void extra_info_free(void* slot);
	void extra_info_init(void* slot);
	void free_slot(void* slot);
	void init(unsigned int slot_size, unsigned int slot_alignment);
	void validate_slot(void* slot);
};

class phys_memory_manager
{
public:
	char* m_buffer_start;
	char* m_buffer_end;
	char* m_buffer_cur;
	int m_list_slot_pool_count;
	minspec_read_write_mutex m_slot_pool_map_mutex;
	minspec_hash_table<phys_slot_pool, 64> m_slot_pool_map;
	minspec_mutex m_slot_pool_allocate_mutex;
	__declspec(align(8)) phys_slot_pool m_list_preallocated_slot_pools[28];
	int m_list_preallocated_slot_pools_count;

	int allocate(unsigned int size, unsigned int alignment);
	phys_slot_pool* allocate_slot_pool();
	phys_slot_pool* get_slot_pool(unsigned int slot_size, unsigned int slot_alignment);
	phys_memory_manager(void* memory_buffer, int memory_buffer_size);
};

phys_slot_pool* GET_PHYS_SLOT_POOL(unsigned int size, unsigned int alignment);
void phys_memory_manager_init(void* memory_buffer, const int memory_buffer_size);
void phys_memory_manager_term();