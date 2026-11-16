/*-------------------------------------------------------------

reallocarr.c -- Array reallocation

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

#include <errno.h>
#include <malloc.h>
#include <stdckdint.h>
#include <stddef.h>

int reallocarr(void *pp, size_t n_elements, size_t elem_size)
{
	void *oldmem = *(void **)pp;
	size_t bytes;

	if (ckd_mul(&bytes, n_elements, elem_size))
		return EOVERFLOW;

	if (bytes == 0) {
		free(oldmem);
		*(void **)pp = NULL;
		return 0;
	}

	void *mem = realloc(oldmem, bytes);

	if (mem == NULL) {
		return ENOMEM;
	} else {
		*(void **)pp = mem;
		return 0;
	}
}

int mspace_reallocarr(mspace msp, void *pp, size_t n_elements, size_t elem_size)
{
	void *oldmem = *(void **)pp;
	size_t bytes;

	if (ckd_mul(&bytes, n_elements, elem_size))
		return EOVERFLOW;

	if (bytes == 0) {
		mspace_free(msp, oldmem);
		*(void **)pp = NULL;
		return 0;
	}

	void *mem = mspace_realloc(msp, oldmem, bytes);

	if (mem == NULL) {
		return ENOMEM;
	} else {
		*(void **)pp = mem;
		return 0;
	}
}
