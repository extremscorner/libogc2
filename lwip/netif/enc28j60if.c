/*-------------------------------------------------------------

enc28j60if.c -- ENC28J60 device driver

Copyright (C) 2023 Extrems

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

#include <gctypes.h>
#include <ogc/exi.h>
#include <ogc/irq.h>
#include <ogc/lwp.h>
#include <ogc/machine/processor.h>
#include <ogc/semaphore.h>
#include <sdcard/card_cmn.h>
#include <sdcard/card_io.h>
#include <unistd.h>
#include "lwip/debug.h"
#include "lwip/err.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "netif/enc28j60if.h"
#include "netif/etharp.h"

static vu32 *const _piReg = (u32 *)0xCC003000;

#define ENC28J60_CMD_RCR(x) ((0x00 | ((x) & 0x1F)) << 24) // Read Control Register
#define ENC28J60_CMD_RBM    ((0x3A) << 24)                // Read Buffer Memory
#define ENC28J60_CMD_WCR(x) ((0x40 | ((x) & 0x1F)) << 24) // Write Control Register
#define ENC28J60_CMD_WBM    ((0x7A) << 24)                // Write Buffer Memory
#define ENC28J60_CMD_BFS(x) ((0x80 | ((x) & 0x1F)) << 24) // Bit Field Set
#define ENC28J60_CMD_BFC(x) ((0xA0 | ((x) & 0x1F)) << 24) // Bit Field Clear
#define ENC28J60_CMD_SRC    ((0xFF) << 24)                // System Reset Command

#define ENC28J60_EXI_SPEED(cmd) (((cmd) == ENC28J60_CMD_SRC) ? EXI_SPEED1MHZ : EXI_SPEED32MHZ)
#define ENC28J60_EXI_DUMMY(cmd) ((((cmd) >> 24) & 0x1F) < 0x1A)

#define ENC28J60_INIT_ERXST (0)
#define ENC28J60_INIT_ERXND (4096 - 1)
#define ENC28J60_INIT_ETXST (4096)
#define ENC28J60_INIT_ETXND (8192 - 1)

typedef enum {
	ENC28J60_ERDPTL   = 0x00, // Read Pointer Low Byte
	ENC28J60_ERDPTH,          // Read Pointer High Byte
	ENC28J60_EWRPTL,          // Write Pointer Low Byte
	ENC28J60_EWRPTH,          // Write Pointer High Byte
	ENC28J60_ETXSTL,          // TX Start Low Byte
	ENC28J60_ETXSTH,          // TX Start High Byte
	ENC28J60_ETXNDL,          // TX End Low Byte
	ENC28J60_ETXNDH,          // TX End High Byte
	ENC28J60_ERXSTL,          // RX Start Low Byte
	ENC28J60_ERXSTH,          // RX Start High Byte
	ENC28J60_ERXNDL,          // RX End Low Byte
	ENC28J60_ERXNDH,          // RX End High Byte
	ENC28J60_ERXRDPTL,        // RX RD Pointer Low Byte
	ENC28J60_ERXRDPTH,        // RX RD Pointer High Byte
	ENC28J60_ERXWRPTL,        // RX WR Pointer Low Byte
	ENC28J60_ERXWRPTH,        // RX WR Pointer High Byte
	ENC28J60_EDMASTL,         // DMA Start Low Byte
	ENC28J60_EDMASTH,         // DMA Start High Byte
	ENC28J60_EDMANDL,         // DMA End Low Byte
	ENC28J60_EDMANDH,         // DMA End High Byte
	ENC28J60_EDMADSTL,        // DMA Destination Low Byte
	ENC28J60_EDMADSTH,        // DMA Destination High Byte
	ENC28J60_EDMACSL,         // DMA Checksum Low Byte
	ENC28J60_EDMACSH,         // DMA Checksum High Byte

	ENC28J60_EHT0     = 0x20, // Hash Table Byte 0
	ENC28J60_EHT1,            // Hash Table Byte 1
	ENC28J60_EHT2,            // Hash Table Byte 2
	ENC28J60_EHT3,            // Hash Table Byte 3
	ENC28J60_EHT4,            // Hash Table Byte 4
	ENC28J60_EHT5,            // Hash Table Byte 5
	ENC28J60_ETH6,            // Hash Table Byte 6
	ENC28J60_ETH7,            // Hash Table Byte 7
	ENC28J60_EPMM0,           // Pattern Match Mask Byte 0
	ENC28J60_EPMM1,           // Pattern Match Mask Byte 1
	ENC28J60_EPMM2,           // Pattern Match Mask Byte 2
	ENC28J60_EPMM3,           // Pattern Match Mask Byte 3
	ENC28J60_EPMM4,           // Pattern Match Mask Byte 4
	ENC28J60_EPMM5,           // Pattern Match Mask Byte 5
	ENC28J60_EPMM6,           // Pattern Match Mask Byte 6
	ENC28J60_EPMM7,           // Pattern Match Mask Byte 7
	ENC28J60_EPMCSL,          // Pattern Match Checksum Low Byte
	ENC28J60_EPMCSH,          // Pattern Match Checksum High Byte
	ENC28J60_EPMOL    = 0x34, // Pattern Match Offset Low Byte
	ENC28J60_EPMOH,           // Pattern Match Offset High Byte
	ENC28J60_ERXFCON  = 0x38, // Ethernet Receive Filter Control Register
	ENC28J60_EPKTCNT,         // Ethernet Packet Count

	ENC28J60_MACON1   = 0x40, // MAC Control Register 1
	ENC28J60_MACON3   = 0x42, // MAC Control Register 3
	ENC28J60_MACON4,          // MAC Control Register 4
	ENC28J60_MABBIPG,         // Back-to-Back Inter-Packet Gap
	ENC28J60_MAIPGL   = 0x46, // Non-Back-to-Back Inter-Packet Gap Low Byte
	ENC28J60_MAIPGH,          // Non-Back-to-Back Inter-Packet Gap High Byte
	ENC28J60_MACLCON1,        // Retransmission Maximum
	ENC28J60_MACLCON2,        // Collision Window
	ENC28J60_MAMXFLL,         // Maximum Frame Length Low Byte
	ENC28J60_MAMXFLH,         // Maximum Frame Length High Byte
	ENC28J60_MICMD    = 0x52, // MII Command Register
	ENC28J60_MIREGADR = 0x54, // MII Register Address
	ENC28J60_MIWRL    = 0x56, // MII Write Data Low Byte
	ENC28J60_MIWRH,           // MII Write Data High Byte
	ENC28J60_MIRDL,           // MII Read Data Low Byte
	ENC28J60_MIRDH,           // MII Read Data High Byte

	ENC28J60_MAADR5   = 0x60, // MAC Address Byte 5
	ENC28J60_MAADR6,          // MAC Address Byte 6
	ENC28J60_MAADR3,          // MAC Address Byte 3
	ENC28J60_MAADR4,          // MAC Address Byte 4
	ENC28J60_MAADR1,          // MAC Address Byte 1
	ENC28J60_MAADR2,          // MAC Address Byte 2
	ENC28J60_EBSTSD,          // Built-in Self-Test Fill Seed
	ENC28J60_EBSTCON,         // Ethernet Self-Test Control Register
	ENC28J60_EBSTCSL,         // Built-in Self-Test Checksum Low Byte
	ENC28J60_EBSTCSH,         // Built-in Self-Test Checksum High Byte
	ENC28J60_MISTAT,          // MII Status Register
	ENC28J60_EREVID   = 0x72, // Ethernet Revision ID
	ENC28J60_ECOCON   = 0x75, // Clock Output Control Register
	ENC28J60_EFLOCON  = 0x77, // Ethernet Flow Control Register
	ENC28J60_EPAUSL,          // Pause Timer Value Low Byte
	ENC28J60_EPAUSH,          // Pause Timer Value High Byte

	ENC28J60_EIE      = 0xFB, // Ethernet Interrupt Enable Register
	ENC28J60_EIR,             // Ethernet Interrupt Request Register
	ENC28J60_ESTAT,           // Ethernet Status Register
	ENC28J60_ECON2,           // Ethernet Control Register 2
	ENC28J60_ECON1,           // Ethernet Control Register 1
} ENC28J60Reg;

typedef enum {
	ENC28J60_ERDPT    = 0x00, // Read Pointer
	ENC28J60_EWRPT    = 0x02, // Write Pointer
	ENC28J60_ETXST    = 0x04, // TX Start
	ENC28J60_ETXND    = 0x06, // TX End
	ENC28J60_ERXST    = 0x08, // RX Start
	ENC28J60_ERXND    = 0x0A, // RX End
	ENC28J60_ERXRDPT  = 0x0C, // RX RD Pointer
	ENC28J60_ERXWRPT  = 0x0E, // RX WR Pointer
	ENC28J60_EDMAST   = 0x10, // DMA Start
	ENC28J60_EDMAND   = 0x12, // DMA End
	ENC28J60_EDMADST  = 0x14, // DMA Destination
	ENC28J60_EDMACS   = 0x16, // DMA Checksum

	ENC28J60_EPMCS    = 0x30, // Pattern Match Checksum
	ENC28J60_EPMO     = 0x34, // Pattern Match Offset

	ENC28J60_MAIPG    = 0x46, // Non-Back-to-Back Inter-Packet Gap
	ENC28J60_MAMXFL   = 0x4A, // Maximum Frame Length
	ENC28J60_MIWR     = 0x56, // MII Write Data
	ENC28J60_MIRD     = 0x58, // MII Read Data

	ENC28J60_EBSTCS   = 0x68, // Built-in Self-Test Checksum
	ENC28J60_EPAUS    = 0x78, // Pause Timer Value
} ENC28J60Reg16;

#define ENC28J60_ERXFCON_UCEN               (1 << 7) // Unicast Filter Enable
#define ENC28J60_ERXFCON_ANDOR              (1 << 6) // AND/OR Filter Select
#define ENC28J60_ERXFCON_CRCEN              (1 << 5) // Post-Filter CRC Check Enable
#define ENC28J60_ERXFCON_PMEN               (1 << 4) // Pattern Match Filter Enable
#define ENC28J60_ERXFCON_MPEN               (1 << 3) // Magic Packet Filter Enable
#define ENC28J60_ERXFCON_HTEN               (1 << 2) // Hash Table Filter Enable
#define ENC28J60_ERXFCON_MCEN               (1 << 1) // Multicast Filter Enable
#define ENC28J60_ERXFCON_BCEN               (1 << 0) // Broadcast Filter Enable

#define ENC28J60_MACON1_TXPAUS              (1 << 3) // Pause Control Frame Transmission Enable
#define ENC28J60_MACON1_RXPAUS              (1 << 2) // Pause Control Frame Reception Enable
#define ENC28J60_MACON1_PASSALL             (1 << 1) // Pass All Received Frames Enable
#define ENC28J60_MACON1_MARXEN              (1 << 0) // MAC Receive Enable

#define ENC28J60_MACON3_PADCFG(x) (((x) & 0x7) << 5) // Automatic Pad and CRC Configuration
#define ENC28J60_MACON3_TXCRCEN             (1 << 4) // Transmit CRC Enable
#define ENC28J60_MACON3_PHDREN              (1 << 3) // Proprietary Header Enable
#define ENC28J60_MACON3_HFRMEN              (1 << 2) // Huge Frame Enable
#define ENC28J60_MACON3_FRMLNEN             (1 << 1) // Frame Length Checking Enable
#define ENC28J60_MACON3_FULDPX              (1 << 0) // MAC Full-Duplex Enable

#define ENC28J60_MACON4_DEFER               (1 << 6) // Defer Transmission Enable
#define ENC28J60_MACON4_BPEN                (1 << 5) // No Backoff During Backpressure Enable
#define ENC28J60_MACON4_NOBKOFF             (1 << 4) // No Backoff Enable

#define ENC28J60_MICMD_MIISCAN              (1 << 1) // MII Scan Enable
#define ENC28J60_MICMD_MIIRD                (1 << 0) // MII Read Enable

#define ENC28J60_EBSTCON_PSV(x)   (((x) & 0x7) << 5) // Pattern Shift Value
#define ENC28J60_EBSTCON_PSEL               (1 << 4) // Port Select
#define ENC28J60_EBSTCON_TMSEL(x) (((x) & 0x3) << 2) // Test Mode Select
#define ENC28J60_EBSTCON_TME                (1 << 1) // Test Mode Enable
#define ENC28J60_EBSTCON_BISTST             (1 << 0) // Built-in Self-Test Start/Busy

#define ENC28J60_MISTAT_NVALID              (1 << 2) // MII Management Read Data Not Valid
#define ENC28J60_MISTAT_SCAN                (1 << 1) // MII Management Scan Operation
#define ENC28J60_MISTAT_BUSY                (1 << 0) // MII Management Busy

#define ENC28J60_EFLOCON_FULDPXS            (1 << 2) // Read-Only MAC Full-Duplex Shadow
#define ENC28J60_EFLOCON_FCEN(x)  (((x) & 0x3) << 0) // Flow Control Enable

#define ENC28J60_EIE_INTIE                  (1 << 7) // Global INT Interrupt Enable
#define ENC28J60_EIE_PKTIE                  (1 << 6) // Receive Packet Pending Interrupt Enable
#define ENC28J60_EIE_DMAIE                  (1 << 5) // DMA Interrupt Enable
#define ENC28J60_EIE_LINKIE                 (1 << 4) // Link Status Change Interrupt Enable
#define ENC28J60_EIE_TXIE                   (1 << 3) // Transmit Interrupt Enable
#define ENC28J60_EIE_TXERIE                 (1 << 1) // Transmit Error Interrupt Enable
#define ENC28J60_EIE_RXERIE                 (1 << 0) // Receive Error Interrupt Enable

#define ENC28J60_EIR_PKTIF                  (1 << 6) // Receive Packet Pending Interrupt Flag
#define ENC28J60_EIR_DMAIF                  (1 << 5) // DMA Interrupt Flag
#define ENC28J60_EIR_LINKIF                 (1 << 4) // Link Status Change Interrupt Flag
#define ENC28J60_EIR_TXIF                   (1 << 3) // Transmit Interrupt Flag
#define ENC28J60_EIR_TXERIF                 (1 << 1) // Transmit Error Interrupt Flag
#define ENC28J60_EIR_RXERIF                 (1 << 0) // Receive Error Interrupt Flag

#define ENC28J60_ESTAT_INT                  (1 << 7) // INT Interrupt Flag
#define ENC28J60_ESTAT_BUFER                (1 << 6) // Ethernet Buffer Error
#define ENC28J60_ESTAT_LATECOL              (1 << 4) // Late Collision Error
#define ENC28J60_ESTAT_RXBUSY               (1 << 2) // Receive Busy
#define ENC28J60_ESTAT_TXABRT               (1 << 1) // Transmit Abort Error
#define ENC28J60_ESTAT_CLKRDY               (1 << 0) // Clock Ready

#define ENC28J60_ECON2_AUTOINC              (1 << 7) // Automatic Buffer Pointer Increment Enable
#define ENC28J60_ECON2_PKTDEC               (1 << 6) // Packet Decrement
#define ENC28J60_ECON2_PWRSV                (1 << 5) // Power Save Enable
#define ENC28J60_ECON2_VRPS                 (1 << 3) // Voltage Regulator Power Save Enable

#define ENC28J60_ECON1_TXRST                (1 << 7) // Transmit Logic Reset
#define ENC28J60_ECON1_RXRST                (1 << 6) // Receive Logic Reset
#define ENC28J60_ECON1_DMAST                (1 << 5) // DMA Start and Busy Status
#define ENC28J60_ECON1_CSUMEN               (1 << 4) // DMA Checksum Enable
#define ENC28J60_ECON1_TXRTS                (1 << 3) // Transmit Request to Send
#define ENC28J60_ECON1_RXEN                 (1 << 2) // Receive Enable
#define ENC28J60_ECON1_BSEL(x)    (((x) & 0x3) << 0) // Bank Select

typedef enum {
	ENC28J60_PHCON1   = 0x00, // PHY Control Register 1
	ENC28J60_PHSTAT1,         // Physical Layer Status Register 1
	ENC28J60_PHID1,           // PHY Identifier Register 1
	ENC28J60_PHID2,           // PHY Identifier Register 2
	ENC28J60_PHCON2   = 0x10, // PHY Control Register 2
	ENC28J60_PHSTAT2,         // Physical Layer Status Register 2
	ENC28J60_PHIE,            // PHY Interrupt Enable Register
	ENC28J60_PHIR,            // PHY Interrupt Request Register
	ENC28J60_PHLCON,          // PHY Module LED Control Register
} ENC28J60PHYReg;

#define ENC28J60_PHCON1_PRST               (1 << 15) // PHY Software Reset
#define ENC28J60_PHCON1_PLOOPBK            (1 << 14) // PHY Loopback
#define ENC28J60_PHCON1_PPWRSV             (1 << 11) // PHY Power-Down
#define ENC28J60_PHCON1_PDPXMD             (1 <<  8) // PHY Duplex Mode

#define ENC28J60_PHSTAT1_PFDPX             (1 << 12) // PHY Full-Duplex Capable
#define ENC28J60_PHSTAT1_PHDPX             (1 << 11) // PHY Half-Duplex Capable
#define ENC28J60_PHSTAT1_LLSTAT            (1 <<  2) // PHY Latching Link Status
#define ENC28J60_PHSTAT1_JBSTAT            (1 <<  1) // PHY Latching Jabber Status

#define ENC28J60_PHCON2_FRCLNK             (1 << 14) // PHY Force Linkup
#define ENC28J60_PHCON2_TXDIS              (1 << 13) // Twisted-Pair Transmitter Disable
#define ENC28J60_PHCON2_JABBER             (1 << 10) // Jabber Correction Disable
#define ENC28J60_PHCON2_HDLDIS             (1 <<  8) // PHY Half-Duplex Loopback Disable

#define ENC28J60_PHSTAT2_TXSTAT            (1 << 13) // PHY Transmit Status
#define ENC28J60_PHSTAT2_RXSTAT            (1 << 12) // PHY Receive Status
#define ENC28J60_PHSTAT2_COLSTAT           (1 << 11) // PHY Collision Status
#define ENC28J60_PHSTAT2_LSTAT             (1 << 10) // PHY Link Status
#define ENC28J60_PHSTAT2_DPXSTAT           (1 <<  9) // PHY Duplex Status
#define ENC28J60_PHSTAT2_PLRITY            (1 <<  5) // Polarity Status

#define ENC28J60_PHIE_PLNKIE               (1 <<  4) // PHY Link Change Interrupt Enable
#define ENC28J60_PHIE_PGEIE                (1 <<  1) // PHY Global Interrupt Enable

#define ENC28J60_PHIR_PLNKIF               (1 <<  4) // PHY Link Change Interrupt Flag
#define ENC28J60_PHIR_PGIF                 (1 <<  2) // PHY Global Interrupt Flag

#define ENC28J60_PHLCON_LACFG(x) (((x) & 0xF) <<  8) // LEDA Configuration
#define ENC28J60_PHLCON_LBCFG(x) (((x) & 0xF) <<  4) // LEDB Configuration
#define ENC28J60_PHLCON_LFRQ(x)  (((x) & 0x3) <<  2) // LED Pulse Stretch Time Configuration
#define ENC28J60_PHLCON_STRCH              (1 <<  1) // LED Pulse Stretching Enable

struct enc28j60if {
	s32 chan;
	lwpq_t threadQueue;
	sem_t txSemaphore;
	struct eth_addr *ethaddr;
	u16 nextPacket;
};

static struct netif *enc28j60_netif;
static u8 CurrBank[EXI_CHANNEL_MAX];

static bool ENC28J60_ReadCmd(s32 chan, u32 cmd, void *buf, u32 len)
{
	bool err = false;

	if (!EXI_Select(chan, EXI_DEVICE_0, ENC28J60_EXI_SPEED(cmd)))
		return false;

	err |= !EXI_Imm(chan, &cmd, 1 + ENC28J60_EXI_DUMMY(cmd), EXI_WRITE, NULL);
	err |= !EXI_Sync(chan);
	err |= !EXI_DmaEx(chan, buf, len, EXI_READ);
	err |= !EXI_Deselect(chan);
	return !err;
}

static bool ENC28J60_WriteCmd(s32 chan, u32 cmd, const void *buf, u32 len)
{
	bool err = false;

	if (!EXI_Select(chan, EXI_DEVICE_0, ENC28J60_EXI_SPEED(cmd)))
		return false;

	err |= !EXI_Imm(chan, &cmd, 1, EXI_WRITE, NULL);
	err |= !EXI_Sync(chan);
	err |= !EXI_DmaEx(chan, (void *)buf, len, EXI_WRITE);
	err |= !EXI_Deselect(chan);
	return !err;
}

static bool ENC28J60_SetBits(s32 chan, ENC28J60Reg addr, u8 data)
{
	return ENC28J60_WriteCmd(chan, ENC28J60_CMD_BFS(addr), &data, 1);
}

static bool ENC28J60_ClearBits(s32 chan, ENC28J60Reg addr, u8 data)
{
	return ENC28J60_WriteCmd(chan, ENC28J60_CMD_BFC(addr), &data, 1);
}

static bool ENC28J60_Reset(s32 chan)
{
	return ENC28J60_WriteCmd(chan, ENC28J60_CMD_SRC, NULL, 0);
}

static bool ENC28J60_SelectBank(s32 chan, u8 addr)
{
	u8 bank = addr >> 5;

	if (CurrBank[chan] != bank && bank < 4) {
		if (CurrBank[chan] && !ENC28J60_ClearBits(chan, ENC28J60_ECON1, ENC28J60_ECON1_BSEL(CurrBank[chan])))
			return false;
		if (bank && !ENC28J60_SetBits(chan, ENC28J60_ECON1, ENC28J60_ECON1_BSEL(bank)))
			return false;

		CurrBank[chan] = bank;
	}

	return true;
}

static bool ENC28J60_ReadReg(s32 chan, ENC28J60Reg addr, u8 *data)
{
	if (!ENC28J60_SelectBank(chan, addr) ||
		!ENC28J60_ReadCmd(chan, ENC28J60_CMD_RCR(addr), data, 1))
		return false;

	return true;
}

static bool ENC28J60_WriteReg(s32 chan, ENC28J60Reg addr, u8 data)
{
	if (!ENC28J60_SelectBank(chan, addr) ||
		!ENC28J60_WriteCmd(chan, ENC28J60_CMD_WCR(addr), &data, 1))
		return false;

	return true;
}

static bool ENC28J60_ReadReg16(s32 chan, ENC28J60Reg addr, u16 *data)
{
	if (!ENC28J60_SelectBank(chan, addr) ||
		!ENC28J60_ReadCmd(chan, ENC28J60_CMD_RCR(addr + 0), &((u8 *)data)[1], 1) ||
		!ENC28J60_ReadCmd(chan, ENC28J60_CMD_RCR(addr + 1), &((u8 *)data)[0], 1))
		return false;

	return true;
}

static bool ENC28J60_WriteReg16(s32 chan, ENC28J60Reg16 addr, u16 data)
{
	if (!ENC28J60_SelectBank(chan, addr) ||
		!ENC28J60_WriteCmd(chan, ENC28J60_CMD_WCR(addr + 0), &((u8 *)&data)[1], 1) ||
		!ENC28J60_WriteCmd(chan, ENC28J60_CMD_WCR(addr + 1), &((u8 *)&data)[0], 1))
		return false;

	return true;
}

static bool ENC28J60_ReadPHYReg(s32 chan, ENC28J60PHYReg addr, u16 *data)
{
	u8 mistat;

	if (!ENC28J60_WriteReg(chan, ENC28J60_MIREGADR, addr) ||
		!ENC28J60_WriteReg(chan, ENC28J60_MICMD, ENC28J60_MICMD_MIIRD))
		return false;

	do {
		if (!ENC28J60_ReadReg(chan, ENC28J60_MISTAT, &mistat))
			return false;
	} while (mistat & ENC28J60_MISTAT_BUSY);

	if (!ENC28J60_WriteReg(chan, ENC28J60_MICMD, 0) ||
		!ENC28J60_ReadReg16(chan, ENC28J60_MIWR, data))
		return false;

	return true;
}

static bool ENC28J60_WritePHYReg(s32 chan, ENC28J60PHYReg addr, u16 data)
{
	u8 mistat;

	if (!ENC28J60_WriteReg(chan, ENC28J60_MIREGADR, addr) ||
		!ENC28J60_WriteReg16(chan, ENC28J60_MIWR, data))
		return false;

	do {
		if (!ENC28J60_ReadReg(chan, ENC28J60_MISTAT, &mistat))
			return false;
	} while (mistat & ENC28J60_MISTAT_BUSY);

	return true;
}

static s32 ExiHandler(s32 chan, s32 dev)
{
	struct enc28j60if *enc28j60if = enc28j60_netif->state;
	u8 eie, eir;

	if (!EXI_Lock(chan, dev, ExiHandler))
		return FALSE;

	ENC28J60_ClearBits(chan, ENC28J60_EIE, ENC28J60_EIE_INTIE);
	ENC28J60_ReadReg(chan, ENC28J60_EIE, &eie);
	ENC28J60_ReadReg(chan, ENC28J60_EIR, &eir);

	if (eir == 0) {
		u8 epktcnt;
		ENC28J60_ReadReg(chan, ENC28J60_EPKTCNT, &epktcnt);
		if (epktcnt > 0) eir |= ENC28J60_EIR_PKTIF;
	}

	eir &= eie;

	if (eir & ENC28J60_EIR_PKTIF) {
		ENC28J60_WriteReg16(chan, ENC28J60_ERDPT, enc28j60if->nextPacket);

		u8 rsv[6];
		ENC28J60_ReadCmd(chan, ENC28J60_CMD_RBM, rsv, sizeof(rsv));
		u16 nextPacket = __lhbrx(rsv, 0);
		u16 byteCount = __lhbrx(rsv, 2);
		//u16 status = __lhbrx(rsv, 4);

		struct pbuf *p = pbuf_alloc(PBUF_RAW, byteCount - 4, PBUF_POOL);

		for (struct pbuf *q = p; q; q = q->next)
			ENC28J60_ReadCmd(chan, ENC28J60_CMD_RBM, q->payload, q->len);

		if (p) enc28j60_netif->input(p, enc28j60_netif);

		enc28j60if->nextPacket = nextPacket;
		ENC28J60_WriteReg16(chan, ENC28J60_ERXRDPT, nextPacket == ENC28J60_INIT_ERXST ? ENC28J60_INIT_ERXND : nextPacket - 1);

		ENC28J60_SetBits(chan, ENC28J60_ECON2, ENC28J60_ECON2_PKTDEC);
	}

	if (eir & ENC28J60_EIR_TXIF) {
		ENC28J60_ClearBits(chan, ENC28J60_EIR, ENC28J60_EIR_TXIF);
		LWP_SemPost(enc28j60if->txSemaphore);
	}

	if (eir & ENC28J60_EIR_TXERIF) {
		ENC28J60_SetBits(chan, ENC28J60_ECON1, ENC28J60_ECON1_TXRST);
		ENC28J60_ClearBits(chan, ENC28J60_ECON1, ENC28J60_ECON1_TXRST);
		ENC28J60_ClearBits(chan, ENC28J60_EIR, ENC28J60_EIR_TXERIF);
	}

	ENC28J60_SetBits(chan, ENC28J60_EIE, ENC28J60_EIE_INTIE);

	EXI_Unlock(chan);
	return TRUE;
}

static s32 ExtHandler(s32 chan, s32 dev)
{
	EXI_RegisterEXICallback(chan, NULL);
	return TRUE;
}

static void DbgHandler(u32 irq, frame_context *ctx)
{
	_piReg[0] = 1 << 12;
	ExiHandler(EXI_CHANNEL_2, EXI_DEVICE_0);
}

static s32 UnlockedHandler(s32 chan, s32 dev)
{
	struct enc28j60if *enc28j60if = enc28j60_netif->state;
	LWP_ThreadBroadcast(enc28j60if->threadQueue);
	return TRUE;
}

static bool enc28j60_init(struct netif *netif)
{
	struct enc28j60if *enc28j60if = netif->state;

	for (s32 chan = EXI_CHANNEL_0; chan < EXI_CHANNEL_MAX; chan++) {
		u32 id;

		if (sdgecko_isInitialized(chan))
			continue;
		if (chan < EXI_CHANNEL_2 && !EXI_Attach(chan, ExtHandler))
			continue;

		u32 level = IRQ_Disable();
		while (!EXI_Lock(chan, EXI_DEVICE_0, UnlockedHandler))
			LWP_ThreadSleep(enc28j60if->threadQueue);
		IRQ_Restore(level);

		ENC28J60_Reset(chan);
		CurrBank[chan] = 0;
		usleep(1000);

		if (!EXI_GetIDEx(chan, EXI_DEVICE_0, &id) || id != 0xFA050000) {
			if (chan < EXI_CHANNEL_2)
				EXI_Detach(chan);
			EXI_Unlock(chan);
			continue;
		}

		enc28j60if->chan = chan;

		enc28j60if->nextPacket = ENC28J60_INIT_ERXST;
		ENC28J60_WriteReg16(chan, ENC28J60_ERXST, ENC28J60_INIT_ERXST);
		ENC28J60_WriteReg16(chan, ENC28J60_ERXND, ENC28J60_INIT_ERXND);
		ENC28J60_WriteReg16(chan, ENC28J60_ERXRDPT, ENC28J60_INIT_ERXND);
		ENC28J60_WriteReg16(chan, ENC28J60_ETXST, ENC28J60_INIT_ETXST);
		ENC28J60_WriteReg16(chan, ENC28J60_ETXND, ENC28J60_INIT_ETXND);

		ENC28J60_WriteReg(chan, ENC28J60_ERXFCON, ENC28J60_ERXFCON_UCEN | ENC28J60_ERXFCON_CRCEN | ENC28J60_ERXFCON_MCEN | ENC28J60_ERXFCON_BCEN);

		ENC28J60_WriteReg(chan, ENC28J60_MACON1, ENC28J60_MACON1_MARXEN);
		ENC28J60_WriteReg(chan, ENC28J60_MACON3, ENC28J60_MACON3_PADCFG(1) | ENC28J60_MACON3_TXCRCEN | ENC28J60_MACON3_FRMLNEN);
		ENC28J60_WriteReg(chan, ENC28J60_MACON4, ENC28J60_MACON4_DEFER);
		ENC28J60_WriteReg16(chan, ENC28J60_MAMXFL, 2048);
		ENC28J60_WriteReg(chan, ENC28J60_MABBIPG, 0x12);
		ENC28J60_WriteReg16(chan, ENC28J60_MAIPG, 0x0C12);

		ENC28J60_WriteReg(chan, ENC28J60_MAADR1, enc28j60if->ethaddr->addr[0]);
		ENC28J60_WriteReg(chan, ENC28J60_MAADR2, enc28j60if->ethaddr->addr[1]);
		ENC28J60_WriteReg(chan, ENC28J60_MAADR3, enc28j60if->ethaddr->addr[2]);
		ENC28J60_WriteReg(chan, ENC28J60_MAADR4, enc28j60if->ethaddr->addr[3]);
		ENC28J60_WriteReg(chan, ENC28J60_MAADR5, enc28j60if->ethaddr->addr[4]);
		ENC28J60_WriteReg(chan, ENC28J60_MAADR6, enc28j60if->ethaddr->addr[5]);

		ENC28J60_WritePHYReg(chan, ENC28J60_PHCON2, ENC28J60_PHCON2_HDLDIS);
		ENC28J60_WritePHYReg(chan, ENC28J60_PHLCON, ENC28J60_PHLCON_LACFG(4) | ENC28J60_PHLCON_LBCFG(7) | ENC28J60_PHLCON_LFRQ(1) | ENC28J60_PHLCON_STRCH);

		ENC28J60_SetBits(chan, ENC28J60_EIE, ENC28J60_EIE_INTIE | ENC28J60_EIE_PKTIE | ENC28J60_EIE_TXIE | ENC28J60_EIE_TXERIE);
		ENC28J60_SetBits(chan, ENC28J60_ECON1, ENC28J60_ECON1_RXEN);
		ENC28J60_SelectBank(chan, 0);

		EXI_Unlock(chan);

		if (chan < EXI_CHANNEL_2) {
			EXI_RegisterEXICallback(chan, ExiHandler);
		} else {
			IRQ_Request(IRQ_PI_DEBUG, DbgHandler);
			__UnmaskIrq(IM_PI_DEBUG);
		}
		return true;
	}

	return false;
}

static err_t enc28j60_output(struct netif *netif, struct pbuf *p)
{
	struct enc28j60if *enc28j60if = netif->state;
	s32 chan = enc28j60if->chan;

	u32 level = IRQ_Disable();
	LWP_SemWait(enc28j60if->txSemaphore);
	while (!EXI_Lock(chan, EXI_DEVICE_0, UnlockedHandler))
		LWP_ThreadSleep(enc28j60if->threadQueue);
	IRQ_Restore(level);

	ENC28J60_WriteReg16(chan, ENC28J60_EWRPT, ENC28J60_INIT_ETXST);
	ENC28J60_WriteReg16(chan, ENC28J60_ETXND, ENC28J60_INIT_ETXST + p->tot_len);

	u8 control = 0;
	ENC28J60_WriteCmd(chan, ENC28J60_CMD_WBM, &control, 1);

	for (struct pbuf *q = p; q; q = q->next)
		ENC28J60_WriteCmd(chan, ENC28J60_CMD_WBM, q->payload, q->len);

	ENC28J60_SetBits(chan, ENC28J60_ECON1, ENC28J60_ECON1_TXRTS);

	EXI_Unlock(chan);
	return ERR_OK;
}

static err_t enc28j60if_output(struct netif *netif, struct pbuf *p, struct ip_addr *ipaddr)
{
	return etharp_output(netif, ipaddr, p);
}

err_t enc28j60if_init(struct netif *netif)
{
	struct enc28j60if *enc28j60if = mem_malloc(sizeof(struct enc28j60if));

	if (enc28j60if == NULL) {
		LWIP_DEBUGF(NETIF_DEBUG, ("enc28j60if_init: out of memory\n"));
		return ERR_MEM;
	}

	netif->state = enc28j60if;
	netif->name[0] = 'e';
	netif->name[1] = 'n';
	netif->output = enc28j60if_output;
	netif->linkoutput = enc28j60_output;

	netif->hwaddr_len = 6;
	netif->hwaddr[0] = 0x00;
	netif->hwaddr[1] = 0x09;
	netif->hwaddr[2] = 0xBF;
	netif->hwaddr[3] = 0x00;
	netif->hwaddr[4] = 0x04;
	netif->hwaddr[5] = 0xA3;
	netif->mtu = 1500;
	netif->flags = NETIF_FLAG_BROADCAST;

	LWP_InitQueue(&enc28j60if->threadQueue);
	LWP_SemInit(&enc28j60if->txSemaphore, 1, 1);

	enc28j60if->ethaddr = (struct eth_addr *)netif->hwaddr;
	enc28j60_netif = netif;

	if (enc28j60_init(netif))
		return ERR_OK;

	LWP_SemDestroy(enc28j60if->txSemaphore);
	LWP_CloseQueue(enc28j60if->threadQueue);

	mem_free(enc28j60if);
	return ERR_IF;
}
