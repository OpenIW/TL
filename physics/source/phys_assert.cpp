#include "phys_assert.h"
#include "tl_system.h"

void (__cdecl* g_phys_debug_callback)(void*);
phys_assert_info* g_list_phys_assert_info;

void phys_set_debug_callback(void(*debug_callback)(void*))
{
	g_phys_debug_callback = debug_callback;
}

void phys_exec_debug_callback(void* data)
{
	g_phys_debug_callback(data);
}

void PHYS_WARNING(const char* file, int line, const char* expr, const char* desc)
{
	char strbuf[512];

	sprintf(strbuf, "%s(%d) - '%s' \"%s\"\n", file, line, expr, desc);
	tlWarning(strbuf);
}

phys_assert_info::phys_assert_info(int max_hits_total, int max_hits_per_frame, bool use_warnings_only)
{
	this->m_max_hits_total = max_hits_total;
	this->m_hits_total_count = 0;
	this->m_hits_frame_count = 0;
	this->m_max_hits_per_frame = max_hits_per_frame;
	this->m_use_warnings_only = use_warnings_only;
	this->m_next = g_list_phys_assert_info;
	g_list_phys_assert_info = this;
}

void phys_assert_info::frame_advance()
{
	this->m_hits_frame_count = 0;
}

void phys_assert_info::phys_assert_info_frame_advance_all()
{
	for (phys_assert_info* i = g_list_phys_assert_info; i; i = i->m_next)
		i->m_hits_frame_count = 0;
}
