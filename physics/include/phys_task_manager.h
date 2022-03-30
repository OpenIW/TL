#pragma once

void phys_task_manager_init();
void phys_task_manager_flush();
int phys_task_manager_needs_flush();
void phys_task_manager_process(jqModule* module, void* input, int input_count);
void phys_task_manager_shutdown();