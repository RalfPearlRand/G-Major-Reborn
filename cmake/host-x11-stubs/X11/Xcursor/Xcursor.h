/* Host-only declaration stub — see cmake/host-x11-stubs/README.md */
#pragma once
#include <X11/Xlib.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int XcursorBool;
typedef unsigned int XcursorUInt;
typedef XcursorUInt XcursorDim;
typedef XcursorUInt XcursorPixel;
typedef struct _XcursorImage { XcursorUInt version; XcursorDim size, width, height, xhot, yhot; XcursorUInt delay; XcursorPixel* pixels; } XcursorImage;
XcursorImage* XcursorImageCreate (int width, int height);
void XcursorImageDestroy (XcursorImage*);
Cursor XcursorImageLoadCursor (Display*, const XcursorImage*);
XcursorBool XcursorSupportsARGB (Display*);
#ifdef __cplusplus
}
#endif
