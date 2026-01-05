#ifndef __OGC_CAST_H__
#define __OGC_CAST_H__

#include <gctypes.h>

#define GQR_TYPE_F32	0
#define GQR_TYPE_U8		4
#define GQR_TYPE_U16	5
#define GQR_TYPE_S8		6
#define GQR_TYPE_S16	7

#define GQR_CAST_U8		2
#define GQR_CAST_U16	3
#define GQR_CAST_S8		4
#define GQR_CAST_S16	5

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GEKKO

#define __set_gqr(_reg,_val) __asm__ __volatile__("mtgqr %0,%1" : : "i"(_reg), "r"(_val))

static inline void CAST_SetGQR2(u8 type,u8 scale)
{
	u32 val = (((((scale)<<8)|(type))<<16)|(((scale)<<8)|(type)));
	__set_gqr(2,val);
}

static inline void CAST_SetGQR3(u8 type,u8 scale)
{
	u32 val = (((((scale)<<8)|(type))<<16)|(((scale)<<8)|(type)));
	__set_gqr(3,val);
}

static inline void CAST_SetGQR4(u8 type,u8 scale)
{
	u32 val = (((((scale)<<8)|(type))<<16)|(((scale)<<8)|(type)));
	__set_gqr(4,val);
}

static inline void CAST_SetGQR5(u8 type,u8 scale)
{
	u32 val = (((((scale)<<8)|(type))<<16)|(((scale)<<8)|(type)));
	__set_gqr(5,val);
}

static inline void CAST_SetGQR6(u8 type,u8 scale)
{
	u32 val = (((((scale)<<8)|(type))<<16)|(((scale)<<8)|(type)));
	__set_gqr(6,val);
}

static inline void CAST_SetGQR7(u8 type,u8 scale)
{
	u32 val = (((((scale)<<8)|(type))<<16)|(((scale)<<8)|(type)));
	__set_gqr(7,val);
}

// does a default init
static inline void CAST_Init(void)
{
	CAST_SetGQR2(GQR_TYPE_U8,0);
	CAST_SetGQR3(GQR_TYPE_U16,0);
	CAST_SetGQR4(GQR_TYPE_S8,0);
	CAST_SetGQR5(GQR_TYPE_S16,0);
}

/******************************************************************/
/*																  */
/* cast from int to float										  */
/*																  */
/******************************************************************/

static inline void castu8f32(const u8 *in,f32 *out)
{
	__asm__ __volatile__("psq_lx %0,%y1,1,%2" : "=f"(*out) : "Z"(*in), "i"(GQR_CAST_U8));
}

static inline void castu16f32(const u16 *in,f32 *out)
{
	__asm__ __volatile__("psq_lx %0,%y1,1,%2" : "=f"(*out) : "Z"(*in), "i"(GQR_CAST_U16));
}

static inline void casts8f32(const s8 *in,f32 *out)
{
	__asm__ __volatile__("psq_lx %0,%y1,1,%2" : "=f"(*out) : "Z"(*in), "i"(GQR_CAST_S8));
}

static inline void casts16f32(const s16 *in,f32 *out)
{
	__asm__ __volatile__("psq_lx %0,%y1,1,%2" : "=f"(*out) : "Z"(*in), "i"(GQR_CAST_S16));
}

/******************************************************************/
/*																  */
/* cast from float to int										  */
/*																  */
/******************************************************************/

static inline void castf32u8(const f32 *in,u8 *out)
{
	__asm__ __volatile__("psq_stx %1,%y0,1,%2" : "=Z"(*out) : "f"(*in), "i"(GQR_CAST_U8));
}

static inline void castf32u16(const f32 *in,u16 *out)
{
	__asm__ __volatile__("psq_stx %1,%y0,1,%2" : "=Z"(*out) : "f"(*in), "i"(GQR_CAST_U16));
}

static inline void castf32s8(const f32 *in,s8 *out)
{
	__asm__ __volatile__("psq_stx %1,%y0,1,%2" : "=Z"(*out) : "f"(*in), "i"(GQR_CAST_S8));
}

static inline void castf32s16(const f32 *in,s16 *out)
{
	__asm__ __volatile__("psq_stx %1,%y0,1,%2" : "=Z"(*out) : "f"(*in), "i"(GQR_CAST_S16));
}

#endif //GEKKO

#ifdef __cplusplus
}
#endif

#endif
