#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerMonsterChassis.generated.h"

// The monster chassis, per ruling O27 and Docs/Design/Power-Curve.md §2.
//
// Before this existed, EnemyLevel drove nothing but loot item level and health
// was the literal constant 220 at every level, so the player's 3-5x power
// growth landed on an enemy that never changed. Health and damage are now
// geometric functions of AREA LEVEL, which is a property of the CONTENT:
//
//     MonsterHealth(AL) = BaseHealth * (1 + g)^(AL - 1) * Rank * Archetype
//     MonsterDamage(AL) = BaseDamage * (1 + d)^(AL - 1) * Rank * Archetype
//
// NOTHING in this header can read the player. Every function is pure, takes
// only an area level, a rank and an authored parameter block, and is therefore
// unit-testable and readable by tools with no actor and no world — the same
// precedent as Combat/BreakerRangedBehavior.h and Weapons/BreakerWeaponMath.h.
// That is not a style preference: player-scaled monsters are the exact failure
// mode O27 exists to forbid, and a function that cannot see a player cannot
// commit it.

// Rank multiplies the chassis; it never replaces it. Difficulty lives here,
// not in trash health (O27: "trash exists to be trivialized").
UENUM(BlueprintType)
enum class EBreakerMonsterRank : uint8
{
    // The baseline chassis. An optimized build should delete these on contact.
    Trash,
    // The pack anchor. The old hardcoded ConfigureElite chassis is folded into
    // this row rather than living as a second source of truth.
    Elite,
    // Carries encounter modifiers. The mods are the threat, not the health.
    ModifierBearing,
    // O18's 20-45s fight.
    Boss
};

// Everything an owner would plausibly tune about the curve, in one authored
// block (O2: every value below is a PLACEHOLDER and is EditAnywhere).
//
// Rank multipliers live here rather than as C++ constants for exactly the same
// reason: the elite ratio is a balance number, and O2 freezes balance numbers
// into surfaces an owner can move without a recompile.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerMonsterChassisParams
{
    GENERATED_BODY()

    // Health of a TRASH monster in an area-level-1 area. Anchored at the
    // chassis session 5 actually measured (220 hp, 1.81s melee trash TTK), so
    // area level 1 is bit-identical to the shipping enemy and the whole change
    // is "the curve now exists" rather than a silent retune. O27 explicitly
    // retired the "trash health 220 -> 120" re-anchor: the TTK correction is
    // the COMPOSITION of this curve with the weapon base-damage curve, not a
    // new constant picked here.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chassis", meta=(ClampMin="1"))
    float BaseHealth = 220.0f;   // O2 PLACEHOLDER

    // Damage of one TRASH attack in an area-level-1 area. The melee archetype's
    // authored figure (the ranged archetype authors its own, and the two move
    // together — see BreakerRangedEnemy).
    //
    // O116 RETUNE. Was 14 with `d` at 0.055, which put time-to-die at 16.4s at
    // area level 1 against O18's 4-5s: early content could not kill anybody,
    // and hits-to-die then fell the whole way to the cap. Solved for TTD inside
    // the band at BOTH ends against the one authored baseline character, which
    // is what fixes the pair — anchoring on either end alone sends the other to
    // five times its target, and anchoring on the CEILING character (the bug
    // this retune was nearly built on) puts base damage 50% higher than this.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chassis", meta=(ClampMin="0"))
    float BaseDamage = 51.1f;   // O2 PLACEHOLDER

    // g. 9% per level is x66.8 over 50 levels — Power-Curve.md's stated shape.
    // Geometric, not linear: linear scaling makes early levels identical and
    // late levels unreachable.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chassis", meta=(ClampMin="0", ClampMax="1"))
    float HealthGrowthPerLevel = 0.09f;   // O2 PLACEHOLDER

    // d. MUST stay materially below g. If incoming damage scales as fast as
    // monster health then defence has to grow as fast as offence and every
    // build becomes a defensive build.
    //
    // O116 RETUNE, 0.055 -> 0.0173: x2.32 over 50 levels against health's x67
    // and against what a baseline character's gear actually buys, x2.33. That
    // second number is the one this solves against and the reason the old value
    // was wrong — 0.055 grew incoming damage x13.8 while carried health grew
    // x2.33, so hits-to-die fell by 5.9x across the levelling range and a
    // character at the cap was flimsier than one at level 1 in content they
    // out-geared. It is set BY that property, per the rule above the table, and
    // Combat.DefenseCurve.HitsToDie is the test that holds it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chassis", meta=(ClampMin="0", ClampMax="1"))
    float DamageGrowthPerLevel = 0.0173f;   // O2 PLACEHOLDER

    // --- Rank table (Power-Curve.md §2) ------------------------------------
    // These are RATIOS to trash at the SAME area level, which is what makes
    // them derivable rather than guessed: O18 wants trash <1s and elite ~3s,
    // so the elite health ratio IS 3. The shipping ConfigureElite used 2.0x
    // health and measured 3.01s elite only because trash was simultaneously
    // 1.81x too slow; the two errors were cancelling.
    //
    // Boss moved 25 -> 75 (owner ruling 2026-08-16: boss HP x3). The RANK row
    // is what tripled, not the boss actor: ABreakerBossEnemy keeps its 0.35
    // archetype ratio, so the fielded boss lands at a net x26.25 over trash
    // (~3x the old effective x8.75) — inside O18's 20-45s band once the
    // archetype discount is counted, where the old net fight was ending in
    // single digits.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chassis|Rank", meta=(ClampMin="1"))
    float EliteHealthMultiplier = 3.0f;   // O2 PLACEHOLDER (doc band x3-4)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chassis|Rank", meta=(ClampMin="1"))
    float EliteDamageMultiplier = 1.5f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chassis|Rank", meta=(ClampMin="1"))
    float ModifierHealthMultiplier = 2.5f;   // O2 PLACEHOLDER (doc band x2-3)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chassis|Rank", meta=(ClampMin="1"))
    float ModifierDamageMultiplier = 1.25f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chassis|Rank", meta=(ClampMin="1"))
    float BossHealthMultiplier = 75.0f;   // O2 PLACEHOLDER (owner ruling 2026-08-16: boss HP x3, 25 -> 75; old doc band x20-40 triples with it)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chassis|Rank", meta=(ClampMin="1"))
    float BossDamageMultiplier = 2.0f;   // O2 PLACEHOLDER
};

UCLASS()
class RIORSEDGE_API UBreakerMonsterChassisLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // The lowest area level that exists. Area level is 1-based so that
    // (1 + g)^(AL - 1) is exactly BaseHealth in a starting area.
    static constexpr int32 BreakerMinAreaLevel = 1;
    // Area level does NOT stop at the character cap of 50: endgame tiers keep
    // climbing, and because area level drives drop item level, climbing tiers
    // is the mechanism by which gear keeps improving after 50.
    static constexpr int32 BreakerMaxAreaLevel = 100;   // O2 PLACEHOLDER

    UFUNCTION(BlueprintPure, Category="Enemy|Chassis")
    static int32 ClampAreaLevel(int32 AreaLevel);

    // Trash health/damage at an area level, before rank and archetype.
    UFUNCTION(BlueprintPure, Category="Enemy|Chassis")
    static float GetChassisHealth(int32 AreaLevel, const FBreakerMonsterChassisParams& Params);

    UFUNCTION(BlueprintPure, Category="Enemy|Chassis")
    static float GetChassisDamage(int32 AreaLevel, const FBreakerMonsterChassisParams& Params);

    // Rank multipliers, read out of the authored table so there is exactly one
    // source of truth for "what an elite is".
    UFUNCTION(BlueprintPure, Category="Enemy|Chassis")
    static float GetRankHealthMultiplier(EBreakerMonsterRank Rank, const FBreakerMonsterChassisParams& Params);

    UFUNCTION(BlueprintPure, Category="Enemy|Chassis")
    static float GetRankDamageMultiplier(EBreakerMonsterRank Rank, const FBreakerMonsterChassisParams& Params);

    // The full composition. ArchetypeMultiplier is the per-archetype ratio an
    // encounter design authors on top of rank — Encounter-Design §2.2's 1.6x
    // Lattice, for instance. It is NOT rank and NOT area level; it is what
    // makes one trash monster different from another at the same area level.
    UFUNCTION(BlueprintPure, Category="Enemy|Chassis")
    static float GetMonsterHealth(int32 AreaLevel, EBreakerMonsterRank Rank,
        const FBreakerMonsterChassisParams& Params, float ArchetypeMultiplier = 1.0f);

    UFUNCTION(BlueprintPure, Category="Enemy|Chassis")
    static float GetMonsterDamage(int32 AreaLevel, EBreakerMonsterRank Rank,
        const FBreakerMonsterChassisParams& Params, float ArchetypeMultiplier = 1.0f);

    // The item level of what an area-level-AL monster drops. Loot item level is
    // clamped to the character cap because affix tiers are authored to 50,
    // while the chassis itself keeps climbing past it — this is the one place
    // the two ceilings differ and it is deliberate.
    UFUNCTION(BlueprintPure, Category="Enemy|Chassis")
    static int32 GetDropItemLevel(int32 AreaLevel);

    // Reporting helper for tools, tests and the playtest report: how long a
    // given health pool takes to remove at a given damage-per-second. Takes a
    // DPS number the CALLER measured; it cannot and must not go looking for a
    // player.
    UFUNCTION(BlueprintPure, Category="Enemy|Chassis")
    static float GetTimeToKillSeconds(float Health, float DamagePerSecond);
};
