/* Host-only declaration stub — see cmake/host-x11-stubs/README.md */
#pragma once
#include <X11/Xlib.h>
#include <X11/extensions/randr.h>
#include <X11/extensions/Xrender.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef XID RROutput;
typedef XID RRCrtc;
typedef XID RRMode;
typedef XID RRProvider;
typedef unsigned short Rotation;
typedef unsigned short SizeID;
typedef unsigned short SubpixelOrder;
typedef unsigned short Connection;
typedef unsigned long XRRModeFlags;
typedef struct _XRRModeInfo { RRMode id; unsigned int width, height; unsigned long dotClock;
    unsigned int hSyncStart, hSyncEnd, hTotal, hSkew, vSyncStart, vSyncEnd, vTotal;
    char* name; unsigned int nameLength; XRRModeFlags modeFlags; } XRRModeInfo;
typedef struct _XRRScreenResources { Time timestamp; Time configTimestamp; int ncrtc; RRCrtc* crtcs;
    int noutput; RROutput* outputs; int nmode; XRRModeInfo* modes; } XRRScreenResources;
typedef struct _XRROutputInfo { Time timestamp; RRCrtc crtc; char* name; int nameLen;
    unsigned long mm_width, mm_height; Connection connection; SubpixelOrder subpixel_order;
    int ncrtc; RRCrtc* crtcs; int nclone; RROutput* clones; int nmode; int npreferred; RRMode* modes; } XRROutputInfo;
typedef struct _XRRCrtcInfo { Time timestamp; int x, y; unsigned int width, height; RRMode mode; Rotation rotation;
    int noutput; RROutput* outputs; Rotation rotations; int npossible; RROutput* possible; } XRRCrtcInfo;
XRRScreenResources* XRRGetScreenResources (Display*, Window);
void XRRFreeScreenResources (XRRScreenResources*);
XRROutputInfo* XRRGetOutputInfo (Display*, XRRScreenResources*, RROutput);
void XRRFreeOutputInfo (XRROutputInfo*);
XRRCrtcInfo* XRRGetCrtcInfo (Display*, XRRScreenResources*, RRCrtc);
void XRRFreeCrtcInfo (XRRCrtcInfo*);
RROutput XRRGetOutputPrimary (Display*, Window);
#ifdef __cplusplus
}
#endif
