#include "Misc/AutomationTest.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerItemRules.h"
#include "Items/BreakerLootLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFinalAffixBudgetTest, "RiorsEdge.Items.Allocation.FinalSpecialBudgets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFinalAffixBudgetTest::RunTest(const FString& Parameters)
{
    TSet<FName> ReachableLegendaries;
    const auto& Generic = UBreakerAffixLibrary::GetSliceAffixPool();
    const auto& Bills = UBreakerAffixLibrary::GetSpecialDownsidePool();
    for (EBreakerItemRarity Rarity : {EBreakerItemRarity::Aberrant, EBreakerItemRarity::Anomalous})
    {
        const auto& Specials = Rarity == EBreakerItemRarity::Aberrant
            ? UBreakerAffixLibrary::GetAberrantAffixPool() : UBreakerAffixLibrary::GetAnomalousAffixPool();
        int32 Minimum = 0, Maximum = 0;
        UBreakerAffixLibrary::AffixCountRangeForRarity(Rarity, Minimum, Maximum);
        for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
        for (int32 Level : {40, 60, 120})
        for (int32 Seed = 1; Seed <= 96; ++Seed)
        {
            const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
            const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Allocation"), Slot, Rarity, Level, Seed);
            if (!TestTrue(TEXT("Every special roll produces a valid item"), Item.IsValid())) return false;
            TestTrue(TEXT("Final count includes specials, signatures and bills"), Item.Affixes.Num() >= Minimum && Item.Affixes.Num() <= Maximum);
            for (EBreakerAffixCategory Category : {EBreakerAffixCategory::Prefix, EBreakerAffixCategory::Suffix})
                TestTrue(TEXT("Final category cap includes signatures and bills"), UBreakerLootLibrary::CountAffixesOfCategory(Item, Category) <= 4);
            TSet<FName> Seen;
            int32 SpecialCount = 0;
            for (const FBreakerRolledAffix& Affix : Item.Affixes)
            {
                TestFalse(TEXT("No duplicate affix or bill ID"), Seen.Contains(Affix.AffixId));
                Seen.Add(Affix.AffixId);
                if (const FBreakerAffixDefinition* Special = Specials.FindByPredicate([&](const FBreakerAffixDefinition& Definition) { return Definition.AffixId == Affix.AffixId; }))
                {
                    ++SpecialCount;
                    if (!Special->PairedAffixId.IsNone())
                    {
                        const FBreakerRolledAffix* Bill = Item.Affixes.FindByPredicate([&](const FBreakerRolledAffix& Candidate) { return Candidate.AffixId == Special->PairedAffixId; });
                        if (!TestNotNull(TEXT("Special payoff never loses its paired bill"), Bill)) return false;
                        const FBreakerAffixDefinition* Definition = Bills.FindByPredicate([&](const FBreakerAffixDefinition& Entry) { return Entry.AffixId == Bill->AffixId; });
                        if (!TestNotNull(TEXT("Bill resolves to authored downside"), Definition)) return false;
                        TestEqual(TEXT("Bill keeps its full authored penalty"), Bill->Value, UBreakerAffixLibrary::ValueForTier(*Definition, Bill->Tier));
                    }
                }
            }
            if (Rarity == EBreakerItemRarity::Aberrant)
            {
                TestTrue(TEXT("Aberrant keeps one or two special payoffs"), SpecialCount >= 1 && SpecialCount <= 2);
                TestNotNull(TEXT("Focused seat remains an ordinary affix"), Generic.FindByPredicate([&](const FBreakerAffixDefinition& Definition) { return Definition.AffixId == Item.Affixes[0].AffixId; }));
                TestTrue(TEXT("Focused seat keeps its earned tier floor"), Item.Affixes[0].Tier <= FMath::Max(
                    UBreakerAffixLibrary::BestTierForItemLevel(Level), UBreakerAffixLibrary::TierCapForRarity(Rarity)));
            }
            else
            {
                TestEqual(TEXT("Unwritten retains exactly one special payoff"), SpecialCount, 1);
                TestTrue(TEXT("Unwritten still carries its rule"), Item.HasRule());
            }
            if (Item.IsLegendary())
            {
                ReachableLegendaries.Add(Item.LegendaryId);
                const FBreakerLegendaryDefinition Definition = UBreakerItemRuleLibrary::FindLegendary(Item.LegendaryId);
                TestTrue(TEXT("Named legendary retains its rule"), Item.Rule == Definition.Rule);
                for (FName Signature : Definition.GuaranteedAffixIds)
                    TestTrue(TEXT("Every named signature fits within the final budget"), Seen.Contains(Signature));
            }
            const FBreakerItemInstance Repeat = UBreakerLootLibrary::RollItem(TEXT("Allocation"), Slot, Rarity, Level, Seed);
            TestEqual(TEXT("Seed repeats final count"), Repeat.Affixes.Num(), Item.Affixes.Num());
            for (int32 Index = 0; Index < FMath::Min(Item.Affixes.Num(), Repeat.Affixes.Num()); ++Index)
            {
                TestEqual(TEXT("Seed repeats identity"), Repeat.Affixes[Index].AffixId, Item.Affixes[Index].AffixId);
                TestEqual(TEXT("Seed repeats tier"), Repeat.Affixes[Index].Tier, Item.Affixes[Index].Tier);
                TestEqual(TEXT("Seed repeats value"), Repeat.Affixes[Index].Value, Item.Affixes[Index].Value);
            }
        }
    }
    TestEqual(TEXT("All four named legendaries remain reachable through normal rolls"), ReachableLegendaries.Num(), 4);
    for (const TCHAR* Id : {TEXT("Legendary.Deadfall"), TEXT("Legendary.Cadence"), TEXT("Legendary.Overrun"), TEXT("Legendary.Refractor")})
        TestTrue(TEXT("Each authored identity remains reachable"), ReachableLegendaries.Contains(FName(Id)));
    return true;
}
#endif
