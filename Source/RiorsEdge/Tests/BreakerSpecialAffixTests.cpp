#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemRules.h"
#include "Items/BreakerLootLibrary.h"
#include "Progression/BreakerBuildConditions.h"

// ---------------------------------------------------------------------------
// THE HIGH-RARITY IDENTITY PASS — special affixes for Aberrant and Anomalous.
// ---------------------------------------------------------------------------
// O11 reserved Aberrant's "1-2 unique modifier affixes" and the seat is now
// occupied; Anomalous gained one signature line BESIDE its rule. These tests
// pin the four properties the pass promised: the pools' content is honest
// (every line reaches gameplay, every condition is evaluable, every bill is a
// real bill), the rarity gate is absolute in the downward direction, the roll
// stays seed-deterministic, and the item shape (prefix/suffix caps, the rule
// roll, the legendary chance) did not regress.

namespace BreakerSpecialAffixTest
{
    // Prefixed with the file's subject: identical anonymous-namespace helper
    // names in two translation units have collided under this unity build.
    TSet<FName> BreakerSpecialAllIds()
    {
        TSet<FName> Ids;
        for (const FBreakerAffixDefinition& Affix : UBreakerAffixLibrary::GetAberrantAffixPool()) Ids.Add(Affix.AffixId);
        for (const FBreakerAffixDefinition& Affix : UBreakerAffixLibrary::GetAnomalousAffixPool()) Ids.Add(Affix.AffixId);
        for (const FBreakerAffixDefinition& Affix : UBreakerAffixLibrary::GetSpecialDownsidePool()) Ids.Add(Affix.AffixId);
        return Ids;
    }

    bool BreakerSpecialItemHasAnyOf(const FBreakerItemInstance& Item, const TArray<FBreakerAffixDefinition>& Pool)
    {
        for (const FBreakerAffixDefinition& Affix : Pool)
        {
            const FName Id = Affix.AffixId;
            if (Item.Affixes.ContainsByPredicate([Id](const FBreakerRolledAffix& Rolled) { return Rolled.AffixId == Id; }))
            {
                return true;
            }
        }
        return false;
    }

    // A one-affix fixture item carrying a hand-placed special line, for the
    // aggregation checks — the same shape BreakerRuleMakeItem uses next door.
    FBreakerItemInstance BreakerSpecialMakeItem(EBreakerEquipSlot Slot, const TArray<FName>& AffixIds, int32 Tier = 1)
    {
        const TArray<FBreakerAffixDefinition>& Slice = UBreakerAffixLibrary::GetSliceAffixPool();
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = TEXT("SpecialAffixTest");
        Item.Slot = Slot;
        Item.Rarity = EBreakerItemRarity::Aberrant;
        Item.ItemLevel = 50;
        for (const FName AffixId : AffixIds)
        {
            // FindAffix's special-pool fallback is itself under test here: the
            // slice pool is passed, and the special ids must still resolve.
            const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Slice, AffixId);
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
}

// ---------------------------------------------------------------------------
// The pools themselves: honest content, in bounds, wired to live channels.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSpecialAffixPoolContentTest,
    "RiorsEdge.Items.SpecialAffixes.PoolContent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSpecialAffixPoolContentTest::RunTest(const FString& Parameters)
{
    using ALib = UBreakerAffixLibrary;
    const TArray<FBreakerAffixDefinition>& Slice = ALib::GetSliceAffixPool();
    const TArray<FBreakerAffixDefinition>& Aberrant = ALib::GetAberrantAffixPool();
    const TArray<FBreakerAffixDefinition>& Anomalous = ALib::GetAnomalousAffixPool();
    const TArray<FBreakerAffixDefinition>& Downsides = ALib::GetSpecialDownsidePool();

    // The owner's brief and O11's number: a small pool of 6-10 for Aberrant,
    // 4-6 rarer ones for Anomalous.
    TestTrue(TEXT("Aberrant pool holds 6-10 special affixes"), Aberrant.Num() >= 6 && Aberrant.Num() <= 10);
    TestTrue(TEXT("Anomalous pool holds 4-6 signature affixes"), Anomalous.Num() >= 4 && Anomalous.Num() <= 6);

    TSet<FName> SeenIds;
    auto CheckSpecialEntry = [this, &Slice, &Downsides, &SeenIds](const FBreakerAffixDefinition& Affix, EBreakerItemRarity ExpectedRarity)
    {
        const FString Context = Affix.AffixId.ToString();
        TestFalse(*(Context + TEXT(" id is unique across all pools")), SeenIds.Contains(Affix.AffixId));
        SeenIds.Add(Affix.AffixId);

        TestEqual(*(Context + TEXT(" is gated at its rarity")),
            static_cast<int32>(Affix.MinimumRarity), static_cast<int32>(ExpectedRarity));
        TestTrue(*(Context + TEXT(" rolls on at least one slot")), Affix.AllowedSlots.Num() > 0);
        TestFalse(*(Context + TEXT(" has a display name")), Affix.DisplayName.IsEmpty());
        // The locked aggregation law: gear never authors a More, and a special
        // is not an exemption.
        TestTrue(*(Context + TEXT(" does not author a More")), Affix.StatBucket != EBreakerStatBucket::MorePercent);
        // Its condition must be answerable TODAY, or the line can never pay —
        // the exact bug class this project keeps finding.
        TestTrue(*(Context + TEXT("'s condition is self-evaluable")),
            FBreakerBuildConditionState::IsSelfEvaluable(Affix.Condition));
        // Positive, ordered anchors: the tier curve's geometric branch.
        TestTrue(*(Context + TEXT(" has a positive, ordered value band")),
            Affix.ValueAtT12 > 0.0f && Affix.ValueAtT1 > Affix.ValueAtT12);

        // NOT in the slice pool: the slice pool is what the generic loop and
        // the Forge's Attune candidate walk iterate, so membership there would
        // leak the line below its rarity.
        TestNull(*(Context + TEXT(" is not in the slice pool")),
            Slice.FindByPredicate([&Affix](const FBreakerAffixDefinition& Entry) { return Entry.AffixId == Affix.AffixId; }));
        // ...but it must RESOLVE through the slice-pool lookup every consumer
        // uses, or the rolled line would aggregate to nothing.
        TestNotNull(*(Context + TEXT(" resolves through FindAffix's fallback")),
            UBreakerAffixLibrary::FindAffix(Slice, Affix.AffixId));

        // PERCEPTIBLE (the O2 rule): where an ordinary line hits the same stat
        // in the same bucket, the special's ceiling must materially beat it.
        for (const FBreakerAffixDefinition& Ordinary : Slice)
        {
            if (Ordinary.StatTarget != Affix.StatTarget || Ordinary.StatBucket != Affix.StatBucket) continue;
            TestTrue(*(Context + TEXT(" ceiling materially exceeds ") + Ordinary.AffixId.ToString()),
                Affix.ValueAtT1 > Ordinary.ValueAtT1 * 1.5f);
        }

        // A named bill must exist in the downside pool, be legal on every slot
        // the carrier rolls on, and actually BE a bill.
        if (!Affix.PairedAffixId.IsNone())
        {
            const FBreakerAffixDefinition* Bill = UBreakerAffixLibrary::FindAffix(Downsides, Affix.PairedAffixId);
            TestNotNull(*(Context + TEXT("'s bill exists in the downside pool")), Bill);
            if (Bill)
            {
                for (const EBreakerEquipSlot Slot : Affix.AllowedSlots)
                {
                    TestTrue(*(Context + TEXT("'s bill is legal on every carrier slot")), Bill->AllowsSlot(Slot));
                }
            }
        }
    };

    for (const FBreakerAffixDefinition& Affix : Aberrant) CheckSpecialEntry(Affix, EBreakerItemRarity::Aberrant);
    for (const FBreakerAffixDefinition& Affix : Anomalous) CheckSpecialEntry(Affix, EBreakerItemRarity::Anomalous);

    // The bills: constant NEGATIVE lines in ordinary buckets. Constant so the
    // deal reads as "this much, always"; negative or they are not bills.
    for (const FBreakerAffixDefinition& Bill : Downsides)
    {
        const FString Context = Bill.AffixId.ToString();
        TestFalse(*(Context + TEXT(" id is unique across all pools")), SeenIds.Contains(Bill.AffixId));
        SeenIds.Add(Bill.AffixId);
        TestTrue(*(Context + TEXT(" is a negative line")), Bill.ValueAtT1 < 0.0f && Bill.ValueAtT12 < 0.0f);
        TestEqual(*(Context + TEXT(" is constant across tiers")), Bill.ValueAtT12, Bill.ValueAtT1, 0.0001f);
        TestTrue(*(Context + TEXT(" does not author a More")), Bill.StatBucket != EBreakerStatBucket::MorePercent);
        TestNull(*(Context + TEXT(" is not in the slice pool")),
            Slice.FindByPredicate([&Bill](const FBreakerAffixDefinition& Entry) { return Entry.AffixId == Bill.AffixId; }));
        // ValueForTier's degenerate-band branch really does hold it constant.
        TestEqual(*(Context + TEXT(" evaluates constant at T12 and T1")),
            UBreakerAffixLibrary::ValueForTier(Bill, 12), UBreakerAffixLibrary::ValueForTier(Bill, 1), 0.0001f);
    }

    // COVERAGE, the invariant that makes the seat a guarantee: every slot must
    // offer, in EACH category, at least one special with no bill in the other
    // category. The ordinary roll can fill at most six of the eight cap slots
    // (4 prefix + 4 suffix), so at least one whole category always has room —
    // and a bill-free candidate in that category means candidate lists can
    // never come up empty. This is what upgrades "nearly every drop" to "every
    // drop carries its identity", and the gating test asserts the consequence.
    auto CheckCoverage = [this, &Downsides](const TArray<FBreakerAffixDefinition>& Pool, const TCHAR* PoolName)
    {
        for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
        {
            const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
            bool bFreePrefix = false;
            bool bFreeSuffix = false;
            for (const FBreakerAffixDefinition& Affix : Pool)
            {
                if (!Affix.AllowsSlot(Slot)) continue;
                const FBreakerAffixDefinition* Bill = Affix.PairedAffixId.IsNone()
                    ? nullptr
                    : UBreakerAffixLibrary::FindAffix(Downsides, Affix.PairedAffixId);
                // A candidate whose bill lands in the OPPOSITE category needs
                // room in both; only bill-free candidates (or same-category
                // bills) count toward the guarantee.
                const bool bNeedsOtherCategory = Bill && Bill->Category != Affix.Category;
                if (bNeedsOtherCategory) continue;
                if (Affix.Category == EBreakerAffixCategory::Prefix) bFreePrefix = true; else bFreeSuffix = true;
            }
            TestTrue(*FString::Printf(TEXT("%s pool: slot %d has a bill-free prefix candidate"), PoolName, SlotIndex), bFreePrefix);
            TestTrue(*FString::Printf(TEXT("%s pool: slot %d has a bill-free suffix candidate"), PoolName, SlotIndex), bFreeSuffix);
        }
    };
    CheckCoverage(Aberrant, TEXT("Aberrant"));
    CheckCoverage(Anomalous, TEXT("Anomalous"));
    return true;
}

// ---------------------------------------------------------------------------
// The rarity gate: at its rarity, never below it, never across it.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSpecialAffixRarityGatingTest,
    "RiorsEdge.Items.SpecialAffixes.RarityGating",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSpecialAffixRarityGatingTest::RunTest(const FString& Parameters)
{
    using namespace BreakerSpecialAffixTest;
    const TArray<FBreakerAffixDefinition>& AberrantPool = UBreakerAffixLibrary::GetAberrantAffixPool();
    const TArray<FBreakerAffixDefinition>& AnomalousPool = UBreakerAffixLibrary::GetAnomalousAffixPool();
    const TSet<FName> AllSpecialIds = BreakerSpecialAllIds();

    // BELOW: no special or downside id may ever appear on Standard, Uncommon
    // or Exceptional, on any slot. This is the test the task names, and the
    // pool-separation design makes it structural rather than probabilistic —
    // but it is asserted against real rolls, not against the design.
    for (int32 RarityIndex = 0; RarityIndex <= static_cast<int32>(EBreakerItemRarity::Exceptional); ++RarityIndex)
    {
        for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
        {
            for (int32 Seed = 1; Seed <= 60; ++Seed)
            {
                const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Gate"),
                    static_cast<EBreakerEquipSlot>(SlotIndex), static_cast<EBreakerItemRarity>(RarityIndex),
                    60, Seed * 131 + SlotIndex * 17 + RarityIndex);
                for (const FBreakerRolledAffix& Rolled : Item.Affixes)
                {
                    TestFalse(*FString::Printf(TEXT("%s never rolls below Aberrant (rarity %d)"),
                        *Rolled.AffixId.ToString(), RarityIndex), AllSpecialIds.Contains(Rolled.AffixId));
                }
            }
        }
    }

    // ACROSS: the pools are exclusive per rarity — an Aberrant never carries an
    // Anomalous signature and an Anomalous never carries an Aberrant special,
    // so each high rarity keeps its own identity. And WITHIN: an Aberrant
    // carries 1-2 special lines (O11's number) whenever the category caps left
    // it any room at all, which the per-slot category coverage makes the
    // overwhelmingly common case.
    int32 AberrantDrops = 0;
    int32 AberrantWithSpecial = 0;
    for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
    {
        for (int32 Seed = 1; Seed <= 80; ++Seed)
        {
            const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Gate"),
                static_cast<EBreakerEquipSlot>(SlotIndex), EBreakerItemRarity::Aberrant, 60, Seed * 977 + SlotIndex);
            ++AberrantDrops;
            if (BreakerSpecialItemHasAnyOf(Item, AberrantPool)) ++AberrantWithSpecial;
            TestFalse(TEXT("An Aberrant never carries an Anomalous signature line"),
                BreakerSpecialItemHasAnyOf(Item, AnomalousPool));

            int32 SpecialLines = 0;
            for (const FBreakerRolledAffix& Rolled : Item.Affixes)
            {
                if (AberrantPool.FindByPredicate([&Rolled](const FBreakerAffixDefinition& A) { return A.AffixId == Rolled.AffixId; })) ++SpecialLines;
            }
            TestTrue(TEXT("An Aberrant carries at most two special lines"), SpecialLines <= 2);
        }
    }
    // EVERY drop, not nearly every: the bill-free per-slot coverage the pool
    // test pins means a candidate always fits in whichever category has room.
    TestEqual(*FString::Printf(TEXT("Every Aberrant carries a special line (%d of %d)"),
        AberrantWithSpecial, AberrantDrops), AberrantWithSpecial, AberrantDrops);

    int32 AnomalousDrops = 0;
    int32 AnomalousWithSignature = 0;
    for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
    {
        for (int32 Seed = 1; Seed <= 80; ++Seed)
        {
            const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Gate"),
                static_cast<EBreakerEquipSlot>(SlotIndex), EBreakerItemRarity::Anomalous, 60, Seed * 613 + SlotIndex);
            ++AnomalousDrops;
            if (BreakerSpecialItemHasAnyOf(Item, AnomalousPool)) ++AnomalousWithSignature;
            TestFalse(TEXT("An Anomalous never carries an Aberrant special line"),
                BreakerSpecialItemHasAnyOf(Item, AberrantPool));
            // The signature sits BESIDE the rule — the rule roll must not have
            // regressed. (Legendaries carry their fixed rule instead.)
            TestTrue(TEXT("Every Anomalous still carries a rule"), Item.HasRule());
        }
    }
    TestEqual(*FString::Printf(TEXT("Every Anomalous carries its signature line (%d of %d)"),
        AnomalousWithSignature, AnomalousDrops), AnomalousWithSignature, AnomalousDrops);

    // EVERY pool entry is actually reachable at its rarity: for each special,
    // roll on one of its allowed slots until it shows. An authored line nobody
    // can roll is this project's named failure.
    auto AssertReachable = [this](const FBreakerAffixDefinition& Affix, EBreakerItemRarity Rarity)
    {
        const EBreakerEquipSlot Slot = Affix.AllowedSlots[0];
        bool bSeen = false;
        for (int32 Seed = 1; Seed <= 500 && !bSeen; ++Seed)
        {
            const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Reach"), Slot, Rarity, 60, Seed * 7919);
            const FName Id = Affix.AffixId;
            bSeen = Item.Affixes.ContainsByPredicate([Id](const FBreakerRolledAffix& Rolled) { return Rolled.AffixId == Id; });
        }
        TestTrue(*(Affix.AffixId.ToString() + TEXT(" is reachable at its rarity")), bSeen);
    };
    for (const FBreakerAffixDefinition& Affix : AberrantPool) AssertReachable(Affix, EBreakerItemRarity::Aberrant);
    for (const FBreakerAffixDefinition& Affix : AnomalousPool) AssertReachable(Affix, EBreakerItemRarity::Anomalous);

    // A bill never appears without its carrier: the downside is part of the
    // deal, not an independent roll.
    for (int32 Seed = 1; Seed <= 300; ++Seed)
    {
        const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Bill"),
            EBreakerEquipSlot::Primary, EBreakerItemRarity::Aberrant, 60, Seed * 37);
        for (const FBreakerRolledAffix& Rolled : Item.Affixes)
        {
            const FName BillId = Rolled.AffixId;
            const FBreakerAffixDefinition* Carrier = AberrantPool.FindByPredicate(
                [BillId](const FBreakerAffixDefinition& A) { return A.PairedAffixId == BillId; });
            if (!UBreakerAffixLibrary::GetSpecialDownsidePool().FindByPredicate(
                [BillId](const FBreakerAffixDefinition& A) { return A.AffixId == BillId; })) continue;
            TestNotNull(TEXT("A rolled bill has a carrier in the pool"), Carrier);
            if (Carrier)
            {
                const FName CarrierId = Carrier->AffixId;
                TestTrue(TEXT("The bill's carrier is on the same item"),
                    Item.Affixes.ContainsByPredicate([CarrierId](const FBreakerRolledAffix& R) { return R.AffixId == CarrierId; }));
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Determinism and item shape: same seed, same item, caps intact.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSpecialAffixDeterminismTest,
    "RiorsEdge.Items.SpecialAffixes.DeterminismAndShape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSpecialAffixDeterminismTest::RunTest(const FString& Parameters)
{
    for (const EBreakerItemRarity Rarity : {EBreakerItemRarity::Aberrant, EBreakerItemRarity::Anomalous})
    {
        for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
        {
            const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
            for (int32 Seed = 1; Seed <= 40; ++Seed)
            {
                const int32 RealSeed = Seed * 271 + SlotIndex * 31 + static_cast<int32>(Rarity);
                const FBreakerItemInstance A = UBreakerLootLibrary::RollItem(TEXT("Det"), Slot, Rarity, 75, RealSeed);
                const FBreakerItemInstance B = UBreakerLootLibrary::RollItem(TEXT("Det"), Slot, Rarity, 75, RealSeed);

                // Same seed -> same item, EXACTLY: the special draws are
                // appended to the same deterministic stream, like the archetype
                // draw was.
                TestEqual(TEXT("Seed reproduces the affix count"), A.Affixes.Num(), B.Affixes.Num());
                TestEqual(TEXT("Seed reproduces the rule"), static_cast<int32>(A.Rule), static_cast<int32>(B.Rule));
                TestEqual(TEXT("Seed reproduces the legendary identity"), A.LegendaryId, B.LegendaryId);
                TestEqual(TEXT("Seed reproduces the archetype"),
                    static_cast<int32>(A.WeaponArchetype), static_cast<int32>(B.WeaponArchetype));
                for (int32 Index = 0; Index < FMath::Min(A.Affixes.Num(), B.Affixes.Num()); ++Index)
                {
                    TestEqual(TEXT("Seed reproduces the affix id"), A.Affixes[Index].AffixId, B.Affixes[Index].AffixId);
                    TestEqual(TEXT("Seed reproduces the tier"), A.Affixes[Index].Tier, B.Affixes[Index].Tier);
                    TestEqual(TEXT("Seed reproduces the value"), A.Affixes[Index].Value, B.Affixes[Index].Value, 0.0001f);
                }

                // The item shape holds WITH the special lines and their bills
                // counted: never more than four of a category, no duplicates,
                // and every line legal for the slot and resolvable. Legendaries
                // are exempt from the CAP half only: RollLegendary deliberately
                // adds missing signature lines without consulting the caps
                // ("identity should not depend on whether the affix draw
                // cooperated" — pre-existing behaviour, not this pass's), so the
                // cap pin applies to every non-legendary roll.
                if (!A.IsLegendary())
                {
                    TestTrue(TEXT("Prefix cap holds with special lines"),
                        UBreakerLootLibrary::CountAffixesOfCategory(A, EBreakerAffixCategory::Prefix) <= 4);
                    TestTrue(TEXT("Suffix cap holds with special lines"),
                        UBreakerLootLibrary::CountAffixesOfCategory(A, EBreakerAffixCategory::Suffix) <= 4);
                }
                TSet<FName> Seen;
                for (const FBreakerRolledAffix& Rolled : A.Affixes)
                {
                    TestFalse(TEXT("No duplicate lines"), Seen.Contains(Rolled.AffixId));
                    Seen.Add(Rolled.AffixId);
                    const FBreakerAffixDefinition* Definition =
                        UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), Rolled.AffixId);
                    TestNotNull(TEXT("Every rolled line resolves to a definition"), Definition);
                    if (Definition)
                    {
                        TestTrue(TEXT("Every rolled line is slot-legal"), Definition->AllowsSlot(Slot));
                    }
                }
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// The lines reach gameplay: conditions gate, magnitudes land, bills are paid.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSpecialAffixReachGameplayTest,
    "RiorsEdge.Items.SpecialAffixes.ReachGameplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSpecialAffixReachGameplayTest::RunTest(const FString& Parameters)
{
    using namespace BreakerSpecialAffixTest;

    auto IncreasedDamage = [](const TArray<FBreakerItemInstance>& Items, const FBreakerBuildConditionState& Conditions)
    {
        FBreakerAttributeContribution Offer;
        UBreakerEquipmentComponent::AggregateStats(Items, &Offer, Conditions);
        return Offer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier);
    };
    auto StateWith = [](EBreakerBuildCondition Condition)
    {
        FBreakerBuildConditionState State;
        State.Set(Condition, true);
        return State;
    };

    // BALLAST: the condition-flipped payoff. Pays while Grounded, and only
    // while Grounded — the movement game's inversion is real, not a label.
    {
        const TArray<FBreakerItemInstance> Items = {BreakerSpecialMakeItem(EBreakerEquipSlot::BodyArmour, {TEXT("Aberrant.Ballast")})};
        TestTrue(TEXT("Ballast pays while grounded"),
            IncreasedDamage(Items, StateWith(EBreakerBuildCondition::Grounded)) > 50.0f);
        TestEqual(TEXT("Ballast pays nothing while airborne"),
            IncreasedDamage(Items, StateWith(EBreakerBuildCondition::Airborne)), 0.0f, 0.001f);
    }

    // ENTROPY DEBT: the Anomalous flip — live only at empty resource.
    {
        const TArray<FBreakerItemInstance> Items = {BreakerSpecialMakeItem(EBreakerEquipSlot::Helmet, {TEXT("Anomaly.EntropyDebt")})};
        TestTrue(TEXT("Entropy Debt pays while the resource is empty"),
            IncreasedDamage(Items, StateWith(EBreakerBuildCondition::ResourceDepleted)) > 150.0f);
        TestEqual(TEXT("Entropy Debt pays nothing on a full tank"),
            IncreasedDamage(Items, FBreakerBuildConditionState()), 0.0f, 0.001f);
    }

    // RIFTBURN and its bill: the DoT channel spikes, direct damage is CUT by
    // exactly the constant the bill states.
    {
        const TArray<FBreakerItemInstance> Items = {BreakerSpecialMakeItem(EBreakerEquipSlot::Helmet,
            {TEXT("Aberrant.Riftburn"), TEXT("Downside.Riftburn")})};
        FBreakerAttributeContribution Offer;
        UBreakerEquipmentComponent::AggregateStats(Items, &Offer, FBreakerBuildConditionState());
        TestTrue(TEXT("Riftburn reaches DamageOverTimeMultiplier at special magnitude"),
            Offer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageOverTimeMultiplier) > 100.0f);
        TestEqual(TEXT("Riftburn's bill cuts direct damage by its constant"),
            Offer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier), -12.0f, 0.001f);
    }

    // OVERWOUND and its bill: fire rate up, crit chance down, no More anywhere.
    {
        const TArray<FBreakerItemInstance> Items = {BreakerSpecialMakeItem(EBreakerEquipSlot::Primary,
            {TEXT("Aberrant.Overwound"), TEXT("Downside.Overwound")})};
        FBreakerAttributeContribution Offer;
        const FBreakerEquipmentStats Stats = UBreakerEquipmentComponent::AggregateStats(Items, &Offer, FBreakerBuildConditionState());
        TestTrue(TEXT("Overwound reaches the fire-rate attribute"),
            Offer.GetIncreasedPercent(EBreakerAggregatedAttribute::FireRateMultiplier) > 30.0f);
        TestEqual(TEXT("Overwound's bill lands as negative flat crit chance"),
            Stats.CriticalChanceBonus, -0.03f, 0.0001f);
        for (int32 Index = 0; Index < FBreakerAttributeContribution::AttributeCount; ++Index)
        {
            TestEqual(TEXT("No special line authors a More"),
                Offer.GetMore(static_cast<EBreakerAggregatedAttribute>(Index)), 1.0f, 0.0001f);
        }
    }

    // RIFTPLATE and its bill: a wall of armour, paid for in movement speed.
    {
        const TArray<FBreakerItemInstance> Items = {BreakerSpecialMakeItem(EBreakerEquipSlot::BodyArmour,
            {TEXT("Anomaly.Riftplate"), TEXT("Downside.Riftplate")})};
        FBreakerAttributeContribution Offer;
        const FBreakerEquipmentStats Stats = UBreakerEquipmentComponent::AggregateStats(Items, &Offer, FBreakerBuildConditionState());
        TestTrue(TEXT("Riftplate reaches the Armor attribute at special magnitude"),
            Offer.GetFlat(EBreakerAggregatedAttribute::Armor) > 300.0f);
        TestTrue(TEXT("Riftplate's bill slows the wearer"), Stats.MoveSpeedMultiplier < 1.0f);
        TestEqual(TEXT("...by exactly the stated constant"),
            Offer.GetIncreasedPercent(EBreakerAggregatedAttribute::MoveSpeed), -10.0f, 0.001f);
    }

    // BREAKER'S TITHE: the clean sustain line pays through the same on-kill
    // stat the ordinary line uses — no new channel, just a bigger number.
    {
        const TArray<FBreakerItemInstance> Items = {BreakerSpecialMakeItem(EBreakerEquipSlot::Gloves, {TEXT("Aberrant.Tithe")})};
        const FBreakerEquipmentStats Stats = UBreakerEquipmentComponent::AggregateStats(Items, nullptr, FBreakerBuildConditionState());
        TestTrue(TEXT("Breaker's Tithe reaches LifeOnKill at special magnitude"), Stats.LifeOnKill > 150.0f);
    }

    // UNBOUND frees the special conditionals exactly as it frees the ordinary
    // ones — the new lines are citizens of the existing rule system.
    {
        FBreakerItemInstance Flipped = BreakerSpecialMakeItem(EBreakerEquipSlot::Helmet, {TEXT("Aberrant.Failsafe")});
        TestEqual(TEXT("Failsafe pays nothing at full health"),
            IncreasedDamage({Flipped}, FBreakerBuildConditionState()), 0.0f, 0.001f);
        Flipped.Rule = EBreakerItemRule::Unbound;
        Flipped.Rarity = EBreakerItemRarity::Anomalous;
        TestTrue(TEXT("Unbound frees a special conditional like any other"),
            IncreasedDamage({Flipped}, FBreakerBuildConditionState()) > 80.0f);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Nothing regressed around the seat: the legendary chance and the O37 axes.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSpecialAffixNoRegressionTest,
    "RiorsEdge.Items.SpecialAffixes.NoRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSpecialAffixNoRegressionTest::RunTest(const FString& Parameters)
{
    using namespace BreakerSpecialAffixTest;

    // A legendary still drops from the ordinary Anomalous pipeline, and now
    // carries a signature line like every other Anomalous — the peak of the
    // ladder is not exempt from the ladder's identity.
    bool bLegendaryDropped = false;
    for (int32 Seed = 1; Seed <= 600 && !bLegendaryDropped; ++Seed)
    {
        const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Drop"),
            EBreakerEquipSlot::Boots, EBreakerItemRarity::Anomalous, 50, Seed);
        bLegendaryDropped = Item.IsLegendary();
    }
    TestTrue(TEXT("A legendary still drops from the ordinary Anomalous pipeline"), bLegendaryDropped);

    const FBreakerItemInstance Deadfall = UBreakerLootLibrary::RollLegendary(TEXT("Legendary.Deadfall"), 50, 4242);
    TestTrue(TEXT("A legendary keeps its fixed rule beside the signature line"),
        Deadfall.Rule == EBreakerItemRule::Deadfall);
    TestTrue(TEXT("A legendary still reproduces from its seed"),
        UBreakerLootLibrary::RollLegendary(TEXT("Legendary.Deadfall"), 50, 4242).Affixes.Num() == Deadfall.Affixes.Num());

    // The O37 equip caps are untouched by the special lines: one legendary,
    // one non-legendary Anomalous, three Aberrant — asserted through the same
    // validator the save path uses, with REAL rolled items carrying real
    // special lines.
    TArray<FBreakerItemInstance> Loadout;
    Loadout.Add(UBreakerLootLibrary::RollLegendary(TEXT("Legendary.Overrun"), 50, 99));
    FBreakerItemInstance Anomalous = UBreakerLootLibrary::RollItem(TEXT("Cap"), EBreakerEquipSlot::Necklace, EBreakerItemRarity::Anomalous, 50, 7);
    Loadout.Add(Anomalous);
    Loadout.Add(UBreakerLootLibrary::RollItem(TEXT("Cap"), EBreakerEquipSlot::Helmet, EBreakerItemRarity::Aberrant, 50, 11));
    Loadout.Add(UBreakerLootLibrary::RollItem(TEXT("Cap"), EBreakerEquipSlot::Gloves, EBreakerItemRarity::Aberrant, 50, 13));
    Loadout.Add(UBreakerLootLibrary::RollItem(TEXT("Cap"), EBreakerEquipSlot::Boots, EBreakerItemRarity::Aberrant, 50, 17));
    FText Failure;
    TestTrue(TEXT("The O37 loadout (1 legendary, 1 Anomalous, 3 Aberrant) still validates"),
        UBreakerEquipmentComponent::ValidateEquipCaps(Loadout, Failure));

    FBreakerItemInstance SecondAnomalous = UBreakerLootLibrary::RollItem(TEXT("Cap"), EBreakerEquipSlot::Waist, EBreakerItemRarity::Anomalous, 50, 19);
    Loadout.Add(SecondAnomalous);
    TestFalse(TEXT("A second non-legendary Anomalous still fails validation"),
        UBreakerEquipmentComponent::ValidateEquipCaps(Loadout, Failure));
    return true;
}

#endif
