/*-------------------------------------------------------------

mallocr.c -- Newlib memory routines

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

#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>

void *_malloc_r(struct _reent *, size_t bytes)
{
	return malloc(bytes);
}

void _free_r(struct _reent *, void *mem)
{
	free(mem);
}

void *_realloc_r(struct _reent *, void *oldmem, size_t bytes)
{
	return realloc(oldmem, bytes);
}

void *aligned_alloc(size_t alignment, size_t bytes)
{
	return memalign(alignment, bytes);
}

void *_memalign_r(struct _reent *, size_t alignment, size_t bytes)
{
	return memalign(alignment, bytes);
}

void *_valloc_r(struct _reent *, size_t bytes)
{
	return valloc(bytes);
}

void *_pvalloc_r(struct _reent *, size_t bytes)
{
	return pvalloc(bytes);
}

void *_calloc_r(struct _reent *, size_t n_elements, size_t elem_size)
{
	return calloc(n_elements, elem_size);
}

void cfree(void *mem)
{
	free(mem);
}

int _malloc_trim_r(struct _reent *, size_t pad)
{
	return malloc_trim(pad);
}

size_t _malloc_usable_size_r(struct _reent *, void *mem)
{
	return malloc_usable_size(mem);
}

void _malloc_stats_r(struct _reent *)
{
	malloc_stats();
}

struct mallinfo _mallinfo_r(struct _reent *)
{
	return mallinfo();
}

int _mallopt_r(struct _reent *, int param_number, int value)
{
	return mallopt(param_number, value);
}
