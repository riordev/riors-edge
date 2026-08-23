#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemTypes.h"
#include "Items/BreakerLootPickup.h"
#include "UI/BreakerUIStyle.h"

namespace
{
    FBreakerItemInstance MakeTestItem(EBreakerItemRarity Rarity, EBreakerEquipSlot Slot)
    {
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = TEXT("TestDrop");
        Item.Rarity = Rarity;
        Item.Slot = Slot;
        Item.ItemLevel = 12;
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLootPickupHoldsItemTest,
    "RiorsEdge.Items.LootPickup.HoldsItem",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLootPickupHoldsItemTest::RunTest(const FString& Parameters)
{
    ABreakerLootPickup* Pickup = NewObject<ABreakerLootPickup>();
    TestNotNull(TEXT("Pickup constructs"), Pickup);
    if (!Pickup) return false;

    TestFalse(TEXT("A fresh pickup holds no valid item"), Pickup->GetItem().IsValid());

    const FBreakerItemInstance Item = MakeTestItem(EBreakerItemRarity::Aberrant, EBreakerEquipSlot::Boots);
    Pickup->SetItem(Item);
    TestTrue(TEXT("SetItem stores the item id"), Pickup->GetItem().ItemId == Item.ItemId);
    TestEqual(TEXT("SetItem stores the rarity"), static_cast<int32>(Pickup->GetItem().Rarity), static_cast<int32>(EBreakerItemRarity::Aberrant));
    TestEqual(TEXT("SetItem stores the slot"), static_cast<int32>(Pickup->GetItem().Slot), static_cast<int32>(EBreakerEquipSlot::Boots));
    TestTrue(TEXT("Pickup lives for five minutes"), FMath::IsNearlyEqual(Pickup->InitialLifeSpan, 300.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLootPickupLabelTest,
    "RiorsEdge.Items.LootPickup.DisplayLabel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLootPickupLabelTest::RunTest(const FString& Parameters)
{
    ABreakerLootPickup* Pickup = NewObject<ABreakerLootPickup>();
    if (!Pickup) return false;

    Pickup->SetItem(MakeTestItem(EBreakerItemRarity::Exceptional, EBreakerEquipSlot::BodyArmour));
    TestEqual(TEXT("Label is RARITY SLOTNAME"), Pickup->GetDisplayLabel().ToString(), FString(TEXT("EXCEPTIONAL BODYARMOUR")));

    Pickup->SetItem(MakeTestItem(EBreakerItemRarity::Anomalous, EBreakerEquipSlot::Primary));
    TestEqual(TEXT("Label tracks the current item"), Pickup->GetDisplayLabel().ToString(), FString(TEXT("ANOMALOUS PRIMARY")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLootPickupRarityColorTest,
    "RiorsEdge.Items.LootPickup.RarityColor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLootPickupRarityColorTest::RunTest(const FString& Parameters)
{
    // THIS TEST SAID THE RIGHT THING AND CHECKED THE WRONG ONE. Its stated
    // claim -- "beam chroma must match the UI rarity colours exactly" -- was
    // correct and was never tested: it compared against four TRANSCRIBED
    // literals which were themselves the bug, each the matching BreakerUI
    // token's sRGB triple passed to a linear constructor. So it was green while
    // asserting the opposite of the truth, for exactly as long as it existed.
    // A test that copies a value cannot notice that the value is wrong.
    //
    // It now reads the token instead of restating it. That is tautological
    // TODAY, because ColorForRarity delegates -- and it is still the invariant
    // worth holding, because it is what fails the moment someone re-adds a
    // local ramp. The second assertion is the one with teeth: the beam must not
    // be the token's sRGB triple misread as linear, which is the specific
    // mistake that was made here, at BreakerGameMode's pylon light, and in
    // bSaturatedTeal's thresholds. The misread is DERIVED from the token rather
    // than typed, so it cannot drift out of agreement with it either.
    for (const EBreakerItemRarity Rarity : { EBreakerItemRarity::Standard, EBreakerItemRarity::Uncommon,
                                             EBreakerItemRarity::Exceptional, EBreakerItemRarity::Aberrant,
                                             EBreakerItemRarity::Anomalous })
    {
        const FLinearColor Token = BreakerUI::RarityColor(Rarity);
        const FLinearColor Beam = ABreakerLootPickup::ColorForRarity(Rarity);
        TestTrue(*FString::Printf(TEXT("Rarity %d: the beam is the UI token"), static_cast<int32>(Rarity)),
            Beam.Equals(Token));

        const FColor Srgb = Token.ToFColor(true);
        const FLinearColor Misread(Srgb.R / 255.0f, Srgb.G / 255.0f, Srgb.B / 255.0f);
        TestFalse(*FString::Printf(TEXT("Rarity %d: the beam is not the sRGB triple read as linear"),
            static_cast<int32>(Rarity)), Beam.Equals(Misread));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLootPickupTransferTest,
    "RiorsEdge.Items.LootPickup.TransferToBackpack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLootPickupTransferTest::RunTest(const FString& Parameters)
{
    // TryPickup itself needs a live actor (authority + Destroy), which a
    // world-free test cannot provide, so this covers the payload it moves:
    // the exact instance the pickup holds lands in the backpack.
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>();
    ABreakerLootPickup* Pickup = NewObject<ABreakerLootPickup>();
    if (!Equipment || !Pickup) return false;

    const FBreakerItemInstance Item = MakeTestItem(EBreakerItemRarity::Uncommon, EBreakerEquipSlot::Helmet);
    Pickup->SetItem(Item);

    TestEqual(TEXT("Backpack starts empty"), Equipment->GetBackpack().Num(), 0);
    TestTrue(TEXT("The pickup carries a backpack-worthy item"), Pickup->GetItem().IsValid());
    TestTrue(TEXT("The carried item is the one that was dropped"), Pickup->GetItem().ItemId == Item.ItemId);

    // The same authority gate TryPickup relies on: an ownerless component
    // refuses the add, so nothing sneaks into the backpack off-server.
    Equipment->AddToBackpack(Pickup->GetItem());
    TestEqual(TEXT("Ownerless equipment refuses the add"), Equipment->GetBackpack().Num(), 0);

    TestFalse(TEXT("TryPickup rejects a null character"), Pickup->TryPickup(nullptr));
    return true;
}

#endif
