/*-------------------------------------------------------------

si_steering.h -- Steering wheel support

Copyright (C) 2017 - 2026 Extrems' Corner.org

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

#ifndef __OGC_SI_STEERING_H__
#define __OGC_SI_STEERING_H__

#include <gctypes.h>

#define SI_STEERING_ERR_READY           0
#define SI_STEERING_ERR_NO_CONTROLLER  -1
#define SI_STEERING_ERR_BUSY           -2
#define SI_STEERING_ERR_TRANSFER       -3

#define SI_STEERING_STATUS_MTR_EN    0x01
#define SI_STEERING_STATUS_MTR_BK    0x02
#define SI_STEERING_STATUS_POWER     0x04
#define SI_STEERING_STATUS_PEDAL     0x08

#define SI_STEERING_BUTTON_LEFT    0x0001
#define SI_STEERING_BUTTON_RIGHT   0x0002
#define SI_STEERING_BUTTON_DOWN    0x0004
#define SI_STEERING_BUTTON_UP      0x0008
#define SI_STEERING_TRIGGER_Z      0x0010
#define SI_STEERING_TRIGGER_R      0x0020
#define SI_STEERING_TRIGGER_L      0x0040
#define SI_STEERING_BUTTON_A       0x0100
#define SI_STEERING_BUTTON_B       0x0200
#define SI_STEERING_BUTTON_X       0x0400
#define SI_STEERING_BUTTON_Y       0x0800
#define SI_STEERING_BUTTON_START   0x1000

#define SI_STEERING_CONTROL_BRAKE   0x000
#define SI_STEERING_CONTROL_STANDBY 0x400
#define SI_STEERING_CONTROL_DRIVE   0x600
#define SI_STEERING_CONTROL_MASK    0x600

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*SISteeringCallback)(s32 chan, s32 result);
typedef void (*SISteeringSamplingCallback)(void);

typedef struct SISteeringStatus {
	u16 button;
	u8 status;
	s8 steering;
	u8 gas;
	u8 brake;
	u8 left;
	u8 right;
	s8 err;
} SISteeringStatus;

void SI_InitSteering(void);
s32 SI_ResetSteering(s32 chan);
s32 SI_ResetSteeringAsync(s32 chan, SISteeringCallback callback);
s32 SI_ReadSteering(s32 chan, SISteeringStatus *status);
SISteeringSamplingCallback SI_SetSteeringSamplingCallback(SISteeringSamplingCallback callback);
void SI_ControlSteering(s32 chan, u32 control, s32 steering);

#ifdef __cplusplus
}
#endif

#endif
