/* SeriousBridge core - draws the guest's frame into Serious Sam's.
 *
 * The world layer (colour + depth) is drawn right after Sam renders its world (OnRenderView), as a full-screen
 * quad whose fragment shader writes gl_FragDepth on Sam's depth scale, so Sam's own depth test hides the guest
 * behind Sam's walls and the guest hides Sam's things behind its blocks. The guest's picture is re-projected from
 * the camera it was rendered for onto Sam's current camera (rotation exactly, position through depth).
 * The overlay layer (hand + hotbar) is drawn over the finished game view (OnPostDraw).
 * Raw OpenGL with every touched state saved and restored, so Sam's own GL state cache stays valid.
 */
#ifndef SBRIDGE_COMPOSITOR_H
#define SBRIDGE_COMPOSITOR_H

#include "StdH.h"

namespace compositor {

// The camera a guest frame was rendered for (Sam coordinates), as echoed in the frame.
struct Pose {
  FLOAT3D vPos;
  ANGLE3D aAngles;
};

// After Sam rendered the main game view with this (prepared) projection.
void DrawWorld(CPerspectiveProjection3D &ppr);
// Over the finished game view.
void DrawOverlay(void);
// Release GL objects and the shared memory (GL objects only if bWithGL, i.e. a context is current).
void Shutdown(BOOL bWithGL);
// Status line for the console.
CTString Status(void);
// Debug view: 0 composite, 1 the guest only (no depth test), 2 off.
extern INDEX sc_iCompositeMode;
// Depth bias (Sam window-depth units) subtracted from the guest's depth.
extern FLOAT sc_fDepthBias;
// How much the guest takes the light of Sam's picture around it (0 = the guest's own noon light).
extern FLOAT sc_fLightMatch;

}; // namespace

#endif
