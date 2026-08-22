#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemRules.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

// ---------------------------------------------------------------------------
// THE BUILD VARIANCE BAND (Power-Curve.md §4, authority O27; split into two
// bands by O36)
// ---------------------------------------------------------------------------
// "The ratio between a baseline build and an optimized one at the SAME area
// level. This is the number O27 is really about, and it needs to be authored
// explicitly rather than emerging by accident." Originally targeted at a
// single "roughly 8-10x"; O29's item-level-120 gear depth moved where the top
// of the band lives, and O36 rules that the band is now authored at TWO
// points instead of retuning content to force one number: AT-CAP (level 50,
// tiers a level-50 drop can produce, 8-10x) and ENDGAME (ilvl 120, producible
// tiers, seed rails 12-20x). FBreakerPowerBandAtCapTest and
// FBreakerPowerBandEndgameTest below are that split, pinned separately.
//
// Both tests build two characters out of the REAL affix pool and the REAL
// fallback trees, folds them through the REAL aggregator
// (FBreakerAttributeAggregator, the same object UBreakerAttributeSet owns),
// and asserts the composed ratio lands in the relevant band. Nothing here
// re-implements the arithmetic; if the fold changes, this moves with it,
// which is the entire point.
//
// Both builds are measured in the SAME movement state — airborne, recently
// dashed, at Redline. That is the fair comparison the doc asks for: same
// content, same instant, different build. The baseline is not a character
// standing still; it is a character who found one conditional line and did not
// organise anything around it.
//
// Both builds also spend their whole point budget, which is why the per-point
// accumulation baseline cancels out of the ratio entirely. Under O27 that is
// the desired property, not an accident: accumulation must not be what
// separates two characters. The point budget does not change between the two
// O36 bands either, because character level (and so points earned) is capped
// at 50 regardless of item level — only gear grows past the cap (O29).
// ---------------------------------------------------------------------------

namespace BreakerPowerBandTest
{
    // O2 PLACEHOLDER. XP-And-Pacing §4/§7: Class Points stop at 30, Core Points
    // are ~50 from levels plus ~15 from world content. A level-50 character who
    // has finished the campaign holds and spends roughly this many. SHARED by
    // both O36 bands below: the character level cap (and so the point budget)
    // does not move between "at cap" and "past cap" gear -- O29's whole thesis
    // is that only GEAR keeps growing past level 50, so the same character,
    // same choices, is measured at two different item levels.
    constexpr int32 PowerBandFullPointBudget = 95;

    // ---------------------------------------------------------------------
    // O36 — TWO BANDS, pinned separately.
    // ---------------------------------------------------------------------
    // "The build variance band is authored at two points: AT-CAP (level 50,
    // tiers a level-50 drop can produce): 8-10x stands. ENDGAME (ilvl 120,
    // producible tiers): seed rails 12-20x (O2 PLACEHOLDER; the back-loaded
    // ladder currently measures ~15x, accepted pending playtest)."
    constexpr float AtCapBandMinimum = 8.0f;
    constexpr float AtCapBandMaximum = 10.0f;
    constexpr float EndgameBandMinimum = 12.0f;   // O2 PLACEHOLDER seed (O36)
    constexpr float EndgameBandMaximum = 20.0f;   // O2 PLACEHOLDER seed (O36)

    // The two measurement points. AT-CAP is the character cap; ENDGAME is the
    // top of the item-level ladder. Same character, same choices, same point
    // budget — only the gear differs, which is the whole thesis.
    constexpr int32 AtCapItemLevel = 50;
    constexpr int32 EndgameItemLevel = 120;

    // ---------------------------------------------------------------------
    // WHAT A BASELINE IS — one definition, applied identically at both points.
    // ---------------------------------------------------------------------
    // The two fixtures used to disagree about this, and the disagreement made
    // both numbers uninterpretable rather than only one.
    //
    // At cap the baseline was WorstTier: every one of twenty-four affix lines
    // landing at the absolute floor of the ladder. That is not "hitting 50 is
    // satisfying with decent power" — it is a character that will never exist,
    // and a band measured against an impossible build measures nothing.
    // Endgame, meanwhile, used a hardcoded T3 against T1: a realistically
    // decent roll against a perfect one, which is the right idea.
    //
    // Worse, the two were not merely different, they leaned opposite ways, so
    // comparing 8.08x against 15.40x compared nothing. The endgame figure
    // looking comfortable inside its rails is precisely why nobody checked it.
    //
    // The definition, owner-ruled: a baseline is a REALISTICALLY DECENT roll,
    // derived from item level exactly as the optimized tier is, and offset by
    // the same number of tiers at both points. The offset is 2 because that is
    // what the endgame pair already encoded (T3 against T1) — so the endgame
    // number is unchanged by construction and only the broken half moves.
    //
    // The offset is the knob. Widening it makes the baseline worse and the
    // band wider; it is not a free parameter and moving it moves both bands.
    constexpr int32 BaselineTierOffset = 2;

    // Tier numbers count DOWN as they improve, so +offset is worse gear.
    int32 OptimizedTierFor(int32 ItemLevel)
    {
        return UBreakerAffixLibrary::BestTierForItemLevel(ItemLevel);
    }

    int32 BaselineTierFor(int32 ItemLevel)
    {
        return FMath::Min(UBreakerAffixLibrary::WorstTier,
                          OptimizedTierFor(ItemLevel) + BaselineTierOffset);
    }

    // One equipped piece, built from the real pool so a value can never drift
    // away from what the game would actually roll. Tier is the printed tier.
    struct FPiece
    {
        EBreakerEquipSlot Slot;
        int32 Tier;
        TArray<FName> AffixIds;
    };

    FBreakerItemInstance MakeItem(const FPiece& Piece, int32 ItemLevel)
    {
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = TEXT("PowerBand");
        Item.Slot = Piece.Slot;
        Item.Rarity = EBreakerItemRarity::Anomalous;
        // Which band this piece belongs to (O36): AtCapItemLevel or
        // EndgameItemLevel, passed by the caller rather than assumed here.
        Item.ItemLevel = ItemLevel;
        for (const FName AffixId : Piece.AffixIds)
        {
            const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, AffixId);
            if (!Definition) continue;
            FBreakerRolledAffix Rolled;
            Rolled.AffixId = AffixId;
            Rolled.Tier = Piece.Tier;
            Rolled.Category = Definition->Category;
            // The tier's exact value, no in-band lerp: a band test must not
            // depend on a random stream.
            Rolled.Value = UBreakerAffixLibrary::ValueForTier(*Definition, Piece.Tier);
            Item.Affixes.Add(Rolled);
        }
        return Item;
    }

    TArray<FBreakerItemInstance> MakeLoadout(const TArray<FPiece>& Pieces, int32 ItemLevel)
    {
        TArray<FBreakerItemInstance> Items;
        for (const FPiece& Piece : Pieces) Items.Add(MakeItem(Piece, ItemLevel));
        return Items;
    }

    TArray<const UBreakerProgressionNode*> AllNodes()
    {
        TArray<const UBreakerProgressionNode*> Nodes;
        for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
        {
            for (const UBreakerProgressionNode* Node : Tree->Nodes) Nodes.Add(Node);
        }
        return Nodes;
    }

    // Everything one character is, folded once. The field names are the layer
    // names in the Power-Curve §4 table so the report and the doc can be read
    // against each other line by line.
    struct FComposedBuild
    {
        float FlatLayer = 1.0f;        // (Base 1.0 + Added Damage), the multiplicand
        float IncreasedLayer = 1.0f;   // 1 + sum(Increased) / 100, ONE bucket
        float MoreLayer = 1.0f;        // product of at most three Mores (O3)
        float EffectiveCrit = 1.0f;    // 1 + Chance * (Multiplier - 1)
        float CriticalChance = 0.0f;
        float CriticalMultiplier = 1.5f;
        float ComposedDamageMultiplier = 1.0f; // FlatLayer * IncreasedLayer * MoreLayer
        float Total = 1.0f;            // ComposedDamageMultiplier * EffectiveCrit
    };

    FComposedBuild Compose(const TArray<FBreakerItemInstance>& Items, const TArray<FBreakerNodeRank>& Ranks,
        const FBreakerBuildConditionState& Conditions)
    {
        FBreakerAttributeContribution EquipmentOffer;
        UBreakerEquipmentComponent::AggregateStats(Items, &EquipmentOffer, Conditions);

        FBreakerAttributeContribution ProgressionOffer;
        UBreakerProgressionComponent::AggregateStats(AllNodes(), Ranks, &ProgressionOffer, Conditions);
        // The one line UBreakerProgressionComponent::RecalculateStats adds after
        // the fold: the per-point accumulation floor, into the same additive
        // bucket. Both builds spend the same budget, so this is identical on
        // both sides and cannot be what separates them — which is exactly what
        // O27 asked for.
        ProgressionOffer.AddIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier,
            PowerBandFullPointBudget * 0.25f); // matches IncreasedDamagePerSpentPoint's default

        // The real aggregator, seeded with UBreakerAttributeSet's authored bases.
        FBreakerAttributeAggregator Aggregator;
        float Bases[FBreakerAttributeAggregator::AttributeCount] = {};
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::CriticalChance)] = 0.05f;
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::CriticalMultiplier)] = 1.5f;
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::DamageMultiplier)] = 1.0f;
        Aggregator.CaptureBases(Bases);
        Aggregator.SetContribution(EBreakerAttributeContributor::Equipment, EquipmentOffer);
        Aggregator.SetContribution(EBreakerAttributeContributor::Progression, ProgressionOffer);

        FComposedBuild Build;
        Build.FlatLayer = 1.0f
            + EquipmentOffer.GetFlat(EBreakerAggregatedAttribute::DamageMultiplier)
            + ProgressionOffer.GetFlat(EBreakerAggregatedAttribute::DamageMultiplier);
        Build.IncreasedLayer = 1.0f + (EquipmentOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier)
            + ProgressionOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier)) / 100.0f;
        Build.MoreLayer = EquipmentOffer.GetMore(EBreakerAggregatedAttribute::DamageMultiplier)
            * ProgressionOffer.GetMore(EBreakerAggregatedAttribute::DamageMultiplier);

        Build.ComposedDamageMultiplier = Aggregator.Compose(EBreakerAggregatedAttribute::DamageMultiplier);
        // PreAttributeChange's clamps, applied here because the aggregator is
        // pure arithmetic and the attribute set is what enforces the ranges.
        Build.CriticalChance = FMath::Clamp(Aggregator.Compose(EBreakerAggregatedAttribute::CriticalChance), 0.0f, 1.0f);
        Build.CriticalMultiplier = FMath::Max(1.0f, Aggregator.Compose(EBreakerAggregatedAttribute::CriticalMultiplier));
        Build.EffectiveCrit = 1.0f + Build.CriticalChance * (Build.CriticalMultiplier - 1.0f);
        Build.Total = Build.ComposedDamageMultiplier * Build.EffectiveCrit;
        return Build;
    }

    // ---- The two characters ------------------------------------------------

    // BASELINE: a full set of gear, every point spent, no direction. Mid-band
    // rolls, Weapon Damage wherever it happened to land, a little crit, one
    // conditional line it did not build around, and no Convergence node at
    // all — so no More multiplier. O27's "hitting 50 must be satisfying with
    // decent power" is what this build is. ItemLevel/Tier are the caller's:
    // O36 measures this same character at two different gear depths.
    TArray<FBreakerItemInstance> BaselineLoadout(int32 ItemLevel, int32 Tier)
    {
        return MakeLoadout({
            {EBreakerEquipSlot::Helmet,     Tier, {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::BodyArmour, Tier, {TEXT("Offense.WeaponDamage"), TEXT("Core.Health"), TEXT("Core.PhysicalDR")}},
            {EBreakerEquipSlot::Gloves,     Tier, {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage")}},
            {EBreakerEquipSlot::Boots,      Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Core.MoveSpeed")}},
            {EBreakerEquipSlot::Necklace,   Tier, {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage")}},
            {EBreakerEquipSlot::Waist,      Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AddedDamage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Primary,    Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AddedDamage"), TEXT("Core.MaxResource")}},
            {EBreakerEquipSlot::Secondary,  Tier, {TEXT("Offense.WeaponDamage"), TEXT("Core.ResourceRegen"), TEXT("Core.Health")}},
        }, ItemLevel);
    }

    TArray<FBreakerNodeRank> BaselineRanks()
    {
        // Gateways, the generalist ladders at partial rank, and a lot of
        // defence and utility. No Convergence, no conditional node.
        return {
            {TEXT("Core.Precision.Sightline"), 1},
            {TEXT("Core.Precision.CalledShot"), 2},
            {TEXT("Core.Volley.TriggerDiscipline"), 1},
            {TEXT("Core.Volley.Cyclic"), 3},
            {TEXT("Core.Volley.Salvo"), 2},
            {TEXT("Core.Affliction.OpenWound"), 1},
            {TEXT("Core.Affliction.Deepen"), 3},
            {TEXT("Core.Bulwark.SetStance"), 1},
            {TEXT("Core.Bulwark.Read"), 3},
            {TEXT("Core.Bulwark.Parry"), 1},
            {TEXT("Core.Kinesis.LightFooting"), 1},
            {TEXT("Core.Kinesis.Loft"), 3},
            {TEXT("Core.Kinesis.AirJump"), 1},
            {TEXT("Core.Kinesis.PhantomStep"), 1},
            {TEXT("Swift.Marksman.LongLens"), 2},
            {TEXT("Swift.Marksman.Steady"), 2},
            {TEXT("Swift.Marksman.Ledger"), 2},
            {TEXT("Swift.Kinetic.ReadTheRoom"), 2},
            {TEXT("Swift.Kinetic.Carry"), 2},
            {TEXT("Swift.Kinetic.Landing"), 2},
        };
    }

    // OPTIMIZED: the airborne Swift build the Velocity constellation exists
    // for. Top-band rolls on every slot, conditional damage lines chosen to
    // match the states it actually holds, and five More sources of which O3
    // lets three count. This is "optimized 50 feels great" (at cap) / "gear
    // depth is real" (at endgame) depending which ItemLevel/Tier is passed.
    TArray<FBreakerItemInstance> OptimizedLoadout(int32 ItemLevel, int32 Tier)
    {
        return MakeLoadout({
            {EBreakerEquipSlot::Helmet,     Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage")}},
            {EBreakerEquipSlot::BodyArmour, Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.RedlineDamage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Gloves,     Tier, {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage"), TEXT("Offense.DashDamage")}},
            {EBreakerEquipSlot::Boots,      Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Move.AirControl"), TEXT("Move.DashCooldown")}},
            {EBreakerEquipSlot::Necklace,   Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage")}},
            {EBreakerEquipSlot::Waist,      Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.DashDamage"), TEXT("Offense.AddedDamage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Primary,    Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.RedlineDamage")}},
            {EBreakerEquipSlot::Secondary,  Tier, {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage"), TEXT("Offense.DashDamage")}},
        }, ItemLevel);
    }

    TArray<FBreakerNodeRank> OptimizedRanks()
    {
        return {
            {TEXT("Core.Precision.Sightline"), 1},
            {TEXT("Core.Precision.CalledShot"), 2},
            {TEXT("Core.Precision.TunnelVision"), 1},
            {TEXT("Core.Precision.Fixate"), 1},              // More x1.22, unconditional
            {TEXT("Core.Volley.TriggerDiscipline"), 1},
            {TEXT("Core.Volley.Cyclic"), 3},
            {TEXT("Core.Volley.Salvo"), 3},
            {TEXT("Core.Volley.Barrage"), 1},                // More x1.22, unconditional
            {TEXT("Core.Velocity.Freefall"), 3},             // airborne
            {TEXT("Core.Velocity.Slipstream"), 3},           // sliding: owned, not live
            {TEXT("Core.Velocity.Traction"), 2},             // wall riding: owned, not live
            {TEXT("Core.Velocity.Afterburn"), 3},            // recently dashed
            {TEXT("Core.Velocity.TerminalVelocity"), 1},     // More x1.30, airborne
            {TEXT("Core.Velocity.RedlineDoctrine"), 1},      // More x1.20, at Redline
            {TEXT("Swift.Marksman.LongLens"), 2},
            {TEXT("Swift.Marksman.Deadeye"), 2},
            {TEXT("Swift.Marksman.PierceDiscipline"), 2},
            {TEXT("Swift.Marksman.Culling"), 1},             // More x1.18, unconditional
            {TEXT("Swift.Kinetic.ReadTheRoom"), 2},
            {TEXT("Swift.Kinetic.Downforce"), 2},            // airborne
        };
    }

    // Airborne, recently dashed, at Redline: the rotation the optimized build is
    // organised around, and the state both builds are measured in.
    FBreakerBuildConditionState MeasurementState()
    {
        FBreakerBuildConditionState State;
        State.Set(EBreakerBuildCondition::Airborne, true);
        State.Set(EBreakerBuildCondition::RecentlyDashed, true);
        State.Set(EBreakerBuildCondition::Redline, true);
        return State;
    }
}

// ---------------------------------------------------------------------------
// O36 split this single test into two, each pinned to its own fixture and its
// own band. NAMING IS LOAD-BEARING: UE's automation tree cannot hold a leaf
// test at a node that is ALSO a parent. Before this split,
// "RiorsEdge.Progression.PowerBand.RuleImpact" did exactly that to
// "RiorsEdge.Progression.PowerBand" — the parent path silently swallowed the
// leaf test of the same name, so the 8-10x band assertion was never
// enumerated for as long as that name collision existed (see
// FBreakerRuleBandImpactTest below, which carries the historical fix). The
// guard this pass adds: AtCap and Endgame are SIBLINGS under the
// "RiorsEdge.Progression.PowerBand" node, and no test anywhere in this suite
// may ever be registered at that bare path — the moment one is, it silently
// swallows whichever sibling the tree happens to enumerate alongside it, the
// exact failure mode this whole comment documents. If a third PowerBand
// fixture is ever added, give it a sibling name here too, never the bare one.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPowerBandAtCapTest,
    "RiorsEdge.Progression.PowerBand.AtCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPowerBandAtCapTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    // O36: "AT-CAP (level 50, tiers a level-50 drop can produce): 8-10x
    // stands." WorstTier is always producible (the floor of every roll);
    // BestTierForItemLevel(AtCapItemLevel) is the best item level alone can
    // reach at the character cap (T6) — this IS "tiers a level-50 drop can
    // produce", read as the widest legal spread rather than a fixed pair, so
    // the fixture tracks the tier curve instead of hardcoding a value that
    // could silently stop being reachable under a future retune.
    const int32 BaselineTier = BaselineTierFor(AtCapItemLevel);
    const int32 OptimizedTier = OptimizedTierFor(AtCapItemLevel);

    const FBreakerBuildConditionState State = MeasurementState();
    const FComposedBuild Baseline = Compose(BaselineLoadout(AtCapItemLevel, BaselineTier), BaselineRanks(), State);
    const FComposedBuild Optimized = Compose(OptimizedLoadout(AtCapItemLevel, OptimizedTier), OptimizedRanks(), State);

    // The layer-by-layer report. Logged rather than only asserted, because the
    // arithmetic is the deliverable: a future tuning pass needs to see WHICH
    // layer moved, not just that the band broke.
    AddInfo(FString::Printf(TEXT("AT-CAP BASELINE  (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f | crit x%.3f (%.0f%% @ x%.2f) => x%.2f"),
        AtCapItemLevel, BaselineTier, Baseline.FlatLayer, Baseline.IncreasedLayer, Baseline.MoreLayer, Baseline.EffectiveCrit,
        Baseline.CriticalChance * 100.0f, Baseline.CriticalMultiplier, Baseline.Total));
    AddInfo(FString::Printf(TEXT("AT-CAP OPTIMIZED (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f | crit x%.3f (%.0f%% @ x%.2f) => x%.2f"),
        AtCapItemLevel, OptimizedTier, Optimized.FlatLayer, Optimized.IncreasedLayer, Optimized.MoreLayer, Optimized.EffectiveCrit,
        Optimized.CriticalChance * 100.0f, Optimized.CriticalMultiplier, Optimized.Total));

    const float Ratio = Optimized.Total / Baseline.Total;
    AddInfo(FString::Printf(TEXT("AT-CAP BAND      flat %.2fx | increased %.2fx | more %.2fx | crit %.2fx => COMPOSED %.2fx (O36 target %.0f-%.0fx)"),
        Optimized.FlatLayer / Baseline.FlatLayer,
        Optimized.IncreasedLayer / Baseline.IncreasedLayer,
        Optimized.MoreLayer / Baseline.MoreLayer,
        Optimized.EffectiveCrit / Baseline.EffectiveCrit,
        Ratio, AtCapBandMinimum, AtCapBandMaximum));

    // THE SPREAD IS DELIBERATELY THE WIDEST ONE, AND THAT IS A FINDING, NOT A
    // CHOICE OF CONVENIENCE. The back-loaded ladder (O29) concentrates almost
    // all of its multiplicative growth between T6 and T1; the shallow low end
    // (T12..T6) that a level-50 drop is confined to has comparatively little
    // gear-tier spread on its own. Narrower baseline/optimized pairings within
    // [T6,T12] were measured against the exact aggregation formula before this
    // fixture was authored and land well under O36's 8x floor — the O3 More
    // budget and the node choices (identical in both O36 bands, because
    // character level does not move with item level) carry most of the band
    // here, and gear supplies the rest only at its full available spread.
    // Reported to CONTEXT.md rather than silently absorbed into the fixture.
    TestTrue(*FString::Printf(TEXT("AT-CAP band %.2fx is at least %.1fx"), Ratio, AtCapBandMinimum), Ratio >= AtCapBandMinimum);
    TestTrue(*FString::Printf(TEXT("AT-CAP band %.2fx is at most %.1fx"), Ratio, AtCapBandMaximum), Ratio <= AtCapBandMaximum);

    // Structural properties of the band, each of which the doc states and each
    // of which a tuning pass could break without moving the ratio.

    // O3 is not broken to reach it: at most three Mores, each at or under 1.30.
    TestTrue(TEXT("Optimized More product respects the O3 cap of three at 1.30x each"),
        Optimized.MoreLayer <= FMath::Pow(UBreakerProgressionComponent::SingleMoreCeiling,
            static_cast<float>(UBreakerProgressionComponent::MaxDamageMoreSources)) + UE_KINDA_SMALL_NUMBER);
    TestTrue(TEXT("The optimized build actually holds More multipliers"), Optimized.MoreLayer > 1.5f);
    TestEqual(TEXT("The baseline build holds none"), Baseline.MoreLayer, 1.0f, 0.0001f);

    // The band is earned across all three layers, so no single one is the build.
    TestTrue(TEXT("Increased carries part of the band"), Optimized.IncreasedLayer / Baseline.IncreasedLayer > 1.8f);
    TestTrue(TEXT("Crit carries part of the band"), Optimized.EffectiveCrit / Baseline.EffectiveCrit > 1.4f);
    TestTrue(TEXT("No single layer is the whole band"),
        FMath::Max3(Optimized.IncreasedLayer / Baseline.IncreasedLayer, Optimized.MoreLayer / Baseline.MoreLayer,
            Optimized.EffectiveCrit / Baseline.EffectiveCrit) < Ratio * 0.5f);

    // O27: choices beat accumulation. Both builds spend the same budget, so the
    // accumulation term is identical on both sides; strip it from both and the
    // band must barely move. If someone raises IncreasedDamagePerSpentPoint back
    // toward 1.0 this is the assertion that notices.
    const float AccumulationPercent = PowerBandFullPointBudget * 0.25f;
    const float BaselineWithout = Baseline.Total * (Baseline.IncreasedLayer - AccumulationPercent / 100.0f) / Baseline.IncreasedLayer;
    const float OptimizedWithout = Optimized.Total * (Optimized.IncreasedLayer - AccumulationPercent / 100.0f) / Optimized.IncreasedLayer;
    const float RatioWithoutAccumulation = OptimizedWithout / BaselineWithout;
    AddInfo(FString::Printf(TEXT("AT-CAP BAND without the per-point accumulation floor: %.2fx"), RatioWithoutAccumulation));
    TestTrue(TEXT("Accumulation is a floor, not the band: removing it widens the band, never narrows it"),
        RatioWithoutAccumulation >= Ratio);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPowerBandEndgameTest,
    "RiorsEdge.Progression.PowerBand.Endgame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPowerBandEndgameTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    const FBreakerBuildConditionState State = MeasurementState();
    const FComposedBuild Baseline = Compose(BaselineLoadout(EndgameItemLevel, BaselineTierFor(EndgameItemLevel)), BaselineRanks(), State);
    const FComposedBuild Optimized = Compose(OptimizedLoadout(EndgameItemLevel, OptimizedTierFor(EndgameItemLevel)), OptimizedRanks(), State);

    // The layer-by-layer report. Logged rather than only asserted, because the
    // arithmetic is the deliverable: a future tuning pass needs to see WHICH
    // layer moved, not just that the band broke.
    AddInfo(FString::Printf(TEXT("ENDGAME BASELINE  (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f | crit x%.3f (%.0f%% @ x%.2f) => x%.2f"),
        EndgameItemLevel, BaselineTierFor(EndgameItemLevel), Baseline.FlatLayer, Baseline.IncreasedLayer, Baseline.MoreLayer, Baseline.EffectiveCrit,
        Baseline.CriticalChance * 100.0f, Baseline.CriticalMultiplier, Baseline.Total));
    AddInfo(FString::Printf(TEXT("ENDGAME OPTIMIZED (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f | crit x%.3f (%.0f%% @ x%.2f) => x%.2f"),
        EndgameItemLevel, OptimizedTierFor(EndgameItemLevel), Optimized.FlatLayer, Optimized.IncreasedLayer, Optimized.MoreLayer, Optimized.EffectiveCrit,
        Optimized.CriticalChance * 100.0f, Optimized.CriticalMultiplier, Optimized.Total));

    const float Ratio = Optimized.Total / Baseline.Total;
    AddInfo(FString::Printf(TEXT("ENDGAME BAND      flat %.2fx | increased %.2fx | more %.2fx | crit %.2fx => COMPOSED %.2fx (O36 seed rails %.0f-%.0fx, O2 PLACEHOLDER)"),
        Optimized.FlatLayer / Baseline.FlatLayer,
        Optimized.IncreasedLayer / Baseline.IncreasedLayer,
        Optimized.MoreLayer / Baseline.MoreLayer,
        Optimized.EffectiveCrit / Baseline.EffectiveCrit,
        Ratio, EndgameBandMinimum, EndgameBandMaximum));

    // O36 (O2 PLACEHOLDER SEED): "seed rails 12-20x... the back-loaded ladder
    // currently measures ~15x, accepted pending playtest." This is the ruling
    // that resolves the fixture this test inherited from the pre-split single
    // PowerBand test: O29 widened the affix ladder and raised every ceiling
    // anchor ~2.2x, the 8-10x band was authored against the pre-O29 ladder,
    // and the honest reading was never "the band broke" — it is that O29 MOVED
    // WHERE THE TOP OF THE BAND LIVES, past the character cap, into gear
    // depth, which is exactly O29's own thesis ("all endgame character power
    // comes from gear"). See FBreakerPowerBandAtCapTest above for the other
    // half of the split: the SAME character, SAME choices, measured at the
    // character cap instead, stays inside the original 8-10x band. Do not
    // "fix" a future measurement outside this range by widening it again
    // without a new O-ruling — that repeats the mistake this split exists to
    // correct.
    TestTrue(*FString::Printf(TEXT("ENDGAME band %.2fx is at least %.1fx"), Ratio, EndgameBandMinimum), Ratio >= EndgameBandMinimum);
    TestTrue(*FString::Printf(TEXT("ENDGAME band %.2fx is at most %.1fx"), Ratio, EndgameBandMaximum), Ratio <= EndgameBandMaximum);

    // Structural properties of the band, each of which the doc states and each
    // of which a tuning pass could break without moving the ratio.

    // O3 is not broken to reach it: at most three Mores, each at or under 1.30.
    TestTrue(TEXT("Optimized More product respects the O3 cap of three at 1.30x each"),
        Optimized.MoreLayer <= FMath::Pow(UBreakerProgressionComponent::SingleMoreCeiling,
            static_cast<float>(UBreakerProgressionComponent::MaxDamageMoreSources)) + UE_KINDA_SMALL_NUMBER);
    TestTrue(TEXT("The optimized build actually holds More multipliers"), Optimized.MoreLayer > 1.5f);
    TestEqual(TEXT("The baseline build holds none"), Baseline.MoreLayer, 1.0f, 0.0001f);

    // The band is earned across all three layers, so no single one is the build.
    TestTrue(TEXT("Increased carries part of the band"), Optimized.IncreasedLayer / Baseline.IncreasedLayer > 1.8f);
    TestTrue(TEXT("Crit carries part of the band"), Optimized.EffectiveCrit / Baseline.EffectiveCrit > 1.4f);
    TestTrue(TEXT("No single layer is the whole band"),
        FMath::Max3(Optimized.IncreasedLayer / Baseline.IncreasedLayer, Optimized.MoreLayer / Baseline.MoreLayer,
            Optimized.EffectiveCrit / Baseline.EffectiveCrit) < Ratio * 0.5f);

    // O27: choices beat accumulation. Both builds spend the same budget, so the
    // accumulation term is identical on both sides; strip it from both and the
    // band must barely move. If someone raises IncreasedDamagePerSpentPoint back
    // toward 1.0 this is the assertion that notices.
    const float AccumulationPercent = PowerBandFullPointBudget * 0.25f;
    const float BaselineWithout = Baseline.Total * (Baseline.IncreasedLayer - AccumulationPercent / 100.0f) / Baseline.IncreasedLayer;
    const float OptimizedWithout = Optimized.Total * (Optimized.IncreasedLayer - AccumulationPercent / 100.0f) / Optimized.IncreasedLayer;
    const float RatioWithoutAccumulation = OptimizedWithout / BaselineWithout;
    AddInfo(FString::Printf(TEXT("ENDGAME BAND without the per-point accumulation floor: %.2fx"), RatioWithoutAccumulation));
    TestTrue(TEXT("Accumulation is a floor, not the band: removing it widens the band, never narrows it"),
        RatioWithoutAccumulation >= Ratio);

    return true;
}

// ---------------------------------------------------------------------------
// WHAT A RULE REWRITE IS WORTH, measured against the band it has to live in.
// ---------------------------------------------------------------------------
// The band above is unchanged by the rarity pass, and that is by construction:
// FBreakerItemInstance::Rule defaults to None and the two loadouts are authored
// affix by affix, so the power-band characters carry no rewrite even though
// every piece is built at Anomalous to lift the tier cap. Which means the band
// test on its own would say NOTHING about whether the rewrites are balanced.
//
// This is that measurement, run at the ENDGAME fixture (ilvl 120): a rewrite
// is available to a baseline and an optimized character alike, so the number
// that matters is not the 12-20x band but the STEP: what one Anomalous piece
// is worth on top of a build that has already done everything else right.
// Logged in full, because the value of the rewrites is the deliverable and a
// future tuning pass needs to see which one moved.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRuleBandImpactTest,
    // SIBLING of PowerBand.AtCap / PowerBand.Endgame above, not a child of
    // either — see their shared naming comment for why UE's automation tree
    // makes that load-bearing. This test itself was RENAMED off
    // "RiorsEdge.Progression.PowerBand.RuleImpact" for the identical reason,
    // historically: that path silently swallowed RiorsEdge.Progression
    // .PowerBand itself, so the (then single) band assertion was not
    // enumerated for the whole time the collision existed. Found while
    // measuring O29's effect on the band.
    "RiorsEdge.Progression.RuleBandImpact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRuleBandImpactTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    const FBreakerBuildConditionState State = MeasurementState();
    const FComposedBuild Baseline = Compose(BaselineLoadout(EndgameItemLevel, BaselineTierFor(EndgameItemLevel)), BaselineRanks(), State);
    const FComposedBuild Optimized = Compose(OptimizedLoadout(EndgameItemLevel, OptimizedTierFor(EndgameItemLevel)), OptimizedRanks(), State);
    const float PlainBand = Optimized.Total / Baseline.Total;

    // O2 PLACEHOLDER, and the reason it is stated here rather than felt later:
    // one Anomalous rewrite is the top of the rarity ladder, so it has to be a
    // real step. It must NOT be so large that finding the right Anomalous is
    // worth more than the whole optimized loadout, which is what "choices beat
    // accumulation" (O27) would look like inverted.
    constexpr float MaximumRuleStep = 1.35f;
    // O36's re-anchor: PROLIFIC gets its OWN, HIGHER ceiling at the endgame
    // fixture. Its whole value IS the size of the T1->T0 tier step (it
    // resolves an affix one tier better), and O29 re-sited that spike from
    // x1.4 to x2.2 -- PROLIFIC got materially stronger without anybody editing
    // it, which is a real and expected consequence of the wider ladder, not a
    // balance regression. Re-using the generic 1.35x ceiling here would fail
    // on content working exactly as designed (measured ~1.462x against the
    // old 1.35x ceiling). Every OTHER rollable rewrite stays at 1.35x.
    constexpr float MaximumProlificRuleStep = 1.5f;   // O36, O2 PLACEHOLDER seed

    for (const FBreakerItemRuleDefinition& Definition : UBreakerItemRuleLibrary::GetRuleDefinitions())
    {
        if (!Definition.bRollable) continue;   // legendaries have their own tests

        // The rewrite lands on ONE piece, because the equip cap is one
        // Anomalous. Helmet: it carries damage, crit and a conditional line, so
        // every rollable rewrite has something on it to bite on.
        TArray<FBreakerItemInstance> WithRule = OptimizedLoadout(EndgameItemLevel, OptimizedTierFor(EndgameItemLevel));
        WithRule[0].Rule = Definition.Rule;
        const FComposedBuild Ruled = Compose(WithRule, OptimizedRanks(), State);

        const float Step = Ruled.Total / Optimized.Total;
        const float RuledBand = Ruled.Total / Baseline.Total;

        // The SAME step measured while STANDING STILL, and it is not a footnote:
        // the band above is measured airborne, recently dashed and at Redline,
        // which is the one state in which UNBOUND is worth exactly nothing. A
        // rewrite whose whole job is to free conditional lines has to be
        // measured somewhere its conditions are false, or the report says it is
        // worthless when it is the largest rewrite in the table.
        const FBreakerBuildConditionState Grounded;
        const FComposedBuild GroundedPlain = Compose(OptimizedLoadout(EndgameItemLevel, OptimizedTierFor(EndgameItemLevel)), OptimizedRanks(), Grounded);
        const FComposedBuild GroundedRuled = Compose(WithRule, OptimizedRanks(), Grounded);
        const float GroundedStep = GroundedRuled.Total / GroundedPlain.Total;

        const float StepCeiling = Definition.Rule == EBreakerItemRule::Prolific ? MaximumProlificRuleStep : MaximumRuleStep;

        AddInfo(FString::Printf(TEXT("RULE %-12s step x%.3f in rotation | x%.3f standing still | band %.2fx (plain %.2fx) | ceiling x%.2f"),
            *Definition.DisplayName.ToString(), Step, GroundedStep, RuledBand, PlainBand, StepCeiling));
        TestTrue(*FString::Printf(TEXT("%s never lowers a grounded build either"),
            *Definition.DisplayName.ToString()), GroundedStep >= 1.0f - UE_KINDA_SMALL_NUMBER);

        TestTrue(*FString::Printf(TEXT("%s never LOWERS an optimized build's damage"),
            *Definition.DisplayName.ToString()), Step >= 1.0f - UE_KINDA_SMALL_NUMBER);
        TestTrue(*FString::Printf(TEXT("%s is worth at most x%.2f on top of an optimized build (measured x%.3f)"),
            *Definition.DisplayName.ToString(), StepCeiling, Step), Step <= StepCeiling);
        // ...and it must not be the whole build. A rewrite that outweighs the
        // endgame band would make every other decision a rounding error.
        TestTrue(*FString::Printf(TEXT("%s is smaller than the band it lives in"),
            *Definition.DisplayName.ToString()), Step < PlainBand);
    }

    // The pass's own claim, asserted: an item with no rewrite composes exactly
    // as it did before rules existed. If this ever fails, a rewrite has leaked
    // out of its item and become a property of rarity.
    TArray<FBreakerItemInstance> Untouched = OptimizedLoadout(EndgameItemLevel, OptimizedTierFor(EndgameItemLevel));
    for (const FBreakerItemInstance& Item : Untouched)
    {
        TestEqual(TEXT("A power-band piece carries no rewrite despite being Anomalous"),
            static_cast<int32>(Item.Rule), static_cast<int32>(EBreakerItemRule::None));
    }
    TestEqual(TEXT("The measured band is untouched by the rarity pass"),
        Compose(Untouched, OptimizedRanks(), State).Total / Baseline.Total, PlainBand, 0.0001f);
    return true;
}

// Conditional lines are the movement pillar's offensive expression. This test
// pins the property that makes them a CHOICE rather than a free bonus: they are
// worth their full value while the state holds and exactly nothing otherwise.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerConditionalDamageTest,
    "RiorsEdge.Progression.ConditionalDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerConditionalDamageTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    const TArray<FBreakerItemInstance> Loadout = OptimizedLoadout(EndgameItemLevel, OptimizedTierFor(EndgameItemLevel));
    const TArray<FBreakerNodeRank> Ranks = OptimizedRanks();

    FBreakerBuildConditionState Grounded;
    FBreakerBuildConditionState Airborne;
    Airborne.Set(EBreakerBuildCondition::Airborne, true);

    const FComposedBuild Standing = Compose(Loadout, Ranks, Grounded);
    const FComposedBuild InAir = Compose(Loadout, Ranks, Airborne);

    TestTrue(TEXT("Going airborne raises the additive bucket"), InAir.IncreasedLayer > Standing.IncreasedLayer + 0.5f);
    TestTrue(TEXT("Going airborne brings a conditional More online"), InAir.MoreLayer > Standing.MoreLayer);
    TestTrue(TEXT("A grounded movement build is materially weaker than an airborne one"), InAir.Total > Standing.Total * 1.5f);

    // The empty state is the neutral one: nothing conditional pays, and nothing
    // unconditional is lost. This is what keeps every pre-existing call site
    // (the skill screen's projection included) behaving exactly as before.
    TestTrue(TEXT("Unconditional power survives with no condition active"), Standing.IncreasedLayer > 1.5f);
    TestTrue(TEXT("Unconditional More survives with no condition active"), Standing.MoreLayer > 1.0f);

    // Display figures: the tooltip must be able to say what a line is worth
    // before the player is in the state that turns it on.
    const FBreakerEquipmentStats Grounded_Stats = UBreakerEquipmentComponent::AggregateStats(Loadout, nullptr, Grounded);
    const FBreakerEquipmentStats Air_Stats = UBreakerEquipmentComponent::AggregateStats(Loadout, nullptr, Airborne);
    TestEqual(TEXT("Nothing conditional is live while grounded"), Grounded_Stats.ActiveConditionalDamagePercent, 0.0f, 0.0001f);
    TestTrue(TEXT("The potential figure is stated even while grounded"), Grounded_Stats.PotentialConditionalDamagePercent > 0.0f);
    TestTrue(TEXT("Airborne turns part of the potential into live power"),
        Air_Stats.ActiveConditionalDamagePercent > 0.0f
        && Air_Stats.ActiveConditionalDamagePercent < Air_Stats.PotentialConditionalDamagePercent);
    TestEqual(TEXT("Potential does not depend on the state"),
        Air_Stats.PotentialConditionalDamagePercent, Grounded_Stats.PotentialConditionalDamagePercent, 0.0001f);
    return true;
}

// Every slot must be able to raise damage. This is the structural failure
// Power-Curve §"More options in every avenue" names outright: "helmet, body,
// boots and waist are structurally incapable of increasing damage".
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAffixBreadthTest,
    "RiorsEdge.Items.Affixes.Breadth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAffixBreadthTest::RunTest(const FString& Parameters)
{
    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();

    int32 OffensiveCount = 0;
    int32 ConditionalCount = 0;
    for (const FBreakerAffixDefinition& Affix : Pool)
    {
        if (UBreakerAffixLibrary::IsOffensiveTarget(Affix.StatTarget)) ++OffensiveCount;
        if (Affix.IsConditional()) ++ConditionalCount;
        TestTrue(*(Affix.AffixId.ToString() + TEXT(" rolls on at least one slot")), Affix.AllowedSlots.Num() > 0);
        // No affix may author a More multiplier; those are reserved for trees
        // and Anomalous rule rewrites (O3, Item-Foundation's locked rule).
        TestTrue(*(Affix.AffixId.ToString() + TEXT(" does not author a More multiplier")),
            Affix.StatBucket != EBreakerStatBucket::MorePercent);
    }

    TestTrue(TEXT("The pool is materially wider than the twelve-line slice"), Pool.Num() >= 18);
    TestTrue(TEXT("Offence is a family, not a single line"), OffensiveCount >= 8);
    TestTrue(TEXT("Conditional damage exists at all"), ConditionalCount >= 5);

    for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
    {
        const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
        int32 OffensiveOnSlot = 0;
        int32 ConditionalOnSlot = 0;
        for (const FBreakerAffixDefinition& Affix : Pool)
        {
            if (!Affix.AllowsSlot(Slot)) continue;
            if (!UBreakerAffixLibrary::IsOffensiveTarget(Affix.StatTarget)) continue;
            ++OffensiveOnSlot;
            if (Affix.IsConditional()) ++ConditionalOnSlot;
        }
        const FString Context = UEnum::GetValueAsString(Slot);
        // The rule O27 exposed: NO slot may be structurally incapable of
        // raising damage.
        TestTrue(*(Context + TEXT(" can raise damage at all")), OffensiveOnSlot >= 2);
        // Per-slot identity: gearing is a set of decisions, so every slot has
        // at least one conditional line of its own to chase.
        TestTrue(*(Context + TEXT(" has a conditional line of its own")), ConditionalOnSlot >= 1);
    }
    return true;
}

#endif
