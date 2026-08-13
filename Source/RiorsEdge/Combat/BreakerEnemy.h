#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerEnemy.generated.h"

class UAbilitySystemComponent;
class UBreakerAttributeSet;
class UBreakerCombatComponent;
class UBreakerStatusComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class USphereComponent;
class UBoxComponent;

UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerEnemy : public APawn, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ABreakerEnemy();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintCallable, Category="Enemy") void ConfigureEncounter(const FVector& NewLeashOrigin, float NewPatrolPhase);
    // Elite modifier: bigger, tougher, hits harder, and its drops are never
    // below Exceptional.
    UFUNCTION(BlueprintCallable, Category="Enemy") void ConfigureElite();
    // Wave spawns: no respawn, scaled level.
    UFUNCTION(BlueprintCallable, Category="Enemy") void ConfigureWave(int32 NewEnemyLevel) { bRespawns = false; EnemyLevel = FMath::Clamp(NewEnemyLevel, 1, 50); }
    UFUNCTION(BlueprintPure, Category="Enemy") bool IsElite() const { return bIsElite; }
    UFUNCTION(BlueprintPure, Category="Enemy") FString GetEnemyStateLabel() const;

protected:
    virtual void BeginPlay() override;
    UFUNCTION() void HandleDeath();
    UFUNCTION() void HandleDamageReceived(const FBreakerDamageResult& Result);
    void GrantLoot();
    // Ammo economy: kills return reserve ammo to the killer (O2 placeholder).
    void GrantAmmo();
    void RespawnEnemy();
    virtual void PerformAttack(APawn* TargetPawn);
    // Shows/hides the whole humanoid assembly across death and respawn.
    // Virtual so archetypes with extra presentation (a charging emitter) can
    // clear it on the same edges.
    virtual void SetBodyVisible(bool bVisible);

    // The per-frame decision an enemy makes while it has a live target. The
    // base implementation is the melee three-gear chase below; ranged
    // archetypes override it to hold an engagement band instead of closing.
    // Everything shared — target selection, the safe-zone rules, applying the
    // move, and the ground snap — stays in Tick and is not overridable.
    virtual void TickEngagedBehaviour(class ABreakerCharacter* Player, float Distance, float DeltaSeconds,
        FVector& OutDirection, float& OutSpeedScale);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCapsuleComponent> BodyCollision;
    // Primitive humanoid assembly. BodyVisual is the torso; the rest are
    // cosmetic siblings under BodyCollision, all NoCollision.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> BodyVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> HeadVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> LeftArmVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> RightArmVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> LeftLegVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> RightLegVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> BodyHitBox;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> WeakPoint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> WeakPointVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBreakerAttributeSet> Attributes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBreakerCombatComponent> Combat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBreakerStatusComponent> Status;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float DetectionRange = 2200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float AttackRange = 260.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float MoveSpeed = 330.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float AttackDamage = 14.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float AttackCooldown = 1.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float RespawnDelay = 3.0f;
    // Item level source for drops. Zone-based sourcing is still an open
    // design question; enemy level is the gym's stand-in.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="1", ClampMax="50")) int32 EnemyLevel = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy") bool bDropsLoot = true;
    // Wave-mode enemies die for good instead of recycling.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy") bool bRespawns = true;
    // Death chain: dying enemies detonate for a fraction of their max
    // health, damaging nearby enemies (never the player). Dense packs pop.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy") bool bExplodesOnDeath = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0", ClampMax="1")) float DeathExplosionHealthFraction = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float DeathExplosionRadius = 420.0f;

    // --- Approach feel (all O2 placeholders) -------------------------------
    // Owner feedback: "the models just slowly walk at me". The chase now has
    // three gears — closing sprint, weave, committed lunge — so the approach
    // reads as intent rather than a conveyor belt. Inspired by Skitter's
    // committed leap (Encounter-Design §2) without the full archetype roster.

    // Beyond this range the enemy sprints to close the gap.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0")) float SprintRange = 1200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="1")) float SprintSpeedMultiplier = 1.4f;
    // Lateral sinusoidal weave layered on the chase vector. Per-enemy phase
    // (seeded from PatrolPhase) keeps a pack from strafing in lockstep.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0")) float WeaveFrequency = 1.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0", ClampMax="1")) float WeaveStrength = 0.45f;
    // Committed lunge: a short burst inside this range, telegraphed one
    // frame ahead through StateLabel so the HUD shows "LUNGE".
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0")) float LungeRange = 450.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="1")) float LungeSpeedMultiplier = 2.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0")) float LungeDuration = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Approach", meta=(ClampMin="0")) float LungeCooldown = 1.2f;

    // Shared state a derived archetype legitimately needs to read or drive.
    // PatrolPhase doubles as the per-enemy desync seed so a pack never acts in
    // lockstep; StateLabel is what the playtest HUD prints over the enemy.
    float PatrolPhase = 0.0f;
    double LastAttackTime = -1000.0;
    bool bDead = false;
    bool bIsElite = false;
    FString StateLabel = TEXT("PATROL");
    // Optional facing override for one frame. Zero means "face the direction
    // you are moving", which is the melee behaviour. A ranged enemy strafes
    // sideways while facing the player, so it sets this every frame.
    FVector DesiredFacing = FVector::ZeroVector;

private:
    FVector LeashOrigin = FVector::ZeroVector;
    int32 KillCount = 0;
    double FirstDamageTime = -1.0;
    // Engagement-gapped TTK: sums time between damage events, capping each
    // gap, so tagging an enemy and returning later doesn't book the idle
    // time as time-to-kill.
    double LastDamageEventTime = -1.0;
    float EngagedSeconds = 0.0f;
    float WeaveTime = 0.0f;
    double LungeStartTime = -1000.0;
};
