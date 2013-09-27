/*
 * Stack trace management functions
 *
 *  Copyright (C) 2006-2009 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 */
#include <linux/sched.h>
#include <linux/stacktrace.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/stacktrace.h>

static int save_stack_stack(void *data, char *name)
{
	return 0;
}

static void
__save_stack_address(void *data, unsigned long addr, bool reliable, bool nosched)
{
	struct stack_trace *trace = data;
#ifdef CONFIG_FRAME_POINTER
	if (!reliable)
		return;
#endif
	if (nosched && in_sched_functions(addr))
		return;
	if (trace->skip > 0) {
		trace->skip--;
		return;
	}
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = addr;
}

static void save_stack_address(void *data, unsigned long addr, int reliable)
{
	return __save_stack_address(data, addr, reliable, false);
}

static void
save_stack_address_nosched(void *data, unsigned long addr, int reliable)
{
	return __save_stack_address(data, addr, reliable, true);
}

static const struct stacktrace_ops save_stack_ops = {
	.stack		= save_stack_stack,
	.address	= save_stack_address,
	.walk_stack	= print_context_stack,
};

static const struct stacktrace_ops save_stack_ops_nosched = {
	.stack		= save_stack_stack,
	.address	= save_stack_address_nosched,
	.walk_stack	= print_context_stack,
};

/*
 * Save stack-backtrace addresses into a stack_trace buffer.
 */
void save_stack_trace(struct stack_trace *trace)
{
	dump_trace(current, NULL, NULL, 0, &save_stack_ops, trace);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}
EXPORT_SYMBOL_GPL(save_stack_trace);

void save_stack_trace_regs(struct pt_regs *regs, struct stack_trace *trace)
{
	dump_trace(current, regs, NULL, 0, &save_stack_ops, trace);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}

void save_stack_trace_tsk(struct task_struct *tsk, struct stack_trace *trace)
{
	dump_trace(tsk, NULL, NULL, 0, &save_stack_ops_nosched, trace);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}
EXPORT_SYMBOL_GPL(save_stack_trace_tsk);

/* Userspace stacktrace - based on kernel/trace/trace_sysprof.c */

/* PJH: on an x86 stack frame, the caller saves the return address at
 * the top of its stack frame, and then the first thing that the
 * callee does is save the ebp (frame pointer register) at the bottom
 * of its stack frame. So, when copying directly from memory we go
 * from low addresses to high: the saved ebp (next_fp) comes first,
 * followed by ret_addr.
 */
struct stack_frame_user {
	const void __user	*next_fp;
	unsigned long		ret_addr;
};

/* PJH: Returns:
 *   -1 if fp is not a valid user space pointer.
 *   -2 if __copy_from_user_inatomic() failed (seems unlikely? I'm not
 *      sure yet why it would fail).
 *    0 on success.
 */
static int
copy_stack_frame(const void __user *fp, struct stack_frame_user *frame)
{
	int ret;

	if (!access_ok(VERIFY_READ, fp, sizeof(*frame)))
		return -1;

	ret = 0;
	pagefault_disable();
	if (__copy_from_user_inatomic(frame, fp, sizeof(*frame)))
		ret = -2;
	pagefault_enable();

	return ret;
}

/* PJH: returns a code describing why the userstacktrace unwind stopped or
 * failed:
 *   'c': copy from stack memory into kernel data structure failed; seems
 *        unlikely?
 *   'e': we read a frame pointer from the stack that's equivalent to
 *        the current function's frame pointer - not sure what this means
 *        yet...
 *   'k': this is a kernel thread.
 *   'l': we read an invalid frame pointer from the stack; not sure if this
 *        is ever expected, or if it just means that we can't tell what's
 *        going on in the current stack (e.g. because it doesn't even
 *        store frame pointers at all).
 *   'n': reached max number of entries
 *   'p': tried to follow a frame pointer that is not a valid user space
 *        address. Means that stack frames aren't correctly storing
 *        frame pointers?
 *   'z': default max_entries is 0 - never expect this.
 */
static inline char __save_stack_trace_user(struct stack_trace *trace)
{
	const struct pt_regs *regs = task_pt_regs(current);
	const void __user *fp = (const void __user *)regs->bp;
	char reason = 'z';
	int ret;

	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = regs->ip;

	while (trace->nr_entries < trace->max_entries) {
		struct stack_frame_user frame;

		frame.next_fp = NULL;
		frame.ret_addr = 0;
		ret = copy_stack_frame(fp, &frame);
		if (ret) {
			if (ret == -1)
				reason = 'p';
			else
				reason = 'c';
			break;
		}
		if ((unsigned long)fp < regs->sp) {
			/* Why doesn't this check happen BEFORE calling copy_stack_frame()?
			 * The check isn't affected by that call...
			 */
			reason = 'l';
			break;
		}
		if (frame.ret_addr) {
			trace->entries[trace->nr_entries++] =
				frame.ret_addr;
		}
		if (fp == frame.next_fp) {
			reason = 'e';
			break;
		}
		fp = frame.next_fp;
		reason = 'n';
	}

	return reason;
}

/* PJH: returns a code describing why the userstacktrace unwind stopped or
 * failed: see __save_stack_trace_user().
 */
char save_stack_trace_user(struct stack_trace *trace)
{
	char reason = 'k';

	/*
	 * Trace user stack if we are not a kernel thread
	 */
	if (current->mm) {
		reason = __save_stack_trace_user(trace);
	}
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;

	return reason;
}

