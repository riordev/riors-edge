#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerRocketProjectile.generated.h"

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

protected:
    virtual void BeginPlay() override;
    UFUNCTION() void HandleImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
    void Explode(const FVector& Location);
    UFUNCTION(NetMulticast, Unreliable) void MulticastExplosionCosmetics(const FVector& Location, float Radius);

    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

private:
    FBreakerDamageRequest Damage;
    float ExplosionRadius = 350.0f;
    bool bExploded = false;
};
