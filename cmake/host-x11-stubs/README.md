# Host-only X11 declaration stubs

Used **only** when cross-compiling for Windows from a Linux host that lacks the
libXrandr / libXinerama / libXcursor development packages. JUCE's helper tool
`juceaide` is compiled for the *build host* and pulls in juce_gui_basics, which
`#include`s these headers; juceaide never opens a window, and JUCE loads every X11
function dynamically at run time, so declarations are all that is needed.

They contain the public API declarations (struct layouts as documented in the Xlib
extension man pages). They are never used by the Windows plugin or by a normal
Linux build — on a regular machine install `libxrandr-dev libxinerama-dev libxcursor-dev`.
