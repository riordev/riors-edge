#include "Combat/BreakerEnemyProjectile.h"

#include "Combat/BreakerEnemy.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"

ABreakerEnemyProjectile::ABreakerEnemyProjectile()
{
    // Every value below is the value this projectile shipped with before the
    // base class existed; the re-expression is deliberately not a retune.
    // All O2 PLACEHOLDER.
    CollisionRadiusCm = 30.0f;
    MaximumLifetime = 6.0f;              // generous: only catches a shot that left the arena
    VisualScale = 0.85f;                 // ~42 cm orb, roughly a beach ball. Big on purpose.
    OrbColor = FLinearColor(0.62f, 0.14f, 0.92f);
    GlowIntensity = 2400.0f;
    GlowRadius = 700.0f;
}

void ABreakerEnemyProjectile::ConfigureIgnoredActors()
{
    Super::ConfigureIgnoredActors();
    if (!Collision) return;
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            Collision->IgnoreActorWhenMoving(*It, true);
        }
    }
}

bool ABreakerEnemyProjectile::ShouldDamageActor(AActor* Candidate) const
{
    // Single target, no splash. Splash would punish a player who dodged
    // correctly, which is the opposite of the point.
    if (Candidate && Candidate->IsA<ABreakerEnemy>()) return false;
    return Super::ShouldDamageActor(Candidate);
}
