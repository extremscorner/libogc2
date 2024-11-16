/*

	gcsd.h

	Hardware routines for reading and writing to SD geckos connected
	to the memory card ports.

	These functions are just wrappers around libsdcard's functions.

 Copyright (c) 2008 Sven "svpe" Peter <svpe@gmx.net>
	
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

#include <sdcard/gcsd.h>
#include <sdcard/card_cmn.h>
#include <sdcard/card_io.h>
#include <sdcard/card_buf.h>

static int __gcsd_init = 0;

static bool __gcsd_startup(int n)
{
	s32 ret;
	u32 dev;

	if(!__gcsd_init) {
		sdgecko_initBufferPool();
		sdgecko_initIODefault();
		__gcsd_init = 1;
	}

	dev = sdgecko_getDevice(n);

	ret = sdgecko_preIO(n);
	if(ret == CARDIO_ERROR_READY)
		return true;

	if(dev == EXI_DEVICE_0)
		sdgecko_setDevice(n, EXI_DEVICE_2);
	else
		sdgecko_setDevice(n, EXI_DEVICE_0);

	ret = sdgecko_preIO(n);
	if(ret == CARDIO_ERROR_READY)
		return true;

	sdgecko_setDevice(n, dev);
	return false;
}

static bool __gcsd_isInserted(int n)
{
	return sdgecko_isInserted(n);
}

static bool __gcsd_readSectors(int n, sec_t sector, sec_t numSectors, void *buffer)
{
	s32 ret;

	if((u32)sector != sector) return false;
	if((u32)numSectors != numSectors) return false;
	if(!SYS_IsDMAAddress(buffer)) return false;

	if(numSectors == 1)
		ret = sdgecko_readSector(n, buffer, sector);
	else
		ret = sdgecko_readSectors(n, sector, numSectors, buffer);

	if(ret == CARDIO_ERROR_READY)
		return true;

	return false;
}

static bool __gcsd_writeSectors(int n, sec_t sector, sec_t numSectors, const void *buffer)
{
	s32 ret;

	if((u32)sector != sector) return false;
	if((u32)numSectors != numSectors) return false;
	if(!SYS_IsDMAAddress(buffer)) return false;

	if(numSectors == 1)
		ret = sdgecko_writeSector(n, buffer, sector);
	else
		ret = sdgecko_writeSectors(n, sector, numSectors, buffer);

	if(ret == CARDIO_ERROR_READY)
		return true;

	return false;
}

static bool __gcsd_clearStatus(int n)
{
	return true; // FIXME
}

static bool __gcsd_shutdown(int n)
{
	sdgecko_doUnmount(n);
	return true;
}


static bool __gcsda_startup(DISC_INTERFACE *disc)
{
	bool ret;

	ret = __gcsd_startup(0);

	if(sdgecko_getDevice(0) == EXI_DEVICE_0) {
		disc->features |= FEATURE_GAMECUBE_SLOTA;
		disc->features &= ~FEATURE_GAMECUBE_PORT1;
	} else {
		disc->features |= FEATURE_GAMECUBE_PORT1;
		disc->features &= ~FEATURE_GAMECUBE_SLOTA;
	}

	return ret;
}

static bool __gcsda_isInserted(DISC_INTERFACE *disc)
{
	return __gcsd_isInserted(0);
}

static bool __gcsda_readSectors(DISC_INTERFACE *disc, sec_t sector, sec_t numSectors, void *buffer)
{
	return __gcsd_readSectors(0, sector, numSectors, buffer);
}

static bool __gcsda_writeSectors(DISC_INTERFACE *disc, sec_t sector, sec_t numSectors, const void *buffer)
{
	return __gcsd_writeSectors(0, sector, numSectors, buffer);
}

static bool __gcsda_clearStatus(DISC_INTERFACE *disc)
{
	return __gcsd_clearStatus(0);
}

static bool __gcsda_shutdown(DISC_INTERFACE *disc)
{
	return __gcsd_shutdown(0);
}



static bool __gcsdb_startup(DISC_INTERFACE *disc)
{
	return __gcsd_startup(1);
}

static bool __gcsdb_isInserted(DISC_INTERFACE *disc)
{
	return __gcsd_isInserted(1);
}

static bool __gcsdb_readSectors(DISC_INTERFACE *disc, sec_t sector, sec_t numSectors, void *buffer)
{
	return __gcsd_readSectors(1, sector, numSectors, buffer);
}

static bool __gcsdb_writeSectors(DISC_INTERFACE *disc, sec_t sector, sec_t numSectors, const void *buffer)
{
	return __gcsd_writeSectors(1, sector, numSectors, buffer);
}

static bool __gcsdb_clearStatus(DISC_INTERFACE *disc)
{
	return __gcsd_clearStatus(1);
}

static bool __gcsdb_shutdown(DISC_INTERFACE *disc)
{
	return __gcsd_shutdown(1);
}



static bool __gcsd2_startup(DISC_INTERFACE *disc)
{
	return __gcsd_startup(2);
}

static bool __gcsd2_isInserted(DISC_INTERFACE *disc)
{
	return __gcsd_isInserted(2);
}

static bool __gcsd2_readSectors(DISC_INTERFACE *disc, sec_t sector, sec_t numSectors, void *buffer)
{
	return __gcsd_readSectors(2, sector, numSectors, buffer);
}

static bool __gcsd2_writeSectors(DISC_INTERFACE *disc, sec_t sector, sec_t numSectors, const void *buffer)
{
	return __gcsd_writeSectors(2, sector, numSectors, buffer);
}

static bool __gcsd2_clearStatus(DISC_INTERFACE *disc)
{
	return __gcsd_clearStatus(2);
}

static bool __gcsd2_shutdown(DISC_INTERFACE *disc)
{
	return __gcsd_shutdown(2);
}

DISC_INTERFACE __io_gcsda = {
	DEVICE_TYPE_GC_SD,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_GAMECUBE_SLOTA,
	__gcsda_startup,
	__gcsda_isInserted,
	__gcsda_readSectors,
	__gcsda_writeSectors,
	__gcsda_clearStatus,
	__gcsda_shutdown,
	(u32)~0,
	512
};

DISC_INTERFACE __io_gcsdb = {
	DEVICE_TYPE_GC_SD,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_GAMECUBE_SLOTB,
	__gcsdb_startup,
	__gcsdb_isInserted,
	__gcsdb_readSectors,
	__gcsdb_writeSectors,
	__gcsdb_clearStatus,
	__gcsdb_shutdown,
	(u32)~0,
	512
};

DISC_INTERFACE __io_gcsd2 = {
	DEVICE_TYPE_GC_SD,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_GAMECUBE_PORT2,
	__gcsd2_startup,
	__gcsd2_isInserted,
	__gcsd2_readSectors,
	__gcsd2_writeSectors,
	__gcsd2_clearStatus,
	__gcsd2_shutdown,
	(u32)~0,
	512
};
