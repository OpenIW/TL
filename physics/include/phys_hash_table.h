#pragma once

template<typename T,int I>
class minspec_hash_table
{
public:
	T* m_hash_table[I];
	unsigned int m_mod;
	unsigned int m_highest_collision;
	unsigned int m_total_collisions;
};