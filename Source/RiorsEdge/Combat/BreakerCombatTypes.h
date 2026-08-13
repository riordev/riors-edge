#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerCombatTypes.generated.h"

UENUM(BlueprintType)
enum class EBreakerDamageFamily : uint8
{
    Physical,
    Elemental,
    TrueDamage
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDamageRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BaseDamage = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerDamageFamily DamageFamily = EBreakerDamageFamily::Physical;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag DamageTypeTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer SourceTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SourceDamageMultiplier = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CriticalChance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CriticalMultiplier = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WeakPointMultiplier = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ArmorPenetration = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProcCoefficient = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCanCritical = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bWeakPointHit = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBypassShield = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsDamageOverTime = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUseSnapshotCritical = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSnapshotCriticalResult = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RandomSeed = 0;
    // Where the hit came from, for frontal block checks. Optional so tests
    // and hazards without a position keep working.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector SourceLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasSourceLocation = false;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDefenseState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Health = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Shield = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Armor = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float IncomingDamageMultiplier = 1.0f;

    // Passive defensive layers — not inputs. Dodge is a chance to evade a
    // hit entirely; block is a chance to reduce it. Classes and gear supply
    // the chances. Neither applies to damage over time.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float DodgeChance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float BlockChance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float BlockMitigation = 0.5f;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDamageResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float RawDamage = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MitigatedDamage = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ShieldDamage = 0.0f;
    UPROPERTY(BlueprintReadOnly) float HealthDamage = 0.0f;
    UPROPERTY(BlueprintReadOnly) float RemainingShield = 0.0f;
    UPROPERTY(BlueprintReadOnly) float RemainingHealth = 0.0f;
    UPROPERTY(BlueprintReadOnly) bool bCritical = false;
    UPROPERTY(BlueprintReadOnly) bool bDodged = false;
    UPROPERTY(BlueprintReadOnly) bool bBlocked = false;
    UPROPERTY(BlueprintReadOnly) bool bWeakPoint = false;
    UPROPERTY(BlueprintReadOnly) bool bShieldBroken = false;
    UPROPERTY(BlueprintReadOnly) bool bKilled = false;
};
