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

// O55: which of O54's two specific damage pools a hit draws. Decided by what
// DELIVERS the damage, never by what triggers it — a melee ability that swings
// the equipped weapon is Weapon-delivered and draws the weapon pool, and a
// Gunsmith deployable is a delivery mechanism for its owner rather than a pet,
// so its shots are Ability-delivered. The rule pre-answers every future case,
// which is why it is an enum on the request and not a judgement at each site.
//
// The shared pool is not a value here: it feeds BOTH lanes and is already
// inside each composed lane by the time a request is filled.
UENUM(BlueprintType)
enum class EBreakerDamageDelivery : uint8
{
    Weapon,
    Ability
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
    // ---- STAGE 6: the source split (Hook-And-Condition-Vocabulary §3.3) ----
    // SourceDamageMultiplier arrives ALREADY COMPOSED as
    // (1 + Increased/100) x MoreProduct, which is fine until the TARGET side
    // wants to add a target-conditional Increased line: folding a rider into
    // the composed value would make it a second More in everything but name,
    // outside the O3 budget and against the one-additive-bucket law. So the
    // request may carry the two halves separately. SourceDamageMultiplier
    // STAYS, as the composed convenience value every existing call site and
    // test keeps reading; the split is additive information.
    //
    // bHasSourceSplit is the presence flag: only a submission site that
    // actually snapshotted the split sets it, and ReceiveDamage recomposes
    // ONLY when it is set AND a rider fired — otherwise the request resolves
    // bit-identically to a request that never heard of the split. Today the
    // weapon paths (hitscan and rocket) fill it; ability submissions and DoT
    // tick requests stay composed-only and get riders in a later pass.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SourceIncreasedPercent = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SourceMoreProduct = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasSourceSplit = false;
    // O54/O55: which pool the source multiplier above was drawn from. Weapon is
    // the default because it is what every pre-split request meant, and because
    // an environmental hazard or a bare test rig that never sets it is not
    // ability-delivered. The target-side rider pass reads it so a weapon-lane
    // rider cannot pay on an ability hit.
    //
    // Do not fill this by hand alongside SourceDamageMultiplier — call
    // UBreakerDamageLibrary::FillSourcePools, which sets the whole source block
    // consistently. RiorsEdge.Combat.Ceiling.AbilitySubmissionConformance
    // enforces that, because a site that sets one of the four and forgets the
    // others is the failure this split can produce.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerDamageDelivery Delivery = EBreakerDamageDelivery::Weapon;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CriticalChance = 0.0f;
    // Struct default only — live requests are filled from the attribute set
    // (or its named fallback constants). O2 PLACEHOLDER
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
    // Where the hit LANDED — the trace or projectile impact point, distinct
    // from SourceLocation (where it came FROM; a rifle's SourceLocation is the
    // shooter, its ImpactLocation is the wound). Presentation only: the hit
    // context's WorldLocation prefers this so a damage number draws where the
    // shot landed instead of at the victim's pivot. Optional — paths without a
    // real impact (DoT ticks, hazards, tests) leave it unset and the dispatch
    // falls back to the victim's own location.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ImpactLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasImpactLocation = false;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float BlockMitigation = 0.5f;   // O2 PLACEHOLDER
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDamageResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float RawDamage = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MitigatedDamage = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ShieldDamage = 0.0f;
    UPROPERTY(BlueprintReadOnly) float HealthDamage = 0.0f;
    // The part of the hit that exceeded the target's remaining health.
    // HealthDamage stays clamped — it is what was actually APPLIED and every
    // vitals write depends on that — but a killing blow's true size is
    // HealthDamage + OverkillDamage, and the HUD displays that sum so a
    // 900-damage rocket on a 30 HP target reads as 900, not 30. The owner uses
    // damage numbers for TTK/balance reading; a number clamped to the corpse's
    // last sliver of health under-reports every killing blow.
    UPROPERTY(BlueprintReadOnly) float OverkillDamage = 0.0f;
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
