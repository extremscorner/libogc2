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

message.h -- Thread subsystem II

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

#ifndef __OGC_MESSAGE_H__
#define __OGC_MESSAGE_H__

/*! \file message.h 
\brief Thread subsystem II

*/ 

#include <gctypes.h>

#define MQ_BOX_NULL				0xffffffff

#define MQ_ERROR_SUCCESSFUL		0
#define MQ_ERROR_TOOMANY		-5

#define MQ_MSG_BLOCK			0
#define MQ_MSG_NOBLOCK			1


#ifdef __cplusplus
extern "C" {
#endif


/*! \typedef u32 mqbox_t
\brief typedef for the message queue handle
*/
typedef u32 mqbox_t;


/*! \typedef void* mqmsg_t
\brief typedef for the message pointer
*/
typedef void* mqmsg_t;



/*! \fn u32 MQ_Init(mqbox_t *mqbox,u32 count)
\brief Initializes a message queue
\param[out] mqbox pointer to the mqbox_t handle.
\param[in] count maximum number of messages the queue can hold

\return 0 on success, <0 on error
*/
s32 MQ_Init(mqbox_t *mqbox,u32 count);


/*! \fn void MQ_Close(mqbox_t mqbox)
\brief Closes the message queue and releases all memory.
\param[in] mqbox handle to the mqbox_t structure.

\return none
*/
void MQ_Close(mqbox_t mqbox);


/*! \fn BOOL MQ_Send(mqbox_t mqbox,mqmsg_t msg,u32 flags)
\brief Sends a message to the given message queue.
\param[in] mqbox mqbox_t handle to the message queue
\param[in] msg message to send
\param[in] flags message flags (MQ_MSG_BLOCK, MQ_MSG_NOBLOCK)

\return bool result
*/
BOOL MQ_Send(mqbox_t mqbox,mqmsg_t msg,u32 flags);


/*! \fn BOOL MQ_TimedSend(mqbox_t mqbox,mqmsg_t msg,const struct timespec *reltime)
\brief Sends a message to the given message queue, blocking until timeout.
\param[in] mqbox mqbox_t handle to the message queue
\param[in] msg message to send
\param[in] reltime pointer to a timespec structure holding the relative time for the timeout.

\return bool result
*/
BOOL MQ_TimedSend(mqbox_t mqbox,mqmsg_t msg,const struct timespec *reltime);


/*! \fn BOOL MQ_Jam(mqbox_t mqbox,mqmsg_t msg,u32 flags)
\brief Sends a message to the given message queue and jams it in front of the queue.
\param[in] mqbox mqbox_t handle to the message queue
\param[in] msg message to send
\param[in] flags message flags (MQ_MSG_BLOCK, MQ_MSG_NOBLOCK)

\return bool result
*/
BOOL MQ_Jam(mqbox_t mqbox,mqmsg_t msg,u32 flags);


/*! \fn BOOL MQ_TimedJam(mqbox_t mqbox,mqmsg_t msg,const struct timespec *reltime)
\brief Sends a message to the given message queue and jams it in front of the queue, blocking until timeout.
\param[in] mqbox mqbox_t handle to the message queue
\param[in] msg message to send
\param[in] reltime pointer to a timespec structure holding the relative time for the timeout.

\return bool result
*/
BOOL MQ_TimedJam(mqbox_t mqbox,mqmsg_t msg,const struct timespec *reltime);


/*! \fn BOOL MQ_Receive(mqbox_t mqbox,mqmsg_t *msg,u32 flags)
\brief Receives a message from the given message queue.
\param[in] mqbox mqbox_t handle to the message queue
\param[in] msg pointer to a mqmsg_t_t-type message to receive.
\param[in] flags message flags (MQ_MSG_BLOCK, MQ_MSG_NOBLOCK)

\return bool result
*/
BOOL MQ_Receive(mqbox_t mqbox,mqmsg_t *msg,u32 flags);


/*! \fn BOOL MQ_TimedReceive(mqbox_t mqbox,mqmsg_t *msg,const struct timespec *reltime)
\brief Receives a message from the given message queue, blocking until timeout.
\param[in] mqbox mqbox_t handle to the message queue
\param[in] msg pointer to a mqmsg_t_t-type message to receive.
\param[in] reltime pointer to a timespec structure holding the relative time for the timeout.

\return bool result
*/
BOOL MQ_TimedReceive(mqbox_t mqbox,mqmsg_t *msg,const struct timespec *reltime);

#ifdef __cplusplus
	}
#endif

#endif
