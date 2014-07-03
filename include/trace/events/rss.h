/* (c) 2013-2014, University of Washington, Department of Computer
 * Science & Engineering, Peter Hornyack.
 *
 * RSS page event tracing.
 * Written in 2013-2014 by Peter Hornyack, pjh@cs.washington.edu
 */

//#include <asm/atomic-long.h>
#include <linux/mm_types.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM rss

#if !defined(_TRACE_RSS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_RSS_H

#include <linux/tracepoint.h>

/* For info about how to fill in a TRACE_EVENT, seetrace-events-sample.h */

#define RSSTRACE_BUF_LEN 256

/* Define a class of trace events, "rss_event", that all take the same args
 * and have the same output. See include/trace/events/kmem.h for an example.
 *
 * The values output from these trace events are *page counts* - you can
 * see how they are used for generating /proc file output for swapped
 * memory and Rss memory in fs/proc/task_mmu.c. Note that these page
 * counts may temporarily go negative due to the way that an mm's rss
 * stats are updated asynchronously from the threads using the mm (see
 * check_sync_rss_stat() etc.).
 *
 * IMPORTANT: when these trace events are enabled, do not set the
 * options/userstacktrace tracing option, or else you will likely cause
 * a kernel deadlock on your CPU core: while the userstack entries are
 * being gathered for a rss_* trace event emitted from the do_page_fault
 * code path, you'll trigger another page fault, which can't be handled
 * because interrupts / preemption are disabled (or something like this...)
 */
DECLARE_EVENT_CLASS(rss_event,
	TP_PROTO(struct task_struct *cur_task,
		int counter_id,
		atomic_long_t *current_val,
		const char *label),
	TP_ARGS(cur_task, counter_id, current_val, label),
	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__field(pid_t, ptgid)
		__array(char, label, RSSTRACE_BUF_LEN)
		__field(int, counter_id)
		__field(long, current_val)
	),
	TP_fast_assign(
		__entry->pid   = cur_task->pid;
		__entry->tgid  = cur_task->tgid;
		/* Grab parent's tgid: this extra pointer dereference is kind of
		 * a bummer, but hopefully not much performance impact.
		 */
		__entry->ptgid = cur_task->real_parent ?
		                 cur_task->real_parent->tgid : -1;
		strncpy(__entry->label, label, RSSTRACE_BUF_LEN-1);
		__entry->label[RSSTRACE_BUF_LEN-1] = '\0';  //defensive
		__entry->counter_id = counter_id;
		__entry->current_val = atomic_long_read(current_val);
		    // https://lwn.net/Articles/174938/
	),

	TP_printk("pid=%d tgid=%d ptgid=%d [%s]: rss_stat[%s]=%ld",
		__entry->pid,
		__entry->tgid,
		__entry->ptgid,
		__entry->label,
		__entry->counter_id     == MM_FILEPAGES ? "MM_FILEPAGES" :
			__entry->counter_id == MM_ANONPAGES ? "MM_ANONPAGES" :
			__entry->counter_id == MM_SWAPENTS  ? "MM_SWAPENTS"  :
			                                      "UNKNOWN!",
		__entry->current_val
	)
);

/* Trace events:
 *   trace_mm_rss(task, counter_id, current_val, label)
 *     - counter_id should be one of [MM_FILEPAGES, MM_ANONPAGES, MM_SWAPENTS]
 *       (see include/linux/mm_types.h)
 *     - current_val should be a *pointer* to the current value that's
 *       kept in the rss_stat array, e.g.:
 *           &mm->rss_stat.count[counter_id]
 *       A terrific benefit of using the current value is that if we screw
 *       up somewhere (e.g. we emit a trace event for the wrong task or
 *       mm struct), the next event that we emit will still be correct.
 */
DEFINE_EVENT(rss_event, mm_rss,  //trace_mm_rss()
	TP_PROTO(struct task_struct *cur_task,
		int counter_id,
		atomic_long_t *current_val,
		const char *label),
	TP_ARGS(cur_task, counter_id, current_val, label)
);

/* dup_mmap() -> copy_page_range() -> copy_pud_range() -> copy_pmd_range() ->
 * copy_huge_pmd() needs to be traced in the context of the new / destination
 * / non-current task, so this alternate trace event is used.
 */
TRACE_EVENT(mm_rss_notcurrent,
	TP_PROTO(pid_t trace_pid,
		pid_t trace_tgid,
		struct task_struct *trace_real_parent,
		int counter_id,
		atomic_long_t *current_val,
		const char *label),
	TP_ARGS(trace_pid, trace_tgid, trace_real_parent, counter_id,
		current_val, label),
	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__field(pid_t, ptgid)
		__array(char, label, RSSTRACE_BUF_LEN)
		__field(int, counter_id)
		__field(long, current_val)
	),
	TP_fast_assign(
		__entry->pid   = trace_pid;
		__entry->tgid  = trace_tgid;
		/* Grab parent's tgid: this extra pointer dereference is kind of
		 * a bummer, but hopefully not much performance impact.
		 */
		__entry->ptgid = trace_real_parent ?
		                 trace_real_parent->tgid : -1;
		strncpy(__entry->label, label, RSSTRACE_BUF_LEN-1);
		__entry->label[RSSTRACE_BUF_LEN-1] = '\0';  //defensive
		__entry->counter_id = counter_id;
		__entry->current_val = atomic_long_read(current_val);
		    // https://lwn.net/Articles/174938/
	),

	TP_printk("pid=%d tgid=%d ptgid=%d [%s]: rss_stat[%s]=%ld",
		__entry->pid,
		__entry->tgid,
		__entry->ptgid,
		__entry->label,
		__entry->counter_id     == MM_FILEPAGES ? "MM_FILEPAGES" :
			__entry->counter_id == MM_ANONPAGES ? "MM_ANONPAGES" :
			__entry->counter_id == MM_SWAPENTS  ? "MM_SWAPENTS"  :
			                                      "UNKNOWN!",
		__entry->current_val
	)
);

#endif /* _TRACE_RSS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
