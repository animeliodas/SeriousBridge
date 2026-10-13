# SeriousBridge core

`sam/` — the Serious Sam Classic side, compiled into each port's Classics Patch plugin (`sam/SeriousBridge.props`;
see the repository README). Modules are plain C++ against the Classics Patch 1.9.4 SDK (v143, x86, static CRT routed to
the engine's heap by `Alloc.cpp`); each starts with the port's precompiled header `StdH.h`, then `SBridge.h`.

A guest-side core (what guests that render next to Sam share: the link server, the stand-in player, the camera, frames
into shared memory) lives in SemCraft 2's Fabric mod for now and moves here with the second such guest (Doom).
