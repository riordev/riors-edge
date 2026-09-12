#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerEnemyProjectile.h"
#include "BreakerBossProjectile.generated.h"

// THE RING VOLLEY'S ROUND (O273). From its ring a boss fires one large, slow,
// telegraphed projectile at the sweep's damage; this is the "large". It is the
// Lattice's orb with bigger numbers and nothing else: the same single-target
// contract, the same enemy-ignore, the same base travel and impact. A subclass
// rather than two fields on the boss because the base applies VisualScale and
// CollisionRadiusCm in its own BeginPlay, so a constructor is the one place a
// bigger round can be authored and still be read off a default object.
//
// The boss tints it after spawn (SetOrbColor) with the apparatus's fire colour,
// so the round is the colour of the thing that launched it. Both numbers are
// O2 PLACEHOLDER; RiorsEdge.Combat.Boss.VolleyShipsDodgeable pins them against
// the ring and the player's sprint.
UCLASS()
class RIORSEDGE_API ABreakerBossProjectile : public ABreakerEnemyProjectile
{
    GENERATED_BODY()

public:
    ABreakerBossProjectile();
};
