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

static bool __gcsd_init = false;

static bool __gcsd_startup(DISC_INTERFACE *disc)
{
	s32 ret,chan = (disc->ioType&0xff)-'0';
	u32 dev;

	if(disc->ioType < DEVICE_TYPE_GAMECUBE_SD(0)) return false;
	if(disc->ioType > DEVICE_TYPE_GAMECUBE_SD(2)) return false;

	if(!__gcsd_init) {
		sdgecko_initBufferPool();
		sdgecko_initIODefault();
		__gcsd_init = true;
	}

	dev = sdgecko_getDevice(chan);

	ret = sdgecko_preIO(chan);
	if(ret != CARDIO_ERROR_READY) {
		if(dev == EXI_DEVICE_0)
			sdgecko_setDevice(chan, EXI_DEVICE_2);
		else
			sdgecko_setDevice(chan, EXI_DEVICE_0);

		ret = sdgecko_preIO(chan);
	}

	if(ret == CARDIO_ERROR_READY) {
		dev = sdgecko_getDevice(chan);

		if(chan == EXI_CHANNEL_0) {
			if(dev == EXI_DEVICE_0) {
				disc->features |= FEATURE_GAMECUBE_SLOTA;
				disc->features &= ~FEATURE_GAMECUBE_PORT1;
			} else {
				disc->features |= FEATURE_GAMECUBE_PORT1;
				disc->features &= ~FEATURE_GAMECUBE_SLOTA;
			}
		}

		if(PERM_WRITE_PROTECT(chan) || TMP_WRITE_PROTECT(chan))
			disc->features &= ~FEATURE_MEDIUM_CANWRITE;
		else
			disc->features |= FEATURE_MEDIUM_CANWRITE;

		switch(CSD_STRUCTURE(chan)) {
			case 0:
				disc->numberOfSectors = ((C_SIZE(chan) + 1) << (C_SIZE_MULT(chan) + 2)) << (READ_BL_LEN(chan) - 9);
				break;
			case 1:
				disc->numberOfSectors = (C_SIZE1(chan) + 1LL) << 10;
				break;
			case 2:
				disc->numberOfSectors = (C_SIZE2(chan) + 1LL) << 10;
				break;
			default:
				disc->numberOfSectors = ~0;
				break;
		}

		return true;
	}

	sdgecko_setDevice(chan, dev);
	return false;
}

static bool __gcsd_isInserted(DISC_INTERFACE *disc)
{
	s32 chan = (disc->ioType&0xff)-'0';

	if(disc->ioType < DEVICE_TYPE_GAMECUBE_SD(0)) return false;
	if(disc->ioType > DEVICE_TYPE_GAMECUBE_SD(2)) return false;

	return sdgecko_isInserted(chan);
}

static bool __gcsd_readSectors(DISC_INTERFACE *disc, sec_t sector, sec_t numSectors, void *buffer)
{
	s32 ret,chan = (disc->ioType&0xff)-'0';

	if(disc->ioType < DEVICE_TYPE_GAMECUBE_SD(0)) return false;
	if(disc->ioType > DEVICE_TYPE_GAMECUBE_SD(2)) return false;
	if(!(disc->features & FEATURE_MEDIUM_CANREAD)) return false;
	if((sector + numSectors) < sector) return false;
	if((sector + numSectors) > disc->numberOfSectors) return false;
	if(disc->bytesPerSector != PAGE_SIZE512) return false;
	if(!SYS_IsDMAAddress(buffer, 1)) return false;
	if(!sdgecko_isInitialized(chan)) return false;

	if(numSectors == 1)
		ret = sdgecko_readSector(chan, buffer, sector);
	else
		ret = sdgecko_readSectors(chan, sector, numSectors, buffer);

	if(ret == CARDIO_ERROR_READY)
		return true;

	return false;
}

static bool __gcsd_writeSectors(DISC_INTERFACE *disc, sec_t sector, sec_t numSectors, const void *buffer)
{
	s32 ret,chan = (disc->ioType&0xff)-'0';

	if(disc->ioType < DEVICE_TYPE_GAMECUBE_SD(0)) return false;
	if(disc->ioType > DEVICE_TYPE_GAMECUBE_SD(2)) return false;
	if(!(disc->features & FEATURE_MEDIUM_CANWRITE)) return false;
	if((sector + numSectors) < sector) return false;
	if((sector + numSectors) > disc->numberOfSectors) return false;
	if(disc->bytesPerSector != PAGE_SIZE512) return false;
	if(!SYS_IsDMAAddress(buffer, 1)) return false;
	if(!sdgecko_isInitialized(chan)) return false;

	if(numSectors == 1)
		ret = sdgecko_writeSector(chan, buffer, sector);
	else
		ret = sdgecko_writeSectors(chan, sector, numSectors, buffer);

	if(ret == CARDIO_ERROR_READY)
		return true;

	return false;
}

static bool __gcsd_clearStatus(DISC_INTERFACE *disc)
{
	return true;
}

static bool __gcsd_shutdown(DISC_INTERFACE *disc)
{
	s32 chan = (disc->ioType&0xff)-'0';

	if(disc->ioType < DEVICE_TYPE_GAMECUBE_SD(0)) return false;
	if(disc->ioType > DEVICE_TYPE_GAMECUBE_SD(2)) return false;

	sdgecko_doUnmount(chan);
	return true;
}

DISC_INTERFACE __io_gcsda = {
	DEVICE_TYPE_GAMECUBE_SD(0),
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_GAMECUBE_SLOTA,
	__gcsd_startup,
	__gcsd_isInserted,
	__gcsd_readSectors,
	__gcsd_writeSectors,
	__gcsd_clearStatus,
	__gcsd_shutdown,
	0,
	PAGE_SIZE512
};

DISC_INTERFACE __io_gcsdb = {
	DEVICE_TYPE_GAMECUBE_SD(1),
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_GAMECUBE_SLOTB,
	__gcsd_startup,
	__gcsd_isInserted,
	__gcsd_readSectors,
	__gcsd_writeSectors,
	__gcsd_clearStatus,
	__gcsd_shutdown,
	0,
	PAGE_SIZE512
};

DISC_INTERFACE __io_gcsd2 = {
	DEVICE_TYPE_GAMECUBE_SD(2),
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_GAMECUBE_PORT2,
	__gcsd_startup,
	__gcsd_isInserted,
	__gcsd_readSectors,
	__gcsd_writeSectors,
	__gcsd_clearStatus,
	__gcsd_shutdown,
	0,
	PAGE_SIZE512
};
