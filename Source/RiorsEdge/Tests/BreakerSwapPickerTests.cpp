#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemTypes.h"
#include "Items/BreakerLootLibrary.h"
#include "UI/BreakerSwapPickerLayout.h"

// ---------------------------------------------------------------------------
// THE EQUIP-LIMIT SWAP PICKER (O205) — the rule, exercised without a screen.
//
// At the equip limit the equipment component supplies the swap candidates
// and the pre-focused one, and validates the choice; the picker offers and
// never computes the list. Everything the picker can get WRONG rather than
// merely ugly is therefore on the component and in BreakerSwapPickerLayout,
// and is asserted here with no widget, no world and no Slate application:
//   - the list is the cap axis minus the in-slot piece and the rule-displaced
//     piece, lowest item level first, and its head IS the preview's victim,
//   - the list is empty wherever the cap does not bite,
//   - a chosen victim off the list is refused and nothing moves,
//   - the rule's own choice is unchanged for every caller of EquipItem,
//   - the shipped caps produce exactly EquipLimitForRarity rows,
//   - the picker opens on exactly the card's limit tell,
//   - the row label the rule can produce is always TAKE OFF.
//
// NOT COVERED, stated plainly: whether any of it is on screen (a live Slate
// application), and the sheet's slide, which is recorded in the layout header
// and not driven.
//
// The fixture is the LimitDisplacement test's own shape: a bare instance with
// an id, a slot, a rarity and an item level. Nothing here grants a piece the
// game would not — the cap fixtures are three Aberrant and one Unwritten, and
// the one legendary is rolled through the library the drop path uses.
// ---------------------------------------------------------------------------

namespace
{
    FBreakerItemInstance BreakerSwapTestItem(EBreakerEquipSlot Slot, EBreakerItemRarity Rarity, int32 ItemLevel)
    {
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.Slot = Slot;
        Item.Rarity = Rarity;
        Item.ItemLevel = ItemLevel;
        return Item;
    }

    bool BreakerSwapHolds(const TArray<FBreakerItemInstance>& Container, const FGuid& ItemId)
    {
        return Container.ContainsByPredicate([&ItemId](const FBreakerItemInstance& Item) { return Item.ItemId == ItemId; });
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSwapPickerTest,
    "RiorsEdge.UI.EquipLimit.SwapPicker",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSwapPickerTest::RunTest(const FString& Parameters)
{
    using namespace BreakerSwapPickerLayout;

    // ---- The list: the cap axis, weakest first, head is the victim --------
    const FBreakerItemInstance Helmet = BreakerSwapTestItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Aberrant, 40);
    const FBreakerItemInstance Gloves = BreakerSwapTestItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Aberrant, 20);
    const FBreakerItemInstance Boots = BreakerSwapTestItem(EBreakerEquipSlot::Boots, EBreakerItemRarity::Aberrant, 30);
    const TArray<FBreakerItemInstance> Loadout = {Helmet, Gloves, Boots};
    const FBreakerItemInstance Waist = BreakerSwapTestItem(EBreakerEquipSlot::Waist, EBreakerItemRarity::Aberrant, 50);

    const TArray<FBreakerItemInstance> Candidates = UBreakerEquipmentComponent::SwapCandidatesAgainst(Loadout, Waist);
    TestEqual(TEXT("A fourth Aberrant offers the three worn Aberrant"), Candidates.Num(), 3);
    if (Candidates.Num() == 3)
    {
        TestEqual(TEXT("Row 0 is the ilvl 20 gloves"), Candidates[0].ItemId.ToString(), Gloves.ItemId.ToString());
        TestEqual(TEXT("Row 1 is the ilvl 30 boots"), Candidates[1].ItemId.ToString(), Boots.ItemId.ToString());
        TestEqual(TEXT("Row 2 is the ilvl 40 helmet"), Candidates[2].ItemId.ToString(), Helmet.ItemId.ToString());
        const FBreakerEquipPreview Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(Loadout, Waist);
        TestEqual(TEXT("The head of the list is the preview's own victim"),
            Candidates[0].ItemId.ToString(), Preview.LimitDisplaced.ItemId.ToString());
    }

    // ---- Empty wherever the cap does not bite ----------------------------
    const FBreakerItemInstance BetterGloves = BreakerSwapTestItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Aberrant, 45);
    TestEqual(TEXT("Aberrant into an occupied Aberrant slot offers nothing: the swap frees its own place"),
        UBreakerEquipmentComponent::SwapCandidatesAgainst(Loadout, BetterGloves).Num(), 0);
    const FBreakerItemInstance ExceptionalWaist = BreakerSwapTestItem(EBreakerEquipSlot::Waist, EBreakerItemRarity::Exceptional, 50);
    TestEqual(TEXT("An uncapped rarity offers nothing"),
        UBreakerEquipmentComponent::SwapCandidatesAgainst(Loadout, ExceptionalWaist).Num(), 0);

    // ---- The rule-displaced piece is excluded ----------------------------
    // Cadence occupies both hands, so a worn Secondary is the slot rule's
    // victim and never the cap's. The legendary axis caps at one, so the only
    // piece that can share Cadence's axis AND be rule-displaced is a worn
    // legendary Secondary — and once the rule takes it the cap has nothing
    // left to bite: no list, no picker. Rolled through the drop library, so
    // the fixture carries exactly what a drop carries.
    const FBreakerItemInstance Cadence = UBreakerLootLibrary::RollLegendary(TEXT("Legendary.Cadence"), 50, 77);
    TestTrue(TEXT("Cadence rolls as a legendary"), Cadence.IsLegendary() && Cadence.IsValid());
    FBreakerItemInstance WornLegendarySecondary = BreakerSwapTestItem(EBreakerEquipSlot::Secondary, EBreakerItemRarity::Unwritten, 30);
    WornLegendarySecondary.LegendaryId = TEXT("Legendary.Test");
    const FBreakerEquipPreview CadencePreview = UBreakerEquipmentComponent::PreviewEquipAgainst({WornLegendarySecondary}, Cadence);
    TestTrue(TEXT("The slot rule names the Secondary"), CadencePreview.bRuleDisplaces);
    TestFalse(TEXT("...and the rule-displaced piece is not also the cap's victim"), CadencePreview.bExceedsRarityLimit);
    TestEqual(TEXT("The rule-displaced piece is never offered as a swap"),
        UBreakerEquipmentComponent::SwapCandidatesAgainst({WornLegendarySecondary}, Cadence).Num(), 0);
    TestFalse(TEXT("...so no picker opens over a rule displacement"), ShouldOpenSwapPicker(CadencePreview));

    // ---- The component: a chosen victim ----------------------------------
    {
        UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
        for (const FBreakerItemInstance& Item : Loadout) Equipment->EquipItem(Item);
        TestTrue(TEXT("The helmet is a valid swap choice"), Equipment->IsValidSwapChoice(Waist, Helmet.ItemId));
        TestFalse(TEXT("An unknown id is not"), Equipment->IsValidSwapChoice(Waist, FGuid::NewGuid()));
        TestFalse(TEXT("An invalid id is not"), Equipment->IsValidSwapChoice(Waist, FGuid()));

        TestTrue(TEXT("Equipping over the cap with the helmet chosen succeeds"), Equipment->EquipItemDisplacing(Waist, Helmet.ItemId));
        TestTrue(TEXT("The helmet is in the backpack"), BreakerSwapHolds(Equipment->GetBackpack(), Helmet.ItemId));
        TestTrue(TEXT("The ilvl 20 gloves are still worn"), BreakerSwapHolds(Equipment->GetEquipped(), Gloves.ItemId));
        TestTrue(TEXT("The waist is worn"), BreakerSwapHolds(Equipment->GetEquipped(), Waist.ItemId));
        TestEqual(TEXT("The count stays at the cap"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Aberrant), 3);
    }

    // ---- The component: a victim off the list is refused, nothing moves --
    {
        UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
        for (const FBreakerItemInstance& Item : Loadout) Equipment->EquipItem(Item);
        TestFalse(TEXT("An id off the list is refused"), Equipment->EquipItemDisplacing(Waist, FGuid::NewGuid()));
        TestEqual(TEXT("Nothing left the loadout"), Equipment->GetEquipped().Num(), 3);
        TestEqual(TEXT("Nothing entered the backpack"), Equipment->GetBackpack().Num(), 0);
        TestFalse(TEXT("The waist is not worn"), BreakerSwapHolds(Equipment->GetEquipped(), Waist.ItemId));

        // The rule's own choice is unchanged for every EquipItem caller.
        TestTrue(TEXT("EquipItem still takes the rule's choice"), Equipment->EquipItem(Waist));
        TestTrue(TEXT("...which ejects the ilvl 20 gloves"), BreakerSwapHolds(Equipment->GetBackpack(), Gloves.ItemId));
        TestTrue(TEXT("...and keeps the helmet"), BreakerSwapHolds(Equipment->GetEquipped(), Helmet.ItemId));
        TestEqual(TEXT("The count stays at the cap"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Aberrant), 3);
    }

    // ---- Shipped configuration: the caps are the row counts --------------
    {
        UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
        for (const FBreakerItemInstance& Item : Loadout) Equipment->EquipItem(Item);
        const FBreakerItemInstance Necklace = BreakerSwapTestItem(EBreakerEquipSlot::Necklace, EBreakerItemRarity::Unwritten, 10);
        Equipment->EquipItem(Necklace);

        const int32 AberrantLimit = UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Aberrant);
        const int32 UnwrittenLimit = UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Unwritten);
        TestEqual(TEXT("Aberrant caps at three"), AberrantLimit, 3);
        TestEqual(TEXT("Unwritten caps at one"), UnwrittenLimit, 1);
        TestEqual(TEXT("At the shipped Aberrant cap the picker has exactly the cap's rows"),
            Equipment->SwapCandidates(Waist).Num(), AberrantLimit);
        const FBreakerItemInstance UnwrittenHelmet = BreakerSwapTestItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Unwritten, 50);
        TestEqual(TEXT("At the shipped Unwritten cap the picker has exactly the cap's rows"),
            Equipment->SwapCandidates(UnwrittenHelmet).Num(), UnwrittenLimit);

        // The picker opens on exactly the card's limit tell.
        for (const FBreakerItemInstance& Incoming : {Waist, UnwrittenHelmet, BetterGloves, ExceptionalWaist})
        {
            const FBreakerEquipPreview Preview = Equipment->PreviewEquip(Incoming);
            TestEqual(TEXT("ShouldOpenSwapPicker is the limit tell"),
                ShouldOpenSwapPicker(Preview), BreakerInventoryLayout::ShouldShowLimitTell(Preview));
            TestEqual(TEXT("...and the tell is the list being non-empty"),
                ShouldOpenSwapPicker(Preview), Equipment->SwapCandidates(Incoming).Num() > 0);
        }

        // Every row the rule can produce reads TAKE OFF: the in-slot piece
        // is never a cap victim, so no row shares the incoming slot.
        for (const FBreakerItemInstance& Incoming : {Waist, UnwrittenHelmet})
        {
            for (const FBreakerItemInstance& Row : Equipment->SwapCandidates(Incoming))
            {
                TestNotEqual(TEXT("No offered row shares the incoming slot"),
                    static_cast<int32>(Row.Slot), static_cast<int32>(Incoming.Slot));
                TestEqual(TEXT("The row label is TAKE OFF"), FString(RowButtonLabel(Row, Incoming)), FString(TEXT("TAKE OFF")));
            }
        }
    }

    // ---- The text and the numbers ----------------------------------------
    TestEqual(TEXT("The title"), Title(EBreakerItemRarity::Aberrant, 3, 3), FString(TEXT("ABERRANT · 3 OF 3 WORN")));
    TestEqual(TEXT("The title prints the O50 display name"), Title(EBreakerItemRarity::Unwritten, 1, 1), FString(TEXT("UNWRITTEN · 1 OF 1 WORN")));
    TestEqual(TEXT("The incoming line"), IncomingLine(EBreakerEquipSlot::Waist), FString(TEXT("INCOMING · WAIST")));
    TestEqual(TEXT("KEEP CURRENT"), FString(KeepCurrentLabel()), FString(TEXT("KEEP CURRENT")));
    TestEqual(TEXT("A row in the incoming slot would read SWAP"), FString(RowButtonLabel(Gloves, BetterGloves)), FString(TEXT("SWAP")));
    TestEqual(TEXT("The modal is 640 wide"), ModalWidth, 640.0f);
    TestEqual(TEXT("A row is 64 tall"), RowHeight, 64.0f);
    return true;
}

#endif
