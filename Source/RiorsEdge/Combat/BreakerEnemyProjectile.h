#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerProjectileBase.h"
#include "BreakerEnemyProjectile.generated.h"

// The thing the owner asked to be able to SEE. A real replicated projectile
// actor — not a hitscan with a cosmetic streak — so the player can watch it
// leave the muzzle and step out of its line.
//
// RE-EXPRESSED on ABreakerProjectileBase. Everything it used to build by hand
// (the sphere, the light, the movement component, the instigator ignore, the
// lifetime, the impact handler, the cosmetic multicast) is the base's now, and
// every number below is the number it shipped with. What remains here is the
// only thing that was ever specific to an enemy shot: it does not hit other
// enemies, either as an obstacle or as a target.
//
// Everything here is deliberately replaceable presentation built from engine
// primitives: an oversized sphere plus a point light. A Blueprint pass dresses
// it later; the contract that must survive is "large, slow, replicated,
// single-target".
//
// Colour: saturated violet-magenta. Not teal — the teal object law
// (UI-Style-Guide-Fieldplate) reserves saturated teal for rift objects and
// suppression hardware, and this is neither. Violet also separates cleanly
// from the gym's warm sand and its blue sky, which orange would not.
UCLASS()
class RIORSEDGE_API ABreakerEnemyProjectile : public ABreakerProjectileBase
{
    GENERATED_BODY()

public:
    ABreakerEnemyProjectile();

protected:
    // Enemies never block each other's shots. A pack whose front rank eats the
    // back rank's volley reads as a bug, and enemy friendly fire is not a
    // system this project has ruled on.
    virtual void ConfigureIgnoredActors() override;
    virtual bool ShouldDamageActor(AActor* Candidate) const override;
};
