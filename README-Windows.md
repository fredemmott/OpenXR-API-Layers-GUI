OpenXR Layers UI - Windows
==========================

Requirements
------------

If the programs won't start, you may need to install the latest MSVC runtimes
from **BOTH**:

- https://aka.ms/vs/17/release/vc_redist.x64.exe
- https://aka.ms/vs/17/release/vc_redist.x86.exe

Contents
--------

There are four versions of this program for Windows:

- `OpenXR-API-Layers-Win32-HKCU.exe`: works with 32-bit API layers that are
  installed just for your user, rather than system-wide (`HKEY_CURRENT_USER` in
  the Windows registry)
- `OpenXR-API-Layers-Win32-HKLM.exe`: works with 32-bit API layers that are
  installed system-wide (`HKEY_LOCAL_MACHINE` in the Windows registry)
- `OpenXR-API-Layers-Win64-HKCU.exe`: works with 64-bit API layers that are
  installed just for your user
- `OpenXR-API-Layers-Win64-HKLM.exe`: works with 64-bit API layers that are
  installed system-wide

The HKLM versions will automatically run as administrator.

Which To Use
------------

You usually want `OpenXR-API-Layers-Win64-HKLM.exe`, as the vast majority of
OpenXR games and API layers are 64-bit, and should be installed system-wide.

If you are currently encountering problems with OpenXR, you should check all
four for errors and fix them.

How To Get Help
---------------

If you have a problem with a specific layer, or aren't sure which order things
should be in, contact the developers of the relevant layers.

If you have a problem with *the tool*, not games or specific API layers, help
may be available from the community in [my
Discord](https://go.fredemmott.com/discord).