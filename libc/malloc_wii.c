/*-------------------------------------------------------------

malloc_wii.c -- Wii split heap implementation

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

#if defined(HW_RVL)

#define USE_DL_PREFIX
#include <malloc.h>
#include <ogc/system.h>
#include <stddef.h>
#include <stdlib.h>

extern BOOL MALLOC_MEM2;
static mspace msp;

static void __attribute__((constructor(0))) init_malloc(void)
{
	if (MALLOC_MEM2) {
		size_t capacity = SYS_GetArena1Size();
		void *base = SYS_AllocArenaMem1Lo(capacity, 1);
		if (base) msp = create_mspace_with_base(base, capacity, 1);
	}
}

void *__attribute__((weak)) malloc(size_t bytes)
{
	if (msp)
		return mspace_malloc(msp, bytes);
	else
		return dlmalloc(bytes);
}

void __attribute__((weak)) free(void *mem)
{
	if (msp)
		mspace_free(msp, mem);
	else
		dlfree(mem);
}

void *__attribute__((weak)) calloc(size_t n_elements, size_t elem_size)
{
	if (msp)
		return mspace_calloc(msp, n_elements, elem_size);
	else
		return dlcalloc(n_elements, elem_size);
}

void *__attribute__((weak)) realloc(void *oldmem, size_t bytes)
{
	if (msp)
		return mspace_realloc(msp, oldmem, bytes);
	else
		return dlrealloc(oldmem, bytes);
}

void *__attribute__((weak)) realloc_in_place(void *oldmem, size_t bytes)
{
	if (msp)
		return mspace_realloc_in_place(msp, oldmem, bytes);
	else
		return dlrealloc_in_place(oldmem, bytes);
}

void *__attribute__((weak)) memalign(size_t alignment, size_t bytes)
{
	if (msp)
		return mspace_memalign(msp, alignment, bytes);
	else
		return dlmemalign(alignment, bytes);
}

void *__attribute__((weak)) valloc(size_t bytes)
{
	if (msp)
		return mspace_valloc(msp, bytes);
	else
		return dlvalloc(bytes);
}

void *__attribute__((weak)) pvalloc(size_t bytes)
{
	if (msp)
		return mspace_pvalloc(msp, bytes);
	else
		return dlpvalloc(bytes);
}

void **__attribute__((weak)) independent_calloc(size_t n_elements, size_t elem_size, void *chunks[])
{
	if (msp)
		return mspace_independent_calloc(msp, n_elements, elem_size, chunks);
	else
		return dlindependent_calloc(n_elements, elem_size, chunks);
}

void **__attribute__((weak)) independent_comalloc(size_t n_elements, size_t sizes[], void *chunks[])
{
	if (msp)
		return mspace_independent_comalloc(msp, n_elements, sizes, chunks);
	else
		return dlindependent_comalloc(n_elements, sizes, chunks);
}

size_t __attribute__((weak)) bulk_free(void *array[], size_t nelem)
{
	if (msp)
		return mspace_bulk_free(msp, array, nelem);
	else
		return dlbulk_free(array, nelem);
}

void __attribute__((weak)) malloc_inspect_all(void (*handler)(void *, void *, size_t, void *), void *arg)
{
	if (msp)
		mspace_inspect_all(msp, handler, arg);
	else
		dlmalloc_inspect_all(handler, arg);
}

int __attribute__((weak)) malloc_trim(size_t pad)
{
	if (msp)
		return mspace_trim(msp, pad);
	else
		return dlmalloc_trim(pad);
}

size_t __attribute__((weak)) malloc_footprint(void)
{
	if (msp)
		return mspace_footprint(msp);
	else
		return dlmalloc_footprint();
}

size_t __attribute__((weak)) malloc_max_footprint(void)
{
	if (msp)
		return mspace_max_footprint(msp);
	else
		return dlmalloc_max_footprint();
}

size_t __attribute__((weak)) malloc_footprint_limit(void)
{
	if (msp)
		return mspace_footprint_limit(msp);
	else
		return dlmalloc_footprint_limit();
}

size_t __attribute__((weak)) malloc_set_footprint_limit(size_t bytes)
{
	if (msp)
		return mspace_set_footprint_limit(msp, bytes);
	else
		return dlmalloc_set_footprint_limit(bytes);
}

struct mallinfo __attribute__((weak)) mallinfo(void)
{
	if (msp)
		return mspace_mallinfo(msp);
	else
		return dlmallinfo();
}

void __attribute__((weak)) malloc_stats(void)
{
	if (msp)
		mspace_malloc_stats(msp);
	else
		dlmalloc_stats();
}

#endif
