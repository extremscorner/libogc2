/*
 disc_io.h
 Interface template for low level disc functions.

 Copyright (c) 2006 Michael "Chishm" Chisholm
 Based on code originally written by MightyMax

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef OGC_DISC_IO_INCLUDE
#define OGC_DISC_IO_INCLUDE

#include <stdint.h>
#include <gctypes.h>

#define FEATURE_MEDIUM_CANREAD      0x00000001
#define FEATURE_MEDIUM_CANWRITE     0x00000002
#define FEATURE_MEDIUM_CANERASE     0x00000004
#define FEATURE_MEDIUM_CANFORMAT    0x00000008
#define FEATURE_GAMECUBE_SLOTA      0x00000010
#define FEATURE_GAMECUBE_SLOTB      0x00000020
#define FEATURE_GAMECUBE_PORT2      0x00000040
#define FEATURE_GAMECUBE_PORT1      0x00000080
#define FEATURE_GAMECUBE_DVD        0x00000100
#define FEATURE_WII_SD              0x00001000
#define FEATURE_WII_USB             0x00002000
#define FEATURE_WII_DVD             0x00004000

typedef uint64_t sec_t;

typedef struct DISC_INTERFACE_STRUCT DISC_INTERFACE ;

typedef bool (* FN_MEDIUM_STARTUP)(DISC_INTERFACE* disc) ;
typedef bool (* FN_MEDIUM_ISINSERTED)(DISC_INTERFACE* disc) ;
typedef bool (* FN_MEDIUM_READSECTORS)(DISC_INTERFACE* disc, sec_t sector, sec_t numSectors, void* buffer) ;
typedef bool (* FN_MEDIUM_WRITESECTORS)(DISC_INTERFACE* disc, sec_t sector, sec_t numSectors, const void* buffer) ;
typedef bool (* FN_MEDIUM_CLEARSTATUS)(DISC_INTERFACE* disc) ;
typedef bool (* FN_MEDIUM_SHUTDOWN)(DISC_INTERFACE* disc) ;

#ifdef LIBOGC_INTERNAL
 #define DISC_INTERFACE_CONST
#else
 #define DISC_INTERFACE_CONST const
#endif

struct DISC_INTERFACE_STRUCT {
	DISC_INTERFACE_CONST uint32_t				ioType ;
	DISC_INTERFACE_CONST uint32_t				features ;
	DISC_INTERFACE_CONST FN_MEDIUM_STARTUP		startup ;
	DISC_INTERFACE_CONST FN_MEDIUM_ISINSERTED	isInserted ;
	DISC_INTERFACE_CONST FN_MEDIUM_READSECTORS	readSectors ;
	DISC_INTERFACE_CONST FN_MEDIUM_WRITESECTORS	writeSectors ;
	DISC_INTERFACE_CONST FN_MEDIUM_CLEARSTATUS	clearStatus ;
	DISC_INTERFACE_CONST FN_MEDIUM_SHUTDOWN		shutdown ;
	DISC_INTERFACE_CONST sec_t					numberOfSectors ;
	DISC_INTERFACE_CONST uint32_t				bytesPerSector ;
} ;

#endif	// define OGC_DISC_IO_INCLUDE
