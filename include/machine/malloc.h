/*-------------------------------------------------------------

malloc.h -- Wii split heap implementation

Copyright (C) 2026 Extrems' Corner.org

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#ifndef __MACHINE_MALLOC_H__
#define __MACHINE_MALLOC_H__

#if defined(HW_RVL)

void *mem1_malloc(size_t bytes);
void mem1_free(void *mem);
void *mem1_calloc(size_t n_elements, size_t elem_size);
void *mem1_realloc(void *oldmem, size_t bytes);
void *mem1_realloc_in_place(void *oldmem, size_t bytes);
void *mem1_memalign(size_t alignment, size_t bytes);
int mem1_posix_memalign(void **pp, size_t alignment, size_t bytes);
void *mem1_valloc(size_t bytes);
void *mem1_pvalloc(size_t bytes);
void **mem1_independent_calloc(size_t n_elements, size_t elem_size, void *chunks[]);
void **mem1_independent_comalloc(size_t n_elements, size_t sizes[], void *chunks[]);
size_t mem1_bulk_free(void *array[], size_t n_elements);
void mem1_malloc_inspect_all(void (*handler)(void *, void *, size_t, void *), void *arg);
int mem1_malloc_trim(size_t pad);
size_t mem1_malloc_footprint(void);
size_t mem1_malloc_max_footprint(void);
size_t mem1_malloc_footprint_limit(void);
size_t mem1_malloc_set_footprint_limit(size_t bytes);
struct mallinfo mem1_mallinfo(void);
void mem1_malloc_stats(void);

void *mem2_malloc(size_t bytes);
void mem2_free(void *mem);
void *mem2_calloc(size_t n_elements, size_t elem_size);
void *mem2_realloc(void *oldmem, size_t bytes);
void *mem2_realloc_in_place(void *oldmem, size_t bytes);
void *mem2_memalign(size_t alignment, size_t bytes);
int mem2_posix_memalign(void **pp, size_t alignment, size_t bytes);
void *mem2_valloc(size_t bytes);
void *mem2_pvalloc(size_t bytes);
void **mem2_independent_calloc(size_t n_elements, size_t elem_size, void *chunks[]);
void **mem2_independent_comalloc(size_t n_elements, size_t sizes[], void *chunks[]);
size_t mem2_bulk_free(void *array[], size_t n_elements);
void mem2_malloc_inspect_all(void (*handler)(void *, void *, size_t, void *), void *arg);
int mem2_malloc_trim(size_t pad);
size_t mem2_malloc_footprint(void);
size_t mem2_malloc_max_footprint(void);
size_t mem2_malloc_footprint_limit(void);
size_t mem2_malloc_set_footprint_limit(size_t bytes);
struct mallinfo mem2_mallinfo(void);
void mem2_malloc_stats(void);

#endif

#endif
