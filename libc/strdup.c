/*-------------------------------------------------------------

strdup.c -- String duplication

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
#include <string.h>

char *strdup(const char *str)
{
	size_t len = strlen(str) + 1;
	char *dup = malloc(len);

	if (dup) dup = memcpy(dup, str, len);
	return dup;
}

char *mspace_strdup(mspace msp, const char *str)
{
	size_t len = strlen(str) + 1;
	char *dup = mspace_malloc(msp, len);

	if (dup) dup = memcpy(dup, str, len);
	return dup;
}
