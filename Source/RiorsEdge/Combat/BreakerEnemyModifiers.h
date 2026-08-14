#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Combat/BreakerEnemyFamily.h"
#include "BreakerEnemyModifiers.generated.h"

// ENEMY MODIFIERS — Encounter-Design §1.2-§1.4, and the missing half of O27.
//
// EBreakerMonsterRank::ModifierBearing existed before this file and multiplied
// health and damage by 2.5x/1.25x. Nothing anywhere authored an actual
// modifier, so the rank was a stat bump wearing the name of a system. O27 puts
// difficulty in "bosses, elites or monsters with MODIFIERS" — with no modifiers
// authored, the only place difficulty could live was health, which is exactly
// the sponge the ruling forbids.
//
// THREE RULES THIS HEADER IS BUILT AROUND
//
// 1. A modifier changes a RULE, never a percentage. "+30% damage" is the stat
//    chassis, not a modifier (Encounter-Design §1.2 test 3). Every entry below
//    changes what the player has to DO.
//
// 2. NOTHING HERE CAN READ THE PLAYER. Every function is pure and takes only a
//    modifier, an authored parameter block, and ENEMY-side numbers. That is not
//    a style preference: player-scaled difficulty is the failure mode O27
//    exists to forbid, and a function that cannot see a player cannot commit
//    it. Two DELIBERATE DEVIATIONS from Encounter-Design §1.2 follow from this
//    and are called out at their parameters:
//      * Volatile's detonation is authored in Encounter-Design as "45% of
//        PLAYER max health". That reads the player. It is re-expressed here as
//        a multiple of the monster's own chassis damage.
//      * Reflective's per-instance cap is authored as "6% of PLAYER max
//        health". Re-expressed as a fraction of the MONSTER's max health.
//    Both keep the design intent (a detonation worth clearing, a reflect that
//    cannot punish a high-DPS build out of existence) without the player read.
//
// 3. Composition is CONSTRAINED, not free (§1.3). Two modifiers that together
//    have no answer are worse than one hard modifier, so forbidden pairs and a
//    pressure-diversity rule are enforced at the point of selection AND
//    checkable after the fact.
//
// O2: every value in FBreakerEnemyModifierParams is EditAnywhere and a
// PLACEHOLDER. None of it is playtested; automation proves the arithmetic and
// the legality rules, and cannot see whether a tell reads on a screen.

// The authored set. Encounter-Design §1.2 lists ten; nine of them are here.
//
// SUPPRESSING IS DELIBERATELY ABSENT. §1.2 tags it NEEDS-RECOST [O1/O2]:
// half its pressure was a stamina drain, O1 deleted the stamina pool, and the
// surviving air-control reduction both lives in Movement/ (not this lane) and
// is too thin to carry an Uncommon weight class on its own. Authoring a
// replacement second effect is a new value decision and is frozen by O2.
// Shipping it as a stub would have been worse than its absence: an enemy
// visibly carrying a modifier that does almost nothing teaches the player that
// modifiers do not matter.
//
// WAKEFUL is §1.2's optional #12, promoted into the shipping set because the
// starting roster needed a fourth pressure kind's worth of variety and it is
// already designed. TETHERED (#11) is not here: it is a rule about a PAIR of
// enemies and needs a spawn-time pairing contract that nothing in Game/ can
// express yet.
UENUM(BlueprintType)
enum class EBreakerEnemyModifier : uint8
{
    None UMETA(Hidden),

    // --- Common ---------------------------------------------------------
    // Regenerating shield. The rule change: DoT bypasses it entirely (the
    // status pipeline already sets bBypassShield on every tick), so a bleed
    // build answers a Warded enemy that a burst build has to out-race.
    Warded,
    // Death detonation on a fuse. Punishes killing things next to you.
    Volatile,
    // Much faster, and it circles instead of standing. Punishes open ground.
    Fleetfoot,
    // Cannot be moved, and its hits SLOW you. The damage is not the punish;
    // being converted into a stationary player is.
    Anchored,

    // --- Uncommon -------------------------------------------------------
    // Death spawns two weaker copies. Punishes killing it inside the pack.
    Splitting,
    // Nearby allies take reduced damage. A target-priority puzzle.
    WardingAura,
    // Returns a fraction of damage dealt. Punishes holding the trigger.
    Reflective,

    // --- Rare -----------------------------------------------------------
    // Periodically blinks toward you, briefly untargetable. Punishes fighting
    // with your back to geometry.
    Phasing,
    // Its attacks leave lingering hazards. The arena degrades; the fight
    // becomes an eviction.
    Cascading,
    // Revives once, unless the killing blow was a weak point. Punishes
    // spraying the body.
    Wakeful,

    Count UMETA(Hidden)
};

// §1.3's diversity axis. A 2+ modifier enemy must draw from at least two of
// these, and a 3-modifier enemy may not take two Durability entries — that
// combination is how sponges are born.
UENUM(BlueprintType)
enum class EBreakerModifierPressure : uint8
{
    Durability,
    SpaceDenial,
    Mobility,
    Attrition
};

// Which FAMILY a modifier is allowed to appear on (Story-Source §1.5).
//
// A modifier is a rule the enemy plays by, and a rule that reads as tactical
// discipline makes sense on an Altered soldier and actively contradicts the
// fiction on a rift-native Vestige — §1.5 says a Vestige has "no tactics that
// resemble a military", and a Vestige that plants its stance to hold ground or
// projects a squad-protection aura is a Vestige with tactics.
//
// The split is deliberately SMALL. Only five of the ten are scoped; the rest
// are things a body does rather than things a soldier decides, and scoping
// those would shrink both pools for no fictional gain.
UENUM(BlueprintType)
enum class EBreakerModifierFamilyScope : uint8
{
    // Legal on anything. Detonating, regenerating, moving fast, leaving a
    // trail, getting back up — none of these imply a decision.
    Any,
    // Alien-body rules: splitting into copies, phasing out of the world,
    // returning damage off a faceted shell. A trained soldier does none of
    // them, and seeing one do them would read as a bug rather than as horror.
    VestigeOnly,
    // Tactical rules: planting to hold ground, protecting the squad. These are
    // decisions, and only something that was a person makes them.
    AlteredOnly
};

// §1.4's selection weights. Internal names only — never surfaced in UI using
// the words Aberrant or Anomalous, which are item rarities (§1.4 naming note).
UENUM(BlueprintType)
enum class EBreakerModifierWeightClass : uint8
{
    Common,
    Uncommon,
    Rare
};

// Everything an owner would plausibly tune about the modifier layer, in one
// authored block — the same shape and the same reasoning as
// FBreakerMonsterChassisParams. O2: every value is a PLACEHOLDER.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerEnemyModifierParams
{
    GENERATED_BODY()

    // --- Composition (Encounter-Design §1.1, §1.4) -----------------------

    // Health step per modifier BEYOND the first (§1.1: "+0.35x per modifier
    // beyond the first"). Composes on top of the ModifierBearing rank row in
    // the chassis rather than replacing it, so there is still exactly one
    // source of truth for what a rank is worth.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Composition", meta=(ClampMin="0"))
    float HealthStepPerExtraModifier = 0.35f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Composition", meta=(ClampMin="0"))
    float WeightOneModifier = 65.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Composition", meta=(ClampMin="0"))
    float WeightTwoModifiers = 28.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Composition", meta=(ClampMin="0"))
    float WeightThreeModifiers = 7.0f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Composition", meta=(ClampMin="0"))
    float CommonWeight = 100.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Composition", meta=(ClampMin="0"))
    float UncommonWeight = 45.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Composition", meta=(ClampMin="0"))
    float RareWeight = 15.0f;   // O2 PLACEHOLDER

    // --- Warded ----------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Warded", meta=(ClampMin="0"))
    float WardShieldFractionOfHealth = 0.60f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Warded", meta=(ClampMin="0"))
    float WardRechargeDelaySeconds = 4.0f;   // O2 PLACEHOLDER
    // Fraction of the full ward restored per second once the delay elapses.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Warded", meta=(ClampMin="0"))
    float WardRechargeRatePerSecond = 0.50f;   // O2 PLACEHOLDER

    // --- Volatile --------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Volatile", meta=(ClampMin="0"))
    float VolatileFuseSeconds = 1.2f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Volatile", meta=(ClampMin="0"))
    float VolatileInnerRadiusCm = 500.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Volatile", meta=(ClampMin="0"))
    float VolatileOuterRadiusCm = 900.0f;   // O2 PLACEHOLDER
    // DELIBERATE DEVIATION from Encounter-Design §1.2, and the reason is O27.
    // The document authors this as "45% of PLAYER max health" — a number the
    // monster cannot know without reading the player. Expressed instead as a
    // multiple of the monster's own chassis damage, which is already a curve in
    // area level, so a Volatile detonation scales with the CONTENT exactly like
    // every other source of enemy damage. Nine attacks' worth is a real threat
    // at any area level and never a percentage of a health bar it cannot see.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Volatile", meta=(ClampMin="0"))
    float VolatileDamageAsAttackMultiple = 9.0f;   // O2 PLACEHOLDER

    // --- Fleetfoot -------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Fleetfoot", meta=(ClampMin="1"))
    float FleetfootSpeedMultiplier = 1.55f;   // O2 PLACEHOLDER
    // Circling instead of standing: the base melee weave, turned up.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Fleetfoot", meta=(ClampMin="0", ClampMax="1"))
    float FleetfootWeaveStrength = 0.85f;   // O2 PLACEHOLDER

    // --- Anchored --------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Anchored", meta=(ClampMin="0", ClampMax="1"))
    float AnchoredSlowFraction = 0.40f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Anchored", meta=(ClampMin="0"))
    float AnchoredSlowSeconds = 2.0f;   // O2 PLACEHOLDER

    // --- Splitting -------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Splitting", meta=(ClampMin="0"))
    int32 SplitCount = 2;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Splitting", meta=(ClampMin="0", ClampMax="1"))
    float SplitHealthFraction = 0.25f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Splitting", meta=(ClampMin="0"))
    float SplitScatterCm = 180.0f;   // O2 PLACEHOLDER

    // --- Warding Aura ----------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|WardingAura", meta=(ClampMin="0"))
    float AuraRadiusCm = 900.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|WardingAura", meta=(ClampMin="0", ClampMax="1"))
    float AuraDamageReduction = 0.35f;   // O2 PLACEHOLDER

    // --- Reflective ------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Reflective", meta=(ClampMin="0", ClampMax="1"))
    float ReflectFraction = 0.12f;   // O2 PLACEHOLDER
    // DELIBERATE DEVIATION, same reason as Volatile: the document caps this at
    // "6% of PLAYER max health". Capped instead against the MONSTER's own max
    // health, which is a chassis curve, so the cap grows with area level and
    // never with the player's build. The cap itself is the load-bearing part —
    // §1.2 is explicit that an uncapped reflect punishes high-DPS builds for
    // existing, which is a build tax and not a mechanic.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Reflective", meta=(ClampMin="0", ClampMax="1"))
    float ReflectCapFractionOfMonsterHealth = 0.05f;   // O2 PLACEHOLDER

    // --- Phasing ---------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Phasing", meta=(ClampMin="0.1"))
    float PhaseIntervalSeconds = 6.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Phasing", meta=(ClampMin="0"))
    float PhaseDistanceCm = 800.0f;   // O2 PLACEHOLDER
    // Untargetable window. Short on purpose: O1 makes movement the only
    // defensive input, so a long untargetable window is dead time the player
    // cannot act on.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Phasing", meta=(ClampMin="0"))
    float PhaseBlinkSeconds = 0.35f;   // O2 PLACEHOLDER
    // The tell. It must precede the blink or the blink is an unannounced
    // teleport, which §1.2 test 1 forbids and which reads as a bug.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Phasing", meta=(ClampMin="0"))
    float PhaseTelegraphSeconds = 0.55f;   // O2 PLACEHOLDER
    // It never blinks past you into your face: a blink that ends inside melee
    // range is a teleport-and-hit, which passive defence has no answer to.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Phasing", meta=(ClampMin="0"))
    float PhaseMinimumStandoffCm = 420.0f;   // O2 PLACEHOLDER

    // --- Cascading -------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Cascading", meta=(ClampMin="0"))
    float HazardDurationSeconds = 5.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Cascading", meta=(ClampMin="0"))
    float HazardRadiusCm = 320.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Cascading", meta=(ClampMin="0.05"))
    float HazardTickInterval = 0.5f;   // O2 PLACEHOLDER
    // Per TICK, as a fraction of one of the monster's attacks. Low on purpose:
    // the hazard is an eviction notice, not a damage source. §1.2's counterplay
    // line is "the arena degrades over time", and a hazard that kills quickly
    // converts that into an instant loss instead.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Cascading", meta=(ClampMin="0"))
    float HazardTickDamageAsAttackFraction = 0.25f;   // O2 PLACEHOLDER
    // Encounter-Design §5.3 caps simultaneous ground hazards at 8. Past that,
    // arena denial is an eviction with nowhere to go.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Cascading", meta=(ClampMin="1"))
    int32 MaximumLiveHazardsPerEnemy = 4;   // O2 PLACEHOLDER

    // --- Wakeful ---------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Wakeful", meta=(ClampMin="0", ClampMax="1"))
    float WakefulReviveHealthFraction = 0.35f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Modifiers|Wakeful", meta=(ClampMin="0"))
    float WakefulReviveDelaySeconds = 4.0f;   // O2 PLACEHOLDER
};

UCLASS()
class RIORSEDGE_API UBreakerEnemyModifierLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static constexpr int32 BreakerMaxModifiersPerEnemy = 3;

    // Every real modifier, in declaration order. Excludes None and Count.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static TArray<EBreakerEnemyModifier> GetAllModifiers();

    // Short, all-caps, and stable — this string is what the player reads over
    // the enemy's head before the modifier matters. §1.2 test 1 requires the
    // modifier to be identifiable within 1.5s of the enemy entering view; an
    // unannounced modifier is an unfair death, not a challenge.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static FString GetModifierName(EBreakerEnemyModifier Modifier);

    // One line of what it DOES, for the same readout. Deliberately phrased as
    // player instructions rather than as numbers.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static FString GetModifierCounterplay(EBreakerEnemyModifier Modifier);

    // The graybox tell colour. Distinct per modifier so the halo reads as an
    // identity even before the name is legible at range.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static FLinearColor GetModifierColor(EBreakerEnemyModifier Modifier);

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static EBreakerModifierPressure GetModifierPressure(EBreakerEnemyModifier Modifier);

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static EBreakerModifierWeightClass GetModifierWeightClass(EBreakerEnemyModifier Modifier);

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static float GetModifierSelectionWeight(EBreakerEnemyModifier Modifier, const FBreakerEnemyModifierParams& Params);

    // --- Composition (§1.3) ----------------------------------------------

    // Symmetric. Includes a modifier with itself, which is the trivially
    // forbidden pair the document notes parenthetically.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static bool AreModifiersForbiddenTogether(EBreakerEnemyModifier A, EBreakerEnemyModifier B);

    // The full legality check: count bounds, no duplicates, no forbidden pair,
    // and the pressure-diversity rules. OutReason is filled on failure so a
    // test failure names the rule that was broken rather than printing false.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static bool IsLegalModifierSet(const TArray<EBreakerEnemyModifier>& Modifiers, FString& OutReason);

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static int32 GetDistinctPressureCount(const TArray<EBreakerEnemyModifier>& Modifiers);

    // --- Family scoping (Story-Source §1.5) -------------------------------

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static EBreakerModifierFamilyScope GetModifierFamilyScope(EBreakerEnemyModifier Modifier);

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static bool IsModifierAllowedOnFamily(EBreakerEnemyModifier Modifier, EBreakerEnemyFamily Family);

    // Legality PLUS the family gate. Kept separate from IsLegalModifierSet so
    // the composition rules stay checkable on their own and so an authored set
    // that predates the family split still validates.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static bool IsLegalModifierSetForFamily(const TArray<EBreakerEnemyModifier>& Modifiers,
        EBreakerEnemyFamily Family, FString& OutReason);

    // --- Selection (§1.4) -------------------------------------------------
    // Deterministic in the seed, which is what makes the 10,000-roll legality
    // sweep in the test suite reproducible rather than flaky.

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static int32 RollModifierCount(int32 Seed, const FBreakerEnemyModifierParams& Params);

    // Weighted draw without replacement, rejecting illegal additions. §1.4:
    // "forbidden pairs re-rolled up to 3 times then the count drops by one" —
    // implemented as bounded retries per slot, so selection ALWAYS terminates
    // and always returns a legal set (possibly shorter than asked for).
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static TArray<EBreakerEnemyModifier> RollModifierSet(int32 Seed, int32 DesiredCount,
        const FBreakerEnemyModifierParams& Params, EBreakerEnemyFamily Family = EBreakerEnemyFamily::Vestige);

    // The whole roll: count then set. What a spawner calls.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static TArray<EBreakerEnemyModifier> RollModifiers(int32 Seed,
        const FBreakerEnemyModifierParams& Params, EBreakerEnemyFamily Family = EBreakerEnemyFamily::Vestige);

    // --- Chassis composition ---------------------------------------------

    // §1.1's "+0.35x per modifier beyond the first". Multiplies the
    // ModifierBearing rank row; it does not replace it.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static float GetModifierCountHealthMultiplier(int32 ModifierCount, const FBreakerEnemyModifierParams& Params);

    // O9's modifier-count-drives-rank rule, mapped onto the code's rank enum.
    // O9 names the ranks Standard / Veteran / Champion; EBreakerMonsterRank
    // predates that rename and calls them Trash / ModifierBearing / Elite. The
    // mapping is: 0 modifiers is Trash, 1 or more is ModifierBearing, and Elite
    // stays what it has always been — an AUTHORED pack anchor, not a
    // modifier-derived rank. Nothing derives Boss.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static EBreakerMonsterRank GetRankForModifierCount(int32 ModifierCount);

    // --- Magnitudes -------------------------------------------------------
    // Every one of these takes ENEMY-side numbers only. There is no overload
    // that takes a player, a character level, an item level or an attribute
    // set, and there must never be one: this is where a player read would
    // enter if it ever entered, so this is where it is structurally impossible.

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static float GetWardShieldAmount(float MonsterMaxHealth, const FBreakerEnemyModifierParams& Params);

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static float GetVolatileDetonationDamage(float MonsterAttackDamage, const FBreakerEnemyModifierParams& Params);

    // Linear falloff from full at the inner radius to zero at the outer, per
    // §1.2. Outside the outer radius it is exactly zero, never a small
    // unexplained tick.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static float GetVolatileFalloff(float DistanceCm, const FBreakerEnemyModifierParams& Params);

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static float GetReflectDamage(float IncomingDamage, float MonsterMaxHealth, const FBreakerEnemyModifierParams& Params);

    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static float GetHazardTickDamage(float MonsterAttackDamage, const FBreakerEnemyModifierParams& Params);

    // Where a blink ends: PhaseDistanceCm closer, but never inside the
    // standoff, and never PAST the target. Pure so the "it cannot blink into
    // your face" rule is testable with no world.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static FVector GetPhaseDestination(const FVector& Origin, const FVector& Target, const FBreakerEnemyModifierParams& Params);

    // The banner the player reads. Empty for an unmodified enemy, so a trash
    // mob's label is untouched.
    UFUNCTION(BlueprintPure, Category="Enemy|Modifiers")
    static FString GetModifierBanner(const TArray<EBreakerEnemyModifier>& Modifiers);
};
