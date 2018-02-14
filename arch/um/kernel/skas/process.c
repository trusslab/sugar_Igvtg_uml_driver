/*
 * Copyright (C) 2002 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <as-layout.h>
#include <kern.h>
#include <os.h>
#include <skas.h>
#include <linux/prints.h>

extern void start_kernel(void);

static int __init start_kernel_proc(void *unused)
{
	int pid;

	block_signals();
	pid = os_getpid();

	cpu_tasks[0].pid = pid;
	cpu_tasks[0].task = current;

	start_kernel();
	return 0;
}

extern int userspace_pid[];

extern char cpu0_irqstack[];

void isol_set_current(void);

int __init start_uml(void)
{
	stack_protections((unsigned long) &cpu0_irqstack);
	set_sigstack(cpu0_irqstack, THREAD_SIZE);

	init_new_thread_signals();

	init_task.thread.request.u.thread.proc = start_kernel_proc;
	init_task.thread.request.u.thread.arg = NULL;
#ifndef CONFIG_ISOL_USER
	return start_idle_thread(task_stack_page(&init_task),
				 &init_task.thread.switch_buf);
#else
	start_kernel();
#endif
}

unsigned long current_stub_stack(void)
{
	if (current->mm == NULL)
		return 0;

	return current->mm->context.id.stack;
}
