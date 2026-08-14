#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerProjectileBase.generated.h"

class UPointLightComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerProjectileImpact, AActor*, HitActor, const FVector&, Location);

// The reusable player/enemy projectile (Ability-Implementation-Spec §5.5's
// "MISSING HOOK: extract ABreakerProjectileBase").
//
// Two projectiles existed before this and both hardcoded their own everything:
// ABreakerEnemyProjectile and ABreakerRocketProjectile each built their own
// sphere, their own movement component, their own instigator-ignore, their own
// lifetime, their own impact handler and their own cosmetic multicast. Fracture
// would have been a third copy, and Gunsmith's Mine Cluster a fourth.
//
// What the base owns:
//   * travel — a replicated actor with a ProjectileMovementComponent, straight
//     by default, gravity as a parameter;
//   * impact resolution through the ORDINARY damage contract, with a proper
//     Instigator, so armour, shields, the passive dodge/block layer, incoming
//     modifiers and kill credit all apply;
//   * an optional carried status, because a projectile that applies a status is
//     a shape three abilities already want;
//   * a lifetime, so a shot that leaves the arena cannot leak;
//   * presentation hooks — a colourable orb and a light, both of which every
//     existing projectile builds by hand, plus a cosmetic multicast on impact.
//
// What it deliberately does NOT own: how it was aimed, splash (the rocket's
// business), and what it looks like beyond the placeholder primitives.
//
// ABreakerRocketProjectile is NOT re-expressed on this base. It lives in
// Weapons/, which this lane does not own, and its explosion, its multi-part
// dark-body-plus-flame presentation and its detonation-flash lifetime are
// entangled enough that folding them in without a playtest would be a
// behaviour change disguised as a refactor. The extraction is mechanical when
// someone who owns Weapons/ wants it.
UCLASS()
class RIORSEDGE_API ABreakerProjectileBase : public AActor
{
    GENERATED_BODY()

public:
    ABreakerProjectileBase();

    // Server-side arming. The damage request carries the shooter as its
    // Instigator; the projectile fills in the impact position and a seed.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Projectile")
    void InitializeProjectile(const FBreakerDamageRequest& InDamage, const FVector& Direction, float Speed);

    // Optional statuses carried to the impact. Set before or after
    // InitializeProjectile; both are server-side arming. A LIST rather than one
    // status because MS7 makes Fracture apply two cycle positions per cast, and
    // a projectile that can carry only one would force that upgrade to spawn
    // two projectiles for one shot.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Projectile")
    void AddImpactStatus(const FBreakerCarriedStatus& InStatus);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Projectile")
    void SetImpactStatuses(const TArray<FBreakerCarriedStatus>& InStatuses);
    UFUNCTION(BlueprintPure, Category="Projectile") const TArray<FBreakerCarriedStatus>& GetImpactStatuses() const { return ImpactStatuses; }

    UFUNCTION(BlueprintPure, Category="Projectile") const FBreakerDamageRequest& GetProjectileDamage() const { return Damage; }
    UFUNCTION(BlueprintPure, Category="Projectile") bool HasImpacted() const { return bImpacted; }

    UPROPERTY(BlueprintAssignable, Category="Projectile") FBreakerProjectileImpact OnImpact;

    // Pure travel maths, world-free and testable in the precedent of
    // Combat/BreakerRangedBehavior.h. Straight-line only, which is what every
    // projectile in this project is today; an arcing shot needs its own solve
    // and must not pretend this one covers it.
    UFUNCTION(BlueprintPure, Category="Projectile")
    static FVector PositionAfter(const FVector& Origin, const FVector& Direction, float Speed, float Seconds);
    UFUNCTION(BlueprintPure, Category="Projectile")
    static float FlightTimeOver(float DistanceCm, float Speed);
    // How far a shot travels before its lifetime kills it. A projectile whose
    // range is shorter than the ability that fires it is a silent dead zone at
    // the edge of the reticle, so the two are checkable against each other.
    UFUNCTION(BlueprintPure, Category="Projectile")
    static float MaximumTravelDistance(float Speed, float LifetimeSeconds);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile", meta=(ClampMin="0")) float MaximumLifetime = 6.0f;
    // Collision radius, applied in BeginPlay so a subclass can retune it in its
    // own constructor and an instance can retune it in the editor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile", meta=(ClampMin="1")) float CollisionRadiusCm = 30.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile", meta=(ClampMin="0")) float GravityScale = 0.0f;

    // --- Presentation (O2 PLACEHOLDER, all of it) -------------------------
    // The same asset-free placeholder pattern the gym dressing and the loot
    // pickups use: an engine sphere plus a point light, coloured through a
    // dynamic material instance. A Blueprint art pass replaces the mesh and the
    // material and no rule changes.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile|Presentation") bool bShowFallbackVisual = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile|Presentation") FLinearColor OrbColor = FLinearColor(0.62f, 0.14f, 0.92f);
    // BasicShapes/Sphere is 100 cm across, so 0.85 is a ~42 cm orb.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile|Presentation", meta=(ClampMin="0")) float VisualScale = 0.85f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile|Presentation", meta=(ClampMin="0")) float GlowIntensity = 2400.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile|Presentation", meta=(ClampMin="0")) float GlowRadius = 700.0f;

protected:
    virtual void BeginPlay() override;

    UFUNCTION() void HandleImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

    // Server-side. Runs once — the bImpacted latch is in Impact(), not here, so
    // an override cannot accidentally re-enter.
    void Impact(AActor* HitActor, const FVector& Location);

    // What actually happens at the impact point. The default is single-target
    // damage plus the carried status; the rocket's splash would be an override.
    virtual void ResolveImpact(AActor* HitActor, const FVector& Location);

    // Whether this actor can be hurt by this projectile. Default: anything with
    // a combat component that is not the shooter. Overridden by
    // ABreakerEnemyProjectile to spare other enemies.
    virtual bool ShouldDamageActor(AActor* Candidate) const;

    // Collision the projectile must not be stopped by. Called in BeginPlay.
    virtual void ConfigureIgnoredActors();

    // Everything after the damage: whether to die, and what to show. Runs on
    // every machine via the multicast.
    virtual void PlayImpactCosmetics(const FVector& Location);

    UFUNCTION(NetMulticast, Unreliable) void MulticastImpactCosmetics(const FVector& Location);

    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

    FBreakerDamageRequest Damage;
    TArray<FBreakerCarriedStatus> ImpactStatuses;
    bool bImpacted = false;
};
