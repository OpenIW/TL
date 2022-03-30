#pragma once

void phys_set_debug_callback(void(*debug_callback)(void*));
void phys_exec_debug_callback(void* data);
void PHYS_WARNING(const char* file, int line, const char* expr, const char* desc);

class phys_assert_info
{
public:
	int m_hits_total_count;
	int m_hits_frame_count;
	int m_max_hits_total;
	int m_max_hits_per_frame;
	bool m_use_warnings_only;
	phys_assert_info* m_next;

	phys_assert_info(int max_hits_total, int max_hits_per_frame, bool use_warnings_only);
	void frame_advance();
	static void phys_assert_info_frame_advance_all();
};