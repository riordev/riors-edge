#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerForgeLibrary.h"
#include "Items/BreakerItemRules.h"
#include "Items/BreakerLootLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Save/BreakerSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    TSet<FName> BreakerForgeLockedIds(const FBreakerItemInstance& Item)
    {
        TSet<FName> Locked;
        if (Item.IsLegendary())
            for (FName Id : UBreakerItemRuleLibrary::FindLegendary(Item.LegendaryId).GuaranteedAffixIds) Locked.Add(Id);
        for (const auto& Line : Item.Affixes)
        {
            const auto* Definition = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), Line.AffixId);
            if (!Definition || !Definition->IsSpecial()) continue;
            Locked.Add(Line.AffixId);
            if (!Definition->PairedAffixId.IsNone()) Locked.Add(Definition->PairedAffixId);
        }
        return Locked;
    }

    FBreakerItemInstance BreakerForgeHistoricalOrder(FBreakerItemInstance Item)
    {
        // Historical RollLegendary appended its signatures after ordinary
        // affixes. Change only stored order, retaining a real roll's values.
        const TSet<FName> Locked = BreakerForgeLockedIds(Item);
        TArray<FBreakerRolledAffix> Ordered;
        for (const auto& Line : Item.Affixes) if (!Locked.Contains(Line.AffixId)) Ordered.Add(Line);
        for (const auto& Line : Item.Affixes) if (Locked.Contains(Line.AffixId)) Ordered.Add(Line);
        Item.Affixes = MoveTemp(Ordered);
        return Item;
    }

    bool BreakerForgeCheckPreserved(FAutomationTestBase& Test, const FBreakerItemInstance& Before, const FBreakerItemInstance& After)
    {
        bool bValid = Test.TestTrue(TEXT("O248 preserves the stored item identity"), Before.ItemId == After.ItemId);
        bValid &= Test.TestEqual(TEXT("O248 preserves the authored legendary identity"), After.LegendaryId, Before.LegendaryId);
        bValid &= Test.TestEqual(TEXT("O248 preserves the rule"), static_cast<uint8>(After.Rule), static_cast<uint8>(Before.Rule));
        bValid &= Test.TestEqual(TEXT("O248 preserves every over-budget affix"), After.Affixes.Num(), Before.Affixes.Num());
        if (After.Affixes.Num() != Before.Affixes.Num()) return false;
        for (int32 Index = 0; Index < Before.Affixes.Num(); ++Index)
        {
            const auto& A = Before.Affixes[Index]; const auto& B = After.Affixes[Index];
            bValid &= Test.TestEqual(TEXT("Stored affix identity and order survive"), B.AffixId, A.AffixId);
            bValid &= Test.TestEqual(TEXT("Stored affix tier survives"), B.Tier, A.Tier);
            bValid &= Test.TestEqual(TEXT("Stored affix value survives"), B.Value, A.Value);
            bValid &= Test.TestEqual(TEXT("Stored affix category survives"), static_cast<uint8>(B.Category), static_cast<uint8>(A.Category));
        }
        return bValid;
    }

    bool BreakerForgeCheckAttuned(FAutomationTestBase& Test, const FBreakerItemInstance& Before, const FBreakerItemInstance& After)
    {
        bool bValid = Test.TestEqual(TEXT("Attune keeps every stored position"), After.Affixes.Num(), Before.Affixes.Num());
        if (!bValid) return false;
        const TSet<FName> Locked = BreakerForgeLockedIds(Before);
        TSet<FName> Seen;
        for (int32 Index = 0; Index < Before.Affixes.Num(); ++Index)
        {
            const auto& Old = Before.Affixes[Index]; const auto& Line = After.Affixes[Index];
            bValid &= Test.TestFalse(TEXT("Attune cannot draw a later locked ID twice"), Seen.Contains(Line.AffixId));
            Seen.Add(Line.AffixId);
            bValid &= Test.TestEqual(TEXT("Tier remains at its original position"), Line.Tier, Old.Tier);
            if (Locked.Contains(Old.AffixId))
            {
                bValid &= Test.TestEqual(TEXT("Locked signature or paired bill retains its position"), Line.AffixId, Old.AffixId);
                bValid &= Test.TestEqual(TEXT("Locked value is unchanged"), Line.Value, Old.Value);
                bValid &= Test.TestEqual(TEXT("Locked category is unchanged"), static_cast<uint8>(Line.Category), static_cast<uint8>(Old.Category));
            }
        }
        if (Before.Affixes.Num() <= 8)
        {
            bValid &= Test.TestTrue(TEXT("Reserved locked prefixes cannot overflow the category cap"), UBreakerLootLibrary::CountAffixesOfCategory(After, EBreakerAffixCategory::Prefix) <= 4);
            bValid &= Test.TestTrue(TEXT("Reserved locked suffixes cannot overflow the category cap"), UBreakerLootLibrary::CountAffixesOfCategory(After, EBreakerAffixCategory::Suffix) <= 4);
        }
        bValid &= Test.TestEqual(TEXT("Rule survives Attune"), static_cast<uint8>(After.Rule), static_cast<uint8>(Before.Rule));
        return bValid;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerForgeLockedAffixRuntimeTest, "RiorsEdge.Items.Forge.LockedAffixOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerForgeLockedAffixRuntimeTest::RunTest(const FString& Parameters)
{
    auto* Save = NewObject<UBreakerSaveGame>();
    Save->SaveVersion = 9; // Historical payload, before the O258 migration.
    Save->CoreLayoutVersion = UBreakerSaveGame::ActiveCoreLayoutVersion;
    FBreakerItemInstance Legendary;
    for (int32 Seed = 1; Seed <= 100; ++Seed)
    {
        Legendary = BreakerForgeHistoricalOrder(UBreakerLootLibrary::RollLegendary(TEXT("Legendary.Deadfall"), 50, Seed));
        if (Legendary.IsValid() && !Legendary.Affixes.IsEmpty() && !BreakerForgeLockedIds(Legendary).Contains(Legendary.Affixes[0].AffixId)) break;
    }
    if (!TestTrue(TEXT("Real legendary has a mutable line before its historical appended signature"),
        Legendary.IsValid() && !Legendary.Affixes.IsEmpty() && !BreakerForgeLockedIds(Legendary).Contains(Legendary.Affixes[0].AffixId))) return false;
    Save->BackpackItems.Add(Legendary);
    // Reconstruct the old append-past-budget shape from real rolled lines.
    // This verifies preservation, not a policy to normalize legacy inventory.
    auto OverBudget = Legendary;
    OverBudget.ItemId = FGuid::NewGuid(); // Distinct historical item, not a duplicate inventory identity.
    for (int32 Seed = 1; OverBudget.Affixes.Num() < 9 && Seed <= 100; ++Seed)
    {
        const auto Donor = UBreakerLootLibrary::RollItem(TEXT("Test.Forge.LegacyLines"), EBreakerEquipSlot::Boots, EBreakerItemRarity::Exceptional, 50, Seed);
        for (const auto& Line : Donor.Affixes)
        {
            if (OverBudget.Affixes.Num() >= 9) break;
            if (!OverBudget.Affixes.ContainsByPredicate([&](const FBreakerRolledAffix& Existing) { return Existing.AffixId == Line.AffixId; })) OverBudget.Affixes.Add(Line);
        }
    }
    if (!TestEqual(TEXT("Historical overflow fixture exceeds eight seats without duplicate IDs"), OverBudget.Affixes.Num(), 9)) return false;
    Save->BackpackItems.Add(BreakerForgeHistoricalOrder(OverBudget));
    auto EquippedOverBudget = Save->BackpackItems.Last();
    EquippedOverBudget.ItemId = FGuid::NewGuid();
    Save->EquippedItems.Add(EquippedOverBudget);
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Historical-order item serializes through native save archive"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return false;
    auto* Restored = Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestTrue(TEXT("Native save archive restores both historical items"), Restored && Restored->BackpackItems.Num() == 2)) return false;
    if (!TestEqual(TEXT("Archive restores the equipped legacy item"), Restored->EquippedItems.Num(), 1)) return false;
    FString Note;
    if (!TestTrue(TEXT("Historical save migrates without normalizing item budgets"), UBreakerSaveGame::MigrateToCurrent(*Restored, Note))) return false;
    if (!TestTrue(TEXT("Repeated save migration is harmless to owned affixes"), UBreakerSaveGame::MigrateToCurrent(*Restored, Note))) return false;
    for (int32 Index = 0; Index < Save->BackpackItems.Num(); ++Index)
        if (!BreakerForgeCheckPreserved(*this, Save->BackpackItems[Index], Restored->BackpackItems[Index])) return false;
    if (!BreakerForgeCheckPreserved(*this, Save->EquippedItems[0], Restored->EquippedItems[0])) return false;
    Legendary = Restored->BackpackItems[0];
    TArray<FBreakerItemInstance> Cases{Legendary};
    Cases.Add(Restored->BackpackItems[1]);
    Cases.Add(BreakerForgeHistoricalOrder(UBreakerLootLibrary::RollItem(TEXT("Test.Forge.Aberrant"), EBreakerEquipSlot::Boots, EBreakerItemRarity::Aberrant, 50, 22)));
    Cases.Add(UBreakerLootLibrary::RollItem(TEXT("Test.Forge.Ordinary"), EBreakerEquipSlot::Boots, EBreakerItemRarity::Exceptional, 50, 31));
    for (const auto& Original : Cases)
    {
        if (!TestTrue(TEXT("Seed sweep starts with actual valid dropped item"), Original.IsValid())) return false;
        for (int32 Seed = 1; Seed <= 256; ++Seed)
        {
            auto Item = Original;
            FBreakerForgeWallet Wallet;
            const int32 Cost = UBreakerForgeLibrary::AttuneCost(Item).Amount;
            Wallet.Add(Cost);
            if (!TestEqual(TEXT("Attune spends its quoted cost"), static_cast<uint8>(UBreakerForgeLibrary::Attune(Item, Wallet, true, Seed)), static_cast<uint8>(EBreakerForgeResult::Success))) return false;
            if (!BreakerForgeCheckAttuned(*this, Original, Item)) return false;
            TestEqual(TEXT("Exact quoted payment leaves no balance"), Wallet.Get(), 0);
        }
    }

    // The real equipment mutation path pays from salvaged rolled loot and
    // updates the held item, rather than granting a test-only free craft.
    auto* Owner = NewObject<AActor>();
    auto* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    auto* Attributes = NewObject<UBreakerAttributeSet>(Owner);
    Equipment->BindAttributes(Attributes);
    Equipment->RestoreState(Restored->EquippedItems, Restored->BackpackItems);
    if (!TestEqual(TEXT("Native load restores both backpack items"), Equipment->GetBackpack().Num(), 2)) return false;
    if (!TestEqual(TEXT("Native load keeps the equipped over-budget item"), Equipment->GetEquipped().Num(), 1)) return false;
    for (int32 Index = 0; Index < Restored->BackpackItems.Num(); ++Index)
        if (!BreakerForgeCheckPreserved(*this, Restored->BackpackItems[Index], Equipment->GetBackpack()[Index])) return false;
    if (!BreakerForgeCheckPreserved(*this, Restored->EquippedItems[0], Equipment->GetEquipped()[0])) return false;
    // Craft the over-budget historical item through actual equipment payment,
    // not only the pure Forge function exercised above.
    Legendary = Restored->BackpackItems[1];
    const int32 Cost = UBreakerForgeLibrary::AttuneCost(Legendary).Amount;
    for (int32 Seed = 1; Equipment->GetForgeWallet().Get() < Cost && Seed <= 100; ++Seed)
    {
        auto Salvage = UBreakerLootLibrary::RollItem(TEXT("Test.Forge.Salvage"), EBreakerEquipSlot::Gloves, EBreakerItemRarity::Exceptional, 50, Seed);
        if (!Equipment->AddToBackpack(Salvage) || !Equipment->SalvageFromBackpack(Salvage.ItemId)) return false;
    }
    const int32 Balance = Equipment->GetForgeWallet().Get();
    if (!TestTrue(TEXT("Actual salvage funds the craft"), Balance >= Cost)) return false;
    TestEqual(TEXT("Away from Forge refuses without payment"), static_cast<uint8>(Equipment->AttuneItem(Legendary.ItemId, false)), static_cast<uint8>(EBreakerForgeResult::NotAtForge));
    TestEqual(TEXT("Refusal preserves salvage balance"), Equipment->GetForgeWallet().Get(), Balance);
    if (!TestEqual(TEXT("Native held-item Attune succeeds at Forge"), static_cast<uint8>(Equipment->AttuneItem(Legendary.ItemId, true)), static_cast<uint8>(EBreakerForgeResult::Success))) return false;
    TestEqual(TEXT("Held-item craft pays the exact quoted amount"), Equipment->GetForgeWallet().Get(), Balance - Cost);
    const auto* Result = Equipment->GetBackpack().FindByPredicate([&](const FBreakerItemInstance& Item) { return Item.ItemId == Legendary.ItemId; });
    return TestNotNull(TEXT("Paid item remains in backpack"), Result) && BreakerForgeCheckAttuned(*this, Legendary, *Result);
}
#endif
