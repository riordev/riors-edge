#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerEnemyProjectile.generated.h"

class UPointLightComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerEnemyProjectileImpact, const FVector&, Location);

// The thing the owner asked to be able to SEE. A real replicated projectile
// actor — not a hitscan with a cosmetic streak — so the player can watch it
// leave the muzzle and step out of its line.
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
class RIORSEDGE_API ABreakerEnemyProjectile : public AActor
{
    GENERATED_BODY()

public:
    ABreakerEnemyProjectile();

    // Server-side arming. Damage carries the shooter as its Instigator, so the
    // hit routes through the ordinary damage contract and credits correctly.
    void InitializeProjectile(const FBreakerDamageRequest& InDamage, const FVector& Direction, float Speed);

    UPROPERTY(BlueprintAssignable, Category="Projectile") FBreakerEnemyProjectileImpact OnImpact;

    // Generous: at the archetype's default speed this is far longer than any
    // legal flight, so it only ever catches a shot that left the arena.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile", meta=(ClampMin="0")) float MaximumLifetime = 6.0f;

    // O2 PLACEHOLDER. Read against bright sand from ~20m; unverified in play.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile") FLinearColor OrbColor = FLinearColor(0.62f, 0.14f, 0.92f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile", meta=(ClampMin="0")) float GlowIntensity = 2400.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile", meta=(ClampMin="0")) float GlowRadius = 700.0f;

protected:
    virtual void BeginPlay() override;
    UFUNCTION() void HandleImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
    void Impact(AActor* HitActor, const FVector& Location);
    UFUNCTION(NetMulticast, Unreliable) void MulticastImpactCosmetics(const FVector& Location);

    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

private:
    FBreakerDamageRequest Damage;
    bool bImpacted = false;
};
