/**
* @file dbg_wrapper.h
*  Wrapper function definition for debug
* Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
*
* $Revision: 1.1 $
* $Date: 2016/08/08 08:01:51 $
*
* ChangeLog:
*  $Log: dbg_wrapper.h,v $
*  Revision 1.1  2016/08/08 08:01:51  ivan
*  update v1 wrapper for GM8135
*
*  Revision 1.1.1.1  2015/06/03 02:20:16  ivan
*
*
*  Revision 1.1  2015/04/30 09:20:35  schumy_c
*  Add 'util' and remove 'log' folder.
*  Add some common stuffs in util.
*
*/

#define __DBG_WRAPPER_H__

#include <linux/types.h>

#define kmalloc(a, b)       kmalloc_tracer((a), (b), __FILE__, __FUNCTION__, __LINE__)
#define __kmalloc(s, f)  kmalloc_node_tracer((s), (f), __FILE__, __FUNCTION__, __LINE__)
#define kzalloc(a, b)   kzalloc_tracer((a), (b), __FILE__, __FUNCTION__, __LINE__)
#define kfree(p)        kfree_tracer((p), __FILE__, __FUNCTION__, __LINE__)
#define kmem_cache_create(n, s, a, f, p) kmem_cache_create_tracer((n), (s), (a), (f), (p), __FILE__, __FUNCTION__, __LINE__)
#define kmem_cache_alloc(c,f)   kmem_cache_alloc_tracer((c), (f), __FILE__, __FUNCTION__, __LINE__)
#define kmem_cache_free(c, p)   kmem_cache_free_tracer((c), (p), __FILE__, __FUNCTION__, __LINE__)
#define kmem_cache_destroy(c)   kmem_cache_destroy_tracer((c), __FILE__, __FUNCTION__, __LINE__)
#define kmem_cache_shrink(c)    kmem_cache_shrink_tracer((c), __FILE__, __FUNCTION__, __LINE__)

#define vmalloc(s)          vmalloc_tracer((s), __FILE__, __FUNCTION__, __LINE__)
#define vzalloc(a)          vzalloc_tracer((a), __FILE__, __FUNCTION__, __LINE__)
#define vfree(p)            vfree_tracer((p), __FILE__, __FUNCTION__, __LINE__)
#define __vmalloc(s, m, p)  __vmalloc_tracer((s), (m), (p), __FILE__, __FUNCTION__, __LINE__)
#define vmalloc_user(s)     vmalloc_user_tracer((s), __FILE__, __FUNCTION__, __LINE__)
#define vmalloc_node(s, n)     vmalloc_node_tracer((s), (n), __FILE__, __FUNCTION__, __LINE__)
#define vzalloc_node(s, n)     vzalloc_node_tracer((s), (n), __FILE__, __FUNCTION__, __LINE__)
#define vmalloc_32(s)       vmalloc_32_tracer((s), __FILE__, __FUNCTION__, __LINE__)
#define vmalloc_32_user(s)  vmalloc_32_user_tracer((s), __FILE__, __FUNCTION__, __LINE__)

void *kmalloc_tracer(size_t size, gfp_t flags, const char *file_name, const char *func_name,
                     unsigned int line_no);
void *__kmalloc_tracer(size_t size, gfp_t flags, const char *file_name,
                          const char *func_name, unsigned int line_no);


void *kzalloc_tracer(size_t size, gfp_t flags, const char *file_name, const char *func_name, unsigned int line_no);
void kfree_tracer(const void *objp, const char *file_name, const char *func_name, unsigned int line_no);

void *vmalloc_tracer(unsigned long size, const char *file_name, const char *func_name, unsigned int line_no);
void *vzalloc_tracer(unsigned long size, const char *file_name, const char *func_name, unsigned int line_no);
void vfree_tracer(const void *objp, const char *file_name, const char *func_name, unsigned int line_no);
void *__vmalloc_tracer(unsigned long size, gfp_t gfp_mask, pgprot_t prot, const char *file_name,
                       const char *func_name, unsigned int line_no);
void *vmalloc_user_tracer(unsigned long size, const char *file_name, const char *func_name, unsigned int line_no);
void *vmalloc_node_tracer(unsigned long size, int node, const char *file_name, const char *func_name, unsigned int line_no);
void *vzalloc_node_tracer(unsigned long size, int node, const char *file_name, const char *func_name, unsigned int line_no);
void *vmalloc_32_tracer(unsigned long size, const char *file_name, const char *func_name, unsigned int line_no);
void *vmalloc_32_user_tracer(unsigned long size, const char *file_name, const char *func_name, unsigned int line_no);


struct kmem_cache * kmem_cache_create_tracer(const char *name, size_t size, size_t align,
                                            unsigned long flags, void (*ctor)(void *),
                                            const char *file_name, const char *func_name,
                                            unsigned int line_no);
void *kmem_cache_alloc_tracer(struct kmem_cache *cachep, gfp_t flags, const char *file_name,
                              const char *func_name, unsigned int line_no);
void kmem_cache_free_tracer(struct kmem_cache *cachep, void *objp, const char *file_name,
                            const char *func_name, unsigned int line_no);
void kmem_cache_destroy_tracer(struct kmem_cache *cachep, const char *file_name,
                               const char *func_name, unsigned int line_no);

int kmem_cache_shrink_tracer(struct kmem_cache *cachep, const char *file_name,
                               const char *func_name, unsigned int line_no);




