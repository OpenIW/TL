#pragma once

#define TABLE_SIZE 64

template<typename T,int I>
class minspec_hash_table
{
public:
	T* m_hash_table[I];
	unsigned int m_mod;
	unsigned int m_highest_collision;
	unsigned int m_total_collisions;

	void add(unsigned int key, T* entry_to_add)
	{
		unsigned int i;
		unsigned int entry = 1;
		unsigned int k;
		unsigned int l;
		unsigned int m;
		unsigned int total_collisions = 0;
		unsigned int highest_collision = 0;
		unsigned int map_key;
		T* j;
		unsigned int collision_counts[TABLE_SIZE];
		T* entry_list[TABLE_SIZE];

		tlAssert(find(key));
		
		entry_list[0] = entry_to_add;
		for (i = 0; i < TABLE_SIZE; ++i)
		{
			for (j = this->m_hash_table[i]; j; ++entry)
			{
				entry_list[entry] = j;
				j = j->m_hash_next;
			}
			m_hash_table[i] = 0;
		}
		m_mod = entry;
		m_highest_collision = 100000;
		m_total_collisions = 100000;

		for (k = entry; k < TABLE_SIZE; ++k)
		{
			for (l = 0; l < k; ++l)
			{
				collision_counts[l] = 0;
			}
			for (m = 0; m < entry; ++m)
			{
				map_key = entry_list[m]->get_count();
				if (++collision_counts[map_key] > highest_collision)
				{
					highest_collision = collision_counts[map_key];
				}
				total_collisions += collision_counts[map_key];
			}
			if (total_collisions < m_total_collisions)
			{
				m_mod = k;
				m_highest_collision = highest_collision;
				m_total_collisions = total_collisions;
			}
		}
		tlAssert(m_mod > 0 && m_mod < TABLE_SIZE);
		for (k = 0; k < entry; ++k)
		{
			map_key = entry_list[k]->get_count();
			entry_list[k]->set_hash_next(m_hash_table[map_key % m_mod]);
			m_hash_table[map_key % m_mod] = entry_list[k];
		}
	}
	T* find(unsigned int key)
	{
		T* i;

		for (i = m_hash_table[key % m_mod]; i && i->get_count() != key; i = i->get_hash_next());
		return i;
	}
};