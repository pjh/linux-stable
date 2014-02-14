/* (c) 2013-2014, University of Washington, Department of Computer
 * Science & Engineering, Peter Hornyack.
 *
 * PTE event tracing.
 * Written in 2013-2014 by Peter Hornyack, pjh@cs.washington.edu
 */

#include <asm/pgtable.h>
#include <asm/pgtable_types.h>
#include <linux/mm.h>

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
 * call mk_pte(page, -1) or mk_pte(page, vma->vm_page_prot)...
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
			"old_pte_pfn=%lu, old_pte_flags=%08lX, "
			"new_pte_pfn=%lu, new_pte_flags=%08lX",
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
 *   pte_update(): a page mapping was not changed or established, but some
 *     part of the PTE changed (e.g. COW was performed). is_major is
 *     probably not meaningful.
 *   pte_cow(): a write to a write-protected page occurred, so a new
 *     page was allocated and the existing page was copied to it. Note
 *     that the new_pte is probably not completely valid yet; however,
 *     this trace event should always (as far as I can tell) be followed
 *     soon by a corresponding pte_mapped() trace event, which will
 *     have the new_pte set correctly. Also, is_major is not meaningful
 *     for this event.
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

DEFINE_EVENT(pte_event, pte_update,   //trace_pte_update()
	TP_PROTO(struct task_struct *cur_task,
		struct vm_area_struct *vma,
		unsigned long faultaddr,
		int is_major,
		pte_t old_pte,
		pte_t new_pte,
		const char *label),
	TP_ARGS(cur_task, vma, faultaddr, is_major, old_pte, new_pte, label)
);

DEFINE_EVENT(pte_event, pte_cow,   //trace_pte_cow()
	TP_PROTO(struct task_struct *cur_task,
		struct vm_area_struct *vma,
		unsigned long faultaddr,
		int is_major,    // ignored for COW
		pte_t old_pte,
		pte_t new_pte,   // not always set...
		const char *label),
	TP_ARGS(cur_task, vma, faultaddr, is_major, old_pte, new_pte, label)
);

//DECLARE_EVENT_CLASS(pmd_event, ...);

/* "Standalone" trace events:
 *   trace_pte_printk(): takes an integer "code", just set to 0 if
 *     not needed.
 *   trace_pte_fault(): specifically for *protection faults* and kernel
 *     faults, where we don't have ptes readily available.
 *     Useful in some other cases as well: unexpected ...
 *   trace_pte_at(): calls to "set_pte_at()" and the like that I haven't
 *     yet converted to proper trace events, but should if they are
 *     emitted commonly.
 *   trace_pmd_at(): same, but for pmd instead of pte.
 */
TRACE_EVENT(pte_printk,   // trace_pte_printk("", 0);
	TP_PROTO(const char *msg, int code),
	TP_ARGS(msg, code),
	TP_STRUCT__entry(
		__array(char, msg, PTETRACE_BUF_LEN)
		__field(int, code)
	),
	TP_fast_assign(
		strncpy(__entry->msg, msg, PTETRACE_BUF_LEN-1);
		__entry->msg[PTETRACE_BUF_LEN-1] = '\0';  //defensive
		__entry->code = code;
	),
	TP_printk("[%s] code=%X", __entry->msg, __entry->code)
);

TRACE_EVENT(pte_fault,   //trace_pte_fault();
	TP_PROTO(const char *label, const char *msg,
		unsigned long faultaddr, int code),
	TP_ARGS(label, msg, faultaddr, code),
	TP_STRUCT__entry(
		__array(char, label, PTETRACE_BUF_LEN)
		__array(char, msg, PTETRACE_BUF_LEN)
		__field(unsigned long, faultaddr)
		__field(int, code)
	),
	TP_fast_assign(
		strncpy(__entry->label, label, PTETRACE_BUF_LEN-1);
		__entry->label[PTETRACE_BUF_LEN-1] = '\0';  //defensive
		strncpy(__entry->msg, msg, PTETRACE_BUF_LEN-1);
		__entry->msg[PTETRACE_BUF_LEN-1] = '\0';  //defensive
		__entry->faultaddr = faultaddr;
		__entry->code = code;
	),
	TP_printk("[%s] [%s] faultaddr=%p, code=%X",
		__entry->label, __entry->msg, (void *)__entry->faultaddr,
		__entry->code)
);

TRACE_EVENT(pte_at,   //trace_pte_at();
	TP_PROTO(const char *label, const char *msg,
		unsigned long addr, pte_t pte),
	TP_ARGS(label, msg, addr, pte),
	TP_STRUCT__entry(
		__array(char, label, PTETRACE_BUF_LEN)
		__array(char, msg, PTETRACE_BUF_LEN)
		__field(unsigned long, addr)
		__field(pteval_t, pteval)
	),
	TP_fast_assign(
		strncpy(__entry->label, label, PTETRACE_BUF_LEN-1);
		__entry->label[PTETRACE_BUF_LEN-1] = '\0';  //defensive
		strncpy(__entry->msg, msg, PTETRACE_BUF_LEN-1);
		__entry->msg[PTETRACE_BUF_LEN-1] = '\0';  //defensive
		__entry->addr = addr;
		__entry->pteval = pte_val(pte);
	),
	TP_printk("[%s] [%s] addr=%p, pte_pfn=%lu, pte_flags=%08lX",
		__entry->label, __entry->msg, (void *)__entry->addr,
		pteval_pfn(__entry->pteval),
		pteval_flags(__entry->pteval))
);

TRACE_EVENT(pmd_at,   //trace_pmd_at();
	TP_PROTO(const char *label, const char *msg,
		unsigned long addr, pmd_t pmd),
	TP_ARGS(label, msg, addr, pmd),
	TP_STRUCT__entry(
		__array(char, label, PTETRACE_BUF_LEN)
		__array(char, msg, PTETRACE_BUF_LEN)
		__field(unsigned long, addr)
		__field(pmdval_t, pmdval)
	),
	TP_fast_assign(
		strncpy(__entry->label, label, PTETRACE_BUF_LEN-1);
		__entry->label[PTETRACE_BUF_LEN-1] = '\0';  //defensive
		strncpy(__entry->msg, msg, PTETRACE_BUF_LEN-1);
		__entry->msg[PTETRACE_BUF_LEN-1] = '\0';  //defensive
		__entry->addr = addr;
		__entry->pmdval = pmd_val(pmd);
	),
	TP_printk("[%s] [%s] addr=%p, pmd_pfn=%lu, pmd_flags=%08lX",
		__entry->label, __entry->msg, (void *)__entry->addr,
		pmdval_pfn(__entry->pmdval),
		pmdval_flags(__entry->pmdval))
);

#endif /* _TRACE_PTE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
