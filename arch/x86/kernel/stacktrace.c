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
 *   -2 if __copy_from_user_inatomic() failed. In experiments with
 *      Graph500, this is the "expected" case - when a long deep stack
 *      unwind concludes, it's because the __copy_from_user_inatomic()
 *      fails in this function. I'm not exactly sure why this case is
 *      "expected" to fail; I'm thinking it happens when we unwind the
 *      frame pointer to a point up ABOVE the bottom of the stack in
 *      memory, hitting a page that is a valid user space ADDRESS (so
 *      the access_ok check succeeds), but is not actually a valid
 *      readable PAGE (so the __copy_from_user_inatomic() fails).
 *    0 on success.
 */
static int
copy_stack_frame(const void __user *fp, struct stack_frame_user *frame,
		bool do_unsafe_copy)
{
	int ret;

	if (!access_ok(VERIFY_READ, fp, sizeof(*frame)))
		return -1;

	ret = 0;
#if 0
	if (do_unsafe_copy) {
		/* PJH: __copy_from_user_inatomic ("in atomic context", not
		 * "non-atomic") has the same signature as __copy_from_user, but
		 * will fail rather than going to sleep. The _inatomic version
		 * is needed because pagefault_disable() actually disables
		 * preemption entirely. Additionally, preemption is disabled
		 * in ftrace_trace_userstack() before this function is called.
		 * When (unnecessarily) investigating the reasons why the
		 * call to __copy_from_user() "fails," I found that disabling
		 * these two preemption-disables does indeed cause deadlock.
		 *
		 * Relevant LKML thread:
		 * http://thread.gmane.org/gmane.linux.kernel/743469/focus=743481
		 * I think
		 * that one reason pagefaults / preemption are disabled is so that
		 * incurring a pagefault doesn't cause more trace events to be
		 * recursively invoked. Another (better) reason, as explained by
		 * is that copy_stack_frame() may be called from ftrace or
		 * rw-semaphore code which may be holding a spinlock.
		 *   Actually, I guess that this may have been true back in 2008,
		 *   but this no longer appears to be the case - copy_stack_frame()
		 *   is *only* called from __save_stack_trace_user() in the tracing
		 *   code right now!
		 *
		 * Unfortunately, I think that calling the "in-atomic" version of
		 * this function causes the stack trace unwinding to fail at least
		 * 50% of the time; I'm not exactly sure how/why the in-atomic
		 * copy is failing, but suspect it may involve trying to trace a
		 * frame pointer that points to a not-yet-valid page, so trying
		 * to read that page would cause a page fault (or other operation
		 * that may sleep), potentially causing deadlock. However, I
		 * don't recall seeing any spinlocks in the *tracing* code for
		 * user stacks that I've been looking at, so I'm hoping that
		 * this unsafe branch of the code that I've added can skip the
		 * pagefault disable, call the normal version of __copy_from_user,
		 * and allow the stack unwinding to succeed much more of the time.
		 * If my hopes are wrong, then this may cause a kernel deadlock.
		 * I guess if sleeping may be required, then this unsafe path
		 * may also add more overhead to kernel tracing.
		 *
		 * Well, I guess I was wrong: running just the new unsafe code
		 * path below does appear to cause deadlocks. I started reading
		 * the code again starting from ftrace_trace_userstack() and
		 * noticed that the recursive tracing problem is handled there
		 * by calling preempt_disable() and by keeping track of a per-CPU
		 * variable (user_stack_count) for the number of entries into this
		 * code path. I'm pretty sure that I can avoid recursive trace
		 * events by only enabling my mmap events and nothing else (like
		 * page fault events or scheduling events), so I can try skipping
		 * the preemption-disabling in the top-level ftrace_trace_userstack
		 * function, which will hopefully mean that sleeping in a call
		 * to __copy_from_user() here won't cause deadlock.
		 *   What about the per-CPU variable? I'm not sure why both
		 *   preemption disabling and this variable are needed, but I'm
		 *   leaving it in for the first try; I could also comment this
		 *   out too...
		 * To enable/disable at runtime, see PJH_UNSAFE_STACK_UNWIND in
		 * kernel/trace/trace.c.
		 *
		 * The only other mention of locking that I can find on this code
		 * path is a call to trace_buffer_lock_reserve(), but I don't
		 * think this is anything like a spinlock - I traced down into it
		 * a bit and it doesn't appear to lock anything permanently, and
		 * it doesn't have a subsequent "unlock" call either.
		 *
		 * TODO: if this does cause deadlock, check on how ftrace and
		 * rw-semaphore userstack tracing works now - if they do something
		 * different than what's still done here, maybe I can change this
		 * function to imitate them!
		 *
		 * __copy_from_user() returns the number of bytes that could NOT
		 * be copied, so on success it will return zero.
		 */
		if (__copy_from_user(frame, fp, sizeof(*frame)))
			ret = -2;
	} else {
#endif
		pagefault_disable();
		/* PJH: copy sizeof(*frame) bytes from fp to frame. Returns 0
		 * on sucess, non-zero on failure.
		 * __copy_from_user_inatomic ("in atomic context", not
		 * "non-atomic") has the same signature as __copy_from_user, but
		 * will fail rather than going to sleep. The _inatomic version
		 * is needed because pagefault_disable() actually disables
		 * preemption entirely. Additionally, preemption is disabled
		 * in ftrace_trace_userstack() before this function is called.
		 * When (unnecessarily) investigating the reasons why the
		 * call to __copy_from_user() "fails," I found that disabling
		 * these two preemption-disables does indeed cause deadlock.
		 *
		 * Relevant LKML thread:
		 * http://thread.gmane.org/gmane.linux.kernel/743469/focus=743481
		 */
		if (__copy_from_user_inatomic(frame, fp, sizeof(*frame)))
			ret = -2;
		pagefault_enable();
#if 0
	}
#endif
	return ret;
}

/* PJH: returns a code describing why the userstacktrace unwind stopped or
 * failed:
 *   'c': copy from stack memory into kernel data structure failed; seems
 *        unlikely?
 *        I'm not sure why this case is hit (why __copy_from_user_inatomic
 *        "fails"), but it looks like this is the reason that gets saved
 *        for pretty much ALL the "good" stack traces (that are deeply
 *        unwound).
 *   'e': we read a frame pointer from the stack that's equivalent to
 *        the current function's frame pointer - not sure what this means
 *        yet...
 *   'k': this is a kernel thread.
 *   'l': we read an invalid frame pointer from the stack; not sure if this
 *        is ever expected, or if it just means that we can't tell what's
 *        going on in the current stack (e.g. because it doesn't even
 *        store frame pointers at all).
 *        In my experiments, this is the case that gets hit for the
 *        userstacktraces that only include a single libc / ld entry
 *        and are not unwound any further :( This means that the initial
 *        fp (coming from the task's ebp/rbp register) is invalid (less
 *        than the stack pointer esp/rsp)!
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
		ret = copy_stack_frame(fp, &frame, false);
		if (ret) {
			if (ret == -1)
				reason = 'p';
			else
				reason = 'c';
			break;
		}
#define PJH_SKIP_DUMBASS_CHECK
#ifndef PJH_SKIP_DUMBASS_CHECK
		if ((unsigned long)fp < regs->sp) {
			/* Why doesn't this check happen BEFORE calling copy_stack_frame()?
			 * The check isn't affected by that call...
			 *
			 * Looking at experimental trace results, THIS is the reason
			 * that stack unwinding often (more than 50% of the time!)
			 * fails to gather more than 1 ip from libc-2.17.so / ld-2.17.so.
			 * Since just a single ip is gathered, it's not the case that
			 * a "bad" fp is found on the stack (from frame.next_fp) -
			 * it's the case that the initial fp (coming from regs->bp
			 * above) is bad!
			 *   I don't know why this is :( Everything was built with
			 *   -fno-omit-frame-pointer and -fno-optimize-sibling-calls...
			 * A thought: even if frame pointers are being correctly
			 * saved on the stack, it could be the case that during the
			 * execution of a function the assembly code saves/spills
			 * the value in rbp to memory, before restoring it when the
			 * function returns. If this function happens to cause a kernel
			 * entry for one of my vma events while the frame pointer
			 * has been temporarily spilled, then the fp we try to START
			 * WITH in this function may be incorrect! And this would
			 * cause just a single ip (for the *current* function) to be
			 * saved in the userstacktrace, since we can't coherently
			 * get anything else off of the stack!
			 *   Looking at implementation of "<mmap>:" in disassembled
			 *   libc-2.17.so (line 213228): what the hell, it doesn't
			 *   touch ebp/rbp, but it doesn't SAVE the previous ebp/rbp
			 *   either! (in effect, it doesn't have a stack frame at
			 *   all).
			 *     Is there a way to force gcc to allocate stack frames
			 *     for every single function call, even if the function
			 *     doesn't need to save anything on the stack?
			 *
			 * Is it possible to force gcc to NEVER spill rbp to
			 * memory, and always keep the frame pointer in this
			 * register at all times??
			 *
			 * Also note that in the kernel, it appears that "unsigned long"
			 * and "void *" will always have the same size; the kernel ABI
			 * "defines" this, for all architectures.
			 *   http://osdir.com/ml/linux.kernel.mm/2004-12/msg00042.html
			 *   "A lot of code in the kernel uses an "unsigned long"
			 *    instead of a "void*" to hold a generic memory address.
			 *    I personally like this practice, if you never intend to
			 *    directly dereference the pointer."
			 */
			reason = 'l';
			break;
		}
#endif
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

