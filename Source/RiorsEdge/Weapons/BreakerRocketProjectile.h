#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerRocketProjectile.generated.h"

class UPointLightComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerRocketExploded, const FVector&, Location, float, Radius);

// Server-spawned rocket. Flies straight, explodes on first blocking hit, and
// applies the carried damage request to every combat component in the
// explosion radius with linear distance falloff. Ignores its instigator
// until self-damage rules get their design pass.
UCLASS()
class RIORSEDGE_API ABreakerRocketProjectile : public AActor
{
    GENERATED_BODY()

public:
    ABreakerRocketProjectile();

    void InitializeRocket(const FBreakerDamageRequest& InDamage, float Speed, float InExplosionRadius);

    UPROPERTY(BlueprintAssignable, Category="Rocket") FBreakerRocketExploded OnExploded;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket", meta=(ClampMin="0", ClampMax="1")) float EdgeDamageFraction = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket", meta=(ClampMin="0")) float MaximumLifetime = 8.0f;

    // --- Presentation -------------------------------------------------------
    // Placeholder primitives + dynamic material instances, exactly like the
    // gym's props: a Blueprint art pass replaces the meshes and materials
    // without touching a rule. Every value below is O2 PLACEHOLDER.
    //
    // The rocket carries the same weapon/heat orange as the hitscan tracer,
    // because both are the player delivering damage and they must read as one
    // game. Nothing here is teal; teal belongs to rift geometry, suppression
    // hardware and Anomalous items.

    // How long the detonation flash stays on screen after the damage lands.
    // The actor survives for this long with collision and movement off, which
    // is why the explosion needs no second actor and no pool.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket|Presentation", meta=(ClampMin="0"))
    float ExplosionFlashSeconds = 0.35f;
    // Roll rate of the warhead, degrees per second. Purely so the round reads
    // as an object rather than a decal sliding through the air.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket|Presentation")
    float SpinDegreesPerSecond = 540.0f;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION() void HandleImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
    void Explode(const FVector& Location);
    UFUNCTION(NetMulticast, Unreliable) void MulticastExplosionCosmetics(const FVector& Location, float Radius);
    // Runs on every machine: swaps the rocket's body for a short detonation
    // flash in place. Called from the multicast, so a client sees it even
    // though only the server resolved the damage.
    void PlayExplosionCosmetics(const FVector& Location, float Radius);

    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
    // Body: a stretched cube for the casing, a cone for the nose, a flat fin
    // pair, and an emissive exhaust bloom. Four primitives, no assets.
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Nose;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FinVertical;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FinHorizontal;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Exhaust;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

private:
    FBreakerDamageRequest Damage;
    float ExplosionRadius = 350.0f;
    bool bExploded = false;
};
