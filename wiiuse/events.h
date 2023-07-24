#ifndef EVENTS_H_INCLUDED
#define EVENTS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

void wiiuse_pressed_buttons(struct wiimote_t* wm, ubyte* msg);

#ifdef __cplusplus
}
#endif

#endif
