#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerForgeLibrary.h"
#include "Items/BreakerLootLibrary.h"
#include "Save/BreakerAccountSave.h"
#include "Weapons/BreakerWeaponMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAffixTierCurveTest,
    "RiorsEdge.Items.Affixes.TierCurve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAffixTierCurveTest::RunTest(const FString& Parameters)
{
    using ALib = UBreakerAffixLibrary;

    // Core.Health's authored anchors, so the numbers in the table on
    // ValueForTier and the numbers here are the same numbers.
    FBreakerAffixDefinition Affix;
    Affix.ValueAtT12 = 25.0f;
    Affix.ValueAtT1 = 400.0f;

    TestEqual(TEXT("T12 returns the floor value exactly"), ALib::ValueForTier(Affix, 12), 25.0f);
    TestEqual(TEXT("T1 returns the ceiling value exactly"), ALib::ValueForTier(Affix, 1), 400.0f);
    TestEqual(TEXT("T0 spikes to 2.2x T1"), ALib::ValueForTier(Affix, 0), 880.0f, 0.01f);
    TestEqual(TEXT("T-1 spikes to 3.6x T1"), ALib::ValueForTier(Affix, -1), 1440.0f, 0.01f);

    // MONOTONIC. Nothing else in the file is meaningful if a better tier can
    // ever be worth less, and a curve with an exponent is exactly the shape
    // where a sign slip produces that silently.
    for (int32 Tier = 12; Tier > ALib::TopTier; --Tier)
    {
        const float Worse = ALib::ValueForTier(Affix, Tier);
        const int32 Better = (Tier == 1) ? 0 : Tier - 1;
        TestTrue(*FString::Printf(TEXT("T%d is worth strictly more than T%d"), Better, Tier),
            ALib::ValueForTier(Affix, Better) > Worse);
    }

    // BACK-LOADED, in both senses (O29: "a materially bigger jump between the
    // high tiers"). Asserting the PROPERTY rather than the eleven values,
    // because the exponent is an O2 placeholder and the shape is the ruling.
    float PreviousStep = 0.0f;
    float PreviousRatio = 0.0f;
    for (int32 Tier = 11; Tier >= 1; --Tier)
    {
        const float Value = ALib::ValueForTier(Affix, Tier);
        const float Below = ALib::ValueForTier(Affix, Tier + 1);
        const float Step = Value - Below;
        const float Ratio = Value / Below;
        TestTrue(*FString::Printf(TEXT("T%d's absolute step exceeds the one below it"), Tier), Step > PreviousStep);
        TestTrue(*FString::Printf(TEXT("T%d's relative step exceeds the one below it"), Tier), Ratio > PreviousRatio);
        PreviousStep = Step;
        PreviousRatio = Ratio;
    }

    // The two headline figures from the derivation, so a retune of the exponent
    // has to come past a number a human wrote down.
    const float BottomStep = ALib::ValueForTier(Affix, 11) / ALib::ValueForTier(Affix, 12);
    const float TopStep = ALib::ValueForTier(Affix, 1) / ALib::ValueForTier(Affix, 2);
    TestEqual(TEXT("Bottom step is +14.8%"), BottomStep, 1.148f, 0.005f);
    TestEqual(TEXT("Top step is +36.5%"), TopStep, 1.365f, 0.005f);
    AddInfo(FString::Printf(TEXT("Tier ladder: bottom step x%.3f, top step x%.3f (%.2fx), T1/T12 = %.1fx"),
        BottomStep, TopStep, (TopStep - 1.0f) / (BottomStep - 1.0f), Affix.ValueAtT1 / Affix.ValueAtT12));

    // T-1 IS THE TOP, and the spike is still a spike. Re-sited from 1.4x/1.8x
    // precisely because against a +36.5% top step those would have been about
    // one ordinary step and the spike would have stopped being one (O29).
    TestTrue(TEXT("T-1 is the highest value on the ladder"),
        ALib::ValueForTier(Affix, ALib::TopTier) > ALib::ValueForTier(Affix, 0));
    TestTrue(TEXT("The T0 spike is worth more than two ordinary top steps"),
        ALib::ValueForTier(Affix, 0) > ALib::ValueForTier(Affix, 1) * TopStep * TopStep);

    // Tiers outside the ladder clamp rather than extrapolate: the equipment
    // uplift path asks for Tier - Uplift and must not be able to walk off the
    // end into a value with no entry on the curve.
    TestEqual(TEXT("Below T-1 clamps to T-1"), ALib::ValueForTier(Affix, -5), ALib::ValueForTier(Affix, -1));
    TestEqual(TEXT("Above T12 clamps to T12"), ALib::ValueForTier(Affix, 99), ALib::ValueForTier(Affix, 12));

    // The degenerate-band fallback. An affix authored with a zero floor cannot
    // take the geometric branch; it must still be monotonic rather than zero or
    // NaN, because a NaN here reaches the damage pipeline.
    FBreakerAffixDefinition ZeroFloor;
    ZeroFloor.ValueAtT12 = 0.0f;
    ZeroFloor.ValueAtT1 = 10.0f;
    TestEqual(TEXT("A zero-floor affix is still exactly 0 at T12"), ALib::ValueForTier(ZeroFloor, 12), 0.0f);
    TestEqual(TEXT("A zero-floor affix still reaches its ceiling at T1"), ALib::ValueForTier(ZeroFloor, 1), 10.0f);
    TestTrue(TEXT("A zero-floor affix is still monotonic"),
        ALib::ValueForTier(ZeroFloor, 5) > ALib::ValueForTier(ZeroFloor, 8));
    return true;
}

// ---------------------------------------------------------------------------
// THE LADDER'S BOUNDARIES (O29)
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTierLadderTest,
    "RiorsEdge.Items.TierLadder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTierLadderTest::RunTest(const FString& Parameters)
{
    using ALib = UBreakerAffixLibrary;

    // THE SEAM. BreakerWeaponMath is deliberately dependency-free, so its item
    // level ceiling is a second copy of this number rather than an include.
    // A weapon clamping below the item system would cap base damage while the
    // affixes on the same item kept climbing — the same class of split as the
    // 74x endgame gap O29 exists to close.
    TestEqual(TEXT("The weapon damage curve supports the full O29 item level range"),
        FBreakerWeaponMath::MaxSupportedItemLevel, ALib::MaxItemLevel);
    TestEqual(TEXT("Item level runs to 120 (O29)"), ALib::MaxItemLevel, 120);
    TestEqual(TEXT("The ladder is 12 tiers deep (O29)"), ALib::WorstTier, 12);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerItemLevelGatingTest,
    "RiorsEdge.Items.Affixes.ItemLevelGating",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerItemLevelGatingTest::RunTest(const FString& Parameters)
{
    using ALib = UBreakerAffixLibrary;

    // TWO SLOPES (owner ruling): the levelling game crosses HALF the ladder,
    // the endgame crosses the other half more slowly because each of its tiers
    // is worth more. A single slope put the character cap at T8 -- a third of
    // the ladder -- and the owner reported that as awkward progression.
    TestEqual(TEXT("Level 1 rolls only T12"), ALib::BestTierForItemLevel(1), 12);
    // THE ANCHOR THE RULING IS ABOUT: half the ladder by the character cap.
    TestEqual(TEXT("Level 50, the character cap, stands on T6"),
        ALib::BestTierForItemLevel(50), ALib::TierAtCharacterCap);
    TestEqual(TEXT("Level 120 is T1"), ALib::BestTierForItemLevel(120), 1);
    // The levelling slope is steeper than the endgame slope, which is the
    // whole point of having two of them.
    const int32 LevellingSteps = ALib::WorstTier - ALib::BestTierForItemLevel(ALib::CharacterCapItemLevel);
    const int32 EndgameSteps = ALib::BestTierForItemLevel(ALib::CharacterCapItemLevel) - ALib::BestNormalTier;
    const float LevellingPace = static_cast<float>(ALib::CharacterCapItemLevel) / FMath::Max(1, LevellingSteps);
    const float EndgamePace = static_cast<float>(ALib::MaxItemLevel - ALib::CharacterCapItemLevel) / FMath::Max(1, EndgameSteps);
    AddInfo(FString::Printf(TEXT("Levelling %.1f ilvl/tier, endgame %.1f ilvl/tier"), LevellingPace, EndgamePace));
    TestTrue(TEXT("Tiers arrive faster while levelling than in the endgame"), LevellingPace < EndgamePace);

    // The full range, walked. Monotonic (a higher item level never rolls a
    // worse ceiling), never off the ends of the ladder, and never reaching the
    // spike tiers from item level alone — T0/T-1 are crafted or carried by a
    // rule, which is what keeps the Forge a destination.
    int32 Previous = ALib::WorstTier;
    for (int32 Level = 1; Level <= ALib::MaxItemLevel; ++Level)
    {
        const int32 Best = ALib::BestTierForItemLevel(Level);
        TestTrue(*FString::Printf(TEXT("ilvl %d: tier ceiling never worsens"), Level), Best <= Previous);
        TestTrue(*FString::Printf(TEXT("ilvl %d: tier ceiling is inside T12..T1"), Level),
            Best >= ALib::BestNormalTier && Best <= ALib::WorstTier);
        Previous = Best;
    }
    // Out of range in both directions clamps rather than extrapolating.
    TestEqual(TEXT("Below 1 clamps to the worst tier"), ALib::BestTierForItemLevel(-40), 12);
    TestEqual(TEXT("Past 120 clamps to the best normal tier"), ALib::BestTierForItemLevel(9999), 1);
    // Every one of the twelve tiers is actually reachable from some item level.
    // A mapping that skipped one would author a tier nobody can ever roll.
    for (int32 Tier = 1; Tier <= ALib::WorstTier; ++Tier)
    {
        bool bReachable = false;
        for (int32 Level = 1; Level <= ALib::MaxItemLevel && !bReachable; ++Level)
        {
            bReachable = ALib::BestTierForItemLevel(Level) == Tier;
        }
        TestTrue(*FString::Printf(TEXT("T%d is reachable from some item level"), Tier), bReachable);
    }

    // RARITY CAPS, re-derived against 11 steps rather than 7. The invariant
    // that survives the widening is the two-tier gap between Standard and
    // Uncommon; the numbers themselves both move up one.
    TestEqual(TEXT("Standard rarity caps at T4"), ALib::TierCapForRarity(EBreakerItemRarity::Standard), 4);
    TestEqual(TEXT("Uncommon rarity caps at T2"), ALib::TierCapForRarity(EBreakerItemRarity::Uncommon), 2);
    TestEqual(TEXT("The Standard->Uncommon gap is still two tiers"),
        ALib::TierCapForRarity(EBreakerItemRarity::Standard) - ALib::TierCapForRarity(EBreakerItemRarity::Uncommon), 2);
    TestEqual(TEXT("Exceptional rarity ceiling is T-1"), ALib::TierCapForRarity(EBreakerItemRarity::Exceptional), -1);
    TestEqual(TEXT("Aberrant rarity ceiling is T-1"), ALib::TierCapForRarity(EBreakerItemRarity::Aberrant), -1);
    TestEqual(TEXT("Anomalous rarity ceiling is T-1"), ALib::TierCapForRarity(EBreakerItemRarity::Anomalous), -1);

    // The caps must remain ORDERED, and no cap may sit outside the ladder — a
    // cap worse than the worst tier would make a rarity unrollable.
    TestTrue(TEXT("Rarity ceilings improve monotonically"),
        ALib::TierCapForRarity(EBreakerItemRarity::Standard) > ALib::TierCapForRarity(EBreakerItemRarity::Uncommon)
        && ALib::TierCapForRarity(EBreakerItemRarity::Uncommon) > ALib::TierCapForRarity(EBreakerItemRarity::Exceptional));
    TestTrue(TEXT("No rarity cap sits outside the ladder"),
        ALib::TierCapForRarity(EBreakerItemRarity::Standard) <= ALib::WorstTier
        && ALib::TierCapForRarity(EBreakerItemRarity::Anomalous) >= ALib::TopTier);

    // A Standard drop at the item-level ceiling is capped by its RARITY, not by
    // its item level — which is the only thing that makes a rarity cap mean
    // anything now that item level reaches T1 on its own.
    const int32 StandardAt120 = FMath::Max(ALib::BestTierForItemLevel(120), ALib::TierCapForRarity(EBreakerItemRarity::Standard));
    TestEqual(TEXT("An ilvl-120 Standard is held at T4 by rarity"), StandardAt120, 4);
    const int32 ExceptionalAt120 = FMath::Max(ALib::BestTierForItemLevel(120), ALib::TierCapForRarity(EBreakerItemRarity::Exceptional));
    TestEqual(TEXT("An ilvl-120 Exceptional reaches T1 from item level"), ExceptionalAt120, 1);
    return true;
}

// ---------------------------------------------------------------------------
// EXISTING SAVES STILL LOAD (O29's implementation note)
// ---------------------------------------------------------------------------
// A rolled affix stores Tier and Value per item, so widening the ladder cannot
// invalidate one. The ruling adds that items rolled before O29 keep the values
// they rolled and "will read as weak. That is correct and should not be
// migrated." This test VERIFIES that rather than assuming it — the failure mode
// it guards is a well-meaning future pass re-deriving Value from Tier on load,
// which would silently rewrite every item anybody owns.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLegacyItemTest,
    "RiorsEdge.Items.LegacyItemsSurviveTheWiderLadder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLegacyItemTest::RunTest(const FString& Parameters)
{
    using ALib = UBreakerAffixLibrary;
    const TArray<FBreakerAffixDefinition>& Pool = ALib::GetSliceAffixPool();

    // An item as the PRE-O29 pipeline would have written it: item level 50 (the
    // old ceiling), tier 1 (the old best), and the value the old linear curve
    // produced for Core.Health at T1 — 180, which is now what the new curve
    // pays at about T4.
    FBreakerItemInstance Legacy;
    Legacy.ItemId = FGuid::NewGuid();
    Legacy.DefinitionId = TEXT("LegacySave");
    Legacy.Slot = EBreakerEquipSlot::Helmet;
    Legacy.Rarity = EBreakerItemRarity::Exceptional;
    Legacy.ItemLevel = 50;
    FBreakerRolledAffix Rolled;
    Rolled.AffixId = TEXT("Core.Health");
    Rolled.Tier = 1;
    Rolled.Value = 180.0f;
    Rolled.Category = EBreakerAffixCategory::Suffix;
    Legacy.Affixes.Add(Rolled);

    TestTrue(TEXT("A pre-O29 item is still a valid item"), Legacy.IsValid());

    // The aggregation path reads the STORED value, never re-derives it.
    const FBreakerBuildConditionState Standing;
    FBreakerAttributeContribution Offer;
    const FBreakerEquipmentStats Stats = UBreakerEquipmentComponent::AggregateStats({Legacy}, &Offer, Standing);
    TestEqual(TEXT("A pre-O29 item contributes exactly the value it rolled"),
        Offer.GetFlat(EBreakerAggregatedAttribute::MaxHealth), 180.0f, 0.001f);
    TestEqual(TEXT("...and the display stat agrees"), Stats.BonusHealth, 180.0f, 0.001f);

    // It reads as WEAK against what the same tier is worth now, and that is the
    // ruled outcome rather than a bug: it is a T1 roll from a shallower ladder.
    const FBreakerAffixDefinition* Definition = ALib::FindAffix(Pool, TEXT("Core.Health"));
    TestNotNull(TEXT("Core.Health is still in the pool"), Definition);
    if (Definition)
    {
        TestTrue(TEXT("A pre-O29 T1 roll is worth less than a post-O29 T1 roll"),
            Rolled.Value < ALib::ValueForTier(*Definition, 1));
        AddInfo(FString::Printf(TEXT("Legacy T1 Core.Health = %.1f; post-O29 T1 = %.1f (%.2fx), roughly today's T%d"),
            Rolled.Value, ALib::ValueForTier(*Definition, 1),
            ALib::ValueForTier(*Definition, 1) / Rolled.Value, 4));
    }

    // Every legal pre-O29 tier — 8 down to -1 — is still inside the new
    // ladder's domain and still produces a finite, ordered value, so nothing
    // stored in an old save can index off the end of the curve.
    if (Definition)
    {
        for (int32 Tier = 8; Tier >= -1; --Tier)
        {
            const float Value = ALib::ValueForTier(*Definition, Tier);
            TestTrue(*FString::Printf(TEXT("Legacy tier %d still evaluates finite and positive"), Tier),
                FMath::IsFinite(Value) && Value > 0.0f);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLootRollTest,
    "RiorsEdge.Items.Loot.RollPipeline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLootRollTest::RunTest(const FString& Parameters)
{
    for (int32 Seed = 0; Seed < 200; ++Seed)
    {
        const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Test"), EBreakerEquipSlot::Boots, EBreakerItemRarity::Exceptional, 30, Seed);
        TestTrue(TEXT("Item is valid"), Item.IsValid());
        TestTrue(TEXT("Affix count within Exceptional range"), Item.Affixes.Num() >= 3 && Item.Affixes.Num() <= 5);
        TestTrue(TEXT("Prefix cap holds"), UBreakerLootLibrary::CountAffixesOfCategory(Item, EBreakerAffixCategory::Prefix) <= 4);
        TestTrue(TEXT("Suffix cap holds"), UBreakerLootLibrary::CountAffixesOfCategory(Item, EBreakerAffixCategory::Suffix) <= 4);

        const int32 BestTier = UBreakerAffixLibrary::BestTierForItemLevel(30);
        TSet<FName> Seen;
        for (const FBreakerRolledAffix& Affix : Item.Affixes)
        {
            TestFalse(TEXT("No duplicate affixes"), Seen.Contains(Affix.AffixId));
            Seen.Add(Affix.AffixId);
            TestTrue(TEXT("Tier respects item level gate"), Affix.Tier >= BestTier);
            const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), Affix.AffixId);
            TestNotNull(TEXT("Rolled affix exists in pool"), Definition);
            if (Definition) TestTrue(TEXT("Affix legal for the slot"), Definition->AllowsSlot(EBreakerEquipSlot::Boots));
        }
    }

    // Weapon Damage is a UNIVERSAL affix as of O27. It used to be restricted to
    // gloves/neck/weapons, which is precisely what made helmet, body armour,
    // boots and waist structurally incapable of raising damage (Power-Curve
    // §"More options in every avenue"). The assertion is inverted deliberately:
    // the old expectation encoded the bug.
    const FBreakerAffixDefinition* WeaponDamage = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), TEXT("Offense.WeaponDamage"));
    TestNotNull(TEXT("Weapon Damage affix exists in the slice pool"), WeaponDamage);
    if (WeaponDamage)
    {
        for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
        {
            TestTrue(TEXT("Weapon Damage rolls on every slot"), WeaponDamage->AllowsSlot(static_cast<EBreakerEquipSlot>(SlotIndex)));
        }
        // Per-slot identity still holds for the CONDITIONAL lines: Damage while
        // Airborne is a boots/helmet/neck/weapon line and must never be a body
        // armour or gloves roll, or every slot would offer the same thing.
        const FBreakerAffixDefinition* Airborne = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), TEXT("Offense.AirborneDamage"));
        TestNotNull(TEXT("The airborne conditional exists"), Airborne);
        if (Airborne)
        {
            TestTrue(TEXT("Airborne damage is a boots line"), Airborne->AllowsSlot(EBreakerEquipSlot::Boots));
            TestFalse(TEXT("Airborne damage is not a body armour line"), Airborne->AllowsSlot(EBreakerEquipSlot::BodyArmour));
        }

        bool bRolledOnPrimary = false;
        for (int32 Seed = 0; Seed < 200; ++Seed)
        {
            const FBreakerItemInstance Weapon = UBreakerLootLibrary::RollItem(TEXT("Test"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Exceptional, 30, Seed);
            for (const FBreakerRolledAffix& Affix : Weapon.Affixes)
            {
                if (Affix.AffixId == WeaponDamage->AffixId) bRolledOnPrimary = true;
            }
        }
        TestTrue(TEXT("Weapon Damage can roll on an allowed slot"), bRolledOnPrimary);
    }

    // Standard items must stay fodder: 1-2 affixes, tiers no better than T3.
    const FBreakerItemInstance White = UBreakerLootLibrary::RollItem(TEXT("Test"), EBreakerEquipSlot::Helmet, EBreakerItemRarity::Standard, 50, 7);
    TestTrue(TEXT("Standard affix count"), White.Affixes.Num() >= 1 && White.Affixes.Num() <= 2);
    for (const FBreakerRolledAffix& Affix : White.Affixes)
    {
        TestTrue(TEXT("Standard tier cap"), Affix.Tier >= 3);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEquipmentAggregationTest,
    "RiorsEdge.Items.Equipment.StatAggregation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEquipmentAggregationTest::RunTest(const FString& Parameters)
{
    auto MakeRolled = [](FName AffixId, float Value, EBreakerAffixCategory Category)
    {
        FBreakerRolledAffix Rolled;
        Rolled.AffixId = AffixId;
        Rolled.Tier = 4;
        Rolled.Value = Value;
        Rolled.Category = Category;
        return Rolled;
    };

    FBreakerItemInstance Boots;
    Boots.ItemId = FGuid::NewGuid();
    Boots.Slot = EBreakerEquipSlot::Boots;
    Boots.Affixes.Add(MakeRolled(TEXT("Core.Health"), 100.0f, EBreakerAffixCategory::Suffix));
    Boots.Affixes.Add(MakeRolled(TEXT("Core.MoveSpeed"), 5.0f, EBreakerAffixCategory::Prefix));

    FBreakerItemInstance Helmet;
    Helmet.ItemId = FGuid::NewGuid();
    Helmet.Slot = EBreakerEquipSlot::Helmet;
    Helmet.Affixes.Add(MakeRolled(TEXT("Core.Health"), 50.0f, EBreakerAffixCategory::Suffix));
    Helmet.Affixes.Add(MakeRolled(TEXT("Core.MoveSpeed"), 3.0f, EBreakerAffixCategory::Prefix));
    Helmet.Affixes.Add(MakeRolled(TEXT("Crit.Chance"), 6.0f, EBreakerAffixCategory::Prefix));

    // Two sources of increased Weapon Damage must land in one additive bucket.
    FBreakerItemInstance Gloves;
    Gloves.ItemId = FGuid::NewGuid();
    Gloves.Slot = EBreakerEquipSlot::Gloves;
    Gloves.Affixes.Add(MakeRolled(TEXT("Offense.WeaponDamage"), 12.0f, EBreakerAffixCategory::Prefix));

    FBreakerItemInstance Necklace;
    Necklace.ItemId = FGuid::NewGuid();
    Necklace.Slot = EBreakerEquipSlot::Necklace;
    Necklace.Affixes.Add(MakeRolled(TEXT("Offense.WeaponDamage"), 8.0f, EBreakerAffixCategory::Prefix));

    const FBreakerEquipmentStats Stats = UBreakerEquipmentComponent::AggregateStats({Boots, Helmet, Gloves, Necklace});
    TestEqual(TEXT("Flat health sums"), Stats.BonusHealth, 150.0f);
    TestTrue(TEXT("Increased move speed is one additive bucket"), FMath::IsNearlyEqual(Stats.MoveSpeedMultiplier, 1.08f, 0.0001f));
    TestTrue(TEXT("Crit chance converts percent to fraction"), FMath::IsNearlyEqual(Stats.CriticalChanceBonus, 0.06f, 0.0001f));
    TestTrue(TEXT("Increased weapon damage is one additive bucket"), FMath::IsNearlyEqual(Stats.WeaponDamageMultiplier, 1.20f, 0.0001f));

    // No weapon damage equipped leaves the multiplier neutral.
    const FBreakerEquipmentStats BareStats = UBreakerEquipmentComponent::AggregateStats({Boots});
    TestTrue(TEXT("Weapon damage multiplier defaults to 1"), FMath::IsNearlyEqual(BareStats.WeaponDamageMultiplier, 1.0f, 0.0001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEquipmentContributionTest,
    "RiorsEdge.Items.Equipment.AttributeContribution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEquipmentContributionTest::RunTest(const FString& Parameters)
{
    FBreakerItemInstance Gloves;
    Gloves.ItemId = FGuid::NewGuid();
    Gloves.Slot = EBreakerEquipSlot::Gloves;

    FBreakerRolledAffix Health;
    Health.AffixId = TEXT("Core.Health");
    Health.Tier = 4;
    Health.Value = 120.0f;
    Health.Category = EBreakerAffixCategory::Suffix;
    Gloves.Affixes.Add(Health);

    FBreakerRolledAffix Speed;
    Speed.AffixId = TEXT("Core.MoveSpeed");
    Speed.Tier = 4;
    Speed.Value = 6.0f;
    Speed.Category = EBreakerAffixCategory::Prefix;
    Gloves.Affixes.Add(Speed);

    // The contribution carries RAW buckets, not composed multipliers: the
    // Increased percentage has to reach the attribute set unmerged so it can
    // join the tree's percentages in one additive bucket.
    FBreakerAttributeContribution Contribution;
    UBreakerEquipmentComponent::AggregateStats({Gloves}, &Contribution);
    TestEqual(TEXT("Flat health lands in the flat lane"), Contribution.GetFlat(EBreakerAggregatedAttribute::MaxHealth), 120.0f);
    TestEqual(TEXT("Increased move speed stays a raw percentage"), Contribution.GetIncreasedPercent(EBreakerAggregatedAttribute::MoveSpeed), 6.0f);
    TestEqual(TEXT("Gear authors no More multipliers"), Contribution.GetMore(EBreakerAggregatedAttribute::MoveSpeed), 1.0f);

    // End to end through the single application path.
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
    Equipment->BindAttributes(Attributes);
    const float BaseHealth = Attributes->GetMaxHealth();
    const float BaseSpeed = Attributes->GetMoveSpeed();

    TestTrue(TEXT("Equip succeeds"), Equipment->EquipItem(Gloves));
    TestEqual(TEXT("Equipping raises max health"), Attributes->GetMaxHealth(), BaseHealth + 120.0f);
    TestEqual(TEXT("Equipping raises move speed"), Attributes->GetMoveSpeed(), BaseSpeed * 1.06f, 0.001f);

    TestTrue(TEXT("Unequip succeeds"), Equipment->UnequipSlot(EBreakerEquipSlot::Gloves));
    TestEqual(TEXT("Unequipping restores the pre-equip health exactly"), Attributes->GetMaxHealth(), BaseHealth);
    TestEqual(TEXT("Unequipping restores the pre-equip move speed exactly"), Attributes->GetMoveSpeed(), BaseSpeed);
    TestEqual(TEXT("The base value is never overwritten"), Attributes->GetAttributeBase(EBreakerAggregatedAttribute::MaxHealth), BaseHealth);
    return true;
}

namespace BreakerEquipmentTestHelpers
{
    // A minimal item that only needs to be identifiable and rarity-tagged.
    FBreakerItemInstance MakeItem(EBreakerEquipSlot Slot, EBreakerItemRarity Rarity, int32 ItemLevel)
    {
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.Slot = Slot;
        Item.Rarity = Rarity;
        Item.ItemLevel = ItemLevel;
        return Item;
    }

    FBreakerRolledAffix MakeAffix(FName AffixId, float Value)
    {
        FBreakerRolledAffix Rolled;
        Rolled.AffixId = AffixId;
        Rolled.Tier = 4;
        Rolled.Value = Value;
        return Rolled;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEquipLimitCountTest,
    "RiorsEdge.Items.Equipment.RarityLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEquipLimitCountTest::RunTest(const FString& Parameters)
{
    using namespace BreakerEquipmentTestHelpers;

    // O11 / master sheet 4.1. Everything below Aberrant is uncapped.
    TestEqual(TEXT("Aberrant caps at three"), UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Aberrant), 3);
    TestEqual(TEXT("Anomalous caps at one"), UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Anomalous), 1);
    TestEqual(TEXT("Exceptional is uncapped"), UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Exceptional), INDEX_NONE);
    TestEqual(TEXT("Standard is uncapped"), UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Standard), INDEX_NONE);

    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
    Equipment->EquipItem(MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Aberrant, 30));
    Equipment->EquipItem(MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Aberrant, 30));
    Equipment->EquipItem(MakeItem(EBreakerEquipSlot::Boots, EBreakerItemRarity::Exceptional, 30));

    TestEqual(TEXT("Aberrant count reads the equipped list"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Aberrant), 2);
    TestEqual(TEXT("Exceptional count reads the equipped list"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Exceptional), 1);
    TestEqual(TEXT("Anomalous count is zero"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Anomalous), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEquipDisplacementTest,
    "RiorsEdge.Items.Equipment.LimitDisplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEquipDisplacementTest::RunTest(const FString& Parameters)
{
    using namespace BreakerEquipmentTestHelpers;

    // Three Aberrant equipped: the cap is met.
    const FBreakerItemInstance Helmet = MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Aberrant, 40);
    const FBreakerItemInstance Gloves = MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Aberrant, 20);
    const FBreakerItemInstance Boots = MakeItem(EBreakerEquipSlot::Boots, EBreakerItemRarity::Aberrant, 30);
    const TArray<FBreakerItemInstance> Loadout = {Helmet, Gloves, Boots};

    // A fourth Aberrant into an EMPTY slot ejects the weakest — the ilvl 20
    // gloves — not the piece in the target slot, because there is none.
    const FBreakerItemInstance Waist = MakeItem(EBreakerEquipSlot::Waist, EBreakerItemRarity::Aberrant, 50);
    FBreakerEquipPreview Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(Loadout, Waist);
    TestFalse(TEXT("The empty target slot displaces nothing itself"), Preview.bSlotOccupied);
    TestTrue(TEXT("A fourth Aberrant exceeds the cap"), Preview.bExceedsRarityLimit);
    TestEqual(TEXT("The preview reports the current count"), Preview.RarityCount, 3);
    TestEqual(TEXT("The preview reports the cap"), Preview.RarityLimit, 3);
    TestEqual(TEXT("The weakest Aberrant is ejected"), Preview.LimitDisplaced.ItemId.ToString(), Gloves.ItemId.ToString());

    // Swapping Aberrant for Aberrant IN THE SAME SLOT ejects nothing extra:
    // the outgoing piece already frees its place under the cap.
    const FBreakerItemInstance BetterGloves = MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Aberrant, 45);
    Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(Loadout, BetterGloves);
    TestTrue(TEXT("The occupied slot is reported"), Preview.bSlotOccupied);
    TestEqual(TEXT("The slot swap names the outgoing gloves"), Preview.SlotDisplaced.ItemId.ToString(), Gloves.ItemId.ToString());
    TestFalse(TEXT("A same-slot same-rarity swap does not exceed the cap"), Preview.bExceedsRarityLimit);
    TestFalse(TEXT("Nothing extra is ejected"), Preview.LimitDisplaced.IsValid());

    // Ties break on wear order, so the disclosure is deterministic.
    const TArray<FBreakerItemInstance> Tied =
    {
        MakeItem(EBreakerEquipSlot::Boots, EBreakerItemRarity::Aberrant, 30),
        MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Aberrant, 30),
        MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Aberrant, 30),
    };
    Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(Tied, Waist);
    TestEqual(TEXT("Equal item levels break on wear order"), static_cast<int32>(Preview.LimitDisplaced.Slot), static_cast<int32>(EBreakerEquipSlot::Helmet));

    // Anomalous is capped at one, and a lower rarity is never capped.
    const TArray<FBreakerItemInstance> OneAnomalous = {MakeItem(EBreakerEquipSlot::Necklace, EBreakerItemRarity::Anomalous, 10)};
    Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(OneAnomalous, MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Anomalous, 50));
    TestTrue(TEXT("A second Anomalous exceeds its cap of one"), Preview.bExceedsRarityLimit);
    Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(OneAnomalous, MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Exceptional, 50));
    TestFalse(TEXT("Exceptional is never capped"), Preview.bExceedsRarityLimit);
    TestEqual(TEXT("Uncapped rarities report no limit"), Preview.RarityLimit, INDEX_NONE);

    // End to end: the cap is disclosed, never blocked. The equip succeeds, the
    // named piece leaves, and the loadout is still legal.
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
    for (const FBreakerItemInstance& Item : Loadout) Equipment->EquipItem(Item);
    TestEqual(TEXT("Three Aberrant equipped"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Aberrant), 3);

    const FBreakerEquipPreview Live = Equipment->PreviewEquip(Waist);
    TestTrue(TEXT("The live preview agrees the cap is met"), Live.bExceedsRarityLimit);
    TestTrue(TEXT("Equipping over the cap is never refused"), Equipment->EquipItem(Waist));
    TestEqual(TEXT("The loadout stays at the cap"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Aberrant), 3);

    FBreakerItemInstance Occupant;
    TestFalse(TEXT("The ejected piece's slot is now empty"), Equipment->GetEquippedItem(EBreakerEquipSlot::Gloves, Occupant));
    TestTrue(TEXT("The new piece is equipped"), Equipment->GetEquippedItem(EBreakerEquipSlot::Waist, Occupant));
    TestEqual(TEXT("The piece named in the preview is the piece that left"), Occupant.ItemId.ToString(), Waist.ItemId.ToString());
    TestTrue(TEXT("The ejected piece returned to the backpack"),
        Equipment->GetBackpack().ContainsByPredicate([&Gloves](const FBreakerItemInstance& Item) { return Item.ItemId == Gloves.ItemId; }));
    return true;
}

// ---------------------------------------------------------------------------
// O37 — per-axis equip caps (audit item 8)
// ---------------------------------------------------------------------------
// Before this, EquipLimitForRarity/PreviewEquipAgainst/CountEquippedOfRarity
// all keyed purely off Item.Rarity, so a legendary (which rolls Anomalous per
// O32) shared the "1 Anomalous" cap with ordinary Anomalous drops: equipping
// one could silently evict the other, and the two axes O37 separates were
// really one. This section pins the split, and that Aberrant (which no
// legendary ever carries) and a solo non-legendary Anomalous are unaffected.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEquipCapAxesTest,
    "RiorsEdge.Items.Equipment.PerAxisCaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEquipCapAxesTest::RunTest(const FString& Parameters)
{
    using namespace BreakerEquipmentTestHelpers;

    auto MakeLegendary = [](EBreakerEquipSlot Slot, int32 ItemLevel)
    {
        FBreakerItemInstance Item = MakeItem(Slot, EBreakerItemRarity::Anomalous, ItemLevel);
        Item.LegendaryId = TEXT("Legendary.Test");
        return Item;
    };

    // --- A legendary and a non-legendary Anomalous coexist, in both directions
    const FBreakerItemInstance Legendary = MakeLegendary(EBreakerEquipSlot::Boots, 50);
    const FBreakerItemInstance OrdinaryAnomalous = MakeItem(EBreakerEquipSlot::Necklace, EBreakerItemRarity::Anomalous, 50);

    FBreakerEquipPreview Preview = UBreakerEquipmentComponent::PreviewEquipAgainst({Legendary}, OrdinaryAnomalous);
    TestFalse(TEXT("A non-legendary Anomalous does not exceed its cap while a legendary is worn"), Preview.bExceedsRarityLimit);
    TestFalse(TEXT("...and does not name the legendary as a victim"), Preview.LimitDisplaced.IsValid());

    Preview = UBreakerEquipmentComponent::PreviewEquipAgainst({OrdinaryAnomalous}, Legendary);
    TestFalse(TEXT("A legendary does not exceed its cap while an ordinary Anomalous is worn"), Preview.bExceedsRarityLimit);
    TestFalse(TEXT("...and does not name the Anomalous piece as a victim"), Preview.LimitDisplaced.IsValid());

    // --- The legendary axis still caps at exactly one -----------------------
    const TArray<FBreakerItemInstance> OneLegendary = {MakeLegendary(EBreakerEquipSlot::Primary, 50)};
    Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(OneLegendary, MakeLegendary(EBreakerEquipSlot::Waist, 40));
    TestTrue(TEXT("A second legendary exceeds the legendary cap of one"), Preview.bExceedsRarityLimit);
    TestTrue(TEXT("...and the first legendary is the one displaced"), Preview.LimitDisplaced.IsLegendary());

    // --- The non-legendary Anomalous axis still caps at exactly one --------
    const TArray<FBreakerItemInstance> OneOrdinaryAnomalous = {OrdinaryAnomalous};
    Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(OneOrdinaryAnomalous, MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Anomalous, 50));
    TestTrue(TEXT("A second non-legendary Anomalous still exceeds its own cap of one"), Preview.bExceedsRarityLimit);
    TestFalse(TEXT("...and the displaced piece is not a legendary (there is none here)"), Preview.LimitDisplaced.IsLegendary());

    // --- Aberrant is untouched: still three, still its own axis ------------
    const TArray<FBreakerItemInstance> ThreeAberrant = {
        MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Aberrant, 10),
        MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Aberrant, 20),
        MakeItem(EBreakerEquipSlot::Boots, EBreakerItemRarity::Aberrant, 30)};
    Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(ThreeAberrant, MakeItem(EBreakerEquipSlot::Waist, EBreakerItemRarity::Aberrant, 50));
    TestTrue(TEXT("A fourth Aberrant still exceeds the O11/O37 cap of three"), Preview.bExceedsRarityLimit);

    // --- End to end through the real component, and the validator ----------
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
    TestTrue(TEXT("The legendary equips"), Equipment->EquipItem(Legendary));
    TestTrue(TEXT("An ordinary Anomalous equips alongside it"), Equipment->EquipItem(OrdinaryAnomalous));
    TestEqual(TEXT("One legendary is equipped"), Equipment->CountEquippedLegendaries(), 1);
    TestEqual(TEXT("...and the Anomalous count excludes it"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Anomalous), 1);

    FBreakerItemInstance Found;
    TestTrue(TEXT("The legendary is still equipped, not evicted by the Anomalous piece"),
        Equipment->GetEquippedItem(Legendary.Slot, Found) && Found.IsLegendary());
    TestTrue(TEXT("The Anomalous piece is still equipped, not evicted by the legendary"),
        Equipment->GetEquippedItem(OrdinaryAnomalous.Slot, Found) && !Found.IsLegendary());

    FText Failure;
    TestTrue(TEXT("One legendary plus one non-legendary Anomalous validates clean"),
        UBreakerEquipmentComponent::ValidateEquipCaps(Equipment->GetEquipped(), Failure));
    TestTrue(TEXT("...and an empty set validates clean"), UBreakerEquipmentComponent::ValidateEquipCaps({}, Failure));

    // A second legendary crammed into the set (as a save predating O37, a
    // hand-edited fixture, or a content bug could) is exactly what the
    // validator exists to catch — equip-time displacement never lets a live
    // component reach this state on its own.
    TArray<FBreakerItemInstance> OverfullLegendary = Equipment->GetEquipped();
    OverfullLegendary.Add(MakeLegendary(EBreakerEquipSlot::Primary, 30));
    TestFalse(TEXT("A second legendary fails validation"), UBreakerEquipmentComponent::ValidateEquipCaps(OverfullLegendary, Failure));
    TestFalse(TEXT("The failure carries a reason"), Failure.IsEmpty());

    TArray<FBreakerItemInstance> OverfullAnomalous = Equipment->GetEquipped();
    OverfullAnomalous.Add(MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Anomalous, 30));
    TestFalse(TEXT("A second non-legendary Anomalous fails validation"), UBreakerEquipmentComponent::ValidateEquipCaps(OverfullAnomalous, Failure));

    TArray<FBreakerItemInstance> OverfullAberrant = ThreeAberrant;
    OverfullAberrant.Add(MakeItem(EBreakerEquipSlot::Waist, EBreakerItemRarity::Aberrant, 30));
    TestFalse(TEXT("A fourth Aberrant fails validation"), UBreakerEquipmentComponent::ValidateEquipCaps(OverfullAberrant, Failure));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAffixDeltaTest,
    "RiorsEdge.Items.Equipment.AffixDeltas",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAffixDeltaTest::RunTest(const FString& Parameters)
{
    using namespace BreakerEquipmentTestHelpers;

    FBreakerItemInstance Candidate = MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Exceptional, 40);
    Candidate.Affixes.Add(MakeAffix(TEXT("Core.Health"), 150.0f));       // better
    Candidate.Affixes.Add(MakeAffix(TEXT("Crit.Chance"), 4.0f));         // worse
    Candidate.Affixes.Add(MakeAffix(TEXT("Crit.Damage"), 20.0f));        // parity
    Candidate.Affixes.Add(MakeAffix(TEXT("Move.DashCooldown"), 9.0f));   // not on the equipped piece

    FBreakerItemInstance Reference = MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Exceptional, 40);
    Reference.Affixes.Add(MakeAffix(TEXT("Core.Health"), 100.0f));
    Reference.Affixes.Add(MakeAffix(TEXT("Crit.Chance"), 6.0f));
    Reference.Affixes.Add(MakeAffix(TEXT("Crit.Damage"), 20.0f));

    TArray<FBreakerAffixComparison> Deltas = UBreakerEquipmentComponent::CompareAffixes(Candidate, Reference);
    TestEqual(TEXT("One row per candidate affix"), Deltas.Num(), 4);
    TestEqual(TEXT("A higher roll is better"), static_cast<int32>(Deltas[0].Delta), static_cast<int32>(EBreakerAffixDelta::Better));
    TestEqual(TEXT("The compared value is the equipped roll"), Deltas[0].ComparedValue, 100.0f);
    TestEqual(TEXT("The difference is signed"), Deltas[0].GetDifference(), 50.0f);
    TestEqual(TEXT("A lower roll is worse"), static_cast<int32>(Deltas[1].Delta), static_cast<int32>(EBreakerAffixDelta::Worse));
    TestEqual(TEXT("An equal roll is parity"), static_cast<int32>(Deltas[2].Delta), static_cast<int32>(EBreakerAffixDelta::Parity));
    TestEqual(TEXT("A stat the equipped piece lacks is better"), static_cast<int32>(Deltas[3].Delta), static_cast<int32>(EBreakerAffixDelta::Better));
    TestEqual(TEXT("A stat the equipped piece lacks compares against zero"), Deltas[3].ComparedValue, 0.0f);

    // Matching is by stat target and bucket, so two equipped affixes feeding
    // the same stat are one number to the player.
    FBreakerItemInstance DoubleUp = MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Exceptional, 40);
    DoubleUp.Affixes.Add(MakeAffix(TEXT("Core.Health"), 90.0f));
    DoubleUp.Affixes.Add(MakeAffix(TEXT("Core.Health"), 80.0f));
    Deltas = UBreakerEquipmentComponent::CompareAffixes(Candidate, DoubleUp);
    TestEqual(TEXT("Same-target rolls sum before comparison"), Deltas[0].ComparedValue, 170.0f);
    TestEqual(TEXT("Losing on the total reads as worse"), static_cast<int32>(Deltas[0].Delta), static_cast<int32>(EBreakerAffixDelta::Worse));

    // An empty slot: everything is an improvement, and the preview says so.
    const FBreakerEquipPreview Preview = UBreakerEquipmentComponent::PreviewEquipAgainst({}, Candidate);
    TestFalse(TEXT("An empty slot displaces nothing"), Preview.bSlotOccupied);
    TestEqual(TEXT("The preview carries the affix deltas"), Preview.AffixDeltas.Num(), 4);
    for (const FBreakerAffixComparison& Row : Preview.AffixDeltas)
    {
        TestEqual(TEXT("Every affix improves on an empty slot"), static_cast<int32>(Row.Delta), static_cast<int32>(EBreakerAffixDelta::Better));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBulkDiscardCountTest,
    "RiorsEdge.Items.Equipment.BulkDiscardCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBulkDiscardCountTest::RunTest(const FString& Parameters)
{
    using namespace BreakerEquipmentTestHelpers;

    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
    Equipment->AddToBackpack(MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Standard, 10));
    Equipment->AddToBackpack(MakeItem(EBreakerEquipSlot::Boots, EBreakerItemRarity::Standard, 12));
    Equipment->AddToBackpack(MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Uncommon, 14));
    Equipment->AddToBackpack(MakeItem(EBreakerEquipSlot::Waist, EBreakerItemRarity::Exceptional, 20));
    Equipment->AddToBackpack(MakeItem(EBreakerEquipSlot::Necklace, EBreakerItemRarity::Aberrant, 22));
    Equipment->AddToBackpack(MakeItem(EBreakerEquipSlot::Primary, EBreakerItemRarity::Anomalous, 24));
    // Equipped gear is a separate container and is never a discard candidate.
    Equipment->EquipItem(MakeItem(EBreakerEquipSlot::Secondary, EBreakerItemRarity::Standard, 5));

    TestEqual(TEXT("Below Uncommon counts only the Standards"), Equipment->CountBackpackBelowRarity(EBreakerItemRarity::Uncommon), 2);
    const int32 Predicted = Equipment->CountBackpackBelowRarity(EBreakerItemRarity::Exceptional);
    TestEqual(TEXT("Below Exceptional counts Standard and Uncommon"), Predicted, 3);

    // The number the modal states is the number destroyed — same predicate.
    const int32 Removed = Equipment->DiscardBackpackBelowRarity(EBreakerItemRarity::Exceptional);
    TestEqual(TEXT("The stated count is the destroyed count"), Removed, Predicted);
    TestEqual(TEXT("Nothing left to discard"), Equipment->CountBackpackBelowRarity(EBreakerItemRarity::Exceptional), 0);

    // Aberrant and Anomalous are above every threshold the screen offers, so
    // a bulk discard can never take them.
    TestTrue(TEXT("Aberrant survives"), Equipment->GetBackpack().ContainsByPredicate(
        [](const FBreakerItemInstance& Item) { return Item.Rarity == EBreakerItemRarity::Aberrant; }));
    TestTrue(TEXT("Anomalous survives"), Equipment->GetBackpack().ContainsByPredicate(
        [](const FBreakerItemInstance& Item) { return Item.Rarity == EBreakerItemRarity::Anomalous; }));
    TestTrue(TEXT("Equipped gear is untouched"), Equipment->GetEquipped().ContainsByPredicate(
        [](const FBreakerItemInstance& Item) { return Item.Slot == EBreakerEquipSlot::Secondary; }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDodgeBlockTest,
    "RiorsEdge.Combat.Defense.DodgeAndBlock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDodgeBlockTest::RunTest(const FString& Parameters)
{
    FBreakerDamageRequest Request;
    Request.BaseDamage = 100.0f;
    Request.bCanCritical = false;
    Request.DamageFamily = EBreakerDamageFamily::TrueDamage;

    FBreakerDefenseState Defense;
    Defense.Health = 1000.0f;

    // A guaranteed dodge fully evades the hit.
    Defense.DodgeChance = 1.0f;
    FBreakerDamageResult Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    TestTrue(TEXT("Guaranteed dodge evades"), Result.bDodged);
    TestEqual(TEXT("No health damage on a dodge"), Result.HealthDamage, 0.0f);

    // A guaranteed block halves the hit at default mitigation.
    Defense.DodgeChance = 0.0f;
    Defense.BlockChance = 1.0f;
    Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    TestTrue(TEXT("Guaranteed block triggers"), Result.bBlocked);
    TestEqual(TEXT("Block mitigates half"), Result.HealthDamage, 50.0f);

    // Dodge and block never apply to damage over time.
    Defense.DodgeChance = 1.0f;
    Request.bIsDamageOverTime = true;
    Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    TestFalse(TEXT("DoTs cannot be dodged"), Result.bDodged);
    TestFalse(TEXT("DoTs cannot be blocked"), Result.bBlocked);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRarityRollTest,
    "RiorsEdge.Items.Loot.RarityWeights",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRarityRollTest::RunTest(const FString& Parameters)
{
    int32 StandardCount = 0;
    int32 StandardCountWithBonus = 0;
    for (int32 Seed = 0; Seed < 2000; ++Seed)
    {
        if (UBreakerLootLibrary::RollRarity(Seed, 0.0f) == EBreakerItemRarity::Standard) ++StandardCount;
        if (UBreakerLootLibrary::RollRarity(Seed, 50.0f) == EBreakerItemRarity::Standard) ++StandardCountWithBonus;
    }
    TestTrue(TEXT("Standard dominates at zero bonus"), StandardCount > 1000);
    TestTrue(TEXT("Drop chance shifts weight out of Standard"), StandardCountWithBonus < StandardCount);
    TestEqual(TEXT("Rarity roll is deterministic"), static_cast<int32>(UBreakerLootLibrary::RollRarity(42, 10.0f)), static_cast<int32>(UBreakerLootLibrary::RollRarity(42, 10.0f)));
    return true;
}

// ---------------------------------------------------------------------------
// THE DEAD-LINE REPAIR — O180's shape, the item half.
// ---------------------------------------------------------------------------
// The tree drops unknown rank rows and credits their points back at load; gear
// had no analog, so a rolled line whose id stopped resolving was skipped
// silently at every aggregation forever. This is the save story, end to end: a
// save carrying a dead line loads, the line drops, the wallet is refunded at
// the fallback cost (the line's share of its item's own melt price), and the
// loaded state audits clean — every surviving line resolves.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDeadLineRepairTest,
    "RiorsEdge.Items.DeadLineRepair",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDeadLineRepairTest::RunTest(const FString& Parameters)
{
    // Never touch a real account slot: RestoreState runs the stash reconcile
    // against the account save, so inject a never-persisting one (the stash
    // tests' rig).
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    UBreakerAccountSave::InjectForTesting(Account);

    AActor* Owner = NewObject<AActor>();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);

    // A save-shaped item: one line that resolves, one that does not. The
    // unknown id stands in for any deleted pool entry — the exact fixture
    // shape SpentPointsPerf uses for the tree half's Some.Removed.Node.
    FBreakerItemInstance Item;
    Item.ItemId = FGuid::NewGuid();
    Item.DefinitionId = TEXT("RepairFixture");
    Item.Slot = EBreakerEquipSlot::Waist;
    Item.Rarity = EBreakerItemRarity::Uncommon;
    Item.ItemLevel = 20;
    auto AddLine = [&Item](const TCHAR* Id, EBreakerAffixCategory Category)
    {
        FBreakerRolledAffix Rolled;
        Rolled.AffixId = Id;
        Rolled.Tier = 8;
        Rolled.Value = 30.0f;
        Rolled.Category = Category;
        Item.Affixes.Add(Rolled);
    };
    AddLine(TEXT("Core.Health"), EBreakerAffixCategory::Suffix);
    AddLine(TEXT("Some.Removed.Line"), EBreakerAffixCategory::Prefix);

    const int32 LinesBefore = Item.Affixes.Num();
    const int32 ExpectedCredit = FMath::Max(1, UBreakerForgeLibrary::SalvageValue(Item).Get() / LinesBefore);

    // The load path's two steps, in the load path's own order: items first,
    // then the WHOLESALE wallet replacement that must not wipe the refund.
    Equipment->RestoreState({Item}, {});
    FBreakerForgeWallet SavedWallet;
    SavedWallet.Add(100);
    Equipment->RestoreForgeWallet(SavedWallet);

    if (!TestEqual(TEXT("the item survives the load"), Equipment->GetEquipped().Num(), 1)) return false;
    TestEqual(TEXT("the dead line is gone from the loaded item"), Equipment->GetEquipped()[0].Affixes.Num(), 1);
    TestEqual(TEXT("the resolving line survives untouched"),
        Equipment->GetEquipped()[0].Affixes[0].AffixId, FName(TEXT("Core.Health")));
    TestEqual(TEXT("the line came back as Riftglass at the fallback cost, on top of the SAVED wallet"),
        Equipment->GetForgeWallet().Get(), 100 + ExpectedCredit);

    // Audits clean: every surviving line resolves through the same FindAffix
    // the aggregation fold uses.
    for (const FBreakerRolledAffix& Rolled : Equipment->GetEquipped()[0].Affixes)
    {
        TestNotNull(*FString::Printf(TEXT("'%s' resolves after the repair"), *Rolled.AffixId.ToString()),
            UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), Rolled.AffixId));
    }

    // Loading the same file twice reproduces the same state, not a doubled
    // refund: the items AND the wallet both come from the file, so the credit
    // is recomputed onto the same base every time.
    Equipment->RestoreState({Item}, {});
    Equipment->RestoreForgeWallet(SavedWallet);
    TestEqual(TEXT("a second load of the same file lands on the same wallet"),
        Equipment->GetForgeWallet().Get(), 100 + ExpectedCredit);

    // A wallet restore with no preceding repair is a plain assignment — no
    // phantom credit rides a fresh component.
    UBreakerEquipmentComponent* Fresh = NewObject<UBreakerEquipmentComponent>(Owner);
    Fresh->RestoreForgeWallet(SavedWallet);
    TestEqual(TEXT("no repair, no phantom credit"), Fresh->GetForgeWallet().Get(), 100);

    // A clean loadout is untouched: no drop, no credit.
    FBreakerItemInstance Clean = Item;
    Clean.ItemId = FGuid::NewGuid();
    Clean.Affixes.RemoveAt(1);
    UBreakerEquipmentComponent* CleanEquipment = NewObject<UBreakerEquipmentComponent>(Owner);
    CleanEquipment->RestoreState({Clean}, {});
    CleanEquipment->RestoreForgeWallet(SavedWallet);
    TestEqual(TEXT("a clean item keeps every line"), CleanEquipment->GetEquipped()[0].Affixes.Num(), 1);
    TestEqual(TEXT("and pays no phantom refund"), CleanEquipment->GetForgeWallet().Get(), 100);

    UBreakerAccountSave::ResetCacheForTesting();
    return true;
}

#endif
