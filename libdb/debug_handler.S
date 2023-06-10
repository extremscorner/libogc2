#include <asm.h>

#define EXCEPTION_PROLOG			\
	mfgqr    r0,0;					\
	stw      r0,GQR0_OFFSET(sp);    \
	mfgqr    r0,1;					\
	stw      r0,GQR1_OFFSET(sp);    \
	mfgqr    r0,2;					\
	stw      r0,GQR2_OFFSET(sp);    \
	mfgqr    r0,3;					\
	stw      r0,GQR3_OFFSET(sp);    \
	mfgqr    r0,4;					\
	stw      r0,GQR4_OFFSET(sp);    \
	mfgqr    r0,5;					\
	stw      r0,GQR5_OFFSET(sp);    \
	mfgqr    r0,6;					\
	stw      r0,GQR6_OFFSET(sp);    \
	mfgqr    r0,7;					\
	stw      r0,GQR7_OFFSET(sp);	\
	stmw     r6,GPR6_OFFSET(sp)
	
#define EXCEPTION_EPILOG			\
	lwz		r4,GQR0_OFFSET(sp);		\
	mtgqr	0,r4;					\
	lwz		r4,GQR1_OFFSET(sp);		\
	mtgqr	1,r4;					\
	lwz		r4,GQR2_OFFSET(sp);		\
	mtgqr	2,r4;					\
	lwz		r4,GQR3_OFFSET(sp);		\
	mtgqr	3,r4;					\
	lwz		r4,GQR4_OFFSET(sp);		\
	mtgqr	4,r4;					\
	lwz		r4,GQR5_OFFSET(sp);		\
	mtgqr	5,r4;					\
	lwz		r4,GQR6_OFFSET(sp);		\
	mtgqr	6,r4;					\
	lwz		r4,GQR7_OFFSET(sp);		\
	mtgqr	7,r4;					\
	lmw		r5,GPR5_OFFSET(sp)

	.extern c_debug_handler
	.extern _cpu_context_save_fp
	.globl dbg_exceptionhandler
dbg_exceptionhandler:
	stwu	sp,-EXCEPTION_FRAME_END(sp)		//now we're able to adjust the stackpointer with it's cached address

	EXCEPTION_PROLOG

	mfmsr	r4
	ori		r4,r4,MSR_FP
	mtmsr	r4
	isync

	addi	r14,sp,0

	lis		r15,__debug_nestlevel@ha
	lwz		r6,__debug_nestlevel@l(r15)
	cmpwi	r6,0
	bne		nested

	lis		sp,__debugstack@h
	ori		sp,sp,__debugstack@l
	lis		r0,0
	stwu	r0,-16(sp)

nested:
	addi	r6,r6,1
	stw		r6,__debug_nestlevel@l(r15)
		
	addi	r3,r14,0x08
	bl		_cpu_context_save_fp
	bl		c_debug_handler
	
	lwz		r6,__debug_nestlevel@l(r15)
	addi	r6,r6,-1
	stw		r6,__debug_nestlevel@l(r15)

	addi	sp,r14,0

exit:
	lwz		r4,CR_OFFSET(sp)
	mtcr	r4
	lwz		r4,LR_OFFSET(sp)
	mtlr	r4
	lwz		r4,CTR_OFFSET(sp)
	mtctr	r4
	lwz		r4,XER_OFFSET(sp)
	mtxer	r4

	EXCEPTION_EPILOG

	mfmsr		r4
	rlwinm		r4,r4,0,19,17
	mtmsr		r4
	isync

	lwz		toc,GPR2_OFFSET(sp)
	lwz		r0,GPR0_OFFSET(sp)

	lwz		r4,SRR0_OFFSET(sp)
	mtsrr0	r4
	lwz		r4,SRR1_OFFSET(sp)
	mtsrr1	r4

	lwz		r4,GPR4_OFFSET(sp)
	lwz		r3,GPR3_OFFSET(sp)
	addi	sp,sp,EXCEPTION_FRAME_END
	rfi

	.globl __set_iabr
__set_iabr:
	mfmsr	r4
	rlwinm	r5,r4,0,18,16
	mtmsr	r5
	clrrwi	r3,r3,2
	mtiabr	r3
	isync
	sync
	mtmsr	r4
	blr

	.globl __enable_iabr
__enable_iabr:
	mfmsr	r4
	rlwinm	r5,r4,0,18,16
	mtmsr	r5
	mfiabr	r3
	ori		r3,r3,0x0003
	mtiabr	r3
	isync
	sync
	mtmsr	r4
	blr

	.globl __disable_iabr
__disable_iabr:
	mfmsr	r4
	rlwinm	r5,r4,0,18,16
	mtmsr	r5
	mfiabr	r3
	clrrwi	r3,r3,2
	mtiabr	r3
	isync
	sync
	mtmsr	r4
	blr

	.globl __clr_iabr
__clr_iabr:
	mfmsr	r4
	rlwinm	r5,r4,0,18,16
	mtmsr	r5
	mtiabr	0
	isync
	sync
	mtmsr	r4
	blr

	.section .bss

	.balign 4
__debug_nestlevel:
	.long	0

	.balign 8
	.globl __debugstack_end,__debugstack
__debugstack_end:
	.space 0x4000
__debugstack:
