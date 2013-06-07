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

TRACE_EVENT(mmap_vma,  // creates a function "trace_mmap_vma"

	TP_PROTO(struct vm_area_struct *vma),

	TP_ARGS(vma),  // names of args in TP_PROTO

	TP_STRUCT__entry(
		__field(struct vm_area_struct *, vma)
		__array(char, filename, PJH_BUF_LEN)
	),

	TP_fast_assign(
		__entry->vma		= vma;
#ifdef PJH_FANCY
		if (vma->vm_file) {
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
		} else {
			__entry->filename[0] = '\0';
		}
		__entry->filename[PJH_BUF_LEN-1] = '\0';  //defensive
#else
		strncpy(__entry->filename, "/some/path/somefile", PJH_BUF_LEN-1);
		__entry->filename[PJH_BUF_LEN-1] = '\0';  //defensive
#endif
	),

	/* See definition of vm_area_struct in include/linux/mm_types.h.
	 * Imitate printing of a vma entry in fs/proc/task_mmu.c:show_map_vma().
	 *   00400000-0040c000 r-xp 00000000 fd:01 41038  /bin/cat
	 */
	TP_printk("%08lx-%08lx %c%c%c%c %08llx %02x:%02x %lu %s",
		stack_guard_page_start(__entry->vma, __entry->vma->vm_start) ?
			__entry->vma->vm_start + PAGE_SIZE :
			__entry->vma->vm_start,
		stack_guard_page_end(__entry->vma, __entry->vma->vm_end) ?
			__entry->vma->vm_end - PAGE_SIZE :
			__entry->vma->vm_end,
		__entry->vma->vm_flags & VM_READ ? 'r' : '-',
		__entry->vma->vm_flags & VM_WRITE ? 'w' : '-',
		__entry->vma->vm_flags & VM_EXEC ? 'x' : '-',
		__entry->vma->vm_flags & VM_MAYSHARE ? 's' : 'p',
		__entry->vma->vm_file ?
			((loff_t)(__entry->vma->vm_pgoff)) << PAGE_SHIFT :
			0,
		__entry->vma->vm_file ?
			MAJOR(file_inode(__entry->vma->vm_file)->i_sb->s_dev) :
			MAJOR(0),
		__entry->vma->vm_file ?
			MINOR(file_inode(__entry->vma->vm_file)->i_sb->s_dev) :
			MINOR(0),
		__entry->vma->vm_file ?
			file_inode(__entry->vma->vm_file)->i_ino :
			0,
		__entry->vma->vm_file ?
			__entry->filename :
			""
		)
		//TODO: add code to print special cases: [heap], [vdso], [stack],
		//  [vsyscall]
);

#if 0
DECLARE_EVENT_CLASS(mmap_alloc,

	TP_PROTO(unsigned long call_site,
		 const void *ptr,
		 size_t bytes_req,
		 size_t bytes_alloc,
		 gfp_t gfp_flags),

	TP_ARGS(call_site, ptr, bytes_req, bytes_alloc, gfp_flags),

	TP_STRUCT__entry(
		__field(	unsigned long,	call_site	)
		__field(	const void *,	ptr		)
		__field(	size_t,		bytes_req	)
		__field(	size_t,		bytes_alloc	)
		__field(	gfp_t,		gfp_flags	)
	),

	TP_fast_assign(
		__entry->call_site	= call_site;
		__entry->ptr		= ptr;
		__entry->bytes_req	= bytes_req;
		__entry->bytes_alloc	= bytes_alloc;
		__entry->gfp_flags	= gfp_flags;
	),

	TP_printk("call_site=%lx ptr=%p bytes_req=%zu bytes_alloc=%zu gfp_flags=%s",
		__entry->call_site,
		__entry->ptr,
		__entry->bytes_req,
		__entry->bytes_alloc,
		show_gfp_flags(__entry->gfp_flags))
);

DEFINE_EVENT(mmap_alloc, kmalloc,

	TP_PROTO(unsigned long call_site, const void *ptr,
		 size_t bytes_req, size_t bytes_alloc, gfp_t gfp_flags),

	TP_ARGS(call_site, ptr, bytes_req, bytes_alloc, gfp_flags)
);

DEFINE_EVENT(mmap_alloc, mmap_cache_alloc,

	TP_PROTO(unsigned long call_site, const void *ptr,
		 size_t bytes_req, size_t bytes_alloc, gfp_t gfp_flags),

	TP_ARGS(call_site, ptr, bytes_req, bytes_alloc, gfp_flags)
);

DECLARE_EVENT_CLASS(mmap_alloc_node,

	TP_PROTO(unsigned long call_site,
		 const void *ptr,
		 size_t bytes_req,
		 size_t bytes_alloc,
		 gfp_t gfp_flags,
		 int node),

	TP_ARGS(call_site, ptr, bytes_req, bytes_alloc, gfp_flags, node),

	TP_STRUCT__entry(
		__field(	unsigned long,	call_site	)
		__field(	const void *,	ptr		)
		__field(	size_t,		bytes_req	)
		__field(	size_t,		bytes_alloc	)
		__field(	gfp_t,		gfp_flags	)
		__field(	int,		node		)
	),

	TP_fast_assign(
		__entry->call_site	= call_site;
		__entry->ptr		= ptr;
		__entry->bytes_req	= bytes_req;
		__entry->bytes_alloc	= bytes_alloc;
		__entry->gfp_flags	= gfp_flags;
		__entry->node		= node;
	),

	TP_printk("call_site=%lx ptr=%p bytes_req=%zu bytes_alloc=%zu gfp_flags=%s node=%d",
		__entry->call_site,
		__entry->ptr,
		__entry->bytes_req,
		__entry->bytes_alloc,
		show_gfp_flags(__entry->gfp_flags),
		__entry->node)
);

DEFINE_EVENT(mmap_alloc_node, kmalloc_node,

	TP_PROTO(unsigned long call_site, const void *ptr,
		 size_t bytes_req, size_t bytes_alloc,
		 gfp_t gfp_flags, int node),

	TP_ARGS(call_site, ptr, bytes_req, bytes_alloc, gfp_flags, node)
);

DEFINE_EVENT(mmap_alloc_node, mmap_cache_alloc_node,

	TP_PROTO(unsigned long call_site, const void *ptr,
		 size_t bytes_req, size_t bytes_alloc,
		 gfp_t gfp_flags, int node),

	TP_ARGS(call_site, ptr, bytes_req, bytes_alloc, gfp_flags, node)
);

DECLARE_EVENT_CLASS(mmap_free,

	TP_PROTO(unsigned long call_site, const void *ptr),

	TP_ARGS(call_site, ptr),

	TP_STRUCT__entry(
		__field(	unsigned long,	call_site	)
		__field(	const void *,	ptr		)
	),

	TP_fast_assign(
		__entry->call_site	= call_site;
		__entry->ptr		= ptr;
	),

	TP_printk("call_site=%lx ptr=%p", __entry->call_site, __entry->ptr)
);

DEFINE_EVENT(mmap_free, kfree,

	TP_PROTO(unsigned long call_site, const void *ptr),

	TP_ARGS(call_site, ptr)
);

DEFINE_EVENT(mmap_free, mmap_cache_free,

	TP_PROTO(unsigned long call_site, const void *ptr),

	TP_ARGS(call_site, ptr)
);

TRACE_EVENT(mm_page_free,

	TP_PROTO(struct page *page, unsigned int order),

	TP_ARGS(page, order),

	TP_STRUCT__entry(
		__field(	struct page *,	page		)
		__field(	unsigned int,	order		)
	),

	TP_fast_assign(
		__entry->page		= page;
		__entry->order		= order;
	),

	TP_printk("page=%p pfn=%lu order=%d",
			__entry->page,
			page_to_pfn(__entry->page),
			__entry->order)
);

TRACE_EVENT(mm_page_free_batched,

	TP_PROTO(struct page *page, int cold),

	TP_ARGS(page, cold),

	TP_STRUCT__entry(
		__field(	struct page *,	page		)
		__field(	int,		cold		)
	),

	TP_fast_assign(
		__entry->page		= page;
		__entry->cold		= cold;
	),

	TP_printk("page=%p pfn=%lu order=0 cold=%d",
			__entry->page,
			page_to_pfn(__entry->page),
			__entry->cold)
);

TRACE_EVENT(mm_page_alloc,

	TP_PROTO(struct page *page, unsigned int order,
			gfp_t gfp_flags, int migratetype),

	TP_ARGS(page, order, gfp_flags, migratetype),

	TP_STRUCT__entry(
		__field(	struct page *,	page		)
		__field(	unsigned int,	order		)
		__field(	gfp_t,		gfp_flags	)
		__field(	int,		migratetype	)
	),

	TP_fast_assign(
		__entry->page		= page;
		__entry->order		= order;
		__entry->gfp_flags	= gfp_flags;
		__entry->migratetype	= migratetype;
	),

	TP_printk("page=%p pfn=%lu order=%d migratetype=%d gfp_flags=%s",
		__entry->page,
		__entry->page ? page_to_pfn(__entry->page) : 0,
		__entry->order,
		__entry->migratetype,
		show_gfp_flags(__entry->gfp_flags))
);

DECLARE_EVENT_CLASS(mm_page,

	TP_PROTO(struct page *page, unsigned int order, int migratetype),

	TP_ARGS(page, order, migratetype),

	TP_STRUCT__entry(
		__field(	struct page *,	page		)
		__field(	unsigned int,	order		)
		__field(	int,		migratetype	)
	),

	TP_fast_assign(
		__entry->page		= page;
		__entry->order		= order;
		__entry->migratetype	= migratetype;
	),

	TP_printk("page=%p pfn=%lu order=%u migratetype=%d percpu_refill=%d",
		__entry->page,
		__entry->page ? page_to_pfn(__entry->page) : 0,
		__entry->order,
		__entry->migratetype,
		__entry->order == 0)
);

DEFINE_EVENT(mm_page, mm_page_alloc_zone_locked,

	TP_PROTO(struct page *page, unsigned int order, int migratetype),

	TP_ARGS(page, order, migratetype)
);

DEFINE_EVENT_PRINT(mm_page, mm_page_pcpu_drain,

	TP_PROTO(struct page *page, unsigned int order, int migratetype),

	TP_ARGS(page, order, migratetype),

	TP_printk("page=%p pfn=%lu order=%d migratetype=%d",
		__entry->page, page_to_pfn(__entry->page),
		__entry->order, __entry->migratetype)
);

TRACE_EVENT(mm_page_alloc_extfrag,

	TP_PROTO(struct page *page,
			int alloc_order, int fallback_order,
			int alloc_migratetype, int fallback_migratetype),

	TP_ARGS(page,
		alloc_order, fallback_order,
		alloc_migratetype, fallback_migratetype),

	TP_STRUCT__entry(
		__field(	struct page *,	page			)
		__field(	int,		alloc_order		)
		__field(	int,		fallback_order		)
		__field(	int,		alloc_migratetype	)
		__field(	int,		fallback_migratetype	)
	),

	TP_fast_assign(
		__entry->page			= page;
		__entry->alloc_order		= alloc_order;
		__entry->fallback_order		= fallback_order;
		__entry->alloc_migratetype	= alloc_migratetype;
		__entry->fallback_migratetype	= fallback_migratetype;
	),

	TP_printk("page=%p pfn=%lu alloc_order=%d fallback_order=%d pageblock_order=%d alloc_migratetype=%d fallback_migratetype=%d fragmenting=%d change_ownership=%d",
		__entry->page,
		page_to_pfn(__entry->page),
		__entry->alloc_order,
		__entry->fallback_order,
		pageblock_order,
		__entry->alloc_migratetype,
		__entry->fallback_migratetype,
		__entry->fallback_order < pageblock_order,
		__entry->alloc_migratetype == __entry->fallback_migratetype)
);
#endif

#endif /* _TRACE_MMAP_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
