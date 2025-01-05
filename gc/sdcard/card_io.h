#ifndef __CARD_IO_H__
#define __CARD_IO_H__

#include <gctypes.h>

#define MAX_MI_NUM							1
#define MAX_DI_NUM							5

#define PAGE_SIZE256						256
#define PAGE_SIZE512						512

/* CID Register */
#define MANUFACTURER_ID(drv_no)				((u8)(g_CID[drv_no][0]))
#define OEM_APPLICATION_ID(drv_no)			((char[2]){g_CID[drv_no][1],g_CID[drv_no][2]})
#define PRODUCT_NAME(drv_no)				((char[5]){g_CID[drv_no][3],g_CID[drv_no][4],g_CID[drv_no][5],g_CID[drv_no][6],g_CID[drv_no][7]})
#define PRODUCT_REVISION(drv_no)			((u8)(g_CID[drv_no][8]))
#define PRODUCT_SERIAL_NUMBER(drv_no)		((u32)((g_CID[drv_no][9]<<24)|(g_CID[drv_no][10]<<16)|(g_CID[drv_no][11]<<8)|g_CID[drv_no][12]))
#define MANUFACTURING_DATE(drv_no)			((u16)(((g_CID[drv_no][13]&0x0f)<<8)|g_CID[drv_no][14]))

/* CSD Register */
#define CSD_STRUCTURE(drv_no)				((u8)((g_CSD[drv_no][0]>>6)&0x03))
#define TRAN_SPEED(drv_no)					((u8)(g_CSD[drv_no][3]))
#define CCC(drv_no)							((u16)((g_CSD[drv_no][4]<<4)|((g_CSD[drv_no][5]>>4)&0x0f)))
#define READ_BL_LEN(drv_no)					((u8)(g_CSD[drv_no][5]&0x0f))
#define C_SIZE(drv_no)						((u16)(((g_CSD[drv_no][6]&0x03)<<10)|(g_CSD[drv_no][7]<<2)|((g_CSD[drv_no][8]>>6)&0x03)))
#define C_SIZE_MULT(drv_no)					((u8)(((g_CSD[drv_no][9]&0x03)<<1)|((g_CSD[drv_no][10]>>7)&0x01)))
#define C_SIZE1(drv_no)						((u32)(((g_CSD[drv_no][7]&0x3f)<<16)|(g_CSD[drv_no][8]<<8)|g_CSD[drv_no][9]))
#define C_SIZE2(drv_no)						((u32)(((g_CSD[drv_no][6]&0x0f)<<24)|(g_CSD[drv_no][7]<<16)|(g_CSD[drv_no][8]<<8)|g_CSD[drv_no][9]))
#define WRITE_BL_LEN(drv_no)				((u8)(((g_CSD[drv_no][12]&0x03)<<2)|((g_CSD[drv_no][13]>>6)&0x03)))
#define PERM_WRITE_PROTECT(drv_no)			((u8)((g_CSD[drv_no][14]>>5)&0x01))
#define TMP_WRITE_PROTECT(drv_no)			((u8)((g_CSD[drv_no][14]>>4)&0x01))

/* SD Status */
#define DAT_BUS_WIDTH(drv_no)				((u8)((g_CardStatus[drv_no][0]>>6)&0x03))
#define SECURED_MODE(drv_no)				((u8)((g_CardStatus[drv_no][0]>>5)&0x01))
#define SECURE_CMD_STATUS(drv_no)			((u8)(g_CardStatus[drv_no][1]&0x07))
#define SD_CARD_TYPE(drv_no)				((u16)((g_CardStatus[drv_no][2]<<8)|g_CardStatus[drv_no][3]))
#define SIZE_OF_PROTECTED_AREA(drv_no)		((u32)((g_CardStatus[drv_no][4]<<24)|(g_CardStatus[drv_no][5]<<16)|(g_CardStatus[drv_no][6]<<8)|g_CardStatus[drv_no][7]))
#define SPEED_CLASS(drv_no)					((u8)(g_CardStatus[drv_no][8]))
#define PERFORMANCE_MOVE(drv_no)			((u8)(g_CardStatus[drv_no][9]))
#define AU_SIZE(drv_no)						((u8)((g_CardStatus[drv_no][10]>>4)&0x0f))
#define ERASE_SIZE(drv_no)					((u16)((g_CardStatus[drv_no][11]<<8)|g_CardStatus[drv_no][12]))
#define ERASE_TIMEOUT(drv_no)				((u8)((g_CardStatus[drv_no][13]>>2)&0x3f))
#define ERASE_OFFSET(drv_no)				((u8)(g_CardStatus[drv_no][13]&0x03))
#define UHS_SPEED_GRADE(drv_no)				((u8)((g_CardStatus[drv_no][14]>>4)&0x0f))
#define UHS_AU_SIZE(drv_no)					((u8)(g_CardStatus[drv_no][14]&0x0f))
#define VIDEO_SPEED_CLASS(drv_no)			((u8)(g_CardStatus[drv_no][15]))
#define VSC_AU_SIZE(drv_no)					((u16)(((g_CardStatus[drv_no][16]&0x03)<<8)|g_CardStatus[drv_no][17]))
#define SUS_ADDR(drv_no)					((u32)((g_CardStatus[drv_no][18]<<14)|(g_CardStatus[drv_no][19]<<6)|((g_CardStatus[drv_no][20]>>2)&0x3f)))
#define APP_PERF_CLASS(drv_no)				((u8)(g_CardStatus[drv_no][21]&0x0f))
#define PERFORMANCE_ENHANCE(drv_no)			((u8)(g_CardStatus[drv_no][22]))
#define WP_UPC_SUPPORT(drv_no)				((u8)((g_CardStatus[drv_no][24]>>3)&0x01))
#define BOOT_PARTITION_SUPPORT(drv_no)		((u8)((g_CardStatus[drv_no][24]>>2)&0x01))
#define DISCARD_SUPPORT(drv_no)				((u8)((g_CardStatus[drv_no][24]>>1)&0x01))
#define FULE_SUPPORT(drv_no)				((u8)(g_CardStatus[drv_no][24]&0x01))

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

enum {
	CARDIO_ADDRESSING_BYTE = 0,
	CARDIO_ADDRESSING_BLOCK
};

enum {
	CARDIO_TRANSFER_IMM = 0,
	CARDIO_TRANSFER_DMA
};

extern u8 g_CSD[MAX_DRIVE][16];
extern u8 g_CID[MAX_DRIVE][16];
extern u8 g_CardStatus[MAX_DRIVE][64];
extern u8 g_mCode[MAX_MI_NUM];
extern u16 g_dCode[MAX_MI_NUM][MAX_DI_NUM];


void sdgecko_initIODefault(void);
s32 sdgecko_initIO(s32 drv_no);
s32 sdgecko_preIO(s32 drv_no);
s32 sdgecko_enableCRC(s32 drv_no,bool enable);
s32 sdgecko_readCID(s32 drv_no);
s32 sdgecko_readCSD(s32 drv_no);
s32 sdgecko_readStatus(s32 drv_no);
s32 sdgecko_readSector(s32 drv_no,void *buf,u32 sector_no);
s32 sdgecko_readSectors(s32 drv_no,u32 sector_no,u32 num_sectors,void *buf);
s32 sdgecko_writeSector(s32 drv_no,const void *buf,u32 sector_no);
s32 sdgecko_writeSectors(s32 drv_no,u32 sector_no,u32 num_sectors,const void *buf);

s32 sdgecko_doUnmount(s32 drv_no);

void sdgecko_insertedCB(s32 drv_no);
void sdgecko_ejectedCB(s32 drv_no);

u32 sdgecko_getDevice(s32 drv_no);
void sdgecko_setDevice(s32 drv_no, u32 dev);

u32 sdgecko_getSpeed(s32 drv_no);
void sdgecko_setSpeed(s32 drv_no, u32 freq);

u32 sdgecko_getPageSize(s32 drv_no);
s32 sdgecko_setPageSize(s32 drv_no, u32 size);

u32 sdgecko_getAddressingType(s32 drv_no);
u32 sdgecko_getTransferMode(s32 drv_no);

bool sdgecko_isInserted(s32 drv_no);
bool sdgecko_isInitialized(s32 drv_no);


#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
