#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Combat/BreakerDamageLibrary.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemRules.h"
#include "Items/BreakerLootLibrary.h"
#include "Progression/BreakerBuildConditions.h"

// ---------------------------------------------------------------------------
// WHAT RARITY MEANS, tested as a chain rather than as a card.
// ---------------------------------------------------------------------------
// The bar these tests are written against is the project's own history: it has
// shipped a skill node structurally incapable of raising damage, an affix pool
// where four of eight slots could not raise damage, and a DamageMultiplier
// attribute read by every damage path and written by nobody. So it is not
// enough to assert that a rule EXISTS or that a legendary carries the affixes
// it claims. Every test below asserts that the rewrite moves a number that a
// live consumer reads: the composed attribute, the mitigation formula, the
// regen tick, or the equipped set itself.

namespace BreakerItemRuleTest
{
    // Prefixed with the file's subject: identical anonymous-namespace helper
    // names in two translation units have collided under this project's unity
    // build before.
    FBreakerItemInstance BreakerRuleMakeItem(EBreakerEquipSlot Slot, int32 Tier, const TArray<FName>& AffixIds,
        EBreakerItemRule Rule = EBreakerItemRule::None)
    {
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = TEXT("RuleTest");
        Item.Slot = Slot;
        Item.Rarity = EBreakerItemRarity::Anomalous;
        Item.ItemLevel = 50;
        Item.Rule = Rule;
        for (const FName AffixId : AffixIds)
        {
            const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, AffixId);
            if (!Definition) continue;
            FBreakerRolledAffix Rolled;
            Rolled.AffixId = AffixId;
            Rolled.Tier = Tier;
            Rolled.Category = Definition->Category;
            Rolled.Value = UBreakerAffixLibrary::ValueForTier(*Definition, Tier);
            Item.Affixes.Add(Rolled);
        }
        return Item;
    }

    // The composed Increased-damage percentage this loadout submits, which is
    // the number that actually reaches UBreakerAttributeSet.
    float BreakerRuleIncreasedDamage(const TArray<FBreakerItemInstance>& Items, const FBreakerBuildConditionState& Conditions)
    {
        FBreakerAttributeContribution Offer;
        UBreakerEquipmentComponent::AggregateStats(Items, &Offer, Conditions);
        return Offer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier);
    }

    FBreakerBuildConditionState BreakerRuleState(EBreakerBuildCondition Condition)
    {
        FBreakerBuildConditionState State;
        State.Set(Condition, true);
        return State;
    }
}

// ---------------------------------------------------------------------------
// The constraint that shaped the whole design: a rewrite is NOT a fourth More.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerItemRuleNoMoreTest,
    "RiorsEdge.Items.Rules.NeverAuthorsAMore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerItemRuleNoMoreTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemRuleTest;

    // Every rule in the table, applied alone to a maximal loadout, and NONE of
    // them may put a More multiplier into the equipment contribution. O3 caps a
    // build at three composed Mores and the trees already offer six options
    // against that cap, so an item More would either be eaten by the global
    // clamp or silently displace one the player chose.
    for (const FBreakerItemRuleDefinition& Definition : UBreakerItemRuleLibrary::GetRuleDefinitions())
    {
        TArray<FBreakerItemInstance> Loadout;
        Loadout.Add(BreakerRuleMakeItem(EBreakerEquipSlot::Boots, 1,
            {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Move.AirControl")}, Definition.Rule));
        Loadout.Add(BreakerRuleMakeItem(EBreakerEquipSlot::Primary, 1,
            {TEXT("Weapon.FireRate"), TEXT("Offense.AddedDamage"), TEXT("Core.ResourceRegen")}));

        FBreakerAttributeContribution Offer;
        UBreakerEquipmentComponent::AggregateStats(Loadout, &Offer, FBreakerBuildConditionState::All());
        for (int32 Index = 0; Index < FBreakerAttributeContribution::AttributeCount; ++Index)
        {
            const EBreakerAggregatedAttribute Attribute = static_cast<EBreakerAggregatedAttribute>(Index);
            TestEqual(*FString::Printf(TEXT("%s authors no More on attribute %d"), *Definition.DisplayName.ToString(), Index),
                Offer.GetMore(Attribute), 1.0f, 0.0001f);
        }
    }

    // And the global ceiling exists, so a rewrite that DID author one could not
    // stack on top of the tree's three. Item-Foundation recorded this hole in
    // so many words; this is the assertion that it is closed.
    FBreakerAttributeContribution TreeOffer;
    TreeOffer.ComposeMore(EBreakerAggregatedAttribute::DamageMultiplier, 1.94f);   // three tree keystones
    FBreakerAttributeContribution ItemOffer;
    ItemOffer.ComposeMore(EBreakerAggregatedAttribute::DamageMultiplier, 1.30f);   // a hypothetical fourth

    FBreakerAttributeAggregator Aggregator;
    float Bases[FBreakerAttributeAggregator::AttributeCount] = {};
    Bases[static_cast<int32>(EBreakerAggregatedAttribute::DamageMultiplier)] = 1.0f;
    Aggregator.CaptureBases(Bases);
    Aggregator.SetContribution(EBreakerAttributeContributor::Progression, TreeOffer);
    Aggregator.SetContribution(EBreakerAttributeContributor::Equipment, ItemOffer);

    const float Composed = Aggregator.Compose(EBreakerAggregatedAttribute::DamageMultiplier);
    TestTrue(*FString::Printf(TEXT("A fourth More cannot pass the global O3 ceiling (composed %.3f)"), Composed),
        Composed <= FBreakerAttributeAggregator::ComposedMoreCeiling() + UE_KINDA_SMALL_NUMBER);
    TestTrue(TEXT("The ceiling is 1.30^3, the two numbers O3 and Damage-Pipeline §4 actually state"),
        FMath::IsNearlyEqual(FBreakerAttributeAggregator::ComposedMoreCeiling(), 2.197f, 0.001f));
    // ...and it does not clamp a build that is INSIDE the budget.
    Aggregator.ClearContribution(EBreakerAttributeContributor::Equipment);
    TestTrue(TEXT("Three tree Mores alone are untouched by the ceiling"),
        FMath::IsNearlyEqual(Aggregator.Compose(EBreakerAggregatedAttribute::DamageMultiplier), 1.94f, 0.001f));
    return true;
}

// ---------------------------------------------------------------------------
// UNBOUND — the condition predicate itself is rewritten.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerItemRuleUnboundTest,
    "RiorsEdge.Items.Rules.Unbound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerItemRuleUnboundTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemRuleTest;

    const TArray<FName> Conditionals = {
        TEXT("Offense.AirborneDamage"), TEXT("Offense.RedlineDamage"), TEXT("Offense.WeaponDamage")};

    TArray<FBreakerItemInstance> Plain    = {BreakerRuleMakeItem(EBreakerEquipSlot::Helmet, 1, Conditionals)};
    TArray<FBreakerItemInstance> Unbound  = {BreakerRuleMakeItem(EBreakerEquipSlot::Helmet, 1, Conditionals, EBreakerItemRule::Unbound)};

    const FBreakerBuildConditionState Standing;
    const float PlainStanding   = BreakerRuleIncreasedDamage(Plain, Standing);
    const float UnboundStanding = BreakerRuleIncreasedDamage(Unbound, Standing);
    const float PlainAirborne   = BreakerRuleIncreasedDamage(Plain, FBreakerBuildConditionState::All());

    TestTrue(TEXT("Unbound pays conditional lines while standing still"), UnboundStanding > PlainStanding + 1.0f);
    // The exact property: it is worth precisely what the conditions would have
    // been worth, not more. A rewrite that also inflated the numbers would be a
    // multiplier wearing a rule's clothes.
    TestEqual(TEXT("Unbound is worth exactly the conditions, never more"), UnboundStanding, PlainAirborne, 0.001f);
    TestEqual(TEXT("Unbound changes nothing for a build with every condition already live"),
        BreakerRuleIncreasedDamage(Unbound, FBreakerBuildConditionState::All()), PlainAirborne, 0.001f);

    // It is a wearer-wide rule: the boots' rule frees the helmet's line.
    TArray<FBreakerItemInstance> CrossSlot = {
        BreakerRuleMakeItem(EBreakerEquipSlot::Helmet, 1, {TEXT("Offense.AirborneDamage")}),
        BreakerRuleMakeItem(EBreakerEquipSlot::Waist, 1, {TEXT("Core.Health")}, EBreakerItemRule::Unbound)};
    TestTrue(TEXT("The rewrite applies to the whole equipped set, not to its own item"),
        BreakerRuleIncreasedDamage(CrossSlot, Standing) > 1.0f);
    return true;
}

// ---------------------------------------------------------------------------
// OVERFLOW — a bucket-CROSSING rewrite, not a new bucket.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerItemRuleOverflowTest,
    "RiorsEdge.Items.Rules.Overflow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerItemRuleOverflowTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemRuleTest;

    const TArray<FName> Lines = {TEXT("Offense.AddedDamage"), TEXT("Offense.WeaponDamage")};
    TArray<FBreakerItemInstance> Plain    = {BreakerRuleMakeItem(EBreakerEquipSlot::Necklace, 1, Lines)};
    TArray<FBreakerItemInstance> Overflow = {BreakerRuleMakeItem(EBreakerEquipSlot::Necklace, 1, Lines, EBreakerItemRule::Overflow)};

    FBreakerAttributeContribution PlainOffer;
    const FBreakerEquipmentStats PlainStats = UBreakerEquipmentComponent::AggregateStats(Plain, &PlainOffer);
    FBreakerAttributeContribution OverflowOffer;
    UBreakerEquipmentComponent::AggregateStats(Overflow, &OverflowOffer);

    const float Added = PlainStats.AddedDamagePercent;
    TestTrue(TEXT("The fixture actually carries Added Damage"), Added > 0.0f);
    TestEqual(TEXT("Overflow adds exactly one point of Increased per point of Added"),
        OverflowOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier),
        PlainOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier) + Added, 0.001f);
    // The Flat lane is untouched: the point still counts where it always did.
    // If it moved instead of doubling, this is the assertion that notices.
    TestEqual(TEXT("Overflow does not move Added Damage out of the Flat lane"),
        OverflowOffer.GetFlat(EBreakerAggregatedAttribute::DamageMultiplier),
        PlainOffer.GetFlat(EBreakerAggregatedAttribute::DamageMultiplier), 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// PROLIFIC — tier resolution rewritten, and the non-Forge route to T0/T-1.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerItemRuleProlificTest,
    "RiorsEdge.Items.Rules.Prolific",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerItemRuleProlificTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemRuleTest;

    const TArray<FName> Lines = {TEXT("Offense.WeaponDamage")};
    TArray<FBreakerItemInstance> Plain    = {BreakerRuleMakeItem(EBreakerEquipSlot::Helmet, 1, Lines)};
    TArray<FBreakerItemInstance> Prolific = {BreakerRuleMakeItem(EBreakerEquipSlot::Helmet, 1, Lines, EBreakerItemRule::Prolific)};

    const float PlainPercent    = BreakerRuleIncreasedDamage(Plain, FBreakerBuildConditionState());
    const float ProlificPercent = BreakerRuleIncreasedDamage(Prolific, FBreakerBuildConditionState());

    // T1 -> T0 is the authored spike. This is what makes the two best tiers in
    // the game reachable without the Forge. O29 re-sited the spike from 1.4x to
    // 2.2x, so PROLIFIC got materially stronger without anybody editing it —
    // its whole value IS the size of a tier step, and the steps grew. That
    // consequence is measured in RiorsEdge.Progression.RuleBandImpact and is a
    // reported finding, not a silent retune here.
    TestEqual(TEXT("A T1 line resolves at T0's authored spike"),
        ProlificPercent, PlainPercent * UBreakerAffixLibrary::TierSpikeT0Multiplier, 0.01f);

    // The uplift is per ITEM. A second, ordinary piece must not inherit it —
    // leaking a rewrite onto the rest of the loadout is the failure mode the
    // per-item resolution exists to prevent.
    TArray<FBreakerItemInstance> Mixed = {
        BreakerRuleMakeItem(EBreakerEquipSlot::Helmet, 1, Lines, EBreakerItemRule::Prolific),
        BreakerRuleMakeItem(EBreakerEquipSlot::Gloves, 1, Lines)};
    TestEqual(TEXT("Prolific does not leak onto other equipped pieces"),
        BreakerRuleIncreasedDamage(Mixed, FBreakerBuildConditionState()),
        PlainPercent * (1.0f + UBreakerAffixLibrary::TierSpikeT0Multiplier), 0.02f);

    // Already at the bottom of the curve: nowhere to go, and no runaway.
    TArray<FBreakerItemInstance> AtFloor = {BreakerRuleMakeItem(EBreakerEquipSlot::Helmet, -1, Lines, EBreakerItemRule::Prolific)};
    TArray<FBreakerItemInstance> FloorPlain = {BreakerRuleMakeItem(EBreakerEquipSlot::Helmet, -1, Lines)};
    TestEqual(TEXT("A T-1 line has nowhere to be uplifted to"),
        BreakerRuleIncreasedDamage(AtFloor, FBreakerBuildConditionState()),
        BreakerRuleIncreasedDamage(FloorPlain, FBreakerBuildConditionState()), 0.001f);
    return true;
}

// ---------------------------------------------------------------------------
// RELENTLESS — a CAP rewrite, all the way through to the mitigation formula.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerItemRuleRelentlessTest,
    "RiorsEdge.Items.Rules.Relentless",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerItemRuleRelentlessTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemRuleTest;

    // Enough Physical DR on enough slots to blow well past 60%.
    TArray<FBreakerItemInstance> Plain;
    for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
    {
        Plain.Add(BreakerRuleMakeItem(static_cast<EBreakerEquipSlot>(SlotIndex), -1, {TEXT("Core.PhysicalDR")}));
    }
    TArray<FBreakerItemInstance> Relentless = Plain;
    Relentless[0].Rule = EBreakerItemRule::Relentless;

    const FBreakerEquipmentStats PlainStats = UBreakerEquipmentComponent::AggregateStats(Plain);
    const FBreakerEquipmentStats RuleStats = UBreakerEquipmentComponent::AggregateStats(Relentless);

    TestEqual(TEXT("Without the rule the cap is 60%"), PlainStats.PhysicalDamageReductionPercent, 60.0f, 0.001f);
    TestEqual(TEXT("The cap in force is published"), PlainStats.PhysicalDamageReductionCap, 60.0f, 0.001f);
    TestEqual(TEXT("Relentless raises the cap to 80%"), RuleStats.PhysicalDamageReductionPercent, 80.0f, 0.001f);
    TestEqual(TEXT("The raised cap is published so the UI can state it"), RuleStats.PhysicalDamageReductionCap, 80.0f, 0.001f);

    // And it reaches the mitigation formula, not just the stats struct. This is
    // the same composition UBreakerCombatComponent::ReceiveDamage performs.
    FBreakerDamageRequest Request;
    Request.BaseDamage = 100.0f;
    Request.DamageFamily = EBreakerDamageFamily::Physical;

    auto ResolveWith = [&Request](float ReductionPercent)
    {
        FBreakerDefenseState Defense;
        Defense.Health = 10000.0f;
        Defense.IncomingDamageMultiplier = 1.0f - ReductionPercent / 100.0f;
        return UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    };
    const float PlainDamage = ResolveWith(PlainStats.PhysicalDamageReductionPercent).HealthDamage;
    const float RuleDamage = ResolveWith(RuleStats.PhysicalDamageReductionPercent).HealthDamage;
    TestTrue(*FString::Printf(TEXT("Relentless is felt in the damage formula (%.1f -> %.1f)"), PlainDamage, RuleDamage),
        RuleDamage < PlainDamage - 1.0f);
    return true;
}

// ---------------------------------------------------------------------------
// Rarity gates the rewrite — and legendary rules never appear on a drop.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerItemRarityMeaningTest,
    "RiorsEdge.Items.Rules.RarityGatesRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerItemRarityMeaningTest::RunTest(const FString& Parameters)
{
    int32 AnomalousWithRule = 0;
    int32 AnomalousTotal = 0;
    TSet<EBreakerItemRule> Seen;

    for (int32 Seed = 1; Seed <= 400; ++Seed)
    {
        // Every rarity below Anomalous, on every slot: none may carry a rule.
        for (int32 RarityIndex = 0; RarityIndex < static_cast<int32>(EBreakerItemRarity::Anomalous); ++RarityIndex)
        {
            const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("RarityTest"),
                static_cast<EBreakerEquipSlot>(Seed % static_cast<int32>(EBreakerEquipSlot::Count)),
                static_cast<EBreakerItemRarity>(RarityIndex), 50, Seed * 31 + RarityIndex);
            TestFalse(TEXT("Only Anomalous carries a rule"), Item.HasRule());
        }

        // The necklace has no legendary, so this exercises the generic branch.
        const FBreakerItemInstance Anomalous = UBreakerLootLibrary::RollItem(TEXT("RarityTest"),
            EBreakerEquipSlot::Necklace, EBreakerItemRarity::Anomalous, 50, Seed);
        ++AnomalousTotal;
        if (Anomalous.HasRule()) { ++AnomalousWithRule; Seen.Add(Anomalous.Rule); }
    }

    TestEqual(TEXT("Every Anomalous drop carries a rewrite"), AnomalousWithRule, AnomalousTotal);
    TestEqual(TEXT("Every rollable rewrite is reachable"), Seen.Num(), UBreakerItemRuleLibrary::GetRollableRules().Num());
    for (const EBreakerItemRule Rule : Seen)
    {
        // A legendary's rewrite must never appear on an ordinary drop, or the
        // legendary stops being the reason to hunt for it.
        TestTrue(TEXT("A legendary rule never rolls on an ordinary Anomalous"),
            UBreakerItemRuleLibrary::FindRule(Rule).bRollable);
    }

    // ABERRANT IS FOCUSED: measurably better tiers than an Exceptional at the
    // same item level, without a higher affix COUNT being the reason.
    auto BestTierAcross = [](EBreakerItemRarity Rarity)
    {
        int32 Sum = 0;
        int32 Count = 0;
        for (int32 Seed = 1; Seed <= 300; ++Seed)
        {
            const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Focus"),
                EBreakerEquipSlot::Necklace, Rarity, 30, Seed * 17);
            // Seeded from the ladder floor, not the literal 8. With the O29
            // ladder an ilvl-30 roll starts at T12, so a hardcoded 8 clamped
            // every sample to 8 and made the Aberrant focus measurably invisible
            // — the test would have reported the feature as broken when only its
            // own initialiser was.
            int32 Best = UBreakerAffixLibrary::WorstTier;
            for (const FBreakerRolledAffix& Rolled : Item.Affixes) Best = FMath::Min(Best, Rolled.Tier);
            Sum += Best;
            ++Count;
        }
        return static_cast<float>(Sum) / FMath::Max(1, Count);
    };
    const float ExceptionalBest = BestTierAcross(EBreakerItemRarity::Exceptional);
    const float AberrantBest = BestTierAcross(EBreakerItemRarity::Aberrant);
    // Lower printed tier is better.
    TestTrue(*FString::Printf(TEXT("Aberrant's focused slot is visible in real rolls (best tier %.2f vs %.2f)"),
        AberrantBest, ExceptionalBest), AberrantBest < ExceptionalBest);
    return true;
}

// ---------------------------------------------------------------------------
// THE THREE LEGENDARIES
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLegendarySignatureTest,
    "RiorsEdge.Items.Legendary.Signature",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLegendarySignatureTest::RunTest(const FString& Parameters)
{
    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    const TArray<FBreakerLegendaryDefinition>& Legendaries = UBreakerItemRuleLibrary::GetLegendaries();

    TestEqual(TEXT("Vertical-Slice.md scopes three build-defining legendaries"), Legendaries.Num(), 3);

    TSet<EBreakerEquipSlot> Slots;
    for (const FBreakerLegendaryDefinition& Definition : Legendaries)
    {
        const FString Context = Definition.LegendaryId.ToString();
        TestTrue(*(Context + TEXT(" carries a rule")), Definition.Rule != EBreakerItemRule::None);
        TestFalse(*(Context + TEXT("'s rule is not on the ordinary roll table")),
            UBreakerItemRuleLibrary::FindRule(Definition.Rule).bRollable);
        TestFalse(*(Context + TEXT(" does not share a slot with another legendary")), Slots.Contains(Definition.Slot));
        Slots.Add(Definition.Slot);

        // A signature naming a line that cannot roll on the slot is a line the
        // player would never see. Caught here rather than shipped.
        for (const FName AffixId : Definition.GuaranteedAffixIds)
        {
            const FBreakerAffixDefinition* Affix = UBreakerAffixLibrary::FindAffix(Pool, AffixId);
            TestNotNull(*(Context + TEXT(" signature line exists: ") + AffixId.ToString()), Affix);
            if (Affix)
            {
                TestTrue(*(Context + TEXT(" signature line is legal on its slot: ") + AffixId.ToString()),
                    Affix->AllowsSlot(Definition.Slot));
            }
        }

        const FBreakerItemInstance Rolled = UBreakerLootLibrary::RollLegendary(Definition.LegendaryId, 50, 4242);
        TestTrue(*(Context + TEXT(" rolls a valid item")), Rolled.IsValid());
        TestTrue(*(Context + TEXT(" is legendary")), Rolled.IsLegendary());
        TestEqual(*(Context + TEXT(" is Anomalous, so the equip cap of one applies")),
            static_cast<int32>(Rolled.Rarity), static_cast<int32>(EBreakerItemRarity::Anomalous));
        TestEqual(*(Context + TEXT(" carries its own rule, not a rolled one")),
            static_cast<int32>(Rolled.Rule), static_cast<int32>(Definition.Rule));
        TestEqual(*(Context + TEXT(" lands in its slot")), static_cast<int32>(Rolled.Slot), static_cast<int32>(Definition.Slot));
        for (const FName AffixId : Definition.GuaranteedAffixIds)
        {
            TestTrue(*(Context + TEXT(" guarantees ") + AffixId.ToString()),
                Rolled.Affixes.ContainsByPredicate([AffixId](const FBreakerRolledAffix& R) { return R.AffixId == AffixId; }));
        }

        // Deterministic, like every other roll in the pipeline.
        const FBreakerItemInstance Again = UBreakerLootLibrary::RollLegendary(Definition.LegendaryId, 50, 4242);
        TestEqual(*(Context + TEXT(" reproduces from its seed")), Again.Affixes.Num(), Rolled.Affixes.Num());
        for (int32 Index = 0; Index < Rolled.Affixes.Num(); ++Index)
        {
            TestEqual(*(Context + TEXT(" reproduces affix ids")), Again.Affixes[Index].AffixId, Rolled.Affixes[Index].AffixId);
            TestEqual(*(Context + TEXT(" reproduces affix values")), Again.Affixes[Index].Value, Rolled.Affixes[Index].Value, 0.0001f);
        }
    }

    // A legendary is reachable from the ordinary drop pipeline. Rare, but it
    // has to be possible or the three items only exist in a dev grant.
    bool bDroppedNaturally = false;
    for (int32 Seed = 1; Seed <= 600 && !bDroppedNaturally; ++Seed)
    {
        const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Drop"),
            EBreakerEquipSlot::Boots, EBreakerItemRarity::Anomalous, 50, Seed);
        bDroppedNaturally = Item.IsLegendary();
    }
    TestTrue(TEXT("A legendary can drop from the ordinary Anomalous pipeline"), bDroppedNaturally);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLegendaryDeadfallTest,
    "RiorsEdge.Items.Legendary.Deadfall",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLegendaryDeadfallTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemRuleTest;

    const TArray<FName> Lines = {TEXT("Offense.AirborneDamage"), TEXT("Move.AirControl")};
    TArray<FBreakerItemInstance> Plain = {BreakerRuleMakeItem(EBreakerEquipSlot::Boots, 1, Lines)};
    TArray<FBreakerItemInstance> Deadfall = {BreakerRuleMakeItem(EBreakerEquipSlot::Boots, 1, Lines, EBreakerItemRule::Deadfall)};

    const FBreakerBuildConditionState Sliding = BreakerRuleState(EBreakerBuildCondition::Sliding);
    const FBreakerBuildConditionState WallRiding = BreakerRuleState(EBreakerBuildCondition::WallRiding);
    const FBreakerBuildConditionState Standing;

    // THE REWRITE: the airborne line pays on the ground, while traversing.
    TestEqual(TEXT("Without Deadfall an airborne line pays nothing while sliding"),
        BreakerRuleIncreasedDamage(Plain, Sliding), 0.0f, 0.001f);
    TestTrue(TEXT("Deadfall makes the airborne line pay while sliding"),
        BreakerRuleIncreasedDamage(Deadfall, Sliding) > 1.0f);
    TestTrue(TEXT("Deadfall makes the airborne line pay while wall riding"),
        BreakerRuleIncreasedDamage(Deadfall, WallRiding) > 1.0f);

    // ...and it is a REDIRECTION, not the generic Unbound rewrite. Standing
    // still pays nothing, so the legendary cannot be strictly better than the
    // Anomalous rule that frees every condition.
    TestEqual(TEXT("Deadfall pays nothing while standing still: it is not Unbound"),
        BreakerRuleIncreasedDamage(Deadfall, Standing), 0.0f, 0.001f);
    TestEqual(TEXT("Deadfall does not free unrelated conditional lines"),
        BreakerRuleIncreasedDamage({BreakerRuleMakeItem(EBreakerEquipSlot::Boots, 1,
            {TEXT("Offense.SlidingDamage")}, EBreakerItemRule::Deadfall)}, BreakerRuleState(EBreakerBuildCondition::Airborne)),
        0.0f, 0.001f);

    // THE BILL, in the additive bucket rather than as a sub-1.0 More.
    FBreakerAttributeContribution PlainOffer;
    UBreakerEquipmentComponent::AggregateStats(Plain, &PlainOffer, Standing);
    FBreakerAttributeContribution DeadfallOffer;
    UBreakerEquipmentComponent::AggregateStats(Deadfall, &DeadfallOffer, Standing);
    TestEqual(TEXT("Deadfall costs exactly 40 points of Air Control, additively"),
        DeadfallOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::AirControlMultiplier),
        PlainOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::AirControlMultiplier) - 40.0f, 0.001f);
    TestEqual(TEXT("...and never as a More multiplier"),
        DeadfallOffer.GetMore(EBreakerAggregatedAttribute::AirControlMultiplier), 1.0f, 0.0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLegendaryCadenceTest,
    "RiorsEdge.Items.Legendary.Cadence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLegendaryCadenceTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemRuleTest;

    const TArray<FName> Lines = {TEXT("Weapon.FireRate"), TEXT("Offense.WeaponDamage")};
    TArray<FBreakerItemInstance> Plain   = {BreakerRuleMakeItem(EBreakerEquipSlot::Primary, 1, Lines)};
    TArray<FBreakerItemInstance> Cadence = {BreakerRuleMakeItem(EBreakerEquipSlot::Primary, 1, Lines, EBreakerItemRule::Cadence)};

    FBreakerAttributeContribution PlainOffer;
    UBreakerEquipmentComponent::AggregateStats(Plain, &PlainOffer);
    FBreakerAttributeContribution CadenceOffer;
    UBreakerEquipmentComponent::AggregateStats(Cadence, &CadenceOffer);

    const float FireRate = PlainOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::FireRateMultiplier);
    TestTrue(TEXT("The fixture actually carries Fire Rate"), FireRate > 0.0f);
    TestEqual(TEXT("Cadence crosses half the Fire Rate into the damage bucket"),
        CadenceOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier),
        PlainOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier) + FireRate * 0.5f, 0.001f);
    // Cadence must not also reduce cadence: the line keeps doing its own job.
    TestEqual(TEXT("Fire Rate still reaches the fire-timing attribute in full"),
        CadenceOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::FireRateMultiplier), FireRate, 0.001f);

    // THE BILL: the Secondary slot, ejected in both directions and never
    // refused. Exercised through the real component, because the rule lives in
    // the equip path and a pure check would not prove the slot actually empties.
    AActor* Owner = NewObject<AActor>();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Owner);
    Equipment->BindAttributes(Attributes);

    FBreakerItemInstance Secondary = BreakerRuleMakeItem(EBreakerEquipSlot::Secondary, 1, {TEXT("Offense.WeaponDamage")});
    Secondary.Rarity = EBreakerItemRarity::Exceptional;
    FBreakerItemInstance CadenceItem = UBreakerLootLibrary::RollLegendary(TEXT("Legendary.Cadence"), 50, 77);

    TestTrue(TEXT("A Secondary equips normally"), Equipment->EquipItem(Secondary));
    const FBreakerEquipPreview Preview = Equipment->PreviewEquip(CadenceItem);
    TestTrue(TEXT("The preview discloses the ejected Secondary before the click"), Preview.bRuleDisplaces);
    TestEqual(TEXT("...and names exactly which piece leaves"),
        static_cast<int32>(Preview.RuleDisplaced.Slot), static_cast<int32>(EBreakerEquipSlot::Secondary));

    TestTrue(TEXT("Cadence is never refused, only disclosed"), Equipment->EquipItem(CadenceItem));
    FBreakerItemInstance Found;
    TestFalse(TEXT("The Secondary slot is empty while Cadence is worn"),
        Equipment->GetEquippedItem(EBreakerEquipSlot::Secondary, Found));
    TestTrue(TEXT("Cadence is worn"), Equipment->GetEquippedItem(EBreakerEquipSlot::Primary, Found));

    // The other direction: a Secondary ejects Cadence rather than being refused.
    TestTrue(TEXT("A Secondary can still be equipped"), Equipment->EquipItem(Secondary));
    TestFalse(TEXT("Equipping a Secondary ejects Cadence, symmetrically"),
        Equipment->GetEquippedItem(EBreakerEquipSlot::Primary, Found));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLegendaryOverrunTest,
    "RiorsEdge.Items.Legendary.Overrun",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLegendaryOverrunTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemRuleTest;

    const TArray<FName> Lines = {TEXT("Core.ResourceRegen"), TEXT("Core.MaxResource")};
    TArray<FBreakerItemInstance> Plain   = {BreakerRuleMakeItem(EBreakerEquipSlot::Waist, 1, Lines)};
    TArray<FBreakerItemInstance> Overrun = {BreakerRuleMakeItem(EBreakerEquipSlot::Waist, 1, Lines, EBreakerItemRule::Overrun)};

    const FBreakerBuildConditionState Standing;
    const FBreakerBuildConditionState Sliding = BreakerRuleState(EBreakerBuildCondition::Sliding);

    const float BaseRegen = UBreakerEquipmentComponent::AggregateStats(Plain, nullptr, Standing).ResourceRegenPerSecond;
    TestTrue(TEXT("The fixture actually carries regeneration"), BaseRegen > 0.0f);
    TestEqual(TEXT("Plain regen does not care about the movement state"),
        UBreakerEquipmentComponent::AggregateStats(Plain, nullptr, Sliding).ResourceRegenPerSecond, BaseRegen, 0.0001f);

    TestEqual(TEXT("Overrun pays nothing while standing still"),
        UBreakerEquipmentComponent::AggregateStats(Overrun, nullptr, Standing).ResourceRegenPerSecond, 0.0f, 0.0001f);
    TestEqual(TEXT("Overrun triples regeneration while sliding"),
        UBreakerEquipmentComponent::AggregateStats(Overrun, nullptr, Sliding).ResourceRegenPerSecond, BaseRegen * 3.0f, 0.0001f);
    TestEqual(TEXT("Overrun triples regeneration while airborne"),
        UBreakerEquipmentComponent::AggregateStats(Overrun, nullptr,
            BreakerRuleState(EBreakerBuildCondition::Airborne)).ResourceRegenPerSecond, BaseRegen * 3.0f, 0.0001f);
    // Redline is not traversal: the rule names three states and means them.
    TestEqual(TEXT("Redline alone is not fast traversal"),
        UBreakerEquipmentComponent::AggregateStats(Overrun, nullptr,
            BreakerRuleState(EBreakerBuildCondition::Redline)).ResourceRegenPerSecond, 0.0f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// THE NON-DAMAGE BREADTH PASS — each new line reaches gameplay.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNonDamageBreadthTest,
    "RiorsEdge.Items.Affixes.NonDamageBreadth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNonDamageBreadthTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemRuleTest;

    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    for (const FName AffixId : {TEXT("Core.Armour"), TEXT("Core.LifeOnKill"), TEXT("Core.ResourceOnKill"), TEXT("Offense.DoTDamage")})
    {
        const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, AffixId);
        TestNotNull(*(FString(TEXT("Pool contains ")) + AffixId.ToString()), Definition);
        if (Definition)
        {
            TestTrue(*(AffixId.ToString() + TEXT(" rolls somewhere")), Definition->AllowedSlots.Num() > 0);
            TestTrue(*(AffixId.ToString() + TEXT(" does not author a More")),
                Definition->StatBucket != EBreakerStatBucket::MorePercent);
        }
    }

    // ARMOUR reaches the mitigation formula. The chain asserted here is exactly
    // the one the combat component walks: affix -> contribution -> composed
    // Armor attribute -> FBreakerDefenseState::Armor -> ResolveDamage.
    TArray<FBreakerItemInstance> Armoured = {BreakerRuleMakeItem(EBreakerEquipSlot::BodyArmour, 1, {TEXT("Core.Armour")})};
    FBreakerAttributeContribution Offer;
    const FBreakerEquipmentStats Stats = UBreakerEquipmentComponent::AggregateStats(Armoured, &Offer);
    TestTrue(TEXT("Armour is submitted as flat on the Armor attribute"),
        Offer.GetFlat(EBreakerAggregatedAttribute::Armor) > 0.0f);
    TestEqual(TEXT("The gear-only display figure matches the submission"),
        Stats.BonusArmour, Offer.GetFlat(EBreakerAggregatedAttribute::Armor), 0.0001f);

    FBreakerAttributeAggregator Aggregator;
    float Bases[FBreakerAttributeAggregator::AttributeCount] = {};
    Aggregator.CaptureBases(Bases);
    Aggregator.SetContribution(EBreakerAttributeContributor::Equipment, Offer);
    const float ComposedArmour = Aggregator.Compose(EBreakerAggregatedAttribute::Armor);
    TestTrue(TEXT("The composed Armor attribute carries it"), ComposedArmour > 0.0f);

    FBreakerDamageRequest Request;
    Request.BaseDamage = 100.0f;
    Request.DamageFamily = EBreakerDamageFamily::Physical;
    FBreakerDefenseState Bare;
    Bare.Health = 10000.0f;
    FBreakerDefenseState WithArmour = Bare;
    WithArmour.Armor = ComposedArmour;
    TestTrue(TEXT("Gear armour actually reduces incoming damage"),
        UBreakerDamageLibrary::ResolveDamage(Request, WithArmour).HealthDamage
        < UBreakerDamageLibrary::ResolveDamage(Request, Bare).HealthDamage);

    // DAMAGE OVER TIME lands on the attribute every DoT snapshots.
    TArray<FBreakerItemInstance> Bleeder = {BreakerRuleMakeItem(EBreakerEquipSlot::Helmet, 1, {TEXT("Offense.DoTDamage")})};
    FBreakerAttributeContribution DoTOffer;
    UBreakerEquipmentComponent::AggregateStats(Bleeder, &DoTOffer);
    TestTrue(TEXT("DoT damage reaches DamageOverTimeMultiplier"),
        DoTOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageOverTimeMultiplier) > 0.0f);
    // ...and NOT the weapon-damage bucket. DoTs snapshot separately, and a line
    // that quietly buffed both would be strictly better than Weapon Damage.
    TestEqual(TEXT("DoT damage does not leak into the weapon-damage bucket"),
        DoTOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier), 0.0f, 0.0001f);
    return true;
}

// The one that matters most: the on-kill lines are paid at an EVENT, so the
// only proof they work is driving the event.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerOnKillAffixTest,
    "RiorsEdge.Items.Affixes.OnKillReachesGameplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerOnKillAffixTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemRuleTest;

    AActor* Owner = NewObject<AActor>();
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>(Owner);
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Owner);
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    Combat->BindAttributes(Attributes);
    Equipment->BindAttributes(Attributes);
    Equipment->BindCombatEvents();

    FBreakerItemInstance Gear = BreakerRuleMakeItem(EBreakerEquipSlot::Gloves, 1,
        {TEXT("Core.LifeOnKill"), TEXT("Core.ResourceOnKill")});
    Gear.Rarity = EBreakerItemRarity::Exceptional;
    TestTrue(TEXT("The gear equips"), Equipment->EquipItem(Gear));
    TestTrue(TEXT("The affixes aggregate"), Equipment->GetStats().LifeOnKill > 0.0f && Equipment->GetStats().ResourceOnKill > 0.0f);

    Attributes->ApplyHealth(Attributes->GetMaxHealth() * 0.5f);
    Attributes->ApplyClassResource(0.0f);
    const float HurtHealth = Attributes->GetHealth();

    // Broadcast the real delegate the real combat component raises on a kill.
    FBreakerHitContext Kill;
    Kill.Instigator = Owner;
    Kill.Result.bKilled = true;
    Combat->OnKillDealt.Broadcast(Kill);

    TestTrue(*FString::Printf(TEXT("Health on Kill heals the killer (%.1f -> %.1f)"), HurtHealth, Attributes->GetHealth()),
        Attributes->GetHealth() > HurtHealth);
    TestTrue(TEXT("Resource on Kill pays the class resource"), Attributes->GetClassResource() > 0.0f);

    // Unequipping stops the payment exactly. Nothing incremental is left behind.
    Equipment->UnequipSlot(EBreakerEquipSlot::Gloves);
    const float SettledHealth = Attributes->GetHealth();
    const float SettledResource = Attributes->GetClassResource();
    Attributes->ApplyHealth(SettledHealth - 10.0f);
    Combat->OnKillDealt.Broadcast(Kill);
    TestEqual(TEXT("Unequipping stops the heal"), Attributes->GetHealth(), SettledHealth - 10.0f, 0.001f);
    TestEqual(TEXT("Unequipping stops the resource grant"), Attributes->GetClassResource(), SettledResource, 0.001f);

    // Bound once: a second bind must not double the payment.
    Equipment->BindCombatEvents();
    Equipment->BindCombatEvents();
    TestTrue(TEXT("Re-binding the combat events is idempotent"), Equipment->EquipItem(Gear));
    const float BeforeDouble = Attributes->GetClassResource();
    Combat->OnKillDealt.Broadcast(Kill);
    TestEqual(TEXT("One kill pays exactly once"),
        Attributes->GetClassResource() - BeforeDouble, Equipment->GetStats().ResourceOnKill, 0.01f);
    return true;
}

#endif
