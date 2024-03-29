#ifndef NUNCHUK_H_INCLUDED
#define NUNCHUK_H_INCLUDED

#include "wiiuse_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

int nunchuk_handshake(struct wiimote_t* wm, struct nunchuk_t* nc, ubyte* data, uword len);
void nunchuk_disconnected(struct nunchuk_t* nc);
void nunchuk_event(struct nunchuk_t* nc, ubyte* msg);

#ifdef __cplusplus
}
#endif

#endif
