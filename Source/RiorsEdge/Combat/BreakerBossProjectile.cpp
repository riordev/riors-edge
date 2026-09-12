#include "Combat/BreakerBossProjectile.h"

ABreakerBossProjectile::ABreakerBossProjectile()
{
    // BasicShapes/Sphere is 100 cm across, so 3.0 is a ~150 cm orb: the
    // Lattice's beach ball is 0.85. Big enough to read across the 800 ring.
    VisualScale = 3.0f;          // O2 PLACEHOLDER (O273)
    // Generous against the visual, as the base's own 30 is against its 0.85:
    // a round the player can see must hit roughly where it looks like it will.
    CollisionRadiusCm = 90.0f;   // O2 PLACEHOLDER (O273)
}
