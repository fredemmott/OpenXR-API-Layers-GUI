OpenXR Layers UI - Windows
==========================

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

As of 2024-01-20, the following OpenXR runtmes are 64-bit only - the 32-bit versions will have no effect on them:
- Meta OpenXR Simulator
- SteamVR
- Varjo

The following support both 32- and 64-bit, however it is still extremely likely that your game will use the 64-bit version, as most games will want to at least support SteamVR:
- Oculus OpenXR
- PimaxXR
- VirtualDesktop-OpenXR
- Window Mixed Reality

How To Get Help
---------------

If you have a problem with a specific layer, or aren't sure which order things
should be in, contact the developers of the relevant layers.

If you have a problem with *the tool*, not games or specific API layers, help
may be available from the community in [my
Discord](https://go.fredemmott.com/discord).
