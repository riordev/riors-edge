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
    // Who dealt this damage. Weak so a request outliving its dealer (a rocket
    // in flight, a DoT ticking after the shooter died) never keeps the actor
    // alive and never dereferences a stale pointer. Optional: hazards and
    // tests leave it null.
    UPROPERTY(BlueprintReadWrite) TWeakObjectPtr<AActor> Instigator = nullptr;

    void SetInstigator(AActor* InInstigator) { Instigator = InInstigator; }
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

// One status carried by something that is not itself a status source — a
// projectile in flight, a zone's payload, a cycle position. The family travels
// WITH the spec because a status without its damage family resolves as
// Physical, and a Void DoT quietly resolving as Physical would bypass shields
// and take half armour mitigation it was never meant to get.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerCarriedStatus
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FBreakerStatusApplicationSpec Spec;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerDamageFamily DamageFamily = EBreakerDamageFamily::Physical;
};

// --- Healing -------------------------------------------------------------
// Healing is the MIRROR of the damage contract, not a shortcut around it
// (Ability-Implementation-Spec §5.4). Before this there was no healing path at
// all: UBreakerCombatComponent::RestoreVitals set both vitals to maximum and
// nothing else existed, so Siphon, Leech, Rend and the whole Support Medic
// branch had no primitive to build on. The failure mode this shape prevents is
// the obvious one — an ability reaching into the attribute set and writing
// Health directly, which no healing-modifier affix, no overheal rule and no
// event listener can ever see.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerHealRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Amount = 0.0f;
    // The healing-side twin of SourceDamageMultiplier. Nothing writes it yet;
    // it exists so an "Increased Healing" affix has one place to land instead
    // of being multiplied in at each call site — the bug class that
    // GearWeaponDamageMultiplier already cost this project once.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HealingMultiplier = 1.0f;
    // Leech and Rend route overheal into shield; Support's Charge loop requires
    // that overheal generate nothing, which is why Overheal is REPORTED
    // separately rather than silently discarded (Class-Kits §5, criterion 4).
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOverhealToShield = false;
    // Cap on shield granted from overheal, as a fraction of MaxShield. 1.0 is
    // "may refill the whole bar".
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float OverhealToShieldFraction = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag SourceTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer SourceTags;
    // Weak for the same reason the damage request's Instigator is: a heal over
    // time outliving its healer must never keep the healer alive.
    UPROPERTY(BlueprintReadWrite) TWeakObjectPtr<AActor> Healer = nullptr;

    void SetHealer(AActor* InHealer) { Healer = InHealer; }
};

// The vitals a heal resolves against. Separated from FBreakerDefenseState so
// the pure resolution function cannot accidentally read a defensive roll:
// healing is never dodged and never blocked.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerVitalsState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Health = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHealth = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Shield = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxShield = 0.0f;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerHealResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float RequestedAmount = 0.0f;
    UPROPERTY(BlueprintReadOnly) float HealthHealed = 0.0f;
    // Support Charge MUST generate 0 from this (Class-Kits §5).
    UPROPERTY(BlueprintReadOnly) float Overheal = 0.0f;
    // Leech's overheal -> shield routing. Zero unless the request asked for it.
    UPROPERTY(BlueprintReadOnly) float ShieldGranted = 0.0f;
    UPROPERTY(BlueprintReadOnly) float RemainingHealth = 0.0f;
    UPROPERTY(BlueprintReadOnly) float RemainingShield = 0.0f;
    // True when the target was already at full health, so a caller can tell
    // "healed for nothing" from "healed for nothing because it was capped".
    UPROPERTY(BlueprintReadOnly) bool bWasAtFullHealth = false;
};

// Healer-side view of one resolved heal, the twin of FBreakerHitContext.
// Broadcast on the HEALER's combat component so Support's Charge generation
// and future healing-done affixes have the same shape to bind as damage does.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerHealContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Healer = nullptr;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target = nullptr;
    UPROPERTY(BlueprintReadOnly) FBreakerHealResult Result;
    UPROPERTY(BlueprintReadOnly) FGameplayTag SourceTag;
};

// Attacker-side view of one resolved damage instance (SI-8). Broadcast on the
// INSTIGATOR's combat component, not the victim's.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerHitContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Instigator = nullptr;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target = nullptr;
    UPROPERTY(BlueprintReadOnly) FBreakerDamageResult Result;
    UPROPERTY(BlueprintReadOnly) bool bFromDoT = false;
    UPROPERTY(BlueprintReadOnly) bool bWeakPoint = false;
    UPROPERTY(BlueprintReadOnly) EBreakerDamageFamily DamageFamily = EBreakerDamageFamily::Physical;
    UPROPERTY(BlueprintReadOnly) FVector WorldLocation = FVector::ZeroVector;
};
