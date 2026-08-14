#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Items/BreakerItemTypes.h"
#include "BreakerAffixLibrary.generated.h"

UCLASS()
class RIORSEDGE_API UBreakerAffixLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ---- The tier ladder (O29) --------------------------------------------
    // O29 is the answer to the 74x endgame gap recorded at the end of
    // Power-Curve.md: THE ENDGAME POWER SOURCE IS GEAR DEPTH. Item level runs
    // to 120, past the character cap of 50 and past the area-level ceiling of
    // 100, and the affix tier ladder widens from T8..T-1 to T12..T-1 to cover
    // it. Affix magnitudes still live inside tier ranges — nothing here
    // authors a new multiplier lane, which is what keeps O3 intact.
    //
    // These are compile-time constants rather than EditAnywhere properties
    // because a UBlueprintFunctionLibrary has no instance for the editor to
    // show. They are still O2 PLACEHOLDERs: shape, not a balance sheet. The
    // per-affix magnitudes they interpolate between ARE EditAnywhere, on
    // FBreakerAffixDefinition.

    // The item-level ceiling, O29. `FBreakerWeaponMath::MaxSupportedItemLevel`
    // must agree with this; RiorsEdge.Items.TierLadder pins that they do.
    static constexpr int32 MaxItemLevel = 120;              // O2 PLACEHOLDER

    static constexpr int32 WorstTier = 12;                  // the floor of the ladder
    static constexpr int32 BestNormalTier = 1;              // best tier item level alone can reach
    static constexpr int32 TopTier = -1;                    // the crafted/Anomalous ceiling

    /**
     * The back-loaded tier value curve, O29.
     *
     *     p(T)     = (WorstTier - T) / (WorstTier - BestNormalTier)   in [0,1]
     *     Value(T) = V12 * (V1 / V12) ^ (p ^ TierCurveExponent)
     *
     * GEOMETRIC in the value, not linear, and then bent once more by the
     * exponent. Two failure modes are being avoided at the same time:
     *
     *  - A LINEAR ladder over 11 steps makes every step the same absolute
     *    size, so a top-tier roll is arithmetically just one more step. O29
     *    forbids exactly that: "back-load the curve so a top-tier roll is an
     *    event rather than one more step."
     *  - A pure p^k lerp back-loads so hard over 11 steps that the bottom four
     *    tiers become indistinguishable — a T12 and a T11 item would read as
     *    the same item, and the ladder would be a cliff with a flat approach.
     *
     * Geometric-with-exponent gives both: every step is a real multiplicative
     * improvement, and the steps grow toward the top in BOTH senses.
     *
     * The arithmetic, on the pool's flagship line (Core.Health, 25 -> 400):
     *
     *   T12  25.00                     T6   91.70  x1.303
     *   T11  28.71  x1.148 (+3.71)     T5  120.87  x1.318
     *   T10  34.75  x1.210             T4  160.93  x1.331
     *   T9   43.18  x1.243             T3  216.23  x1.344
     *   T8   54.70  x1.267             T2  292.97  x1.355
     *   T7   70.36  x1.286             T1  400.00  x1.365 (+107.03)
     *
     *  - T1 is 16.0x T12 (the authored ratio; every affix uses its own two
     *    anchors, so 16.0x is a property of the pool, not of this function).
     *  - The low step T12->T11 is +14.8% / +3.71 absolute. The high step
     *    T2->T1 is +36.5% / +107.03 absolute. The top step is 2.46x the
     *    bottom step in RELATIVE terms and 28.8x in ABSOLUTE terms, which is
     *    what "a materially bigger jump between the high tiers" has to mean on
     *    a ladder whose values are themselves growing.
     *  - Against the old 8-tier linear ladder, tier for tier: T8 2.19x,
     *    T7 1.49x, T6 1.32x, T5 1.32x, T4 1.42x, T3 1.59x, T2 1.86x,
     *    T1 2.22x. Every tier number is materially up (the trough is +32% at
     *    T5/T6) and the top is up most, per O29's "tuned up significantly
     *    across the whole ladder, with a materially bigger jump between the
     *    HIGH tiers."
     *  - T12 is deliberately the OLD T8 value, unchanged. An item level 1 drop
     *    is therefore bit-identical to what it was before O29, exactly as
     *    FBreakerWeaponMath::ItemLevelDamageScalar is exactly 1.0 at ilvl 1.
     *    The curve is opt-in by content rather than a silent retune of the
     *    only content anybody has actually played.
     *
     * T0 and T-1 are RE-SITED, not carried over. They were 1.4x and 1.8x T1
     * against a linear ladder whose top step was +14%, so T0 was ~2.9 ordinary
     * steps above T1 and read as a spike. Against a top step of +36.5% those
     * same multipliers would be barely one step, which would demote the spike
     * into "one more step" — the precise thing O29 rules out. They move to
     * 2.2x and 3.6x T1, which is ~2.5 and ~4.1 top-steps above T1 and
     * preserves what the spike MEANT rather than what it measured.
     */
    UFUNCTION(BlueprintPure, Category="Items|Affixes")
    static float ValueForTier(const FBreakerAffixDefinition& Affix, int32 Tier);

    // The shape of the back-load. 1.0 would be plain geometric (equal relative
    // steps); above 1.0 the relative steps grow toward the top.
    static constexpr float TierCurveExponent = 1.25f;        // O2 PLACEHOLDER
    static constexpr float TierSpikeT0Multiplier = 2.2f;     // O2 PLACEHOLDER, ~2.5 top-steps
    static constexpr float TierSpikeTopMultiplier = 3.6f;    // O2 PLACEHOLDER, ~4.1 top-steps

    // Item level gates the best rollable tier: 1-120 onto T12..T1, one tier
    // per 10 levels. Level 1 rolls only T12; T1 opens at ilvl 111.
    // T0/T-1 never come from item level alone — they are crafted or carried by
    // a rule, which is what keeps the Forge a destination.
    //
    // The old mapping was 1-50 onto T8..T1 at one tier per 7 levels. Both the
    // range and the ladder roughly tripled, so the levels-per-tier only rises
    // from 7 to 10: an item level still buys a tier at a comparable rate, and
    // the endgame is longer because there is more of it, not because it is
    // slower.
    UFUNCTION(BlueprintPure, Category="Items|Affixes")
    static int32 BestTierForItemLevel(int32 ItemLevel);

    // Rarity soft-caps the tier ceiling. RE-DERIVED for the 12-tier ladder
    // (O29): the old Standard T3 / Uncommon T1 were chosen against 7 steps and
    // mean something else against 11.
    //
    // The invariant preserved is the SHAPE of the withholding, not the tier
    // numbers. Standard used to be denied the top 2 of 7 steps (29% of the
    // ladder); 29% of 11 steps is 3.1, so Standard is denied T3/T2/T1 and caps
    // at T4. The Standard->Uncommon gap stays exactly two tiers, as it was.
    //
    // Uncommon moves off T1 deliberately. On the old linear ladder letting an
    // Uncommon reach the top normal tier cost little, because T1 was only +14%
    // over T2. On a back-loaded ladder T1 is +36.5% over T2 and O29 wants it to
    // be an event; a rarity that drops from every third kill cannot be the
    // thing that hands it out. T1 becomes an Exceptional-and-above line.
    //
    //   Standard    T4   (8 of 11 steps; 40% of the top normal value)
    //   Uncommon    T2   (10 of 11 steps; 73%)
    //   Exceptional+ T-1 (the full ladder including the spikes)
    UFUNCTION(BlueprintPure, Category="Items|Affixes")
    static int32 TierCapForRarity(EBreakerItemRarity Rarity);

    UFUNCTION(BlueprintPure, Category="Items|Affixes")
    static void AffixCountRangeForRarity(EBreakerItemRarity Rarity, int32& OutMinimum, int32& OutMaximum);

    // The affix pool. Universal core six (Elemental DR still waits on a
    // resistance model), three movement lines, both crit stats, an
    // unconditional Increased and a flat Added damage line, and five
    // conditional damage lines keyed to the movement pillar.
    //
    // Plus the non-damage breadth pass: flat Armour, Health and Resource on
    // Kill, and Increased Damage over Time — the survivability, resource and
    // DoT axes, each of which had a live consumer sitting unused.
    //
    // Twenty-two entries, ten of them offensive. It was twelve with exactly one
    // offensive line that rolled on four of eight slots, which is the concrete
    // reason "full level 50 gear" did not feel like anything (O27, Power-Curve
    // §"More options in every avenue"). EVERY slot can now raise damage, and
    // each does it with a different line — see the per-slot identity table in
    // Docs/Item-Foundation.md.
    static const TArray<FBreakerAffixDefinition>& GetSliceAffixPool();

    // ---- Per-archetype affix leans ----------------------------------------
    // Owner's instruction, verbatim: "make sure certain guns have certain
    // leans towards affixes -- like smg fire rate, lmg damage, sidearm slide
    // speed and so on -- NOT REQUIRED STATS but they can drop more likely
    // with those affixes."
    //
    // So this is a WEIGHT MULTIPLIER, never a filter. Every affix legal on a
    // weapon slot stays legal on every archetype; a lean only bends the odds.
    // That distinction is the whole design: a hard restriction would make an
    // SMG with a huge damage roll impossible, and the item you were not
    // supposed to get is the one that makes a looter interesting. It also
    // means a lean can be retuned to 1.0 to switch the whole feature off
    // without any item becoming unrollable.
    //
    // Returns 1.0 for any pairing with no authored opinion, and for every
    // non-weapon slot, so armour rolls exactly as it did before this existed.
    //
    // O2 PLACEHOLDER: the multipliers are shape, not balance.
    UFUNCTION(BlueprintPure, Category="Items|Affixes")
    static float ArchetypeAffixWeightMultiplier(EBreakerWeaponArchetype Archetype, FName AffixId);

    // True when the affix moves outgoing damage in any bucket or under any
    // condition. The content test uses it to assert that no slot is
    // structurally incapable of raising damage; the UI can use it to group.
    static bool IsOffensiveTarget(EBreakerStatTarget Target);

    static const FBreakerAffixDefinition* FindAffix(const TArray<FBreakerAffixDefinition>& Pool, FName AffixId);
};
