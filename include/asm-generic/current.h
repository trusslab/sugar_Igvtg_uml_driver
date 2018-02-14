#ifndef __ASM_GENERIC_CURRENT_H
#define __ASM_GENERIC_CURRENT_H

#include <linux/thread_info.h>

#define get_current() (current_thread_info()->task)
extern struct task_struct *isol_task;
#define current isol_task

#endif /* __ASM_GENERIC_CURRENT_H */
