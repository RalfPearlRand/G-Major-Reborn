/* Host-only declaration stub — see cmake/host-x11-stubs/README.md */
#pragma once
#include <X11/Xlib.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { int screen_number; short x_org, y_org, width, height; } XineramaScreenInfo;
Bool XineramaIsActive (Display*);
XineramaScreenInfo* XineramaQueryScreens (Display*, int*);
#ifdef __cplusplus
}
#endif
