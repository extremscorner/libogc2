/*
 * Copyright (C) 1989-1999 On-Line Applications Research Corporation (OAR).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-------------------------------------------------------------

semaphore.h -- Thread subsystem IV

Copyright (C) 2004 - 2025
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Extrems' Corner.org

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

#ifndef __OGC_SEMAPHORE_H__
#define __OGC_SEMAPHORE_H__

/*! \file semaphore.h 
\brief Thread subsystem IV

*/ 

#include <gctypes.h>
#include <time.h>

#define LWP_SEM_NULL			0xffffffff

#ifdef __cplusplus
extern "C" {
#endif


/*! \typedef u32 sem_t
\brief typedef for the semaphore handle
*/
typedef u32 sem_t;


/*! \fn s32 LWP_SemInit(sem_t *sem,u32 start,u32 max)
\brief Initializes a semaphore.
\param[out] sem pointer to a sem_t handle.
\param[in] start start count of the semaphore
\param[in] max maximum count of the semaphore

\return 0 on success, <0 on error
*/
s32 LWP_SemInit(sem_t *sem,u32 start,u32 max);


/*! \fn s32 LWP_SemDestroy(sem_t sem)
\brief Close and destroy a semaphore, release all threads and handles locked on this semaphore.
\param[in] sem handle to the sem_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_SemDestroy(sem_t sem);


/*! \fn s32 LWP_SemWait(sem_t sem)
\brief Count down semaphore counter and enter lock if counter <=0
\param[in] sem handle to the sem_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_SemWait(sem_t sem);


/*! \fn s32 LWP_SemTimedWait(sem_t sem,const struct timespec *reltime)
\brief Count down semaphore counter and try to enter lock if counter <=0 until timeout.
\param[in] sem handle to the sem_t structure.
\param[in] reltime pointer to a timespec structure holding the relative time for the timeout.

\return 0 on success, <0 on error
*/
s32 LWP_SemTimedWait(sem_t sem,const struct timespec *reltime);


/*! \fn s32 LWP_SemTryWait(sem_t sem)
\brief Count down semaphore counter and try to enter lock if counter <=0
\param[in] sem handle to the sem_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_SemTryWait(sem_t sem);


/*! \fn s32 LWP_SemPost(sem_t sem)
\brief Count up semaphore counter and release lock if counter >0
\param[in] sem handle to the sem_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_SemPost(sem_t sem);


/*! \fn s32 LWP_SemGetValue(sem_t sem,u32 *value)
\brief Get the semaphore counter.
\param[in] sem handle to the sem_t structure.
\param[out] value pointer to receive the current count of the semaphore

\return 0 on success, <0 on error
*/
s32 LWP_SemGetValue(sem_t sem,u32 *value);

#ifdef __cplusplus
	}
#endif

#endif
