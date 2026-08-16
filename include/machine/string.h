/*-------------------------------------------------------------

string.h -- String operations

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

#ifndef __MACHINE_STRING_H__
#define __MACHINE_STRING_H__

#if defined(HW_RVL)

char *mem1_strdup(const char *str);
char *mem1_strndup(const char *str, size_t n);

char *mem2_strdup(const char *str);
char *mem2_strndup(const char *str, size_t n);

#endif

#endif
