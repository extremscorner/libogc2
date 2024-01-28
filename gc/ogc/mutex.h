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

mutex.h -- Thread subsystem III

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

#ifndef __OGC_MUTEX_H__
#define __OGC_MUTEX_H__

/*! \file mutex.h 
\brief Thread subsystem III

*/ 

#include <gctypes.h>
#include <time.h>

#define LWP_MUTEX_NULL		0xffffffff

#ifdef __cplusplus
	extern "C" {
#endif


/*! \typedef u32 mutex_t
\brief typedef for the mutex handle
*/
typedef u32 mutex_t;


/*! \fn s32 LWP_MutexInit(mutex_t *mutex,bool use_recursive)
\brief Initializes a mutex lock.
\param[out] mutex pointer to a mutex_t handle.
\param[in] use_recursive whether to allow the thread, whithin the same context, to enter multiple times the lock or not.

\return 0 on success, <0 on error
*/
s32 LWP_MutexInit(mutex_t *mutex,bool use_recursive);


/*! \fn s32 LWP_MutexDestroy(mutex_t mutex)
\brief Close mutex lock, release all threads and handles locked on this mutex.
\param[in] mutex handle to the mutex_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_MutexDestroy(mutex_t mutex);


/*! \fn s32 LWP_MutexLock(mutex_t mutex)
\brief Enter the mutex lock.
\param[in] mutex handle to the mutex_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_MutexLock(mutex_t mutex);


/*! \fn s32 LWP_MutexTimedLock(mutex_t mutex,const struct timespec *abstime)
\brief Try to enter the mutex lock until timeout.
\param[in] mutex handle to the mutex_t structure.
\param[in] abstime pointer to a timespec structure holding the abs time for the timeout.

\return 0 on success, <0 on error
*/
s32 LWP_MutexTimedLock(mutex_t mutex,const struct timespec *abstime);


/*! \fn s32 LWP_MutexTryLock(mutex_t mutex)
\brief Try to enter the mutex lock.
\param[in] mutex handle to the mutex_t structure.

\return 0: on first aquire, 1: would lock
*/
s32 LWP_MutexTryLock(mutex_t mutex);


/*! \fn s32 LWP_MutexUnlock(mutex_t mutex)
\brief Release the mutex lock and let other threads process further on this mutex.
\param[in] mutex handle to the mutex_t structure.

\return 0 on success, <0 on error
*/
s32 LWP_MutexUnlock(mutex_t mutex);

#ifdef __cplusplus
	}
#endif

#endif
