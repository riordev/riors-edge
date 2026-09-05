#pragma once

#include "CoreMinimal.h"

class UWorld;

// The runtime nav bounds (NAV-1). No map carries a NavMeshBoundsVolume and
// none may be hand-edited; every level's walkable geometry is spawned by
// code (the gym's blocks, Fernhall's yards, the rift yard), so the bounds
// are derived from what was spawned: one volume, sized to the union of every
// colliding static mesh in the world, grown whenever a later build outgrows
// it. Gym, Fernhall and the rift yard are covered by the same rule, and
// nothing per map is authored.
namespace BreakerNavBounds
{
    // Spawn or grow the world's one bounds volume so it contains every
    // colliding static mesh. Cheap enough to call on every enemy possession:
    // one actor iteration, and at most once per second per world.
    RIORSEDGE_API void EnsureCoverage(UWorld* World);

    // The union it would cover right now, padded. Exposed for the test and
    // the probe; returns an invalid box when there is nothing to cover.
    RIORSEDGE_API FBox ComputeCoverage(UWorld* World);

    // Padding around the geometry union, so a body at a slab's edge is still
    // on the mesh and a step's top is inside the volume. O2 PLACEHOLDER.
    constexpr float LateralPaddingCm = 500.0f;
    constexpr float VerticalPaddingCm = 400.0f;
}
