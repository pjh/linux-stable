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
//#include <linux/types.h>
  // needed?
//#include <trace/events/gfpflags.h>
  // needed?

/* For information about how to fill in a TRACE_EVENT, see
 * trace-events-sample.h
 */

//#define PJH_FANCY
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
		__entry->pid = cur_task->pid;
		__entry->tgid = cur_task->tgid;
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
#ifdef PJH_FANCY
			// Imitate: show_map_vma() calls seq_path() calls d_path()
			char *p = d_path(vma->vm_file->f_path,
				(char *)__entry->filename, PJH_BUF_LEN)
			while (p > (char *)__entry->filename) {
				/* d_path() may return a pointer that is some distance into
				 * the filename buffer, with the initial characters as 0
				 * bytes. So, just replace them with spaces:
				 */
				p--;
				*p = ' ';
			}
#else
			strncpy(__entry->filename, "/some/path/somefile", PJH_BUF_LEN-1);
#endif
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
	TP_printk("pid=%d tgid=%d [%s]: %p @ %08lx-%08lx %c%c%c%c %08llx %02x:%02x %lu %s",
		__entry->pid,
		__entry->tgid,
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

#if 0
TRACE_EVENT(mmap_vma,  // creates a function "trace_mmap_vma"

	TP_PROTO(struct task_struct *cur_task, struct vm_area_struct *vma,
		const char *label),

	TP_ARGS(cur_task, vma, label),  // names of args in TP_PROTO

	/* Rather than just copying the vm_area_struct *vma pointer, we must
	 * copy all of the field values that we'll need into the ring buffer
	 * struct - if not, then when we try to get these values later on
	 * (when printing the trace output), the vma may be already deallocated
	 * and we'll get a NULL pointer reference + kernel oops (note that the
	 * kmem.h events file doesn't dereference any pointers in its printk).
	 */
	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__array(char, label, PJH_BUF_LEN)
		__field(struct vm_area_struct *, vma)
		__field(unsigned long, vm_start)
		__field(unsigned long, vm_end)
		//__field(pgprot_t, vm_page_prot)
		__field(unsigned long, vm_flags)
		__field(unsigned long long, vm_pgoff)
		__field(unsigned int, dev_major)
		__field(unsigned int, dev_minor)
		__field(unsigned long, inode)
		__array(char, filename, PJH_BUF_LEN)
	),

	TP_fast_assign(
		__entry->pid = cur_task->pid;
		__entry->tgid = cur_task->tgid;
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
		//__entry->vm_page_prot = vma->vm_page_prot;
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
#ifdef PJH_FANCY
			// Imitate: show_map_vma() calls seq_path() calls d_path()
			char *p = d_path(vma->vm_file->f_path,
				(char *)__entry->filename, PJH_BUF_LEN)
			while (p > (char *)__entry->filename) {
				/* d_path() may return a pointer that is some distance into
				 * the filename buffer, with the initial characters as 0
				 * bytes. So, just replace them with spaces:
				 */
				p--;
				*p = ' ';
			}
#else
			strncpy(__entry->filename, "/some/path/somefile", PJH_BUF_LEN-1);
#endif
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
	TP_printk("pid=%d tgid=%d [%s]: %p @ %08lx-%08lx %c%c%c%c %08llx %02x:%02x %lu %s",
		__entry->pid,
		__entry->tgid,
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

TRACE_EVENT(munmap_vma,  // creates a function "trace_munmap_vma"

	TP_PROTO(struct task_struct *cur_task, struct vm_area_struct *vma,
		const char *label),

	TP_ARGS(cur_task, vma, label),  // names of args in TP_PROTO

	/* Rather than just copying the vm_area_struct *vma pointer, we must
	 * copy all of the field values that we'll need into the ring buffer
	 * struct - if not, then when we try to get these values later on
	 * (when printing the trace output), the vma may be already deallocated
	 * and we'll get a NULL pointer reference + kernel oops (note that the
	 * kmem.h events file doesn't dereference any pointers in its printk).
	 */
	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(pid_t, tgid)
		__array(char, label, PJH_BUF_LEN)
		__field(struct vm_area_struct *, vma)
		__field(unsigned long, vm_start)
		__field(unsigned long, vm_end)
		//__field(pgprot_t, vm_page_prot)
		__field(unsigned long, vm_flags)
		__field(unsigned long long, vm_pgoff)
		__field(unsigned int, dev_major)
		__field(unsigned int, dev_minor)
		__field(unsigned long, inode)
		__array(char, filename, PJH_BUF_LEN)
	),

	TP_fast_assign(
		__entry->pid = cur_task->pid;
		__entry->tgid = cur_task->tgid;
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
		//__entry->vm_page_prot = vma->vm_page_prot;
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
#ifdef PJH_FANCY
			// Imitate: show_map_vma() calls seq_path() calls d_path()
			char *p = d_path(vma->vm_file->f_path,
				(char *)__entry->filename, PJH_BUF_LEN)
			while (p > (char *)__entry->filename) {
				/* d_path() may return a pointer that is some distance into
				 * the filename buffer, with the initial characters as 0
				 * bytes. So, just replace them with spaces:
				 */
				p--;
				*p = ' ';
			}
#else
			strncpy(__entry->filename, "/some/path/somefile", PJH_BUF_LEN-1);
#endif
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
	TP_printk("pid=%d tgid=%d [%s]: %p @ %08lx-%08lx %c%c%c%c %08llx %02x:%02x %lu %s",
		__entry->pid,
		__entry->tgid,
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
#endif

TRACE_EVENT(mmap_disable_sim,   //trace_mmap_disable_sim("");
	TP_PROTO(const char *descr),
	TP_ARGS(descr),
	TP_STRUCT__entry(
		__array(char, descr, PJH_BUF_LEN)
	),
	TP_fast_assign(
		strncpy(__entry->descr, descr, PJH_BUF_LEN-1);
		__entry->descr[PJH_BUF_LEN-1] = '\0';  //defensive
	),
	TP_printk("%s", __entry->descr)
);

TRACE_EVENT(mmap_enable_sim,   //trace_mmap_enable_sim("");
	TP_PROTO(const char *descr),
	TP_ARGS(descr),
	TP_STRUCT__entry(
		__array(char, descr, PJH_BUF_LEN)
	),
	TP_fast_assign(
		strncpy(__entry->descr, descr, PJH_BUF_LEN-1);
		__entry->descr[PJH_BUF_LEN-1] = '\0';  //defensive
	),
	TP_printk("%s", __entry->descr)
);

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
