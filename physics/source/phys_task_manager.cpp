#include "tl_system.h"
#include "jobqueue.h"

#include "phys_task_manager.h"

extern void IW_task_manager_flush();
extern void IW_task_manager_add_batch(jqBatch* ptr);

int g_phys_task_manager_inited;
int g_phys_task_manager_current_worker_count;
jqBatch g_phys_task_manager_batch;

void phys_task_manager_init()
{
	tlAssert(g_phys_task_manager_inited == 0);
	tlAssert(g_phys_task_manager_current_worker_count == 0);

	g_phys_task_manager_inited = 1;
	g_phys_task_manager_batch.Module = 0;
	g_phys_task_manager_batch.Input = 0;
	g_phys_task_manager_batch.Output = 0;
}

void phys_task_manager_flush()
{
	tlAssert(g_phys_task_manager_inited == 1);
	if (g_phys_task_manager_current_worker_count > 0)
	{
		g_phys_task_manager_current_worker_count = 0;
		IW_task_manager_flush();
	}
}

int phys_task_manager_needs_flush()
{
	tlAssert(g_phys_task_manager_inited == 1);
	return g_phys_task_manager_current_worker_count > 0;
}

void phys_task_manager_process(jqModule* module, void* input, int input_count)
{
	tlAssert(g_phys_task_manager_inited == 1);
	tlAssert(g_phys_task_manager_current_worker_count == 0);
	if (input_count)
	{
		if (input_count > 6)
		{
			input_count = 6;
		}
		g_phys_task_manager_current_worker_count = input_count;
		g_phys_task_manager_batch.Module = module;
		g_phys_task_manager_batch.Input = input;
		if (input_count > 0)
		{
			do
			{
				IW_task_manager_add_batch(&g_phys_task_manager_batch);
				--input_count;
			} while (input_count);
		}
	}
}

void phys_task_manager_shutdown()
{
	tlAssert(g_phys_task_manager_inited == 1);
	tlAssert(g_phys_task_manager_current_worker_count == 0);
	g_phys_task_manager_inited = 0;
}
