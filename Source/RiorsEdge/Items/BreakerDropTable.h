#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Items/BreakerItemTypes.h"
#include "BreakerDropTable.generated.h"

// THE DROP PIPELINE. Owner playtest, verbatim: "not every single enemy needs to
// drop an item — we need to make the calculations for drop rates and how they
// should work. I was getting way way too many Aberrants at this item level when
// it shouldn't even be fundamentally possible, and every single enemy dropped
// an item."
//
// Both halves of that report were one structural gap, confirmed in code before
// anything here was written. `ABreakerEnemy::GrantLoot` called
// `UBreakerLootLibrary::RollRarity` unconditionally on every death and spawned
// whatever came back, so:
//
//   1. There was NO "does this enemy drop at all" step. Kill count WAS item
//      count. At the wave-mode kill rate that is ~690 items an hour.
//   2. The rarity table was doing the whole job on its own, and it is a FLAT
//      table — a 2.5% Aberrant weight applied at area level 1 to a trash mob
//      exactly as it did to a boss at area level 50. ~17 Aberrants an hour,
//      at any level, against O11's cap of THREE EQUIPPED. The owner's "it
//      shouldn't even be fundamentally possible" was literally true: nothing
//      in the code made it impossible, because nothing in the code looked at
//      the level or the rank at all.
//
// This header is the fix and it is deliberately world-free, in the precedent of
// `Combat/BreakerMonsterChassis.h` and `Weapons/BreakerWeaponMath.h`: pure
// functions over a seed, an item level, a monster rank and one authored
// parameter block. It cannot read a player, an actor or a world, so it is
// unit-testable, tool-readable, and structurally incapable of the kind of
// hidden coupling that made the old path impossible to reason about.
//
// The pipeline is now THREE steps where it used to be one:
//
//     1. DROP CHANCE   — per monster rank. Most trash kills drop nothing.
//     2. RARITY GATE   — a rarity whose item-level or rank gate is unmet has
//                        its weight zeroed before the roll, not rerolled after.
//     3. RARITY ROLL   — the weighted table, over whatever survived step 2.
//
// O2: every number below is EditAnywhere and a PLACEHOLDER. The whole point of
// putting them in one struct is that the owner retunes drop rates from the
// details panel without a recompile, exactly as FBreakerMonsterChassisParams
// does for the monster curve.

// Where a rank sits in the loot ORDER, stated explicitly rather than inferred
// from the enum's declaration order. Reordering EBreakerMonsterRank for an
// unrelated reason must not silently re-rank what drops from what.
UENUM(BlueprintType)
enum class EBreakerLootRankOrder : uint8
{
    Trash = 0,
    Elite = 1,
    ModifierBearing = 2,
    Boss = 3
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDropTableParams
{
    GENERATED_BODY()

    // --- Step 1: does this kill drop anything at all? ----------------------
    // O27 is explicit that trash exists to be trivialized and that difficulty
    // lives in elites, modifier-bearers and bosses. The loot follows the
    // difficulty: a trash kill is an occasional drop, an elite is reliable, a
    // boss is guaranteed. A player who wants items goes and fights the things
    // that are actually dangerous.
    //
    // Set every one of these to 1.0 to recover the old "every enemy drops"
    // behaviour exactly; the rest of the pipeline is unaffected.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Chance", meta=(ClampMin="0", ClampMax="1"))
    float TrashDropChance = 0.10f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Chance", meta=(ClampMin="0", ClampMax="1"))
    float EliteDropChance = 0.75f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Chance", meta=(ClampMin="0", ClampMax="1"))
    float ModifierBearingDropChance = 0.90f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Chance", meta=(ClampMin="0", ClampMax="1"))
    float BossDropChance = 1.0f;   // O2 PLACEHOLDER

    // How hard the Drop Chance affix pulls on step 1 (QUANTITY) as opposed to
    // step 3 (QUALITY, which it already did and still does). At 1.0, +100%
    // Drop Chance doubles the per-rank chance. Set to 0 and the affix goes back
    // to being a pure quality stat.
    //
    // It bids on both on purpose: a stat printed as "Drop Chance" that changed
    // only which rarity came out of a drop you were getting anyway is the kind
    // of quiet lie this project has shipped before (see the DamageMultiplier
    // that nothing wrote).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Chance", meta=(ClampMin="0"))
    float DropChanceQuantityScale = 1.0f;   // O2 PLACEHOLDER

    // --- Step 3: the rarity weights ---------------------------------------
    // Relative weights, not percentages — they do not have to sum to anything,
    // and they are renormalized over whatever step 2 left unlocked.
    //
    // Aberrant was 2.5 and is now 1.2. O11 caps Aberrant at THREE EQUIPPED,
    // globally. A rarity capped at 3 has to be rare enough that 3 is a goal;
    // at the old weight the cap was reached in the first twenty minutes and
    // then only ever displaced. The gates below do most of the work, but the
    // weight had to come down too or elites alone would refill the cap hourly.
    //
    // O29 raises the stakes further (unmerged as this was written — this lane
    // built against MaxItemLevel 50, tiers T8..T-1, Standard capped at T3): a
    // wider T12..T-1 ladder makes each rarity step worth MORE, not less, so if
    // anything these two weights want to fall again when O29 lands.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Rarity", meta=(ClampMin="0"))
    float StandardWeight = 62.0f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Rarity", meta=(ClampMin="0"))
    float UncommonWeight = 25.0f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Rarity", meta=(ClampMin="0"))
    float ExceptionalWeight = 10.0f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Rarity", meta=(ClampMin="0"))
    float AberrantWeight = 1.2f;   // O2 PLACEHOLDER (was 2.5 flat, ungated)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Rarity", meta=(ClampMin="0"))
    float AnomalousWeight = 0.25f;   // O2 PLACEHOLDER (was 0.5 flat, ungated)

    // --- Step 2: THE GATES -------------------------------------------------
    // THE RULE, in one sentence, so the owner can overrule it:
    //
    //   A rarity can only roll when BOTH the drop's item level is at or above
    //   that rarity's unlock level AND the killed monster's rank is at or above
    //   that rarity's minimum rank; a gated-out rarity's weight is zero and its
    //   share redistributes across the rarities that are open.
    //
    // Two levers, because the owner's complaint had two halves. Item level
    // answers "it shouldn't be possible at this item level". Rank answers
    // "every single enemy dropped an item" — the top tiers now come from the
    // encounter content O27 says the difficulty lives in.
    //
    // Redistribution rather than a reroll is deliberate. A reroll would keep
    // rolling until something legal came out, which preserves the SHAPE of the
    // table and just relabels the illegal results; zeroing the weight means a
    // low-level trash kill genuinely has a different rarity distribution, not
    // the same one with the top clipped off and pushed into Standard by chance.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Gates", meta=(ClampMin="1"))
    int32 ExceptionalMinimumItemLevel = 8;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Gates", meta=(ClampMin="1"))
    int32 AberrantMinimumItemLevel = 25;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Gates", meta=(ClampMin="1"))
    int32 AnomalousMinimumItemLevel = 40;   // O2 PLACEHOLDER

    // NOTE ON O29, recorded rather than acted on. These three unlocks were
    // derived against the old 1-50 item level range, and O29 took item level to
    // 120. The obvious move is to scale them proportionally (8/25/40 becomes
    // roughly 19/60/96) and it is very probably WRONG.
    //
    // Item level tracks AREA level, and the campaign is area levels 1-50; the
    // endgame is 50-100. Scaling proportionally would put Aberrant at ilvl 60
    // and Anomalous at 96, so a player would finish the entire campaign having
    // never seen either rarity - the ladder would be introduced only after the
    // content that teaches it. These gates pace the player's INTRODUCTION to
    // rarity, and that introduction still happens across 1-50 regardless of how
    // far the tier ladder now runs.
    //
    // What O29 does argue for is the top two WEIGHTS falling again, because a
    // longer tier ladder makes each rarity worth more than it was. That is a
    // tuning question with a measurable answer (the hours-per-Aberrant figure
    // in the projection) and it wants a playtest, not a guess.

    // Exceptional is deliberately reachable from trash: something has to be
    // worth picking up off an ordinary kill, or the 10% trash drop is 10% of
    // nothing and the player stops looking at the floor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Gates")
    EBreakerMonsterRank ExceptionalMinimumRank = EBreakerMonsterRank::Trash;   // O2 PLACEHOLDER

    // THE OWNER'S SPECIFIC COMPLAINT, encoded: a trash kill cannot produce an
    // Aberrant at any item level whatsoever. Pinned by
    // RiorsEdge.Items.Drops.TrashCannotRollAberrant.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Gates")
    EBreakerMonsterRank AberrantMinimumRank = EBreakerMonsterRank::Elite;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Gates")
    EBreakerMonsterRank AnomalousMinimumRank = EBreakerMonsterRank::Elite;   // O2 PLACEHOLDER
};

// An hour of play, described as kills. This is an INPUT to the projection, not
// a measurement — nothing in the codebase measures kills per hour yet, and the
// wave-mode report is the instrument that eventually should. The defaults are
// the wave-mode shape the gym actually spawns, and they are O2 PLACEHOLDER like
// everything else: the owner retunes the ARITHMETIC by retuning this struct.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerKillRateSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Projection", meta=(ClampMin="0"))
    float TrashKillsPerHour = 600.0f;   // O2 PLACEHOLDER (one trash every 6s)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Projection", meta=(ClampMin="0"))
    float EliteKillsPerHour = 60.0f;   // O2 PLACEHOLDER (one elite a minute)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Projection", meta=(ClampMin="0"))
    float ModifierBearingKillsPerHour = 30.0f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drops|Projection", meta=(ClampMin="0"))
    float BossKillsPerHour = 2.0f;   // O2 PLACEHOLDER
};

// What an hour is worth. This is the number the owner actually wants to tune
// against and nothing in the project could answer it before.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerLootRateProjection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Drops|Projection") float ItemsPerHour = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Drops|Projection") float ExceptionalOrBetterPerHour = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Drops|Projection") float AberrantPerHour = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Drops|Projection") float AnomalousPerHour = 0.0f;

    // Infinity when the rarity is gated out entirely at this item level, which
    // is the honest answer to "how long before an Aberrant" in a level-10 area:
    // never. A large finite number would read as "eventually".
    UPROPERTY(BlueprintReadOnly, Category="Drops|Projection") float HoursPerAberrant = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Drops|Projection") float HoursPerAnomalous = 0.0f;
};

UCLASS()
class RIORSEDGE_API UBreakerDropTableLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Explicit loot ordering; see EBreakerLootRankOrder.
    UFUNCTION(BlueprintPure, Category="Items|Drops")
    static int32 GetRankLootOrder(EBreakerMonsterRank Rank);

    UFUNCTION(BlueprintPure, Category="Items|Drops")
    static float GetRankDropChance(EBreakerMonsterRank Rank, const FBreakerDropTableParams& Params);

    // Step 1, with the Drop Chance affix folded in. Clamped to [0,1]: a
    // guaranteed drop cannot become two drops, because the drop COUNT per kill
    // is a separate design nobody has ruled on.
    UFUNCTION(BlueprintPure, Category="Items|Drops")
    static float GetEffectiveDropChance(EBreakerMonsterRank Rank, float DropChanceBonusPercent, const FBreakerDropTableParams& Params);

    UFUNCTION(BlueprintPure, Category="Items|Drops")
    static bool RollsDrop(int32 RandomSeed, EBreakerMonsterRank Rank, float DropChanceBonusPercent, const FBreakerDropTableParams& Params);

    // Step 2, exposed on its own so the gate is assertable directly rather than
    // only through a statistical sweep.
    UFUNCTION(BlueprintPure, Category="Items|Drops")
    static bool IsRarityUnlocked(EBreakerItemRarity Rarity, int32 ItemLevel, EBreakerMonsterRank Rank, const FBreakerDropTableParams& Params);

    // Steps 2 + 3.
    UFUNCTION(BlueprintPure, Category="Items|Drops")
    static EBreakerItemRarity RollGatedRarity(int32 RandomSeed, int32 ItemLevel, EBreakerMonsterRank Rank,
        float DropChanceBonusPercent, const FBreakerDropTableParams& Params);

    // THE ONE ENTRY POINT a kill should call. Derives two independent sub-seeds
    // from the kill seed so the drop decision and the rarity do not correlate —
    // sharing a stream would make "dropped at all" and "dropped well" the same
    // coin, which shows up as the rare tiers clustering in the same seeds.
    // Deterministic: the same kill seed always produces the same answer, which
    // is the locked property the roll pipeline is tested on.
    UFUNCTION(BlueprintCallable, Category="Items|Drops")
    static bool RollDrop(int32 RandomSeed, int32 ItemLevel, EBreakerMonsterRank Rank,
        float DropChanceBonusPercent, const FBreakerDropTableParams& Params, EBreakerItemRarity& OutRarity);

    // The loot-per-hour arithmetic, computed rather than asserted, so
    // Docs/Item-Foundation.md's table and the automation sweep read the same
    // source. Analytic — no sampling — because a projection that needed 100k
    // rolls to answer a design question is not a tool anyone will use.
    UFUNCTION(BlueprintPure, Category="Items|Drops")
    static FBreakerLootRateProjection ProjectLootRate(const FBreakerKillRateSample& Kills, int32 ItemLevel,
        float DropChanceBonusPercent, const FBreakerDropTableParams& Params);

    // The per-rarity probability vector for one DROP (i.e. conditional on step
    // 1 having already passed) at this item level and rank. Sums to 1.
    static void GetGatedRarityProbabilities(int32 ItemLevel, EBreakerMonsterRank Rank, float DropChanceBonusPercent,
        const FBreakerDropTableParams& Params, TArray<float>& OutProbabilities);
};
