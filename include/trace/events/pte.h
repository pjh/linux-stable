/* (c) 2013-2014, University of Washington, Department of Computer
 * Science & Engineering, Peter Hornyack.
 *
 * PTE event tracing.
 * Written in 2013-2014 by Peter Hornyack, pjh@cs.washington.edu
 */

#include <asm/pgtable.h>
#include <asm/pgtable_types.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM pte

#if !defined(_TRACE_PTE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PTE_H

#include <linux/tracepoint.h>

/* For info about how to fill in a TRACE_EVENT, seetrace-events-sample.h */

#define PTETRACE_BUF_LEN 256

/* Define a class of trace events, "pte_event", that all take the same args
 * and have the same output. See include/trace/events/kmem.h for an example.
 *
 * Note that old_pte and new_pte are taken as "pte_t" values, rather than
 * "pte_t *". If old_pte or new_pte is not available, caller can maybe
 * call mk_pte(-1, -1)...
 *
 * See arch/x86/include/asm/pgtable_types.h and
 * arch/x86/include/asm/pgtable.h for functions to convert between
 * pte_t and pteval_t, to extract flags and pfn out of pteval_t, etc.
 */
DECLARE_EVENT_CLASS(pte_event,
	TP_PROTO(struct task_struct *cur_task,
		struct vm_area_struct *vma,
		unsigned long faultaddr,
		int is_major,
		pte_t old_pte,
		pte_t new_pte,
		const char *label),
	TP_ARGS(cur_task, vma, faultaddr, is_major, old_pte, new_pte, label),
	TP_STRUCT__entry(
		// Old fields kept from vma trace events:
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__field(pid_t, ptgid)
		__array(char, label, PTETRACE_BUF_LEN)
		__field(struct vm_area_struct *, vma)
		__field(unsigned long, vm_start)
		__field(unsigned long, vm_end)
		__field(unsigned long, vm_flags)
//		__field(unsigned long long, vm_pgoff)
//		__field(unsigned int, dev_major)
//		__field(unsigned int, dev_minor)
//		__field(unsigned long, inode)
		__array(char, filename, PTETRACE_BUF_LEN)
		/* New fields for page / pte trace events:
		 *   Note: we have to store pteval_t types here, rather than pte_t:
		 *   pte_t is a struct, but we need a scalar field. (pteval_t is
		 *   just an unsigned long)
		 */
		__field(unsigned long, faultaddr)
		__field(int, is_major)
		__field(pteval_t, old_pteval)
		__field(pteval_t, new_pteval)
	),
	TP_fast_assign(
		__entry->pid   = cur_task->pid;
		__entry->tgid  = cur_task->tgid;
		/* Grab parent's tgid: this extra pointer dereference is kind of
		 * a bummer, but hopefully not much performance impact.
		 */
		__entry->ptgid = cur_task->real_parent ?
		                 cur_task->real_parent->tgid : -1;
		strncpy(__entry->label, label, PTETRACE_BUF_LEN-1);
		__entry->label[PTETRACE_BUF_LEN-1] = '\0';  //defensive
		__entry->vma = vma;
		__entry->vm_start =
			stack_guard_page_start(vma, vma->vm_start) ?
				vma->vm_start + PAGE_SIZE :
				vma->vm_start;
		__entry->vm_end =
			stack_guard_page_end(vma, vma->vm_end) ?
				vma->vm_end - PAGE_SIZE :
				vma->vm_end;
		__entry->vm_flags = vma->vm_flags;
//		__entry->vm_pgoff =
//			vma->vm_file ? ((loff_t)(vma->vm_pgoff)) << PAGE_SHIFT : 0;
//		__entry->dev_major =
//			vma->vm_file ? MAJOR(file_inode(vma->vm_file)->i_sb->s_dev) : 0;
//		__entry->dev_minor = 
//			vma->vm_file ? MINOR(file_inode(vma->vm_file)->i_sb->s_dev) : 0;
//		__entry->inode = 
//			vma->vm_file ? file_inode(vma->vm_file)->i_ino : 0;
		if (vma->vm_file) {
			char *path = d_path(&(vma->vm_file->f_path),
				(char *)(__entry->filename), PTETRACE_BUF_LEN);
			strncpy(__entry->filename, path, PTETRACE_BUF_LEN-1);
			  // I hope the kernel strncpy works with overlapping strings!
			  //   Seems to work just fine...
		} else {
			__entry->filename[0] = '\0';
		}
		__entry->filename[PTETRACE_BUF_LEN-1] = '\0';  //defensive

		__entry->faultaddr = faultaddr;
		__entry->is_major = is_major;
		__entry->old_pteval = pte_val(old_pte);
		__entry->new_pteval = pte_val(new_pte);
	),

	/* See pte_dirty() and the like in arch/x86/include/asm/pgtable.h
	 * for ways to extract the different pte flags (which are defined
	 * in arch/x86/include/asm/pgtable_types.h).
	 */
	TP_printk("pid=%d tgid=%d ptgid=%d [%s]: %p @ %08lx-%08lx %c%c%c%c "
//			"%08llx %02x:%02x %lu "
			"%s faultaddr=%p, is_major=%d, "
			"old_pte_pfn=%lu, old_pte_flags=%08lx, "
			"new_pte_pfn=%lu, new_pte_flags=%08lx",
		__entry->pid,
		__entry->tgid,
		__entry->ptgid,
		__entry->label,
		__entry->vma,
		__entry->vm_start,
		__entry->vm_end,
		__entry->vm_flags & VM_READ ? 'r' : '-',
		__entry->vm_flags & VM_WRITE ? 'w' : '-',
		__entry->vm_flags & VM_EXEC ? 'x' : '-',
		__entry->vm_flags & VM_MAYSHARE ? 's' : 'p',
//		__entry->vm_pgoff,
//		__entry->dev_major,
//		__entry->dev_minor,
//		__entry->inode,
		__entry->filename,

		(void *)__entry->faultaddr,
		__entry->is_major,
		pteval_pfn(__entry->old_pteval),    // type pteval_t (unsigned long)
		pteval_flags(__entry->old_pteval),  // type unsigned long
		pteval_pfn(__entry->new_pteval),
		pteval_flags(__entry->new_pteval)
	)
);

/* Trace events:
 *   pte_mapped(): a new virtual page to physical page mapping has been
 *     established. This trace event should usually be emitted
 *     immediately after a call to set_pte_at() or set_pre_at_notify().
 *   pte_info(): ...
 */
DEFINE_EVENT(pte_event, pte_mapped,  //trace_pte_mapped()
	TP_PROTO(struct task_struct *cur_task,
		struct vm_area_struct *vma,
		unsigned long faultaddr,
		int is_major,
		pte_t old_pte,
		pte_t new_pte,
		const char *label),
	TP_ARGS(cur_task, vma, faultaddr, is_major, old_pte, new_pte, label)
);

DEFINE_EVENT(pte_event, pte_info,   //trace_pte_info()
	TP_PROTO(struct task_struct *cur_task,
		struct vm_area_struct *vma,
		unsigned long faultaddr,
		int is_major,
		pte_t old_pte,
		pte_t new_pte,
		const char *label),
	TP_ARGS(cur_task, vma, faultaddr, is_major, old_pte, new_pte, label)
);

//DECLARE_EVENT_CLASS(pmd_event, ...);

/* "Standalone" trace event: */
TRACE_EVENT(pte_printk,   //trace_pte_printk("");
	TP_PROTO(const char *msg),
	TP_ARGS(msg),
	TP_STRUCT__entry(
		__array(char, msg, PTETRACE_BUF_LEN)
	),
	TP_fast_assign(
		strncpy(__entry->msg, msg, PTETRACE_BUF_LEN-1);
		__entry->msg[PTETRACE_BUF_LEN-1] = '\0';  //defensive
	),
	TP_printk("%s", __entry->msg)
);

#if 0
/* New special event for dup_pte function: */
TRACE_EVENT(pte_vma_alloc_dup_pte,
	TP_PROTO(struct vm_area_struct *vma,
		pid_t trace_pid,
		pid_t trace_tgid,
		struct task_struct *trace_real_parent),
	TP_ARGS(vma, trace_pid, trace_tgid, trace_real_parent),
	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__field(pid_t, ptgid)
		__field(struct vm_area_struct *, vma)
		__field(unsigned long, vm_start)
		__field(unsigned long, vm_end)
		__field(unsigned long, vm_flags)
		__field(unsigned long long, vm_pgoff)
		__field(unsigned int, dev_major)
		__field(unsigned int, dev_minor)
		__field(unsigned long, inode)
		__array(char, filename, PTETRACE_BUF_LEN)
	),
	TP_fast_assign(
		__entry->pid   = trace_pid;
		__entry->tgid  = trace_tgid;
		/* Grab parent's tgid: this extra pointer dereference is kind of
		 * a bummer, but hopefully not much performance impact.
		 */
		__entry->ptgid = trace_real_parent ?
		                 trace_real_parent->tgid : -1;
		__entry->vma = vma;
		__entry->vm_start =
			stack_guard_page_start(vma, vma->vm_start) ?
				vma->vm_start + PAGE_SIZE :
				vma->vm_start;
		__entry->vm_end =
			stack_guard_page_end(vma, vma->vm_end) ?
				vma->vm_end - PAGE_SIZE :
				vma->vm_end;
		__entry->vm_flags = vma->vm_flags;
		__entry->vm_pgoff =
			vma->vm_file ? ((loff_t)(vma->vm_pgoff)) << PAGE_SHIFT : 0;
		__entry->dev_major =
			vma->vm_file ? MAJOR(file_inode(vma->vm_file)->i_sb->s_dev) : 0;
		__entry->dev_minor = 
			vma->vm_file ? MINOR(file_inode(vma->vm_file)->i_sb->s_dev) : 0;
		__entry->inode = 
			vma->vm_file ? file_inode(vma->vm_file)->i_ino : 0;
		if (vma->vm_file) {
			char *path = d_path(&(vma->vm_file->f_path),
				(char *)(__entry->filename), PTETRACE_BUF_LEN);
			strncpy(__entry->filename, path, PTETRACE_BUF_LEN-1);
			  // I hope the kernel strncpy works with overlapping strings!
			  //   Seems to work just fine...
		} else {
			__entry->filename[0] = '\0';
		}
		__entry->filename[PTETRACE_BUF_LEN-1] = '\0';  //defensive
		//TODO: add code for special cases of filename: [heap], [vdso],
		//  [stack], [vsyscall]
	),

	/* Note: "[dup_page]" must be printed exactly like this - must exactly
	 * match format of other page_vma_alloc events, and kernel fn must
	 * be dup_page in order for analysis scripts to work.
	 */
	TP_printk("pid=%d tgid=%d ptgid=%d [dup_page]: %p @ %08lx-%08lx %c%c%c%c %08llx %02x:%02x %lu %s",
		__entry->pid,
		__entry->tgid,
		__entry->ptgid,
		__entry->vma,
		__entry->vm_start,
		__entry->vm_end,
		__entry->vm_flags & VM_READ ? 'r' : '-',
		__entry->vm_flags & VM_WRITE ? 'w' : '-',
		__entry->vm_flags & VM_EXEC ? 'x' : '-',
		__entry->vm_flags & VM_MAYSHARE ? 's' : 'p',
		__entry->vm_pgoff,
		__entry->dev_major,
		__entry->dev_minor,
		__entry->inode,
		__entry->filename
		)
);
#endif

#if 0
/* Define a class of trace events, "pte_sim_event", that all take the
 * same args and have the same output. Unlike the pte_event class of
 * events, these events don't take a specific vma as an argument.
 *
 * Note that these events retrieve the current task's parent tgid using
 * the real_parent pointer kept in the task_struct - this pointer should
 * now be set just before a dup_page occurs, and as far as I know that's
 * typically / always the first event that a particular task will emit,
 * so when calling these "sim" events e.g. in shift_arg_pages and
 * load_elf_binary, we're always after the dup_page and the parent tgid
 * should always be retrievable and correct.
 */
DECLARE_EVENT_CLASS(page_sim_event,
	TP_PROTO(struct task_struct *cur_task, const char *descr),
	TP_ARGS(cur_task, descr),
	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__field(pid_t, ptgid)
		__array(char, descr, PTETRACE_BUF_LEN)
	),
	TP_fast_assign(
		__entry->pid   = cur_task->pid;
		__entry->tgid  = cur_task->tgid;
		__entry->ptgid = cur_task->real_parent ?
		                 cur_task->real_parent->tgid : -1;
		strncpy(__entry->descr, descr, PTETRACE_BUF_LEN-1);
		__entry->descr[PTETRACE_BUF_LEN-1] = '\0';  //defensive
	),
	TP_printk("pid=%d tgid=%d ptgid=%d %s",
		__entry->pid, __entry->tgid, __entry->ptgid, __entry->descr)
);

DEFINE_EVENT(page_sim_event, page_disable_sim,   //trace_page_disable_sim()
	TP_PROTO(struct task_struct *cur_task, const char *descr),
	TP_ARGS(cur_task, descr)
);

DEFINE_EVENT(page_sim_event, page_enable_sim,   //trace_page_enable_sim()
	TP_PROTO(struct task_struct *cur_task, const char *descr),
	TP_ARGS(cur_task, descr)
);

DEFINE_EVENT(page_sim_event, page_reset_sim,   //trace_page_reset_sim()
	TP_PROTO(struct task_struct *cur_task, const char *descr),
	TP_ARGS(cur_task, descr)
);
#endif

#endif /* _TRACE_PTE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
