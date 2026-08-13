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
    // SECOND PASS, and the honest judgement is that the previous build did not
    // read as a rocket. It was an ORANGE casing with a cone nose, a cross of
    // fins, a sphere stuck on the back, and a 540 deg/s roll — five bright
    // shapes tumbling together, which is a spinning toy. Three things were
    // wrong about it:
    //   * the body was the same orange as its own flame, so nothing separated
    //     the object from the thrust;
    //   * fins on a 50 cm primitive at 3000 cm/s are never resolved by the eye
    //     and only add silhouette noise;
    //   * a rolling warhead is not a thing. A rocket does not spin; it burns.
    // So: a DARK body carrying a bright tail. The casing and nose are panel
    // greys, the exhaust is the only orange, and the motion in it is a flicker
    // of the flame rather than a rotation of the object. Nothing here is teal;
    // teal belongs to rift geometry, suppression hardware and Anomalous items.

    // How long the detonation flash stays on screen after the damage lands.
    // The actor survives for this long with collision and movement off, which
    // is why the explosion needs no second actor and no pool.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket|Presentation", meta=(ClampMin="0"))
    float ExplosionFlashSeconds = 0.35f;
    // Exhaust flame length in centimetres, and how far it flickers either side
    // of that. The flicker is the ONLY animation on the rocket.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket|Presentation", meta=(ClampMin="0"))
    float ExhaustLengthCm = 46.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket|Presentation", meta=(ClampMin="0", ClampMax="1"))
    float ExhaustFlickerAmount = 0.22f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket|Presentation", meta=(ClampMin="0"))
    float ExhaustFlickerHz = 26.0f;

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
    // Body: a stretched cube for the casing and a cone for the nose, both dark
    // and lit. Three primitives total, no assets. The fins are gone.
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Nose;
    // The flame: unlit additive, so it reads as light rather than as a painted
    // ball, and doubles as the detonation fireball on impact.
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Exhaust;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> ExhaustMaterial;

private:
    FBreakerDamageRequest Damage;
    float ExplosionRadius = 350.0f;
    bool bExploded = false;
};
