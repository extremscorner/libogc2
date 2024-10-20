/*-------------------------------------------------------------

w5500if.c -- W5500 device driver

Copyright (C) 2024 Extrems

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
#include <string.h>
#include "lwip/debug.h"
#include "lwip/err.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "netif/etharp.h"
#include "netif/w5500if.h"

static vu32 *const _piReg = (u32 *)0xCC003000;

#define W5500_BSB(x)         (((x) & 0x1F) << 27) // Block Select Bits
#define W5500_RWB                       (1 << 26) // Read/Write Access Mode Bit
#define W5500_OM(x)           (((x) & 0x3) << 24) // SPI Operation Mode Bits
#define W5500_ADDR(x)              ((x) & 0xFFFF) // Offset Address

#define W5500_REG(addr)        (W5500_BSB(0)         | W5500_ADDR(addr))
#define W5500_REG_S(n, addr)   (W5500_BSB(n * 4 + 1) | W5500_ADDR(addr))
#define W5500_TXBUF_S(n, addr) (W5500_BSB(n * 4 + 2) | W5500_ADDR(addr))
#define W5500_RXBUF_S(n, addr) (W5500_BSB(n * 4 + 3) | W5500_ADDR(addr))

#define W5500_INIT_S0_RXBUF_SIZE (16)
#define W5500_INIT_S0_TXBUF_SIZE (16)

enum {
	Sn_MR         = 0x00, // Socket n Mode Register
	Sn_CR,                // Socket n Command Register
	Sn_IR,                // Socket n Interrupt Register
	Sn_SR,                // Socket n Status Register
	Sn_PORT,              // Socket n Source Port Register
	Sn_DHAR       = 0x06, // Socket n Destination Hardware Address Register
	Sn_DIPR       = 0x0C, // Socket n Destination IP Address Register
	Sn_DPORT      = 0x10, // Socket n Destination Port Register
	Sn_MSSR       = 0x12, // Socket n Maximum Segment Size Register
	Sn_TOS        = 0x15, // Socket n IP TOS Register
	Sn_TTL,               // Socket n IP TTL Register
	Sn_RXBUF_SIZE = 0x1E, // Socket n RX Buffer Size Register
	Sn_TXBUF_SIZE,        // Socket n TX Buffer Size Register
	Sn_TX_FSR,            // Socket n TX Free Buffer Size Register
	Sn_TX_RD      = 0x22, // Socket n TX Read Pointer Register
	Sn_TX_WR      = 0x24, // Socket n TX Write Pointer Register
	Sn_RX_RSR     = 0x26, // Socket n RX Received Size Register
	Sn_RX_RD      = 0x28, // Socket n RX Read Pointer Register
	Sn_RX_WR      = 0x2A, // Socket n RX Write Pointer Register
	Sn_IMR        = 0x2C, // Socket n Interrupt Mask Register
	Sn_FRAG,              // Socket n Fragment Offset in IP Header Register
	Sn_KPALVTR    = 0x2F, // Socket n Keepalive Time Register
};

typedef enum {
	W5500_MR                = W5500_REG(0x00), // Mode Register
	W5500_GAR0,                                // Gateway IP Address Register 0
	W5500_GAR1,                                // Gateway IP Address Register 1
	W5500_GAR2,                                // Gateway IP Address Register 2
	W5500_GAR3,                                // Gateway IP Address Register 3
	W5500_SUBR0,                               // Subnet Mask Register 0
	W5500_SUBR1,                               // Subnet Mask Register 1
	W5500_SUBR2,                               // Subnet Mask Register 2
	W5500_SUBR3,                               // Subnet Mask Register 3
	W5500_SHAR0,                               // Source Hardware Address Register 0
	W5500_SHAR1,                               // Source Hardware Address Register 1
	W5500_SHAR2,                               // Source Hardware Address Register 2
	W5500_SHAR3,                               // Source Hardware Address Register 3
	W5500_SHAR4,                               // Source Hardware Address Register 4
	W5500_SHAR5,                               // Source Hardware Address Register 5
	W5500_SIPR0,                               // Source IP Address Register 0
	W5500_SIPR1,                               // Source IP Address Register 1
	W5500_SIPR2,                               // Source IP Address Register 2
	W5500_SIPR3,                               // Source IP Address Register 3
	W5500_INTLEVEL0,                           // Interrupt Low-Level Timer Register 0
	W5500_INTLEVEL1,                           // Interrupt Low-Level Timer Register 1
	W5500_IR,                                  // Interrupt Register
	W5500_IMR,                                 // Interrupt Mask Register
	W5500_SIR,                                 // Socket Interrupt Register
	W5500_SIMR,                                // Socket Interrupt Mask Register
	W5500_RTR0,                                // Retransmission Time Register 0
	W5500_RTR1,                                // Retransmission Time Register 1
	W5500_RCR,                                 // Retransmission Count Register
	W5500_PTIMER,                              // PPP LCP Request Timer Register
	W5500_PMAGIC,                              // PPP LCP Magic Number Register
	W5500_PHAR0,                               // PPPoE Server Hardware Address Register 0
	W5500_PHAR1,                               // PPPoE Server Hardware Address Register 1
	W5500_PHAR2,                               // PPPoE Server Hardware Address Register 2
	W5500_PHAR3,                               // PPPoE Server Hardware Address Register 3
	W5500_PHAR4,                               // PPPoE Server Hardware Address Register 4
	W5500_PHAR5,                               // PPPoE Server Hardware Address Register 5
	W5500_PSID0,                               // PPPoE Session ID Register 0
	W5500_PSID1,                               // PPPoE Session ID Register 1
	W5500_PMRU0,                               // PPPoE Maximum Receive Unit Register 0
	W5500_PMRU1,                               // PPPoE Maximum Receive Unit Register 1
	W5500_UIPR0,                               // Unreachable IP Address Register 0
	W5500_UIPR1,                               // Unreachable IP Address Register 1
	W5500_UIPR2,                               // Unreachable IP Address Register 2
	W5500_UIPR3,                               // Unreachable IP Address Register 3
	W5500_UPORTR0,                             // Unreachable Port Register 0
	W5500_UPORTR1,                             // Unreachable Port Register 1
	W5500_PHYCFGR,                             // PHY Configuration Register
	W5500_VERSIONR          = W5500_REG(0x39), // Chip Version Register

#define W5500_S(n) \
	W5500_S##n##_MR         = W5500_REG_S(n, Sn_MR),         \
	W5500_S##n##_CR,                                         \
	W5500_S##n##_IR,                                         \
	W5500_S##n##_SR,                                         \
	W5500_S##n##_PORT0,                                      \
	W5500_S##n##_PORT1,                                      \
	W5500_S##n##_DHAR0,                                      \
	W5500_S##n##_DHAR1,                                      \
	W5500_S##n##_DHAR2,                                      \
	W5500_S##n##_DHAR3,                                      \
	W5500_S##n##_DHAR4,                                      \
	W5500_S##n##_DHAR5,                                      \
	W5500_S##n##_DIPR0,                                      \
	W5500_S##n##_DIPR1,                                      \
	W5500_S##n##_DIPR2,                                      \
	W5500_S##n##_DIPR3,                                      \
	W5500_S##n##_DPORT0,                                     \
	W5500_S##n##_DPORT1,                                     \
	W5500_S##n##_MSSR0,                                      \
	W5500_S##n##_MSSR1,                                      \
	W5500_S##n##_TOS        = W5500_REG_S(n, Sn_TOS),        \
	W5500_S##n##_TTL,                                        \
	W5500_S##n##_RXBUF_SIZE = W5500_REG_S(n, Sn_RXBUF_SIZE), \
	W5500_S##n##_TXBUF_SIZE,                                 \
	W5500_S##n##_TX_FSR0,                                    \
	W5500_S##n##_TX_FSR1,                                    \
	W5500_S##n##_TX_RD0,                                     \
	W5500_S##n##_TX_RD1,                                     \
	W5500_S##n##_TX_WR0,                                     \
	W5500_S##n##_TX_WR1,                                     \
	W5500_S##n##_RX_RSR0,                                    \
	W5500_S##n##_RX_RSR1,                                    \
	W5500_S##n##_RX_RD0,                                     \
	W5500_S##n##_RX_RD1,                                     \
	W5500_S##n##_RX_WR0,                                     \
	W5500_S##n##_RX_WR1,                                     \
	W5500_S##n##_IMR,                                        \
	W5500_S##n##_FRAG0,                                      \
	W5500_S##n##_FRAG1,                                      \
	W5500_S##n##_KPALVTR,                                    \

W5500_S(0)
W5500_S(1)
W5500_S(2)
W5500_S(3)
W5500_S(4)
W5500_S(5)
W5500_S(6)
W5500_S(7)
#undef W5500_S
} W5500Reg;

typedef enum {
	W5500_INTLEVEL          = W5500_REG(0x13), // Interrupt Low-Level Timer Register
	W5500_RTR               = W5500_REG(0x19), // Retransmission Time Register
	W5500_PSID              = W5500_REG(0x24), // PPPoE Session ID Register
	W5500_PMRU              = W5500_REG(0x26), // PPPoE Maximum Receive Unit Register
	W5500_UPORTR            = W5500_REG(0x2C), // Unreachable Port Register

#define W5500_S(n) \
	W5500_S##n##_PORT       = W5500_REG_S(n, Sn_PORT),       \
	W5500_S##n##_DPORT      = W5500_REG_S(n, Sn_DPORT),      \
	W5500_S##n##_MSSR       = W5500_REG_S(n, Sn_MSSR),       \
	W5500_S##n##_TX_FSR     = W5500_REG_S(n, Sn_TX_FSR),     \
	W5500_S##n##_TX_RD      = W5500_REG_S(n, Sn_TX_RD),      \
	W5500_S##n##_TX_WR      = W5500_REG_S(n, Sn_TX_WR),      \
	W5500_S##n##_RX_RSR     = W5500_REG_S(n, Sn_RX_RSR),     \
	W5500_S##n##_RX_RD      = W5500_REG_S(n, Sn_RX_RD),      \
	W5500_S##n##_RX_WR      = W5500_REG_S(n, Sn_RX_WR),      \
	W5500_S##n##_FRAG       = W5500_REG_S(n, Sn_FRAG),       \

W5500_S(0)
W5500_S(1)
W5500_S(2)
W5500_S(3)
W5500_S(4)
W5500_S(5)
W5500_S(6)
W5500_S(7)
#undef W5500_S
} W5500Reg16;

typedef enum {
	W5500_GAR               = W5500_REG(0x01), // Gateway IP Address Register
	W5500_SUBR              = W5500_REG(0x05), // Subnet Mask Register
	W5500_SIPR              = W5500_REG(0x0F), // Source IP Address Register
	W5500_UIPR              = W5500_REG(0x28), // Unreachable IP Address Register

#define W5500_S(n) \
	W5500_S##n##_DIPR       = W5500_REG_S(n, Sn_DIPR),       \

W5500_S(0)
W5500_S(1)
W5500_S(2)
W5500_S(3)
W5500_S(4)
W5500_S(5)
W5500_S(6)
W5500_S(7)
#undef W5500_S
} W5500Reg32;

#define W5500_MR_RST                     (1 << 7) // Software Reset
#define W5500_MR_WOL                     (1 << 5) // Wake-on-LAN
#define W5500_MR_PB                      (1 << 4) // Ping Reply Block
#define W5500_MR_PPPOE                   (1 << 3) // PPPoE Mode
#define W5500_MR_FARP                    (1 << 1) // Force ARP

#define W5500_IR_CONFLICT                (1 << 7) // IP Conflict Interrupt
#define W5500_IR_UNREACH                 (1 << 6) // Destination Port Unreachable Interrupt
#define W5500_IR_PPPOE                   (1 << 5) // PPPoE Terminated Interrupt
#define W5500_IR_MP                      (1 << 4) // WOL Magic Packet Interrupt

#define W5500_IMR_CONFLICT               (1 << 7) // IP Conflict Interrupt Mask
#define W5500_IMR_UNREACH                (1 << 6) // Destination Port Unreachable Interrupt Mask
#define W5500_IMR_PPPOE                  (1 << 5) // PPPoE Terminated Interrupt Mask
#define W5500_IMR_MP                     (1 << 4) // WOL Magic Packet Interrupt Mask

#define W5500_SIR_S(n)                 (1 << (n)) // Socket n Interrupt

#define W5500_SIMR_S(n)                (1 << (n)) // Socket n Interrupt Mask

#define W5500_PHYCFGR_RST                (1 << 7) // PHY Reset
#define W5500_PHYCFGR_OPMD               (1 << 6) // Configure PHY Operation Mode
#define W5500_PHYCFGR_OPMDC(x) (((x) & 0x7) << 3) // PHY Operation Mode Configuration
#define W5500_PHYCFGR_DPX                (1 << 2) // PHY Duplex Status
#define W5500_PHYCFGR_SPD                (1 << 1) // PHY Speed Status
#define W5500_PHYCFGR_LNK                (1 << 0) // PHY Link Status

#define W5500_Sn_MR_MULTI                (1 << 7) // Multicast Mode
#define W5500_Sn_MR_MFEN                 (1 << 7) // MAC Filter Enable
#define W5500_Sn_MR_BCASTB               (1 << 6) // Broadcast Block
#define W5500_Sn_MR_ND                   (1 << 5) // No Delayed ACK
#define W5500_Sn_MR_MC                   (1 << 5) // Multicast IGMP Version
#define W5500_Sn_MR_MMB                  (1 << 5) // Multicast Block in MACRAW Mode
#define W5500_Sn_MR_UCASTB               (1 << 4) // Unicast Block
#define W5500_Sn_MR_MIP6B                (1 << 4) // IPv6 Block in MACRAW Mode
#define W5500_Sn_MR_P(x)       (((x) & 0xF) << 0) // Protocol Mode

enum {
	W5500_Sn_MR_CLOSE       = 0x00,
	W5500_Sn_MR_TCP,
	W5500_Sn_MR_UDP,
	W5500_Sn_MR_MACRAW      = 0x04,
};

enum {
	W5500_Sn_CR_OPEN        = 0x01,
	W5500_Sn_CR_LISTEN,
	W5500_Sn_CR_CONNECT     = 0x04,
	W5500_Sn_CR_DISCON      = 0x08,
	W5500_Sn_CR_CLOSE       = 0x10,
	W5500_Sn_CR_SEND        = 0x20,
	W5500_Sn_CR_SEND_MAC,
	W5500_Sn_CR_SEND_KEEP,
	W5500_Sn_CR_RECV        = 0x40,
};

#define W5500_Sn_IR_SENDOK               (1 << 4) // SEND OK Interrupt
#define W5500_Sn_IR_TIMEOUT              (1 << 3) // TIMEOUT Interrupt
#define W5500_Sn_IR_RECV                 (1 << 2) // RECEIVED Interrupt
#define W5500_Sn_IR_DISCON               (1 << 1) // DISCONNECTED Interrupt
#define W5500_Sn_IR_CON                  (1 << 0) // CONNECTED Interrupt

enum {
	W5500_Sn_SR_CLOSED      = 0x00,
	W5500_Sn_SR_INIT        = 0x13,
	W5500_Sn_SR_LISTEN,
	W5500_Sn_SR_SYNSENT,
	W5500_Sn_SR_SYNRECV,
	W5500_Sn_SR_ESTABLISHED,
	W5500_Sn_SR_FIN_WAIT,
	W5500_Sn_SR_CLOSING     = 0x1A,
	W5500_Sn_SR_TIME_WAIT,
	W5500_Sn_SR_CLOSE_WAIT,
	W5500_Sn_SR_LAST_ACK,
	W5500_Sn_SR_UDP         = 0x22,
	W5500_Sn_SR_MACRAW      = 0x42,
};

#define W5500_Sn_IMR_SENDOK              (1 << 4) // SEND OK Interrupt Mask
#define W5500_Sn_IMR_TIMEOUT             (1 << 3) // TIMEOUT Interrupt Mask
#define W5500_Sn_IMR_RECV                (1 << 2) // RECEIVED Interrupt Mask
#define W5500_Sn_IMR_DISCON              (1 << 1) // DISCONNECTED Interrupt Mask
#define W5500_Sn_IMR_CON                 (1 << 0) // CONNECTED Interrupt Mask

#define W5500_TX_BUFSIZE  (W5500_INIT_S0_TXBUF_SIZE * 1024)
#define W5500_TX_QUEUELEN ((W5500_TX_BUFSIZE * 27) / (1536 * 20))

struct w5500if {
	s32 chan;
	s32 dev;
	s32 txQueued;
	u16 txQueue[W5500_TX_QUEUELEN + 1];
	lwpq_t unlockQueue;
	struct eth_addr *ethaddr;
};

static struct netif *w5500_netif;
static u8 Dev[EXI_CHANNEL_MAX];

static bool W5500_ReadCmd(s32 chan, u32 cmd, void *buf, u32 len)
{
	bool err = false;

	cmd &= ~W5500_RWB;
	cmd  = (cmd << 16) | (cmd >> 16);

	if (!EXI_Select(chan, Dev[chan], EXI_SPEED32MHZ))
		return false;

	err |= !EXI_ImmEx(chan, &cmd, 3, EXI_WRITE);
	err |= !EXI_DmaEx(chan, buf, len, EXI_READ);
	err |= !EXI_Deselect(chan);
	return !err;
}

static bool W5500_WriteCmd(s32 chan, u32 cmd, const void *buf, u32 len)
{
	bool err = false;

	cmd |= W5500_RWB;
	cmd  = (cmd << 16) | (cmd >> 16);

	if (!EXI_Select(chan, Dev[chan], EXI_SPEED32MHZ))
		return false;

	err |= !EXI_ImmEx(chan, &cmd, 3, EXI_WRITE);
	err |= !EXI_DmaEx(chan, (void *)buf, len, EXI_WRITE);
	err |= !EXI_Deselect(chan);
	return !err;
}

static bool W5500_ReadReg(s32 chan, W5500Reg addr, u8 *data)
{
	return W5500_ReadCmd(chan, W5500_OM(1) | addr, data, 1);
}

static bool W5500_WriteReg(s32 chan, W5500Reg addr, u8 data)
{
	return W5500_WriteCmd(chan, W5500_OM(1) | addr, &data, 1);
}

static bool W5500_ReadReg16(s32 chan, W5500Reg16 addr, u16 *data)
{
	u16 tmp;

	do {
		if (!W5500_ReadCmd(chan, W5500_OM(2) | addr, data, 2) ||
			!W5500_ReadCmd(chan, W5500_OM(2) | addr, &tmp, 2))
			return false;
	} while (*data != tmp);

	return true;
}

static bool W5500_WriteReg16(s32 chan, W5500Reg16 addr, u16 data)
{
	return W5500_WriteCmd(chan, W5500_OM(2) | addr, &data, 2);
}

#if 0
static bool W5500_ReadReg32(s32 chan, W5500Reg32 addr, u32 *data)
{
	u32 tmp;

	do {
		if (!W5500_ReadCmd(chan, W5500_OM(3) | addr, data, 4) ||
			!W5500_ReadCmd(chan, W5500_OM(3) | addr, &tmp, 4))
			return false;
	} while (*data != tmp);

	return true;
}

static bool W5500_WriteReg32(s32 chan, W5500Reg32 addr, u32 data)
{
	return W5500_WriteCmd(chan, W5500_OM(3) | addr, &data, 4);
}
#endif

static bool W5500_Reset(s32 chan)
{
	u8 mr;

	if (!W5500_WriteReg(chan, W5500_MR, W5500_MR_RST) ||
		!W5500_ReadReg(chan, W5500_MR, &mr) || (mr & W5500_MR_RST))
		return false;

	return true;
}

static bool W5500_GetLinkState(s32 chan)
{
	u8 phycfgr;

	if (!W5500_ReadReg(chan, W5500_PHYCFGR, &phycfgr))
		return false;

	return !!(phycfgr & W5500_PHYCFGR_LNK);
}

static void W5500_GetMACAddr(s32 chan, u8 macaddr[6])
{
	union {
		u32 cid[4];
		u8 data[18 + 1];
	} ecid = {{
		mfspr(ECID0),
		mfspr(ECID1),
		mfspr(ECID2),
		mfspr(ECID3)
	}};

	u32 sum = chan;

	ecid.data[15] ^= 0x00;
	ecid.data[16] ^= 0x08;
	ecid.data[17] ^= 0xDC;

	for (int i = 0; i < 18; i += 3) {
		sum += *(u32 *)&ecid.data[i] >> 8;
		sum = (sum & 0xFFFFFF) + (sum >> 24);
	}

	macaddr[0] = 0x00; macaddr[3] = sum >> 16;
	macaddr[1] = 0x09; macaddr[4] = sum >> 8;
	macaddr[2] = 0xBF; macaddr[5] = sum;
}

static s32 ExiHandler(s32 chan, s32 dev)
{
	struct w5500if *w5500if = w5500_netif->state;
	chan = w5500if->chan;
	dev = w5500if->dev;

	if (!EXI_Lock(chan, dev, ExiHandler))
		return FALSE;

	u8 ir, sir;
	W5500_WriteReg(chan, W5500_SIMR, 0);
	W5500_ReadReg(chan, W5500_SIR, &sir);

	if (sir & W5500_SIR_S(0)) {
		W5500_ReadReg(chan, W5500_S0_IR, &ir);

		if (ir & W5500_Sn_IR_SENDOK) {
			W5500_WriteReg(chan, W5500_S0_IR, W5500_Sn_IR_SENDOK);

			if (w5500if->txQueued) {
				memmove(&w5500if->txQueue[0], &w5500if->txQueue[1], w5500if->txQueued * sizeof(u16));

				if (--w5500if->txQueued) {
					W5500_WriteReg16(chan, W5500_S0_TX_WR, w5500if->txQueue[1]);
					W5500_WriteReg(chan, W5500_S0_CR, W5500_Sn_CR_SEND);
				}
			}
		}

		if (ir & W5500_Sn_IR_RECV) {
			W5500_WriteReg(chan, W5500_S0_IR, W5500_Sn_IR_RECV);

			u16 rd, size;
			W5500_ReadReg16(chan, W5500_S0_RX_RD, &rd);
			W5500_ReadCmd(chan, W5500_RXBUF_S(0, rd), &size, sizeof(size));
			rd += sizeof(size);

			struct pbuf *p = pbuf_alloc(PBUF_RAW, size - sizeof(size), PBUF_POOL);

			for (struct pbuf *q = p; q; q = q->next) {
				W5500_ReadCmd(chan, W5500_RXBUF_S(0, rd), q->payload, q->len);
				rd += q->len;
			}

			if (p) w5500_netif->input(p, w5500_netif);

			W5500_WriteReg16(chan, W5500_S0_RX_RD, rd);
			W5500_WriteReg(chan, W5500_S0_CR, W5500_Sn_CR_RECV);
		}
	}

	W5500_WriteReg(chan, W5500_SIMR, W5500_SIMR_S(0));

	if (W5500_GetLinkState(chan))
		w5500_netif->flags |= NETIF_FLAG_LINK_UP;
	else
		w5500_netif->flags &= ~NETIF_FLAG_LINK_UP;

	EXI_Unlock(chan);
	return TRUE;
}

static s32 ExtHandler(s32 chan, s32 dev)
{
	w5500_netif->flags &= ~NETIF_FLAG_LINK_UP;
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
	struct w5500if *w5500if = w5500_netif->state;
	LWP_ThreadBroadcast(w5500if->unlockQueue);
	return TRUE;
}

static bool W5500_Init(s32 chan, s32 dev, struct w5500if *w5500if)
{
	bool err = false;
	u8 sr, versionr;
	u32 id;

	while (!EXI_ProbeEx(chan));
	if (chan < EXI_CHANNEL_2 && dev == EXI_DEVICE_0)
		if (!EXI_Attach(chan, ExtHandler))
			return false;

	u32 level = IRQ_Disable();
	while (!EXI_Lock(chan, dev, UnlockedHandler))
		LWP_ThreadSleep(w5500if->unlockQueue);
	IRQ_Restore(level);

	if (!EXI_GetID(chan, dev, &id) || id != 0x03000000) {
		if (chan < EXI_CHANNEL_2 && dev == EXI_DEVICE_0)
			EXI_Detach(chan);
		EXI_Unlock(chan);
		return false;
	}

	Dev[chan] = dev;
	if (!W5500_ReadReg(chan, W5500_VERSIONR, &versionr) || versionr != 0x04) {
		if (chan < EXI_CHANNEL_2 && dev == EXI_DEVICE_0)
			EXI_Detach(chan);
		EXI_Unlock(chan);
		return false;
	}

	w5500if->chan = chan;
	w5500if->dev = dev;
	w5500if->txQueued = 0;
	w5500if->txQueue[0] = 0;

	err |= !W5500_Reset(chan);

	W5500_GetMACAddr(chan, w5500if->ethaddr->addr);
	err |= !W5500_WriteReg(chan, W5500_SHAR0, w5500if->ethaddr->addr[0]);
	err |= !W5500_WriteReg(chan, W5500_SHAR1, w5500if->ethaddr->addr[1]);
	err |= !W5500_WriteReg(chan, W5500_SHAR2, w5500if->ethaddr->addr[2]);
	err |= !W5500_WriteReg(chan, W5500_SHAR3, w5500if->ethaddr->addr[3]);
	err |= !W5500_WriteReg(chan, W5500_SHAR4, w5500if->ethaddr->addr[4]);
	err |= !W5500_WriteReg(chan, W5500_SHAR5, w5500if->ethaddr->addr[5]);

	err |= !W5500_WriteReg(chan, W5500_S0_RXBUF_SIZE, W5500_INIT_S0_RXBUF_SIZE);
	err |= !W5500_WriteReg(chan, W5500_S0_TXBUF_SIZE, W5500_INIT_S0_TXBUF_SIZE);

	for (int n = 1; n < 8; n++) {
		err |= !W5500_WriteReg(chan, W5500_REG_S(n, Sn_RXBUF_SIZE), 0);
		err |= !W5500_WriteReg(chan, W5500_REG_S(n, Sn_TXBUF_SIZE), 0);
	}

	err |= !W5500_WriteReg(chan, W5500_S0_MR, W5500_Sn_MR_MFEN | W5500_Sn_MR_MACRAW);
	err |= !W5500_WriteReg(chan, W5500_S0_CR, W5500_Sn_CR_OPEN);
	err |= !W5500_ReadReg(chan, W5500_S0_SR, &sr);

	if (err || sr != W5500_Sn_SR_MACRAW) {
		EXI_Unlock(chan);
		return false;
	}

	err |= !W5500_WriteReg(chan, W5500_S0_IMR, W5500_Sn_IMR_SENDOK | W5500_Sn_IMR_RECV);
	err |= !W5500_WriteReg(chan, W5500_SIMR, W5500_SIMR_S(0));

	if (W5500_GetLinkState(chan))
		w5500_netif->flags |= NETIF_FLAG_LINK_UP;
	else
		w5500_netif->flags &= ~NETIF_FLAG_LINK_UP;

	EXI_Unlock(chan);

	if (!err) {
		if (chan < EXI_CHANNEL_2 && dev == EXI_DEVICE_0) {
			EXI_RegisterEXICallback(chan, ExiHandler);
		} else if (chan == EXI_CHANNEL_0 && dev == EXI_DEVICE_2) {
			EXI_RegisterEXICallback(EXI_CHANNEL_2, ExiHandler);
		} else if (chan == EXI_CHANNEL_2 && dev == EXI_DEVICE_0) {
			IRQ_Request(IRQ_PI_DEBUG, DbgHandler);
			__UnmaskIrq(IM_PI_DEBUG);
		}
	}

	return !err;
}

static err_t w5500_output(struct netif *netif, struct pbuf *p)
{
	struct w5500if *w5500if = netif->state;
	s32 chan = w5500if->chan;
	s32 dev = w5500if->dev;

	u32 level = IRQ_Disable();

	if (!(netif->flags & NETIF_FLAG_LINK_UP)) {
		IRQ_Restore(level);
		return ERR_CONN;
	}

	if (w5500if->txQueued < 0 || w5500if->txQueued >= W5500_TX_QUEUELEN ||
		(u16)(w5500if->txQueue[w5500if->txQueued] - w5500if->txQueue[0]) > (W5500_TX_BUFSIZE - p->tot_len)) {
		IRQ_Restore(level);
		return ERR_IF;
	}

	while (!EXI_Lock(chan, dev, UnlockedHandler))
		LWP_ThreadSleep(w5500if->unlockQueue);
	IRQ_Restore(level);

	u16 wr = w5500if->txQueue[w5500if->txQueued];

	for (struct pbuf *q = p; q; q = q->next) {
		W5500_WriteCmd(chan, W5500_TXBUF_S(0, wr), q->payload, q->len);
		wr += q->len;
	}

	w5500if->txQueue[++w5500if->txQueued] = wr;

	if (w5500if->txQueued == 1) {
		W5500_WriteReg16(chan, W5500_S0_TX_WR, w5500if->txQueue[1]);
		W5500_WriteReg(chan, W5500_S0_CR, W5500_Sn_CR_SEND);
	}

	EXI_Unlock(chan);
	return ERR_OK;
}

static err_t w5500if_output(struct netif *netif, struct pbuf *p, struct ip_addr *ipaddr)
{
	return etharp_output(netif, ipaddr, p);
}

err_t w5500if_init(struct netif *netif)
{
	struct w5500if *w5500if = mem_malloc(sizeof(struct w5500if));

	if (w5500if == NULL) {
		LWIP_DEBUGF(NETIF_DEBUG, ("w5500if_init: out of memory\n"));
		return ERR_MEM;
	}

	netif->state = w5500if;
	netif->output = w5500if_output;
	netif->linkoutput = w5500_output;

	netif->hwaddr_len = 6;
	netif->mtu = 1500;
	netif->flags = NETIF_FLAG_BROADCAST;

	LWP_InitQueue(&w5500if->unlockQueue);

	w5500if->ethaddr = (struct eth_addr *)netif->hwaddr;
	w5500_netif = netif;

	if (W5500_Init(EXI_CHANNEL_0, EXI_DEVICE_2, w5500if)) {
		netif->name[0] = 'W';
		netif->name[1] = '1';
		return ERR_OK;
	}

	if (W5500_Init(EXI_CHANNEL_2, EXI_DEVICE_0, w5500if)) {
		netif->name[0] = 'W';
		netif->name[1] = '2';
		return ERR_OK;
	}

	for (s32 chan = EXI_CHANNEL_0; chan < EXI_CHANNEL_2; chan++) {
		if (W5500_Init(chan, EXI_DEVICE_0, w5500if)) {
			netif->name[0] = 'W';
			netif->name[1] = 'A' + chan;
			return ERR_OK;
		}
	}

	LWP_CloseQueue(w5500if->unlockQueue);

	mem_free(w5500if);
	return ERR_IF;
}
