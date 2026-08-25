#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/BreakerBaselineLoadout.h"
#include "Tests/BreakerStatusEmit.h"
#include "Tests/BreakerPowerBandFixture.h"
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

    // O99: THE PARITY BAND. An ability-geared build and a weapon-geared build
    // land within roughly 15% of each other at the same gear depth. This is the
    // first time "neither lane trivializes the other" is a number rather than a
    // sentiment, and it is a TARGET: the measurement is 0.647x and pinning
    // there would enshrine abilities as a second-class lane, which is the same
    // mistake as pinning the defence inversion at its current 3.76.
    //
    // Ruled at the CAP. Endgame parity is measured and reported beside it but
    // deliberately unpinned — whether the figure holds at item level 120 is a
    // different question, because the endgame band is far more crit-driven and
    // crit is currently a weapon-lane story. Divergence between the two is a
    // finding in its own right, not a second edge of this one.
    constexpr float AbilityParityBandMinimum = 0.85f;
    constexpr float AbilityParityBandMaximum = 1.15f;

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
        // O54's second delivery lane, composed from the same fold. Crit is a
        // site multiplier and applies to both lanes identically, so AbilityTotal
        // uses the same EffectiveCrit — the two totals differ only by which
        // additive bucket and which More product fed them, which is exactly the
        // comparison the parity figure wants to make.
        float ComposedAbilityMultiplier = 1.0f;
        // Structurally 1.0 today, and that is a measurement rather than a
        // placeholder: Added Damage bids Flat into the WEAPON lane only, and
        // O54's three pools are three INCREASED pools — the flat half has no
        // ability counterpart at all. Printed so the report says so.
        float AbilityFlatLayer = 1.0f;
        float AbilityIncreasedLayer = 1.0f;
        float AbilityMoreLayer = 1.0f;
        float AbilityTotal = 1.0f;
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
        // Shared, matching RecalculateStats: the floor lands in both lanes.
        ProgressionOffer.AddSharedIncreasedDamage(
            PowerBandFullPointBudget * 0.25f); // matches IncreasedDamagePerSpentPoint's default

        // The real aggregator, seeded with UBreakerAttributeSet's authored bases.
        FBreakerAttributeAggregator Aggregator;
        float Bases[FBreakerAttributeAggregator::AttributeCount] = {};
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::CriticalChance)] = 0.05f;
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::CriticalMultiplier)] = 1.5f;
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::DamageMultiplier)] = 1.0f;
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::AbilityDamageMultiplier)] = 1.0f;
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

        Build.AbilityIncreasedLayer = 1.0f + (EquipmentOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier)
            + ProgressionOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier)) / 100.0f;
        Build.AbilityMoreLayer = EquipmentOffer.GetMore(EBreakerAggregatedAttribute::AbilityDamageMultiplier)
            * ProgressionOffer.GetMore(EBreakerAggregatedAttribute::AbilityDamageMultiplier);
        Build.AbilityFlatLayer = 1.0f
            + EquipmentOffer.GetFlat(EBreakerAggregatedAttribute::AbilityDamageMultiplier)
            + ProgressionOffer.GetFlat(EBreakerAggregatedAttribute::AbilityDamageMultiplier);
        Build.ComposedAbilityMultiplier = Aggregator.Compose(EBreakerAggregatedAttribute::AbilityDamageMultiplier);
        Build.AbilityTotal = Build.ComposedAbilityMultiplier * Build.EffectiveCrit;
        return Build;
    }

    // ---- The two characters ------------------------------------------------

    // BASELINE: a full set of gear, every point spent, no direction. Mid-band
    // rolls, Weapon Damage wherever it happened to land, a little crit, one
    // conditional line it did not build around, and no Convergence node at
    // all — so no More multiplier. O27's "hitting 50 must be satisfying with
    // decent power" is what this build is. ItemLevel/Tier are the caller's:
    // O36 measures this same character at two different gear depths.
    // Built from the ONE authored baseline in Tests/BreakerBaselineLoadout.h,
    // which BreakerPromotedFindingTests reads for the same character. The list
    // used to live here, and time-to-die was measured against a different
    // character in the other file — eight Health lines against this four — so
    // the two disagreed by 1.79x on whether the cap met O18.
    TArray<FBreakerItemInstance> BaselineLoadout(int32 ItemLevel, int32 Tier)
    {
        TArray<FPiece> Pieces;
        for (const BreakerBaselineLoadout::FSlotAffixes& Slot : BreakerBaselineLoadout::BreakerBaselineSlots())
        {
            Pieces.Add({Slot.Slot, Tier, Slot.AffixIds});
        }
        return MakeLoadout(Pieces, ItemLevel);
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

    // THE BAND ITSELF, so that nothing has to transcribe it. Declared in
    // BreakerPowerBandFixture.h and used by two tests: PowerBand.AtCap emits
    // what this returns, and Combat.PowerCurve.BossOptimized divides by what
    // this returns. Neither keeps a copy, which is the whole point -- the copy
    // that used to live in BossOptimized went stale the first time the band
    // moved and claimed in a comment that it could not.
    float AtCapBand()
    {
        const FBreakerBuildConditionState State = MeasurementState();
        const FComposedBuild Baseline = Compose(
            BaselineLoadout(AtCapItemLevel, BaselineTierFor(AtCapItemLevel)), BaselineRanks(), State);
        const FComposedBuild Optimized = Compose(
            OptimizedLoadout(AtCapItemLevel, OptimizedTierFor(AtCapItemLevel)), OptimizedRanks(), State);
        return Optimized.Total / Baseline.Total;
    }

    // -----------------------------------------------------------------------
    // THE REWRITE CEILINGS, all of them in one place, consumed by the
    // RuleBandImpact tests below.
    // -----------------------------------------------------------------------
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
    // MEASURED, NOT CHANGED: 16.0 is the ARITHMETIC mean of 12-20, in a file
    // that treats the band multiplicatively everywhere else -- the at-cap
    // derivation immediately below is Loge(AtCapBandMid) / Loge(EndgameBandMid),
    // i.e. log-space. The geometric mean of 12-20 is 15.4919.
    //
    // The measured no-rewrite build is 15.4720: 0.13% under the geometric mean
    // and 3.30% under this one. Whether that closeness is structural was asked
    // and answered NO in mechanism: nothing in BaselineLoadout, OptimizedLoadout,
    // BaselineRanks, OptimizedRanks or Compose reads a band edge, so no code
    // path pulls the measurement toward either centre -- it is the product of
    // four independently authored layers (1.30 x 2.71 x 1.93 x 2.27). But it is
    // not surprising either: a ratio of two multiplicatively composed builds,
    // tuned by feel to sit mid-band, lands near the GEOMETRIC centre, because
    // that is what "the middle" means for a product. Coincidence in mechanism,
    // a real tendency in kind.
    //
    // WHAT IT WOULD CHANGE, so the owner can rule on it with the numbers in
    // hand: a geometric mid puts the rewrite layer ceiling at 1.2910 rather
    // than 1.2500. Prolific breaches by 13.4% instead of 17.1%, and the
    // authored major-plus-stack of 1.2384 still fits. The red stands either
    // way, which is why this is a measurement and not an edit -- a band edge or
    // its centre is the owner's number.
    constexpr float EndgameBandMid = 16.0f;   // O36 authored 12-20x
    // AND THE SAME INCONSISTENCY IS LIVE HERE, one line down. 9.0 is the
    // arithmetic mean of 8-10; the geometric mean is 8.9443. Recording it
    // beside the note above rather than only there, because a correction
    // applied to one instance of a repeated shape while its neighbour keeps the
    // shape is how this file got two tautologies and four stale justifications.
    //
    // BOTH mids feed the same expression -- Loge(AtCapBandMid) /
    // Loge(EndgameBandMid) -- so switching one and not the other is not a
    // smaller change, it is a wrong one. Consistently geometric:
    // BandShare 0.79248 -> 0.79955 (+0.89%), and MaximumRuleStepAtCap
    // 1.2685 -> 1.2712. No verdict moves: at-cap measures 6.54 against 8-10
    // either way. So this is a note and not an edit, on the same standing rule
    // as the endgame mid -- a band's centre is the owner's number.
    constexpr float AtCapBandMid = 9.0f;      // O36 authored 8-10x

    // A ceiling is a share of the band it sits in, and the two bands are not
    // the same size, so an endgame ceiling carried down to level 50 would
    // assert nothing there. The share is taken in LOG space because a step is
    // multiplicative and so is a band. O2 PLACEHOLDER, AND SO IS THIS LAW
    // ITSELF, which is the part that wants a ruling rather than a number; the
    // arithmetic-share reading (1 + 0.35 * 9/16 = 1.197) is the other
    // candidate and is stated so the choice is visible rather than implied.
    // Anchored to the AUTHORED bands at their midpoints — never to the
    // measurements, because the at-cap measurement is itself out of band and
    // expected-red, and anchoring a ceiling to a number that is already wrong
    // bakes the error in twice.
    inline float AtCapCeilingFor(float EndgameCeiling)
    {
        return FMath::Pow(EndgameCeiling, FMath::Loge(AtCapBandMid) / FMath::Loge(EndgameBandMid));
    }

    // ---- O96: the two rewrite-impact ceilings, derived before authoring ----
    // The restructure (O63/O68) makes the worst-case rewrite layer THREE
    // minors plus ONE major, and O96 orders both ceilings derived before any
    // rewrite is authored against them. The derivation:
    //
    // The LAYER: identity has four independently expandable avenues (O33 —
    // class, Core axes, gear affixes, rule rewrites) and no avenue may be the
    // trunk, so the rewrite avenue takes an equal LOG share of the authored
    // endgame band midpoint: 16^(1/4) = 2.0. Everything below is arithmetic;
    // this equal-share law is the one seed that wants a ruling (O2).
    //
    // The PARTITION: the major slot inherits the ruled top single step. 1.5 is
    // what O36 already allows at the top of the ladder, the legendary pair
    // already lives under it, and it is 97% spent (the 1.46 the
    // `rewrite-impact` pin tracks) — deriving a different major ceiling would
    // re-price shipped content as a side effect. The three-minor stack gets
    // what is left: 2.0 / 1.5 = 4/3. A full stack of three minors is worth
    // less than one major, which is O65's distinction priced — a minor changes
    // the terms of a rule, a major changes the shape of what happens on
    // screen.
    //
    // What this prices TODAY: the stack ceiling implies (4/3)^(1/3) = 1.101
    // per minor. When O63 reclassifies the four rolled rewrites as Aberrant's
    // minor pool, any of them worth more than ~1.10 on an optimized build
    // must come down or stay major-slot content — that is the breach O96
    // predicted, now a number instead of a surprise.
    //
    // MinorStack deliberately has NO measuring test: no minor classification
    // exists and the equip caps admit one rule today, so a three-minor stack
    // cannot be composed, and a derivation-only test filed under
    // Progression.RuleBandImpact.MinorStack would retire that invariant
    // without measuring a stack — the exact partial-test-under-full-name
    // failure the naming comment on the Step test records. The ceiling
    // precedes the content; the measurement arrives with the content.
    // ---- RE-DERIVED, AND THE FIRST DERIVATION WAS WRONG TWICE ------------
    //
    // IT WAS: layer = EndgameBandMid^(1/4) = 2.0, on the reading that O33's four
    // avenues take equal log shares of the endgame band; major inherited
    // Prolific's 1.5 and the stack took the remainder. Both halves failed.
    //
    // WRONG BASIS. The endgame band is measured on a loadout carrying NO
    // rewrite -- the endgame test asserts exactly that, piece by piece ("A
    // power-band piece carries no rewrite despite being Anomalous"), and then
    // asserts the measured band is untouched by the rarity pass. So 16x is what
    // the OTHER avenues produce with the rewrite layer absent, and taking a
    // quarter-share of it for rewrites shares a band that does not contain the
    // thing being shared. A rewrite multiplies that band rather than living
    // inside it.
    //
    // WHAT THE BAND ACTUALLY AFFORDS is the headroom between where the other
    // avenues land and the top of the authored band: 20 / 16 = 1.25. Derived
    // from the two authored edges, not from an invented log split, and stricter
    // than the measured basis (20 / 15.47 = 1.29) because pinning against a
    // measurement would let the ceiling drift with content.
    //
    // THE CONSEQUENCE IS ALREADY SHIPPED, and the first derivation hid it: at
    // 15.47x measured, one Prolific at 1.4634 composes to 22.6x, outside the
    // authored 12-20 band on its own. O96 predicted the restructure would cause
    // a breach; the breach predates it. Progression.RuleBandImpact.LayerFit is
    // the enumerated red that says so.
    // DERIVED, AND WRITTEN AS A DERIVATION so the report can read it without
    // transcribing it. status.py resolves this form; a literal 1.25 here would
    // be the second copy of a number whose whole value is having one.
    constexpr float RewriteLayerCeilingValue = EndgameBandMaximum / EndgameBandMid;
    inline float RewriteLayerCeiling() { return RewriteLayerCeilingValue; }

    // THE SPLIT IS AUTHORED, NOT DERIVED, AND THAT IS THE POINT.
    //
    // The first version defined the stack AS layer/major and then asserted
    // major x stack == layer, which reduces to x * (k/x) == k and is true for
    // every x. It could not fail. That is the defect this report has now found
    // three times in other people's work and once in its own: a value fixed by
    // construction cannot report a problem.
    //
    // All three numbers are independent now. The layer is derived from the band
    // edges; the major and the per-minor are authored placeholders; and the
    // assertion is that what they compose to FITS, which breaks the moment
    // either is raised. O2 PLACEHOLDER on both magnitudes -- the ratio between
    // a major and three minors is the owner's call, and what is not negotiable
    // is that their product stays under the layer.
    constexpr float MaximumMajorStep = 1.15f;   // O2 PLACEHOLDER
    constexpr float MaximumMinorStep = 1.025f;  // O2 PLACEHOLDER, per minor
    inline float MaximumMinorStackStep() { return FMath::Pow(MaximumMinorStep, 3.0f); }
}

// ---------------------------------------------------------------------------
// EVERY FIXTURE NODE ID RESOLVES TO A NODE THAT EXISTS.
//
// THIS IS THE GUARD PHASE 4 NEEDS AND THE PLAN SCHEDULED FOR PHASE 5, which is
// one phase too late: Phase 4 replaces Core's thirty nodes with an atlas of a
// hundred and sixty-eight, and every id in these fixtures changes with it.
//
// AggregateStats drops an unknown id in SILENCE -- BreakerProgressionComponent
// .cpp:941 is `if (!Found) continue;`. So a renamed Core node does not fail
// here, it contributes nothing, and both fixtures quietly compose down toward
// gear-plus-floor with band drift as the only symptom. Thirty-four Core ids are
// named across the two rank lists; a rename lands them all at once, and the
// bands are already out of band, so the drift would arrive looking like the
// thing everyone is expecting to move anyway.
//
// It asserts the ids specifically rather than a rank count, because a count
// would be satisfied by thirty-four ids that all resolve to nothing.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPowerBandFixtureIdsResolveTest,
    "RiorsEdge.Progression.PowerBand.FixtureIdsResolve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPowerBandFixtureIdsResolveTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    TSet<FName> Known;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        if (!Tree) continue;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (Node) Known.Add(Node->NodeId);
        }
    }
    TestTrue(TEXT("The fallback trees carry nodes to resolve against"), Known.Num() > 100);

    int32 Checked = 0;
    TArray<FString> Missing;
    for (const TCHAR* Label : {TEXT("baseline"), TEXT("optimized")})
    {
        const TArray<FBreakerNodeRank> Ranks =
            FString(Label) == TEXT("baseline") ? BaselineRanks() : OptimizedRanks();
        for (const FBreakerNodeRank& Rank : Ranks)
        {
            ++Checked;
            if (!Known.Contains(Rank.NodeId))
            {
                Missing.Add(FString::Printf(TEXT("%s: %s"), Label, *Rank.NodeId.ToString()));
            }
        }
    }

    TestTrue(TEXT("Both fixtures name ranks at all"), Checked > 30);
    TestEqual(*FString::Printf(TEXT("Every fixture id resolves to a real node (%d checked): %s"),
        Checked, Missing.Num() ? *FString::Join(Missing, TEXT("; ")) : TEXT("all resolve")),
        Missing.Num(), 0);
    return true;
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

    // THE SAME FUNCTION BossOptimized READS. Composed identically from the
    // same fixtures a few lines above -- the pair above stays because the
    // layer-by-layer report needs both halves, but the NUMBER this test emits
    // comes from the one place that number is defined.
    const float Ratio = AtCapBand();
    AddInfo(FString::Printf(TEXT("AT-CAP BAND      flat %.2fx | increased %.2fx | more %.2fx | crit %.2fx => COMPOSED %.2fx (O36 target %.0f-%.0fx)"),
        Optimized.FlatLayer / Baseline.FlatLayer,
        Optimized.IncreasedLayer / Baseline.IncreasedLayer,
        Optimized.MoreLayer / Baseline.MoreLayer,
        Optimized.EffectiveCrit / Baseline.EffectiveCrit,
        Ratio, AtCapBandMinimum, AtCapBandMaximum));
    BreakerStatus::Emit(TEXT("power-band-atcap"), Ratio);

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
    BreakerStatus::Emit(TEXT("power-band-endgame"), Ratio);

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
    //
    // AND THE OTHER HALF OF NAMING, which costs more than the collision did:
    // A TEST COVERING PART OF AN INVARIANT IS NAMED FOR THE PART.
    // `make status` credits an asserted invariant when a test of that NAME
    // exists. Nothing checks that the test's SCOPE matches the invariant's, so
    // a partial test filed under the full name retires the whole invariant and
    // the ceiling falls for free. Twice in two days: UI.Teal.ObjectLaw asserts
    // teal on no interface element ANYWHERE and was nearly claimed by a test of
    // one widget pair (it is now UI.Teal.SealedCluster), and this test claimed
    // "rewrite impact stays under its PER-BAND ceiling" while measuring one
    // band. Two bands are asserted, so two bands are measured below.
    //
    // RENAMED AGAIN — ".Step" — for the FIRST reason: O96's ceilings brought a
    // sibling (RuleBandImpact.Major below, MinorStack to follow when a stack
    // exists), and a test named for the bare prefix swallows its own children
    // in UE's automation tree exactly as PowerBand.RuleImpact once swallowed
    // this one. The name now says what it measures: the STEP of one rollable
    // rewrite on one piece, per band.
    "RiorsEdge.Progression.RuleBandImpact.Step",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRuleBandImpactTest::RunTest(const FString& Parameters)
{
    // Worst step any single rewrite is worth on an optimized build — the one
    // figure `make status` tracks for this section. It stays the ENDGAME
    // figure: `rewrite-impact` is pinned against a ceiling derived there, and
    // emitting a max across two bands would move a pinned number sideways
    // without anyone ruling it. The at-cap band is asserted here and reported
    // in the log; whether it earns its own status row is an open question.
    float WorstRuleStep = 0.0f;
    using namespace BreakerPowerBandTest;

    const FBreakerBuildConditionState State = MeasurementState();

    // The ceilings live in the fixture namespace above, beside the O96 pair
    // they now share a derivation block with; the at-cap variants come from
    // AtCapCeilingFor, the log-space band-share law recorded there.
    const float MaximumRuleStepAtCap = AtCapCeilingFor(MaximumRuleStep);
    const float MaximumProlificRuleStepAtCap = AtCapCeilingFor(MaximumProlificRuleStep);
    const float BandShare = FMath::Loge(AtCapBandMid) / FMath::Loge(EndgameBandMid);

    struct FRuleBandFixture
    {
        const TCHAR* Label;
        int32 ItemLevel;
        float Ceiling;
        float ProlificCeiling;
        bool bEmitStatus;
    };
    const FRuleBandFixture Fixtures[] = {
        { TEXT("ENDGAME"), EndgameItemLevel, MaximumRuleStep, MaximumProlificRuleStep, true },
        { TEXT("AT CAP"), AtCapItemLevel, MaximumRuleStepAtCap, MaximumProlificRuleStepAtCap, false },
    };

    AddInfo(FString::Printf(TEXT("CEILINGS  endgame x%.3f / prolific x%.3f  |  at cap x%.3f / prolific x%.3f (band share %.4f)"),
        MaximumRuleStep, MaximumProlificRuleStep, MaximumRuleStepAtCap, MaximumProlificRuleStepAtCap, BandShare));

    for (const FRuleBandFixture& Fixture : Fixtures)
    {
    const int32 BandItemLevel = Fixture.ItemLevel;
    const FComposedBuild Baseline = Compose(BaselineLoadout(BandItemLevel, BaselineTierFor(BandItemLevel)), BaselineRanks(), State);
    const FComposedBuild Optimized = Compose(OptimizedLoadout(BandItemLevel, OptimizedTierFor(BandItemLevel)), OptimizedRanks(), State);
    const float PlainBand = Optimized.Total / Baseline.Total;

    for (const FBreakerItemRuleDefinition& Definition : UBreakerItemRuleLibrary::GetRuleDefinitions())
    {
        if (!Definition.bRollable) continue;   // legendaries have their own tests

        // The rewrite lands on ONE piece, because the equip cap is one
        // Anomalous. Helmet: it carries damage, crit and a conditional line, so
        // every rollable rewrite has something on it to bite on.
        TArray<FBreakerItemInstance> WithRule = OptimizedLoadout(BandItemLevel, OptimizedTierFor(BandItemLevel));
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
        const FComposedBuild GroundedPlain = Compose(OptimizedLoadout(BandItemLevel, OptimizedTierFor(BandItemLevel)), OptimizedRanks(), Grounded);
        const FComposedBuild GroundedRuled = Compose(WithRule, OptimizedRanks(), Grounded);
        const float GroundedStep = GroundedRuled.Total / GroundedPlain.Total;

        const float StepCeiling = Definition.Rule == EBreakerItemRule::Prolific ? Fixture.ProlificCeiling : Fixture.Ceiling;
        if (Fixture.bEmitStatus)
        {
            WorstRuleStep = FMath::Max(WorstRuleStep, Step);
        }

        AddInfo(FString::Printf(TEXT("[%-7s] RULE %-12s step x%.3f in rotation | x%.3f standing still | band %.2fx (plain %.2fx) | ceiling x%.3f"),
            Fixture.Label, *Definition.DisplayName.ToString(), Step, GroundedStep, RuledBand, PlainBand, StepCeiling));
        TestTrue(*FString::Printf(TEXT("[%s] %s never lowers a grounded build either"),
            Fixture.Label, *Definition.DisplayName.ToString()), GroundedStep >= 1.0f - UE_KINDA_SMALL_NUMBER);

        TestTrue(*FString::Printf(TEXT("[%s] %s never LOWERS an optimized build's damage"),
            Fixture.Label, *Definition.DisplayName.ToString()), Step >= 1.0f - UE_KINDA_SMALL_NUMBER);
        TestTrue(*FString::Printf(TEXT("[%s] %s is worth at most x%.3f on top of an optimized build (measured x%.3f)"),
            Fixture.Label, *Definition.DisplayName.ToString(), StepCeiling, Step), Step <= StepCeiling);
        // ...and it must not be the whole build. A rewrite that outweighs the
        // endgame band would make every other decision a rounding error.
        TestTrue(*FString::Printf(TEXT("[%s] %s is smaller than the band it lives in"),
            Fixture.Label, *Definition.DisplayName.ToString()), Step < PlainBand);
    }
    }

    // The pass's own claim, asserted: an item with no rewrite composes exactly
    // as it did before rules existed. If this ever fails, a rewrite has leaked
    // out of its item and become a property of rarity.
    const FComposedBuild EndgameBaseline = Compose(BaselineLoadout(EndgameItemLevel, BaselineTierFor(EndgameItemLevel)), BaselineRanks(), State);
    const FComposedBuild EndgameOptimized = Compose(OptimizedLoadout(EndgameItemLevel, OptimizedTierFor(EndgameItemLevel)), OptimizedRanks(), State);
    TArray<FBreakerItemInstance> Untouched = OptimizedLoadout(EndgameItemLevel, OptimizedTierFor(EndgameItemLevel));
    for (const FBreakerItemInstance& Item : Untouched)
    {
        TestEqual(TEXT("A power-band piece carries no rewrite despite being Anomalous"),
            static_cast<int32>(Item.Rule), static_cast<int32>(EBreakerItemRule::None));
    }
    TestEqual(TEXT("The measured band is untouched by the rarity pass"),
        Compose(Untouched, OptimizedRanks(), State).Total / EndgameBaseline.Total,
        EndgameOptimized.Total / EndgameBaseline.Total, 0.0001f);
    // ---- DOES THE LAYER FIT THE BAND IT MULTIPLIES? -----------------------
    // The question O96 asks and nothing asked before. Every ceiling above
    // bounds a rewrite against OTHER REWRITES; this bounds the composed result
    // against the band the game actually promises. A rewrite multiplies the
    // endgame band rather than living inside it, so the honest check is the
    // band WITH the worst rewrite on it.
    //
    // EXPECTED RED, and it is red on shipped content rather than on anything
    // the restructure adds: at 15.47x measured, one Prolific at 1.4634 composes
    // to 22.6x against an authored 12-20x. O96 predicted the restructure would
    // guarantee a breach. The breach predates it, and the first derivation of
    // these ceilings hid it by giving the rewrite layer a quarter-share of a
    // band measured without any rewrite in it -- a budget 1.55x larger than the
    // headroom that exists.
    const float BandWithWorstRewrite = (EndgameOptimized.Total / EndgameBaseline.Total) * WorstRuleStep;
    AddInfo(FString::Printf(
        TEXT("ENDGAME BAND with the worst rewrite: %.2fx x %.4f = %.2fx (authored %.0f-%.0fx, layer ceiling %.4f)"),
        EndgameOptimized.Total / EndgameBaseline.Total, WorstRuleStep, BandWithWorstRewrite,
        EndgameBandMinimum, EndgameBandMaximum, RewriteLayerCeiling()));
    TestTrue(*FString::Printf(
        TEXT("the worst rewrite leaves the endgame band inside its authored maximum (%.2fx vs %.0fx)"),
        BandWithWorstRewrite, EndgameBandMaximum),
        BandWithWorstRewrite <= EndgameBandMaximum);
    TestTrue(*FString::Printf(
        TEXT("...and the worst single rewrite fits the rewrite layer (%.4f vs %.4f)"),
        WorstRuleStep, RewriteLayerCeiling()),
        WorstRuleStep <= RewriteLayerCeiling() + 0.0001f);

    BreakerStatus::Emit(TEXT("rewrite-impact"), WorstRuleStep);
    return true;
}

// ---------------------------------------------------------------------------
// O96/O68 — the MAJOR ceiling, asserted against its full current population.
// No rolled major exists (O63's minor/major classification is unbuilt), and
// O68 rules that a legendary's authored pair OCCUPIES the major slot rather
// than sitting beside it — so today the legendaries ARE the majors, and a
// test that waited for rolled majors would leave the ceiling asserted by
// nothing while three legendaries ship against it. The step method is the
// Step test's: the rule lands on the optimized helmet and the measurement is
// what it adds on top of a build that already did everything else right. The
// PAIR's other half — a legendary's generic affixes (O87) — is the power
// band's own subject, not this ceiling's: the ceiling governs the rewrite.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRuleBandImpactMajorTest,
    "RiorsEdge.Progression.RuleBandImpact.Major",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRuleBandImpactMajorTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    // The O96 derivation, asserted where it is consumed: the layer is the
    // O33 four-avenue log share of the authored band, and major times
    // minor-stack spans it exactly. If either equality breaks, somebody
    // edited one constant without re-deriving the pair — which is the
    // authoring-before-deriving failure O96 exists to forbid.
    // THERE IS NO ASSERTION HERE THAT THE LAYER CEILING EQUALS THE BAND
    // HEADROOM, AND THAT IS DELIBERATE. There was one, and it was the same
    // tautology as the partition it replaced: RewriteLayerCeilingValue IS
    // EndgameBandMaximum / EndgameBandMid, so asserting the two are equal is
    // A/B == A/B -- true for every value of both edges, with a comment claiming
    // it would fail if an edge moved. Both sides move together.
    //
    // It also could not catch the case it was written for. Replace the
    // declaration with a literal 1.25f and it still passes, because 1.25 really
    // does equal 20/16 -- and a transcribed constant is only wrong LATER, when
    // an edge moves and the copy does not. No runtime assertion can distinguish
    // a derivation from a correct transcription of its result, because at
    // runtime they are the same float.
    //
    // So the guard for that lives where the two forms ARE distinguishable: in
    // the source. Scripts/status.py's parse_band_edges refuses the report if
    // this constant is not declared as a quotient of two named edges. What is
    // asserted here instead is the thing a runtime check can actually see --
    // that what the layer permits FITS, below.

    // THIS ONE CAN FAIL, WHICH THE VERSION BEFORE IT COULD NOT. It asserted
    // major x stack == layer while the stack was DEFINED as layer / major --
    // x * (k/x) == k, true for every x, a partition asserting itself. Now all
    // three are independent and the claim is that they FIT: raise either
    // authored magnitude and this breaks.
    TestTrue(*FString::Printf(
        TEXT("a major plus a three-minor stack fits the rewrite layer (%.4f x %.4f = %.4f, layer %.4f)"),
        MaximumMajorStep, MaximumMinorStackStep(),
        MaximumMajorStep * MaximumMinorStackStep(), RewriteLayerCeiling()),
        MaximumMajorStep * MaximumMinorStackStep() <= RewriteLayerCeiling() + 0.0001f);

    const FBreakerBuildConditionState State = MeasurementState();

    struct FMajorFixture
    {
        const TCHAR* Label;
        int32 ItemLevel;
        float Ceiling;
    };
    const FMajorFixture Fixtures[] = {
        { TEXT("ENDGAME"), EndgameItemLevel, MaximumMajorStep },
        { TEXT("AT CAP"), AtCapItemLevel, AtCapCeilingFor(MaximumMajorStep) },
    };

    // COVERAGE IS THE WHOLE RULE TABLE, split two ways with no remainder:
    // every rollable definition is the Step test's, every non-rollable one is
    // measured here. A future rule kind cannot fall between the two loops —
    // a new enum entry needs a definition, and a definition is one or the
    // other.
    int32 MajorCount = 0;
    for (const FMajorFixture& Fixture : Fixtures)
    {
        const FComposedBuild Baseline = Compose(BaselineLoadout(Fixture.ItemLevel, BaselineTierFor(Fixture.ItemLevel)), BaselineRanks(), State);
        const FComposedBuild Optimized = Compose(OptimizedLoadout(Fixture.ItemLevel, OptimizedTierFor(Fixture.ItemLevel)), OptimizedRanks(), State);
        const float PlainBand = Optimized.Total / Baseline.Total;

        for (const FBreakerItemRuleDefinition& Definition : UBreakerItemRuleLibrary::GetRuleDefinitions())
        {
            if (Definition.bRollable) continue;   // the Step test's population

            TArray<FBreakerItemInstance> WithRule = OptimizedLoadout(Fixture.ItemLevel, OptimizedTierFor(Fixture.ItemLevel));
            WithRule[0].Rule = Definition.Rule;
            const FComposedBuild Ruled = Compose(WithRule, OptimizedRanks(), State);
            const float Step = Ruled.Total / Optimized.Total;

            // Grounded as well, for the same reason the Step test measures it:
            // the rotation state is the one state a condition-bending rule
            // (Deadfall) is worth nothing in, and a ceiling only ever checked
            // where the subject is inert asserts nothing.
            const FBreakerBuildConditionState Grounded;
            const FComposedBuild GroundedPlain = Compose(OptimizedLoadout(Fixture.ItemLevel, OptimizedTierFor(Fixture.ItemLevel)), OptimizedRanks(), Grounded);
            const FComposedBuild GroundedRuled = Compose(WithRule, OptimizedRanks(), Grounded);
            const float GroundedStep = GroundedRuled.Total / GroundedPlain.Total;

            AddInfo(FString::Printf(TEXT("[%-7s] MAJOR %-10s step x%.3f in rotation | x%.3f standing still | ceiling x%.3f"),
                Fixture.Label, *Definition.DisplayName.ToString(), Step, GroundedStep, Fixture.Ceiling));

            // Today every authored forfeit is non-damage (air control, the
            // slot, regen), so a legendary rule never lowers the damage
            // total. A major whose FORFEIT is damage would legitimately break
            // this pair of floors — re-scope them when one is authored, do
            // not delete the ceiling above.
            TestTrue(*FString::Printf(TEXT("[%s] %s never lowers an optimized build"),
                Fixture.Label, *Definition.DisplayName.ToString()), Step >= 1.0f - UE_KINDA_SMALL_NUMBER);
            TestTrue(*FString::Printf(TEXT("[%s] %s never lowers a grounded build"),
                Fixture.Label, *Definition.DisplayName.ToString()), GroundedStep >= 1.0f - UE_KINDA_SMALL_NUMBER);
            TestTrue(*FString::Printf(TEXT("[%s] %s lands inside the major ceiling x%.3f (measured x%.3f)"),
                Fixture.Label, *Definition.DisplayName.ToString(), Fixture.Ceiling, Step),
                Step <= Fixture.Ceiling);
            TestTrue(*FString::Printf(TEXT("[%s] %s grounded step also lands inside the major ceiling"),
                Fixture.Label, *Definition.DisplayName.ToString()), GroundedStep <= Fixture.Ceiling);
            ++MajorCount;
        }
    }
    // Three legendaries, two fixtures. A fourth legendary joins the loop by
    // existing; a shrink here means a definition vanished from the table.
    TestEqual(TEXT("the major population is every non-rollable rule, both bands"), MajorCount, 6);
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
        // O54's half of the same invariant, and the half that was written in
        // the spec and never checked: every slot can raise weapon damage AND
        // every slot can raise ability damage. Before the pool split the second
        // clause was not merely unchecked, it was unsatisfiable — there was no
        // ability line to roll.
        int32 WeaponLinesOnSlot = 0;
        int32 AbilityLinesOnSlot = 0;
        for (const FBreakerAffixDefinition& Affix : Pool)
        {
            if (!Affix.AllowsSlot(Slot)) continue;
            const EBreakerStatTarget Target = Affix.StatTarget;
            if (Target == EBreakerStatTarget::WeaponDamage || Target == EBreakerStatTarget::SharedDamage) ++WeaponLinesOnSlot;
            if (Target == EBreakerStatTarget::AbilityDamage || Target == EBreakerStatTarget::SharedDamage) ++AbilityLinesOnSlot;
        }
        TestTrue(*(Context + TEXT(" can raise weapon damage")), WeaponLinesOnSlot >= 1);
        TestTrue(*(Context + TEXT(" can raise ability damage")), AbilityLinesOnSlot >= 1);
        // Per-slot identity: gearing is a set of decisions, so every slot has
        // at least one conditional line of its own to chase.
        TestTrue(*(Context + TEXT(" has a conditional line of its own")), ConditionalOnSlot >= 1);
    }
    return true;
}


// ---------------------------------------------------------------------------
// O54: THE ABILITY LANE, MEASURED FOR THE FIRST TIME
// ---------------------------------------------------------------------------
// Ability throughput was measured at roughly 4% of rifle throughput, and the
// ruled fix was giving abilities a pool of their own to scale in. Until the
// three-pool split there was ONE damage bucket, so an ability build could only
// grow by growing weapon damage - every ability build was a weapon build with
// extra steps, and no number anywhere said so.
//
// This is that number. It reports two things and asserts only the one that is
// derivable today:
//
//   PARITY   - what an ability-geared build's ability lane composes to against
//              a weapon-geared build's weapon lane, at the character cap. The
//              spec asserts this "sits within the parity band"; the band itself
//              is UNAUTHORED, so the figure is emitted and left unpinned and
//              the report prints it without judging it. Measuring before
//              pinning is the correct order, and the one O2 asks for.
//
//   RESPONSE - whether the ability lane responds to being built for at all.
//              That IS derivable, and it is what was actually broken: before
//              the split an ability build's composed ability multiplier was
//              identical to a baseline's, because nothing it could equip or
//              purchase reached a lane that did not exist.
// ---------------------------------------------------------------------------

namespace BreakerPowerBandTest
{
    // The optimized loadout's shape with the offensive line swapped for the
    // ability pool, plus the shared line on the three slots that carry it. Same
    // slots, same tier, same number of offensive lines - so this compares two
    // POOLS and not a rich build against a poor one.
    TArray<FBreakerItemInstance> AbilityOptimizedLoadout(int32 ItemLevel, int32 Tier)
    {
        return MakeLoadout({
            {EBreakerEquipSlot::Helmet,     Tier, {TEXT("Offense.AbilityDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage")}},
            {EBreakerEquipSlot::BodyArmour, Tier, {TEXT("Offense.AbilityDamage"), TEXT("Offense.SharedDamage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Gloves,     Tier, {TEXT("Offense.AbilityDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage")}},
            {EBreakerEquipSlot::Boots,      Tier, {TEXT("Offense.AbilityDamage"), TEXT("Move.AirControl"), TEXT("Move.DashCooldown")}},
            {EBreakerEquipSlot::Necklace,   Tier, {TEXT("Offense.AbilityDamage"), TEXT("Offense.SharedDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage")}},
            {EBreakerEquipSlot::Waist,      Tier, {TEXT("Offense.AbilityDamage"), TEXT("Offense.SharedDamage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Primary,    Tier, {TEXT("Offense.AbilityDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Core.MaxResource")}},
            {EBreakerEquipSlot::Secondary,  Tier, {TEXT("Offense.AbilityDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Core.ResourceRegen")}},
        }, ItemLevel);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPowerBandAbilityLaneTest,
    "RiorsEdge.Progression.PowerBand.AbilityLane",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPowerBandAbilityLaneTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    const int32 Tier = OptimizedTierFor(AtCapItemLevel);
    const FBreakerBuildConditionState State = MeasurementState();

    const FComposedBuild WeaponBuild = Compose(OptimizedLoadout(AtCapItemLevel, Tier), OptimizedRanks(), State);
    const FComposedBuild AbilityBuild = Compose(AbilityOptimizedLoadout(AtCapItemLevel, Tier), OptimizedRanks(), State);
    const FComposedBuild Baseline = Compose(BaselineLoadout(AtCapItemLevel, BaselineTierFor(AtCapItemLevel)), BaselineRanks(), State);

    AddInfo(FString::Printf(TEXT("ABILITY LANE  weapon build, weapon lane   (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f => x%.2f"),
        AtCapItemLevel, Tier, WeaponBuild.FlatLayer, WeaponBuild.IncreasedLayer, WeaponBuild.MoreLayer, WeaponBuild.Total));
    AddInfo(FString::Printf(TEXT("ABILITY LANE  ability build, ability lane (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f => x%.2f"),
        AtCapItemLevel, Tier, AbilityBuild.AbilityFlatLayer, AbilityBuild.AbilityIncreasedLayer, AbilityBuild.AbilityMoreLayer, AbilityBuild.AbilityTotal));

    // PARITY AT THE CAP, against O99's ruled band. EXPECTED RED: the figure is
    // 0.647x and the band is 0.85-1.15x, and that gap is a work item with a
    // number on it rather than an open question in a document.
    //
    // THE GAP IS AFFIX BREADTH, AND THE DIAGNOSIS IS IN THE LAYERS ABOVE.
    // The two builds hold an identical More product and identical crit lines,
    // so both cancel exactly and parity is the flat ratio times the increased
    // ratio. BOTH are short, and they are two different pieces of work:
    //
    //   increased 3.35 against 4.49 — the ability pool is one seeded line per
    //     slot at placeholder values, where the weapon pool is that line plus
    //     added damage, fire rate, five conditional lines and the projectile
    //     family. This half closes by authoring ability affix breadth.
    //
    //   flat 1.000 against 1.154 — the ability lane has NO FLAT LINE AT ALL.
    //     Added Damage bids Flat into the weapon lane only, and O54 names three
    //     INCREASED pools and says nothing about the flat half. This is an
    //     unanswered design question rather than unauthored content, and it is
    //     recorded as one.
    //
    // Neither half closes by touching the composition, and a future reader who
    // "fixes" this by folding the weapon pool back into ability hits has
    // deleted the partition rather than closed the gap.
    const float Parity = AbilityBuild.AbilityTotal / WeaponBuild.Total;
    AddInfo(FString::Printf(TEXT("ABILITY LANE  PARITY (cap) %.3fx against O99's %.2f-%.2fx"),
        Parity, AbilityParityBandMinimum, AbilityParityBandMaximum));
    BreakerStatus::Emit(TEXT("power-band-ability"), Parity);
    TestTrue(*FString::Printf(TEXT("PARITY %.3fx is at least %.2fx (O99)"), Parity, AbilityParityBandMinimum),
        Parity >= AbilityParityBandMinimum);
    TestTrue(*FString::Printf(TEXT("PARITY %.3fx is at most %.2fx (O99)"), Parity, AbilityParityBandMaximum),
        Parity <= AbilityParityBandMaximum);
    // The decomposition, because a single ratio does not say what to author.
    // Crit and the More product CANCEL EXACTLY — the two builds hold identical
    // crit lines and an identical More product — so parity is the flat ratio
    // times the increased ratio and nothing else.
    AddInfo(FString::Printf(TEXT("ABILITY LANE  PARITY (cap) decomposes: flat %.3fx x increased %.3fx (crit and More cancel exactly)"),
        AbilityBuild.AbilityFlatLayer / WeaponBuild.FlatLayer,
        AbilityBuild.AbilityIncreasedLayer / WeaponBuild.IncreasedLayer));

    // PARITY AT ENDGAME, measured and reported, deliberately UNPINNED. Whether
    // the cap figure holds at item level 120 is a different question: the
    // endgame band is far more crit-driven and crit is currently a weapon-lane
    // story, so the two are free to diverge — and if they do, THAT is the
    // finding, not a second edge of O99. Asserting it against the cap's band
    // would answer a question nobody has asked yet.
    {
        const int32 EndgameTier = OptimizedTierFor(EndgameItemLevel);
        const FComposedBuild EndgameWeapon = Compose(OptimizedLoadout(EndgameItemLevel, EndgameTier), OptimizedRanks(), State);
        const FComposedBuild EndgameAbility = Compose(AbilityOptimizedLoadout(EndgameItemLevel, EndgameTier), OptimizedRanks(), State);
        const float EndgameParity = EndgameAbility.AbilityTotal / EndgameWeapon.Total;
        AddInfo(FString::Printf(TEXT("ABILITY LANE  weapon build, weapon lane   (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f => x%.2f"),
            EndgameItemLevel, EndgameTier, EndgameWeapon.FlatLayer, EndgameWeapon.IncreasedLayer, EndgameWeapon.MoreLayer, EndgameWeapon.Total));
        AddInfo(FString::Printf(TEXT("ABILITY LANE  ability build, ability lane (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f => x%.2f"),
            EndgameItemLevel, EndgameTier, EndgameAbility.AbilityFlatLayer, EndgameAbility.AbilityIncreasedLayer, EndgameAbility.AbilityMoreLayer, EndgameAbility.AbilityTotal));
        AddInfo(FString::Printf(TEXT("ABILITY LANE  PARITY (endgame) %.3fx — UNPINNED; divergence from the cap figure is its own finding"),
            EndgameParity));
        AddInfo(FString::Printf(TEXT("ABILITY LANE  PARITY (endgame) decomposes: flat %.3fx x increased %.3fx (crit and More cancel exactly)"),
            EndgameAbility.AbilityFlatLayer / EndgameWeapon.FlatLayer,
            EndgameAbility.AbilityIncreasedLayer / EndgameWeapon.IncreasedLayer));
        // BOTH halves widen with gear depth, and the reason is the same in each:
        // the back-loaded ladder multiplies what a line is worth, so a lane with
        // more lines compounds harder as tiers deepen. The breadth deficit is
        // not a constant offset that deep gear dilutes — deep gear WIDENS it.
        // That is why parity has to be measured at two points and not one.
        AddInfo(TEXT("ABILITY LANE  the deficit widens with gear depth: a lane with more lines compounds harder up a back-loaded ladder"));
        BreakerStatus::Emit(TEXT("power-band-ability-endgame"), EndgameParity);
    }

    // RESPONSE - the assertion the pools were built to make possible.
    const float Response = AbilityBuild.AbilityTotal / Baseline.AbilityTotal;
    AddInfo(FString::Printf(TEXT("ABILITY LANE  RESPONSE %.2fx (an ability build's lane against a baseline's)"), Response));
    TestTrue(*FString::Printf(TEXT("The ability lane responds to being built for (%.2fx)"), Response), Response > 2.0f);

    // The shared pool reaches BOTH lanes - the one structural claim of the
    // three-pool model that no single ratio can show. An ability build still
    // carries the shared line and the per-point floor in its weapon lane.
    TestTrue(TEXT("A shared line reaches the weapon lane of an ability build"),
        AbilityBuild.IncreasedLayer > 1.0f);
    TestTrue(TEXT("An ability build's ability lane outgrows its own weapon lane"),
        AbilityBuild.AbilityIncreasedLayer > AbilityBuild.IncreasedLayer);
    // And the mirror: a weapon build's ability lane carries the shared pool and
    // nothing narrow, so it sits below its weapon lane. Neither lane is a copy
    // of the other, which is the whole point of partitioning them.
    TestTrue(TEXT("A weapon build's ability lane sits below its weapon lane"),
        WeaponBuild.AbilityIncreasedLayer < WeaponBuild.IncreasedLayer);

    // O74: ONE More ceiling spanning the pools. Neither lane alone may pass
    // 1.30^3; the strongest-three selection upstream is what stops a build
    // holding three in each, and this is the belt-and-braces reading of it.
    const float Ceiling = FBreakerAttributeAggregator::ComposedMoreCeiling();
    TestTrue(TEXT("The ability lane's More product respects the one ceiling"),
        AbilityBuild.AbilityMoreLayer <= Ceiling + UE_KINDA_SMALL_NUMBER);
    TestTrue(TEXT("The weapon lane's More product respects the one ceiling"),
        AbilityBuild.MoreLayer <= Ceiling + UE_KINDA_SMALL_NUMBER);

    return true;
}

#endif
