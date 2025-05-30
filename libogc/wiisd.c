/*

	wiisd.c

	Hardware routines for reading and writing to the Wii's internal
	SD slot.

 Copyright (C) 2008 - 2025
   Michael Wiedenbauer (shagkur)
   Dave Murphy (WinterMute)
   Sven Peter (svpe) <svpe@gmx.net>
   Extrems' Corner.org

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

#if defined(HW_RVL)

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <gcutil.h>
#include <ogc/ipc.h>
#include <ogc/system.h>
#include <unistd.h>
#include <ogc/disc_io.h>
#include <sdcard/wiisd_io.h>

#include <asm.h>
#include <processor.h>

#define PAGE_SIZE512				512

#define SDIOHCR_RESPONSE			0x10
#define SDIOHCR_HOSTCONTROL			0x28
#define SDIOHCR_POWERCONTROL		0x29
#define SDIOHCR_CLOCKCONTROL		0x2c
#define SDIOHCR_TIMEOUTCONTROL		0x2e
#define SDIOHCR_SOFTWARERESET		0x2f

#define SDIOHCR_HOSTCONTROL_4BIT	0x02
#define SDIOHCR_HOSTCONTROL_HS		0x04

#define SDIO_DEFAULT_TIMEOUT		0xe

#define IOCTL_SDIO_WRITEHCREG		0x01
#define IOCTL_SDIO_READHCREG		0x02
#define IOCTL_SDIO_READCREG			0x03
#define IOCTL_SDIO_RESETCARD		0x04
#define IOCTL_SDIO_WRITECREG		0x05
#define IOCTL_SDIO_SETCLK			0x06
#define IOCTL_SDIO_SENDCMD			0x07
#define IOCTL_SDIO_SETBUSWIDTH		0x08
#define IOCTL_SDIO_READMCREG		0x09
#define IOCTL_SDIO_WRITEMCREG		0x0A
#define IOCTL_SDIO_GETSTATUS		0x0B
#define IOCTL_SDIO_GETOCR			0x0C
#define IOCTL_SDIO_READDATA			0x0D
#define IOCTL_SDIO_WRITEDATA		0x0E

#define SDIOCMD_TYPE_BC				1
#define SDIOCMD_TYPE_BCR			2
#define SDIOCMD_TYPE_AC				3
#define SDIOCMD_TYPE_ADTC			4

#define SDIO_RESPONSE_NONE			0
#define SDIO_RESPONSE_R1			1
#define SDIO_RESPONSE_R1B			2
#define SDIO_RESPONSE_R2			3
#define SDIO_RESPONSE_R3			4
#define SDIO_RESPONSE_R4			5
#define SDIO_RESPONSE_R5			6
#define SDIO_RESPONSE_R6			7

#define SDIO_CMD_GOIDLE				0x00
#define SDIO_CMD_ALL_SENDCID		0x02
#define SDIO_CMD_SENDRCA			0x03
#define SDIO_CMD_SWITCHFUNC			0x06
#define SDIO_CMD_SELECT				0x07
#define SDIO_CMD_DESELECT			0x07
#define SDIO_CMD_SENDIFCOND			0x08
#define SDIO_CMD_SENDCSD			0x09
#define SDIO_CMD_SENDCID			0x0A
#define SDIO_CMD_SENDSTATUS			0x0D
#define SDIO_CMD_SETBLOCKLEN		0x10
#define SDIO_CMD_READBLOCK			0x11
#define SDIO_CMD_READMULTIBLOCK		0x12
#define SDIO_CMD_WRITEBLOCK			0x18
#define SDIO_CMD_WRITEMULTIBLOCK	0x19
#define SDIO_CMD_APPCMD				0x37

#define SDIO_ACMD_SETBUSWIDTH		0x06
#define SDIO_ACMD_SENDSCR			0x33
#define SDIO_ACMD_SENDOPCOND		0x29

#define SDIO_STATUS_CARD_INSERTED		0x1
#define SDIO_STATUS_CARD_LOCKED			0x4
#define SDIO_STATUS_CARD_INITIALIZED	0x10000
#define SDIO_STATUS_CARD_SDHC			0x100000

#define CSD_STRUCTURE				((u8)((__sd0_csd[13]>>6)&0x03))
#define TRAN_SPEED					((u8)(__sd0_csd[8]))
#define CCC							((u16)((__sd0_csd[9]<<4)|((__sd0_csd[10]>>4)&0x0f)))
#define READ_BL_LEN					((u8)(__sd0_csd[10]&0x0f))
#define C_SIZE						((u16)(((__sd0_csd[11]&0x03)<<10)|(__sd0_csd[4]<<2)|((__sd0_csd[5]>>6)&0x03)))
#define C_SIZE_MULT					((u8)(((__sd0_csd[6]&0x03)<<1)|((__sd0_csd[7]>>7)&0x01)))
#define C_SIZE1						((u32)(((__sd0_csd[4]&0x3f)<<16)|(__sd0_csd[5]<<8)|__sd0_csd[6]))
#define C_SIZE2						((u32)(((__sd0_csd[11]&0x0f)<<24)|(__sd0_csd[4]<<16)|(__sd0_csd[5]<<8)|__sd0_csd[6]))
#define WRITE_BL_LEN				((u8)(((__sd0_csd[1]&0x03)<<2)|((__sd0_csd[2]>>6)&0x03)))
#define PERM_WRITE_PROTECT			((u8)((__sd0_csd[3]>>5)&0x01))
#define TMP_WRITE_PROTECT			((u8)((__sd0_csd[3]>>4)&0x01))

struct _sdiorequest
{
	u32 cmd;
	u32 cmd_type;
	u32 rsp_type;
	u32 arg;
	u32 blk_cnt;
	u32 blk_size;
	void *dma_addr;
	u32 isdma;
	u32 pad0;
};

struct _sdioresponse
{
	u32 rsp_fields[3];
	u32 acmd12_response;
};

static s32 __sd0_fd = -1;
static u16 __sd0_rca = 0;
static s32 __sd0_initialized = 0;
static s32 __sd0_sdhc = 0;
static u8 __sd0_csd[16];
static u8 __sd0_cid[16];
static u8 __sd0_functions[64] ATTRIBUTE_ALIGN(32);

static bool __sdio_initialized = false;

static char _sd0_fs[] ATTRIBUTE_ALIGN(32) = "/dev/sdio/slot0";

static s32 __sdio_sendcommand(u32 cmd,u32 cmd_type,u32 rsp_type,u32 arg,u32 blk_cnt,u32 blk_size,void *buffer,void *reply,u32 rlen)
{
	s32 ret;
	STACK_ALIGN(ioctlv,iovec,3,32);
	STACK_ALIGN(struct _sdiorequest,request,1,32);
	STACK_ALIGN(struct _sdioresponse,response,1,32);

	request->cmd = cmd;
	request->cmd_type = cmd_type;
	request->rsp_type = rsp_type;
	request->arg = arg;
	request->blk_cnt = blk_cnt;
	request->blk_size = blk_size;
	request->dma_addr = buffer;
	request->isdma = ((buffer!=NULL)?1:0);
	request->pad0 = 0;

	if(request->isdma || __sd0_sdhc == 1) {
		iovec[0].data = request;
		iovec[0].len = sizeof(struct _sdiorequest);
		iovec[1].data = buffer;
		iovec[1].len = (blk_size*blk_cnt);
		iovec[2].data = response;
		iovec[2].len = sizeof(struct _sdioresponse);
		ret = IOS_Ioctlv(__sd0_fd,IOCTL_SDIO_SENDCMD,2,1,iovec);
	} else
		ret = IOS_Ioctl(__sd0_fd,IOCTL_SDIO_SENDCMD,request,sizeof(struct _sdiorequest),response,sizeof(struct _sdioresponse));

	if(reply && !(rlen>16)) memcpy(reply,response,rlen);

//	printf("  cmd= %08x\n", cmd);

	return ret;
}

static s32 __sdio_setclock(u32 set)
{
	s32 ret;
	STACK_ALIGN(u32,clock,1,32);

	*clock = set;
	ret = IOS_Ioctl(__sd0_fd,IOCTL_SDIO_SETCLK,clock,sizeof(u32),NULL,0);

	return ret;
}

static s32 __sdio_getstatus(void)
{
	s32 ret;
	STACK_ALIGN(u32,status,1,32);

	ret = IOS_Ioctl(__sd0_fd,IOCTL_SDIO_GETSTATUS,NULL,0,status,sizeof(u32));
	if(ret<0) return ret;

	return *status;
}

static s32 __sdio_resetcard(void)
{
	s32 ret;
	STACK_ALIGN(u32,status,1,32);

	__sd0_rca = 0;
	ret = IOS_Ioctl(__sd0_fd,IOCTL_SDIO_RESETCARD,NULL,0,status,sizeof(u32));
	if(ret<0) return ret;

	__sd0_rca = (u16)(*status>>16);
	return (*status&0xffff);
}

static s32 __sdio_gethcr(u8 reg, u8 size, u32 *val)
{
	s32 ret;
	STACK_ALIGN(u32,hcr_value,1,32);
	STACK_ALIGN(u32,hcr_query,6,32);

	if(val==NULL) return IPC_EINVAL;

	*hcr_value = 0;
	*val = 0;
	hcr_query[0] = reg;
	hcr_query[1] = 0;
	hcr_query[2] = 0;
	hcr_query[3] = size;
	hcr_query[4] = 0;
	hcr_query[5] = 0;
	ret = IOS_Ioctl(__sd0_fd,IOCTL_SDIO_READHCREG,(void*)hcr_query,24,hcr_value,sizeof(u32));
	*val = *hcr_value;

	return ret;
}

static s32 __sdio_sethcr(u8 reg, u8 size, u32 data)
{
	s32 ret;
	STACK_ALIGN(u32,hcr_query,6,32);

	hcr_query[0] = reg;
	hcr_query[1] = 0;
	hcr_query[2] = 0;
	hcr_query[3] = size;
	hcr_query[4] = data;
	hcr_query[5] = 0;
	ret = IOS_Ioctl(__sd0_fd,IOCTL_SDIO_WRITEHCREG,(void*)hcr_query,24,NULL,0);

	return ret;
}

static s32 __sdio_waithcr(u8 reg, u8 size, u8 unset, u32 mask)
{
	u32 val;
	s32 ret;
	s32 tries = 10;

	while(tries-- > 0)
	{
		ret = __sdio_gethcr(reg, size, &val);
		if(ret < 0) return ret;
		if((unset && !(val & mask)) || (!unset && (val & mask))) return 0;
		usleep(10000);
	}

	return -1;
}

static s32 __sdio_setbuswidth(u32 bus_width)
{
	s32 ret;
	u32 hc_reg = 0;

	ret = __sdio_gethcr(SDIOHCR_HOSTCONTROL, 1, &hc_reg);
	if(ret<0) return ret;

	hc_reg &= 0xff;
	hc_reg &= ~SDIOHCR_HOSTCONTROL_4BIT;
	if(bus_width==4) hc_reg |= SDIOHCR_HOSTCONTROL_4BIT;

	return __sdio_sethcr(SDIOHCR_HOSTCONTROL, 1, hc_reg);
}

static s32 __sdio_sethighspeed(u32 enable)
{
	s32 ret;
	u32 hc_reg = 0;

	ret = __sdio_gethcr(SDIOHCR_HOSTCONTROL, 1, &hc_reg);
	if(ret<0) return ret;

	hc_reg &= 0xff;
	hc_reg &= ~SDIOHCR_HOSTCONTROL_HS;
	if(enable) hc_reg |= SDIOHCR_HOSTCONTROL_HS;

	return __sdio_sethcr(SDIOHCR_HOSTCONTROL, 1, hc_reg);
}

#if 0
static s32 __sd0_getstatus(void)
{
	s32 ret;
	u32 status = 0;

	ret = __sdio_sendcommand(SDIO_CMD_SENDSTATUS,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,(__sd0_rca<<16),0,0,NULL,&status,sizeof(u32));
	if(ret<0) return ret;

	return status;
}
#endif

static s32 __sd0_getrca(void)
{
	s32 ret;
	u32 rca;

	ret = __sdio_sendcommand(SDIO_CMD_SENDRCA,0,SDIO_RESPONSE_R5,0,0,0,NULL,&rca,sizeof(rca));
	if(ret<0) return ret;

	__sd0_rca = (u16)(rca>>16);
	return (rca&0xffff);
}

static s32 __sd0_select(void)
{
	s32 ret;

	ret = __sdio_sendcommand(SDIO_CMD_SELECT,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1B,(__sd0_rca<<16),0,0,NULL,NULL,0);

	return ret;
}

static s32 __sd0_deselect(void)
{
	s32 ret;

	ret = __sdio_sendcommand(SDIO_CMD_DESELECT,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1B,0,0,0,NULL,NULL,0);

	return ret;
}

static s32 __sd0_setblocklength(u32 blk_len)
{
	s32 ret;

	ret = __sdio_sendcommand(SDIO_CMD_SETBLOCKLEN,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,blk_len,0,0,NULL,NULL,0);

	return ret;
}

static s32 __sd0_setbuswidth(u32 bus_width)
{
	u16 val;
	s32 ret;

	val = 0x0000;
	if(bus_width==4) val = 0x0002;

	ret = __sdio_sendcommand(SDIO_CMD_APPCMD,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,(__sd0_rca<<16),0,0,NULL,NULL,0);
	if(ret<0) return ret;

	ret = __sdio_sendcommand(SDIO_ACMD_SETBUSWIDTH,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,val,0,0,NULL,NULL,0);
	if(ret<0) return ret;

	return __sdio_setbuswidth(bus_width);
}

static s32 __sd0_getcsd(void)
{
	s32 ret;

	ret = __sdio_sendcommand(SDIO_CMD_SENDCSD,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R2,(__sd0_rca<<16),0,0,NULL,__sd0_csd,sizeof(__sd0_csd));

	return ret;
}

static s32 __sd0_getcid(void)
{
	s32 ret;

	ret = __sdio_sendcommand(SDIO_CMD_ALL_SENDCID,0,SDIO_RESPONSE_R2,(__sd0_rca<<16),0,0,NULL,__sd0_cid,sizeof(__sd0_cid));

	return ret;
}

static bool __sd0_initio(DISC_INTERFACE *disc)
{
	s32 ret;
	s32 tries;
	u32 status;
	u32 clock = 1;
	struct _sdioresponse resp;

	__sdio_resetcard();
	status = __sdio_getstatus();

	if(!(status & SDIO_STATUS_CARD_INSERTED))
		return false;

	if(!(status & SDIO_STATUS_CARD_LOCKED))
		disc->features |= FEATURE_MEDIUM_CANWRITE;
	else
		disc->features &= ~FEATURE_MEDIUM_CANWRITE;

	if(!(status & SDIO_STATUS_CARD_INITIALIZED))
	{
		// IOS doesn't like this card, so we need to convice it to accept it.

		// reopen the handle which makes IOS clean stuff up
		IOS_Close(__sd0_fd);
		__sd0_fd = IOS_Open(_sd0_fs,1);

		// reset the host controller
		if(__sdio_sethcr(SDIOHCR_SOFTWARERESET, 1, 7) < 0) goto fail;
		if(__sdio_waithcr(SDIOHCR_SOFTWARERESET, 1, 1, 7) < 0) goto fail;

		// initialize interrupts (sd_reset_card does this on success)
		__sdio_sethcr(0x34, 4, 0x13f00c3);
		__sdio_sethcr(0x38, 4, 0x13f00c3);

		// enable power
		__sd0_sdhc = 1;
		ret = __sdio_sethcr(SDIOHCR_POWERCONTROL, 1, 0xe);
		if(ret < 0) goto fail;
		ret = __sdio_sethcr(SDIOHCR_POWERCONTROL, 1, 0xf);
		if(ret < 0) goto fail;

		// enable internal clock, wait until it gets stable and enable sd clock
		ret = __sdio_sethcr(SDIOHCR_CLOCKCONTROL, 2, 0);
		if(ret < 0) goto fail;
		ret = __sdio_sethcr(SDIOHCR_CLOCKCONTROL, 2, 0x101);
		if(ret < 0) goto fail;
		ret = __sdio_waithcr(SDIOHCR_CLOCKCONTROL, 2, 0, 2);
		if(ret < 0) goto fail;
		ret = __sdio_sethcr(SDIOHCR_CLOCKCONTROL, 2, 0x107);
		if(ret < 0) goto fail;

		// setup timeout
		ret = __sdio_sethcr(SDIOHCR_TIMEOUTCONTROL, 1, SDIO_DEFAULT_TIMEOUT);
		if(ret < 0) goto fail;

		// standard SDHC initialization process
		ret = __sdio_sendcommand(SDIO_CMD_GOIDLE, 0, 0, 0, 0, 0, NULL, NULL, 0);
		if(ret < 0) goto fail;
		ret = __sdio_sendcommand(SDIO_CMD_SENDIFCOND, 0, SDIO_RESPONSE_R6, 0x1aa, 0, 0, NULL, &resp, sizeof(resp));
		if(ret < 0) goto fail;
		if((resp.rsp_fields[0] & 0xff) != 0xaa) goto fail;

		tries = 10;
		while(tries-- > 0)
		{
			ret = __sdio_sendcommand(SDIO_CMD_APPCMD,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,0,0,0,NULL,NULL,0);
			if(ret < 0) goto fail;
			ret = __sdio_sendcommand(SDIO_ACMD_SENDOPCOND, 0, SDIO_RESPONSE_R3, 0x40300000, 0, 0, NULL, &resp, sizeof(resp));
			if(ret < 0) goto fail;
			if(resp.rsp_fields[0] & (1 << 31)) break;

			usleep(10000);
		}
		if(tries < 0) goto fail;

		// FIXME: SDv2 cards which are not high-capacity won't work :/
		if(resp.rsp_fields[0] & (1 << 30))
			__sd0_sdhc = 1;
		else
			__sd0_sdhc = 0;

		ret = __sd0_getcid();
		if(ret < 0) goto fail;
		ret = __sd0_getrca();
		if(ret < 0) goto fail;
	}
	else if(status&SDIO_STATUS_CARD_SDHC)
		__sd0_sdhc = 1;
	else
		__sd0_sdhc = 0;

	ret = __sd0_getcsd();
	if(ret<0) return false;

	ret = __sd0_select();
	if(ret<0) return false;

	ret = __sd0_setblocklength(PAGE_SIZE512);
	if(ret<0) {
		__sd0_deselect();
		return false;
	}

	ret = __sd0_setbuswidth(4);
	if(ret<0) {
		__sd0_deselect();
		return false;
	}

	if(CCC & (1 << 10)) {
		ret = __sdio_sendcommand(SDIO_CMD_SWITCHFUNC, 1, SDIO_RESPONSE_R1, 0x00fffff0, 1, sizeof(__sd0_functions), __sd0_functions, NULL, 0);
		if(ret<0) {
			__sd0_deselect();
			return false;
		}

		if(((u16*)__sd0_functions)[6] & (1 << 1)) {
			ret = __sdio_sendcommand(SDIO_CMD_SWITCHFUNC, 1, SDIO_RESPONSE_R1, 0x80fffff1, 1, sizeof(__sd0_functions), __sd0_functions, NULL, 0);
			if(ret<0) {
				__sd0_deselect();
				return false;
			}

			if((__sd0_functions[16] & 0xf) == 1) {
				ret = __sdio_sethighspeed(1);
				if(ret<0) {
					__sd0_deselect();
					return false;
				}
				clock = 0;
			}
		}
	}

	ret = __sdio_setclock(clock);
	if(ret<0) {
		__sd0_deselect();
		return false;
	}

	if(PERM_WRITE_PROTECT || TMP_WRITE_PROTECT)
		disc->features &= ~FEATURE_MEDIUM_CANWRITE;

	switch(CSD_STRUCTURE) {
		case 0:
			disc->numberOfSectors = ((C_SIZE + 1) << (C_SIZE_MULT + 2)) << (READ_BL_LEN - 9);
			break;
		case 1:
			disc->numberOfSectors = (C_SIZE1 + 1LL) << 10;
			break;
		case 2:
			disc->numberOfSectors = (C_SIZE2 + 1LL) << 10;
			break;
		default:
			disc->numberOfSectors = ~0;
			break;
	}
	__sd0_deselect();
	__sd0_initialized = 1;
	return true;

	fail:
	__sdio_sethcr(SDIOHCR_SOFTWARERESET, 1, 7);
	__sdio_waithcr(SDIOHCR_SOFTWARERESET, 1, 1, 7);
	IOS_Close(__sd0_fd);
	__sd0_fd = IOS_Open(_sd0_fs,1);
	return false;
}

static bool sdio_Deinitialize(void)
{
	if(__sd0_fd>=0)
		IOS_Close(__sd0_fd);

	__sd0_fd = -1;
	__sd0_initialized = 0;
	return true;
}

static bool sdio_Startup(DISC_INTERFACE *disc)
{
	if(disc->ioType != DEVICE_TYPE_WII_SD) return false;
	if(__sdio_initialized) return true;

	__sd0_fd = IOS_Open(_sd0_fs,1);

	if(__sd0_fd<0) {
		sdio_Deinitialize();
		return false;
	}

	if(__sd0_initio(disc)==false) {
		sdio_Deinitialize();
		return false;
	}
	__sdio_initialized = true;
	return true;
}

static bool sdio_Shutdown(DISC_INTERFACE *disc)
{
	if(disc->ioType != DEVICE_TYPE_WII_SD) return false;
	if(!__sdio_initialized) return true;

	sdio_Deinitialize();

	__sdio_initialized = false;
	return true;
}

static bool sdio_ReadSectors(DISC_INTERFACE *disc, sec_t sector, sec_t numSectors, void *buffer)
{
	s32 ret;
	u32 sec;
	u32 secs_to_read;
	u8 *dest = (u8*)buffer;

	if(disc->ioType != DEVICE_TYPE_WII_SD) return false;
	if(!(disc->features & FEATURE_MEDIUM_CANREAD)) return false;
	if((sector + numSectors) < sector) return false;
	if((sector + numSectors) > disc->numberOfSectors) return false;
	if(disc->bytesPerSector != PAGE_SIZE512) return false;
	if(!SYS_IsDMAAddress(buffer, 1)) return false;
	if(!__sdio_initialized) return false;

	ret = __sd0_select();
	if(ret<0) return false;

	while(numSectors>0) {
		if(__sd0_sdhc == 0) sec = (sector*PAGE_SIZE512);
		else sec = sector;

		if(numSectors>0xffff) secs_to_read = 0xffff;
		else secs_to_read = numSectors;

		ret = __sdio_sendcommand(SDIO_CMD_READMULTIBLOCK,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,sec,secs_to_read,PAGE_SIZE512,dest,NULL,0);
		if(ret<0) break;

		dest += (secs_to_read*PAGE_SIZE512);
		sector += secs_to_read;
		numSectors -= secs_to_read;
	}

	__sd0_deselect();
	return (ret>=0);
}

static bool sdio_WriteSectors(DISC_INTERFACE *disc, sec_t sector, sec_t numSectors, const void *buffer)
{
	s32 ret;
	u32 sec;
	u32 secs_to_write;
	u8 *src = (u8*)buffer;

	if(disc->ioType != DEVICE_TYPE_WII_SD) return false;
	if(!(disc->features & FEATURE_MEDIUM_CANWRITE)) return false;
	if((sector + numSectors) < sector) return false;
	if((sector + numSectors) > disc->numberOfSectors) return false;
	if(disc->bytesPerSector != PAGE_SIZE512) return false;
	if(!SYS_IsDMAAddress(buffer, 1)) return false;
	if(!__sdio_initialized) return false;

	ret = __sd0_select();
	if(ret<0) return false;

	while(numSectors>0) {
		if(__sd0_sdhc == 0) sec = (sector*PAGE_SIZE512);
		else sec = sector;

		if(numSectors>0xffff) secs_to_write = 0xffff;
		else secs_to_write = numSectors;

		ret = __sdio_sendcommand(SDIO_CMD_WRITEMULTIBLOCK,SDIOCMD_TYPE_AC,SDIO_RESPONSE_R1,sec,secs_to_write,PAGE_SIZE512,src,NULL,0);
		if(ret<0) break;

		src += (secs_to_write*PAGE_SIZE512);
		sector += secs_to_write;
		numSectors -= secs_to_write;
	}

	__sd0_deselect();
	return (ret>=0);
}

static bool sdio_ClearStatus(DISC_INTERFACE *disc)
{
	return true;
}

static bool sdio_IsInserted(DISC_INTERFACE *disc)
{
	if(disc->ioType != DEVICE_TYPE_WII_SD) return false;
	if(!__sdio_initialized) return false;

	return ((__sdio_getstatus() & SDIO_STATUS_CARD_INSERTED) ==
			SDIO_STATUS_CARD_INSERTED);
}

#if 0
static bool sdio_IsInitialized(void)
{
	return ((__sdio_getstatus() & SDIO_STATUS_CARD_INITIALIZED) ==
			SDIO_STATUS_CARD_INITIALIZED);
}
#endif

DISC_INTERFACE __io_wiisd = {
	DEVICE_TYPE_WII_SD,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_SD,
	sdio_Startup,
	sdio_IsInserted,
	sdio_ReadSectors,
	sdio_WriteSectors,
	sdio_ClearStatus,
	sdio_Shutdown,
	0,
	PAGE_SIZE512
};

#endif
