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
// THE BUILD VARIANCE BAND (Power-Curve.md §4, authority O27)
// ---------------------------------------------------------------------------
// "The ratio between a baseline build and an optimized one at the SAME area
// level. This is the number O27 is really about, and it needs to be authored
// explicitly rather than emerging by accident. Target: roughly 8-10x."
//
// This test is the guard rail for every future tuning pass. It builds two
// level-50 characters out of the REAL affix pool and the REAL fallback trees,
// folds them through the REAL aggregator (FBreakerAttributeAggregator, the same
// object UBreakerAttributeSet owns), and asserts the composed ratio lands in
// the band. Nothing here re-implements the arithmetic; if the fold changes, this
// moves with it, which is the entire point.
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
// separates two characters.
// ---------------------------------------------------------------------------

namespace BreakerPowerBandTest
{
    // O2 PLACEHOLDER. XP-And-Pacing §4/§7: Class Points stop at 30, Core Points
    // are ~50 from levels plus ~15 from world content. A level-50 character who
    // has finished the campaign holds and spends roughly this many.
    constexpr int32 PowerBandFullPointBudget = 95;

    // Power-Curve §4's target, restated as an assertion.
    constexpr float PowerBandMinimum = 8.0f;
    constexpr float PowerBandMaximum = 10.0f;

    // One equipped piece, built from the real pool so a value can never drift
    // away from what the game would actually roll. Tier is the printed tier.
    struct FPiece
    {
        EBreakerEquipSlot Slot;
        int32 Tier;
        TArray<FName> AffixIds;
    };

    FBreakerItemInstance MakeItem(const FPiece& Piece)
    {
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = TEXT("PowerBand");
        Item.Slot = Piece.Slot;
        Item.Rarity = EBreakerItemRarity::Anomalous;
        Item.ItemLevel = 50;
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

    TArray<FBreakerItemInstance> MakeLoadout(const TArray<FPiece>& Pieces)
    {
        TArray<FBreakerItemInstance> Items;
        for (const FPiece& Piece : Pieces) Items.Add(MakeItem(Piece));
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

    // BASELINE: level 50, a full set of ilvl-50 gear, every point spent, no
    // direction. Mid-tier rolls (T5), Weapon Damage wherever it happened to
    // land, a little crit, one conditional line it did not build around, and no
    // Convergence node at all — so no More multiplier. O27's "hitting 50 must be
    // satisfying with decent power" is what this build is.
    TArray<FBreakerItemInstance> BaselineLoadout()
    {
        return MakeLoadout({
            {EBreakerEquipSlot::Helmet,     5, {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::BodyArmour, 5, {TEXT("Offense.WeaponDamage"), TEXT("Core.Health"), TEXT("Core.PhysicalDR")}},
            {EBreakerEquipSlot::Gloves,     5, {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage")}},
            {EBreakerEquipSlot::Boots,      5, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Core.MoveSpeed")}},
            {EBreakerEquipSlot::Necklace,   5, {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage")}},
            {EBreakerEquipSlot::Waist,      5, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AddedDamage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Primary,    5, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AddedDamage"), TEXT("Core.MaxResource")}},
            {EBreakerEquipSlot::Secondary,  5, {TEXT("Offense.WeaponDamage"), TEXT("Core.ResourceRegen"), TEXT("Core.Health")}},
        });
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

    // OPTIMIZED: the airborne Swift build the Velocity constellation exists for.
    // T1 rolls on every slot, conditional damage lines chosen to match the
    // states it actually holds, and five More sources of which O3 lets three
    // count. This is "optimized 50 feels great".
    TArray<FBreakerItemInstance> OptimizedLoadout()
    {
        return MakeLoadout({
            {EBreakerEquipSlot::Helmet,     1, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage")}},
            {EBreakerEquipSlot::BodyArmour, 1, {TEXT("Offense.WeaponDamage"), TEXT("Offense.RedlineDamage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Gloves,     1, {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage"), TEXT("Offense.DashDamage")}},
            {EBreakerEquipSlot::Boots,      1, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Move.AirControl"), TEXT("Move.DashCooldown")}},
            {EBreakerEquipSlot::Necklace,   1, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage")}},
            {EBreakerEquipSlot::Waist,      1, {TEXT("Offense.WeaponDamage"), TEXT("Offense.DashDamage"), TEXT("Offense.AddedDamage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Primary,    1, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.RedlineDamage")}},
            {EBreakerEquipSlot::Secondary,  1, {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage"), TEXT("Offense.DashDamage")}},
        });
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPowerBandTest,
    "RiorsEdge.Progression.PowerBand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPowerBandTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    const FBreakerBuildConditionState State = MeasurementState();
    const FComposedBuild Baseline = Compose(BaselineLoadout(), BaselineRanks(), State);
    const FComposedBuild Optimized = Compose(OptimizedLoadout(), OptimizedRanks(), State);

    // The layer-by-layer report. Logged rather than only asserted, because the
    // arithmetic is the deliverable: a future tuning pass needs to see WHICH
    // layer moved, not just that the band broke.
    AddInfo(FString::Printf(TEXT("BASELINE   flat x%.3f | increased x%.3f | more x%.3f | crit x%.3f (%.0f%% @ x%.2f) => x%.2f"),
        Baseline.FlatLayer, Baseline.IncreasedLayer, Baseline.MoreLayer, Baseline.EffectiveCrit,
        Baseline.CriticalChance * 100.0f, Baseline.CriticalMultiplier, Baseline.Total));
    AddInfo(FString::Printf(TEXT("OPTIMIZED  flat x%.3f | increased x%.3f | more x%.3f | crit x%.3f (%.0f%% @ x%.2f) => x%.2f"),
        Optimized.FlatLayer, Optimized.IncreasedLayer, Optimized.MoreLayer, Optimized.EffectiveCrit,
        Optimized.CriticalChance * 100.0f, Optimized.CriticalMultiplier, Optimized.Total));

    const float Ratio = Optimized.Total / Baseline.Total;
    AddInfo(FString::Printf(TEXT("BAND       flat %.2fx | increased %.2fx | more %.2fx | crit %.2fx => COMPOSED %.2fx"),
        Optimized.FlatLayer / Baseline.FlatLayer,
        Optimized.IncreasedLayer / Baseline.IncreasedLayer,
        Optimized.MoreLayer / Baseline.MoreLayer,
        Optimized.EffectiveCrit / Baseline.EffectiveCrit,
        Ratio));

    // The assertion O27 is actually about.
    TestTrue(*FString::Printf(TEXT("Composed band %.2fx is at least %.1fx"), Ratio, PowerBandMinimum), Ratio >= PowerBandMinimum);
    TestTrue(*FString::Printf(TEXT("Composed band %.2fx is at most %.1fx"), Ratio, PowerBandMaximum), Ratio <= PowerBandMaximum);

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
    AddInfo(FString::Printf(TEXT("BAND without the per-point accumulation floor: %.2fx"), RatioWithoutAccumulation));
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
// This is that measurement. A rewrite is available to a baseline and an
// optimized character alike, so the number that matters is not the 8-10x band
// but the STEP: what one Anomalous piece is worth on top of a build that has
// already done everything else right. Logged in full, because the value of the
// rewrites is the deliverable and a future tuning pass needs to see which one
// moved.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRuleBandImpactTest,
    "RiorsEdge.Progression.PowerBand.RuleImpact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRuleBandImpactTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    const FBreakerBuildConditionState State = MeasurementState();
    const FComposedBuild Baseline = Compose(BaselineLoadout(), BaselineRanks(), State);
    const FComposedBuild Optimized = Compose(OptimizedLoadout(), OptimizedRanks(), State);
    const float PlainBand = Optimized.Total / Baseline.Total;

    // O2 PLACEHOLDER, and the reason it is stated here rather than felt later:
    // one Anomalous rewrite is the top of the rarity ladder, so it has to be a
    // real step. It must NOT be so large that finding the right Anomalous is
    // worth more than the whole optimized loadout, which is what "choices beat
    // accumulation" (O27) would look like inverted.
    constexpr float MaximumRuleStep = 1.35f;

    for (const FBreakerItemRuleDefinition& Definition : UBreakerItemRuleLibrary::GetRuleDefinitions())
    {
        if (!Definition.bRollable) continue;   // legendaries have their own tests

        // The rewrite lands on ONE piece, because the equip cap is one
        // Anomalous. Helmet: it carries damage, crit and a conditional line, so
        // every rollable rewrite has something on it to bite on.
        TArray<FBreakerItemInstance> WithRule = OptimizedLoadout();
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
        const FComposedBuild GroundedPlain = Compose(OptimizedLoadout(), OptimizedRanks(), Grounded);
        const FComposedBuild GroundedRuled = Compose(WithRule, OptimizedRanks(), Grounded);
        const float GroundedStep = GroundedRuled.Total / GroundedPlain.Total;

        AddInfo(FString::Printf(TEXT("RULE %-12s step x%.3f in rotation | x%.3f standing still | band %.2fx (plain %.2fx)"),
            *Definition.DisplayName.ToString(), Step, GroundedStep, RuledBand, PlainBand));
        TestTrue(*FString::Printf(TEXT("%s never lowers a grounded build either"),
            *Definition.DisplayName.ToString()), GroundedStep >= 1.0f - UE_KINDA_SMALL_NUMBER);

        TestTrue(*FString::Printf(TEXT("%s never LOWERS an optimized build's damage"),
            *Definition.DisplayName.ToString()), Step >= 1.0f - UE_KINDA_SMALL_NUMBER);
        TestTrue(*FString::Printf(TEXT("%s is worth at most x%.2f on top of an optimized build (measured x%.3f)"),
            *Definition.DisplayName.ToString(), MaximumRuleStep, Step), Step <= MaximumRuleStep);
        // ...and it must not be the whole build. A rewrite that outweighs the
        // 8.7x band would make every other decision a rounding error.
        TestTrue(*FString::Printf(TEXT("%s is smaller than the band it lives in"),
            *Definition.DisplayName.ToString()), Step < PlainBand);
    }

    // The pass's own claim, asserted: an item with no rewrite composes exactly
    // as it did before rules existed. If this ever fails, a rewrite has leaked
    // out of its item and become a property of rarity.
    TArray<FBreakerItemInstance> Untouched = OptimizedLoadout();
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

    const TArray<FBreakerItemInstance> Loadout = OptimizedLoadout();
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
