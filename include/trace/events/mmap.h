/* (c) 2013, University of Washington, Department of Computer
 * Science & Engineering, Peter Hornyack.
 *
 * mmap / vma event tracing.
 * Written in 2013 by Peter Hornyack, pjh@cs.washington.edu
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmap

#if !defined(_TRACE_MMAP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMAP_H

#include <linux/tracepoint.h>

/* For information about how to fill in a TRACE_EVENT, see
 * trace-events-sample.h
 */

#define PJH_BUF_LEN 256

/* DESCRIPTION OF TRACE EVENTS:
 * vma allocate / free:
 *   trace_mmap_vma_alloc
 *   trace_mmap_vma_free
 * vma resize:
 *   trace_mmap_vma_resize_unmap
 *   trace_mmap_vma_resize_remap
 * vma relocate:
 *   trace_mmap_vma_reloc_unmap
 *   trace_mmap_vma_reloc_remap
 * vma access change (read / write / execute / private / shared):
 *   trace_mmap_vma_access_unmap
 *   trace_mmap_vma_access_remap
 * vma flags change (OS-level flags that I probably don’t care about):
 *   trace_mmap_vma_flags_unmap
 *   trace_mmap_vma_flags_remap
 */

/* Define a class of trace events, "mmap_vma", that all take the same args
 * and have the same output. See include/trace/events/kmem.h for an example.
 */
DECLARE_EVENT_CLASS(mmap_vma,
	TP_PROTO(struct task_struct *cur_task,
		struct vm_area_struct *vma,
		const char *label),
	TP_ARGS(cur_task, vma, label),
	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__field(pid_t, ptgid)
		__array(char, label, PJH_BUF_LEN)
		__field(struct vm_area_struct *, vma)
		__field(unsigned long, vm_start)
		__field(unsigned long, vm_end)
		__field(unsigned long, vm_flags)
		__field(unsigned long long, vm_pgoff)
		__field(unsigned int, dev_major)
		__field(unsigned int, dev_minor)
		__field(unsigned long, inode)
		__array(char, filename, PJH_BUF_LEN)
	),
	TP_fast_assign(
		__entry->pid   = cur_task->pid;
		__entry->tgid  = cur_task->tgid;
		/* Grab parent's tgid: this extra pointer dereference is kind of
		 * a bummer, but hopefully not much performance impact.
		 */
		__entry->ptgid = cur_task->real_parent ?
		                 cur_task->real_parent->tgid : -1;
		strncpy(__entry->label, label, PJH_BUF_LEN-1);
		__entry->label[PJH_BUF_LEN-1] = '\0';  //defensive
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
				(char *)(__entry->filename), PJH_BUF_LEN);
			strncpy(__entry->filename, path, PJH_BUF_LEN-1);
			  // I hope the kernel strncpy works with overlapping strings!
			  //   Seems to work just fine...
		} else {
			__entry->filename[0] = '\0';
		}
		__entry->filename[PJH_BUF_LEN-1] = '\0';  //defensive
		//TODO: add code for special cases of filename: [heap], [vdso],
		//  [stack], [vsyscall]
	),

	/* See definition of vm_area_struct in include/linux/mm_types.h.
	 * Imitate printing of a vma entry in fs/proc/task_mmu.c:show_map_vma().
	 *   00400000-0040c000 r-xp 00000000 fd:01 41038  /bin/cat
	 */
	TP_printk("pid=%d tgid=%d ptgid=%d [%s]: %p @ %08lx-%08lx %c%c%c%c %08llx %02x:%02x %lu %s",
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
		__entry->vm_pgoff,
		__entry->dev_major,
		__entry->dev_minor,
		__entry->inode,
		__entry->filename
		)
);

/* Define a trace event function "trace_mmap_vma_alloc()", which has
 * arguments and output like the mmap_vma class defined above.
 * Not sure why I still have to define TP_PROTO and TP_ARGS...
 */
DEFINE_EVENT(mmap_vma, mmap_vma_alloc,
	TP_PROTO(struct task_struct *cur_task, struct vm_area_struct *vma,
		const char *label),
	TP_ARGS(cur_task, vma, label)
);

DEFINE_EVENT(mmap_vma, mmap_vma_free,
	TP_PROTO(struct task_struct *cur_task, struct vm_area_struct *vma,
		const char *label),
	TP_ARGS(cur_task, vma, label)
);

DEFINE_EVENT(mmap_vma, mmap_vma_resize_unmap,
	TP_PROTO(struct task_struct *cur_task, struct vm_area_struct *vma,
		const char *label),
	TP_ARGS(cur_task, vma, label)
);

DEFINE_EVENT(mmap_vma, mmap_vma_resize_remap,
	TP_PROTO(struct task_struct *cur_task, struct vm_area_struct *vma,
		const char *label),
	TP_ARGS(cur_task, vma, label)
);

DEFINE_EVENT(mmap_vma, mmap_vma_reloc_unmap,
	TP_PROTO(struct task_struct *cur_task, struct vm_area_struct *vma,
		const char *label),
	TP_ARGS(cur_task, vma, label)
);

DEFINE_EVENT(mmap_vma, mmap_vma_reloc_remap,
	TP_PROTO(struct task_struct *cur_task, struct vm_area_struct *vma,
		const char *label),
	TP_ARGS(cur_task, vma, label)
);

DEFINE_EVENT(mmap_vma, mmap_vma_access_unmap,
	TP_PROTO(struct task_struct *cur_task, struct vm_area_struct *vma,
		const char *label),
	TP_ARGS(cur_task, vma, label)
);

DEFINE_EVENT(mmap_vma, mmap_vma_access_remap,
	TP_PROTO(struct task_struct *cur_task, struct vm_area_struct *vma,
		const char *label),
	TP_ARGS(cur_task, vma, label)
);

DEFINE_EVENT(mmap_vma, mmap_vma_flags_unmap,
	TP_PROTO(struct task_struct *cur_task, struct vm_area_struct *vma,
		const char *label),
	TP_ARGS(cur_task, vma, label)
);

DEFINE_EVENT(mmap_vma, mmap_vma_flags_remap,
	TP_PROTO(struct task_struct *cur_task, struct vm_area_struct *vma,
		const char *label),
	TP_ARGS(cur_task, vma, label)
);

/* New special event for dup_mmap function:
 */
TRACE_EVENT(mmap_vma_alloc_dup_mmap,
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
		__array(char, filename, PJH_BUF_LEN)
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
				(char *)(__entry->filename), PJH_BUF_LEN);
			strncpy(__entry->filename, path, PJH_BUF_LEN-1);
			  // I hope the kernel strncpy works with overlapping strings!
			  //   Seems to work just fine...
		} else {
			__entry->filename[0] = '\0';
		}
		__entry->filename[PJH_BUF_LEN-1] = '\0';  //defensive
		//TODO: add code for special cases of filename: [heap], [vdso],
		//  [stack], [vsyscall]
	),

	/* See definition of vm_area_struct in include/linux/mm_types.h.
	 * Imitate printing of a vma entry in fs/proc/task_mmu.c:show_map_vma().
	 *   00400000-0040c000 r-xp 00000000 fd:01 41038  /bin/cat
	 */
	TP_printk("pid=%d tgid=%d ptgid=%d [dup_mmap]: %p @ %08lx-%08lx %c%c%c%c %08llx %02x:%02x %lu %s",
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

/* Define a class of trace events, "mmap_sim_event", that all take the
 * same args and have the same output. Unlike the mmap_vma class of
 * events, these events don't take a specific vma as an argument.
 *
 * Note that these events retrieve the current task's parent tgid using
 * the real_parent pointer kept in the task_struct - this pointer should
 * now be set just before a dup_mmap occurs, and as far as I know that's
 * typically / always the first event that a particular task will emit,
 * so when calling these "sim" events e.g. in shift_arg_pages and
 * load_elf_binary, we're always after the dup_mmap and the parent tgid
 * should always be retrievable and correct.
 */
DECLARE_EVENT_CLASS(mmap_sim_event,
	TP_PROTO(struct task_struct *cur_task, const char *descr),
	TP_ARGS(cur_task, descr),
	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__field(pid_t, ptgid)
		__array(char, descr, PJH_BUF_LEN)
	),
	TP_fast_assign(
		__entry->pid   = cur_task->pid;
		__entry->tgid  = cur_task->tgid;
		__entry->ptgid = cur_task->real_parent ?
		                 cur_task->real_parent->tgid : -1;
		strncpy(__entry->descr, descr, PJH_BUF_LEN-1);
		__entry->descr[PJH_BUF_LEN-1] = '\0';  //defensive
	),
	TP_printk("pid=%d tgid=%d ptgid=%d %s",
		__entry->pid, __entry->tgid, __entry->ptgid, __entry->descr)
);

DEFINE_EVENT(mmap_sim_event, mmap_disable_sim,   //trace_mmap_disable_sim()
	TP_PROTO(struct task_struct *cur_task, const char *descr),
	TP_ARGS(cur_task, descr)
);

DEFINE_EVENT(mmap_sim_event, mmap_enable_sim,   //trace_mmap_enable_sim()
	TP_PROTO(struct task_struct *cur_task, const char *descr),
	TP_ARGS(cur_task, descr)
);

DEFINE_EVENT(mmap_sim_event, mmap_reset_sim,   //trace_mmap_reset_sim()
	TP_PROTO(struct task_struct *cur_task, const char *descr),
	TP_ARGS(cur_task, descr)
);

/* "Standalone" trace event: */
TRACE_EVENT(mmap_printk,   //trace_mmap_printk("");
	TP_PROTO(const char *msg),
	TP_ARGS(msg),
	TP_STRUCT__entry(
		__array(char, msg, PJH_BUF_LEN)
	),
	TP_fast_assign(
		strncpy(__entry->msg, msg, PJH_BUF_LEN-1);
		__entry->msg[PJH_BUF_LEN-1] = '\0';  //defensive
	),
	TP_printk("%s", __entry->msg)
);

#endif /* _TRACE_MMAP_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
