#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Interaction/BreakerStashPoint.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemTypes.h"
#include "Save/BreakerAccountSave.h"
#include "UI/BreakerStashLayout.h"

// ---------------------------------------------------------------------------
// STASH SCREEN — the screen's rules, exercised without a screen.
//
// UI/BreakerStashScreen.cpp's BuildStashScreen is a view over the account's
// stash and the character's backpack. Everything in it that can be WRONG
// rather than merely ugly lives in BreakerStashLayout, and is asserted here
// with no widget, no world and no Slate application:
//   - the tab strip's three labels, and that no MATERIALS tab exists,
//   - that the two grids ARE the two caps (10x7 = 70, 5x5 = 25),
//   - the counter text,
//   - that WEAPONS / ARMOUR partition the eight slots,
//   - that a claim-marked stash copy is not drawn,
//   - that TAKE TO BACKPACK is painted at the cap,
//   - that the stash point offers nowhere and refuses every id.
//
// NOT COVERED, stated plainly: whether any of it is on screen (a live Slate
// application), and whether a deposit or withdrawal is LEGAL — that is
// UBreakerEquipmentComponent's rule with its own suite, and this screen only
// echoes its answer.
// ---------------------------------------------------------------------------

namespace
{
    FBreakerItemInstance BreakerStashTestItem(EBreakerEquipSlot Slot)
    {
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.Slot = Slot;
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStashScreenTest,
    "RiorsEdge.UI.Stash.Screen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStashScreenTest::RunTest(const FString& Parameters)
{
    using namespace BreakerStashLayout;

    // ---- The tab strip: ALL / WEAPONS / ARMOUR, and nothing else ----------
    const TArray<FString>& Labels = TabLabels();
    TestEqual(TEXT("Three tabs"), Labels.Num(), 3);
    if (Labels.Num() == 3)
    {
        TestEqual(TEXT("Tab 0 is ALL"), Labels[0], FString(TEXT("ALL")));
        TestEqual(TEXT("Tab 1 is WEAPONS"), Labels[1], FString(TEXT("WEAPONS")));
        TestEqual(TEXT("Tab 2 is ARMOUR"), Labels[2], FString(TEXT("ARMOUR")));
    }
    // No material item exists; a MATERIALS tab would be content for plumbing
    // that does not exist.
    TestFalse(TEXT("No MATERIALS tab"), Labels.Contains(FString(TEXT("MATERIALS"))));

    // ---- The grids ARE the caps -----------------------------------------
    TestEqual(TEXT("The stash grid holds exactly the stash cap"),
        StashColumns * StashRows, UBreakerAccountSave::StashCapacity);
    TestEqual(TEXT("The stash cap is 70"), UBreakerAccountSave::StashCapacity, 70);
    TestEqual(TEXT("The backpack grid holds exactly the backpack cap"),
        BackpackColumns * BackpackRows, UBreakerEquipmentComponent::BackpackCapacity);
    TestEqual(TEXT("The backpack cap is 25"), UBreakerEquipmentComponent::BackpackCapacity, 25);

    // ---- The counters -----------------------------------------------------
    TestEqual(TEXT("47 stashed reads 47 / 70"), StashCounter(47), FString(TEXT("47 / 70")));
    TestEqual(TEXT("12 carried reads 12 / 25"), BackpackCounter(12), FString(TEXT("12 / 25")));

    // ---- WEAPONS / ARMOUR partition the slots ---------------------------
    for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
    {
        const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
        const bool bWeapons = PassesTab(Slot, EStashTab::Weapons);
        const bool bArmour = PassesTab(Slot, EStashTab::Armour);
        TestTrue(*FString::Printf(TEXT("Slot %d passes exactly one of WEAPONS / ARMOUR"), SlotIndex), bWeapons != bArmour);
        TestTrue(*FString::Printf(TEXT("Slot %d passes ALL"), SlotIndex), PassesTab(Slot, EStashTab::All));
    }
    // The stated placement: a necklace is under ARMOUR, not a tab of its own.
    TestTrue(TEXT("A Necklace lands in ARMOUR"), PassesTab(EBreakerEquipSlot::Necklace, EStashTab::Armour));
    TestTrue(TEXT("A Primary lands in WEAPONS"), PassesTab(EBreakerEquipSlot::Primary, EStashTab::Weapons));

    // ---- A claim-marked copy is not drawn; the rest sort by wear order ---
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    const FBreakerItemInstance Boots = BreakerStashTestItem(EBreakerEquipSlot::Boots);
    const FBreakerItemInstance Helmet = BreakerStashTestItem(EBreakerEquipSlot::Helmet);
    const FBreakerItemInstance Claimed = BreakerStashTestItem(EBreakerEquipSlot::Gloves);
    Account->StashItems = { Boots, Helmet, Claimed };
    Account->PendingWithdrawals.Add(Claimed.ItemId);

    const TArray<FBreakerItemInstance> Visible = VisibleStashItems(*Account);
    TestEqual(TEXT("The claim-marked copy is dropped from the grid"), Visible.Num(), 2);
    for (const FBreakerItemInstance& Item : Visible)
    {
        TestNotEqual(TEXT("No visible cell carries the claimed guid"), Item.ItemId, Claimed.ItemId);
    }
    if (Visible.Num() == 2)
    {
        TestEqual(TEXT("Helmet sorts before Boots (wear order, slot only)"), Visible[0].ItemId, Helmet.ItemId);
        TestEqual(TEXT("...and Boots follows"), Visible[1].ItemId, Boots.ItemId);
    }

    // ---- TAKE TO BACKPACK is painted at the cap ----------------------------
    TestFalse(TEXT("At 25 / 25 nothing can be taken"), CanTakeToBackpack(25));
    TestTrue(TEXT("At 24 / 25 one more can"), CanTakeToBackpack(24));
    TestFalse(TEXT("At 70 / 70 nothing can be moved in"), CanMoveToStash(70));

    // ---- The refusal lines follow the component's own order ---------------
    TestEqual(TEXT("A deposit outside the Anchor says so"),
        DepositRefusal(false, 0), FString(TEXT("THE STASH IS IN THE ANCHOR.")));
    TestTrue(TEXT("A deposit at the cap says so"), DepositRefusal(true, 70).Contains(TEXT("FULL")));
    TestTrue(TEXT("A legal deposit predicts no refusal"), DepositRefusal(true, 69).IsEmpty());
    TestTrue(TEXT("A claimed withdrawal says so"), WithdrawRefusal(true, true, 1, 50, 0).Contains(TEXT("WAY OUT")));
    TestTrue(TEXT("An over-level withdrawal names the level (O182)"), WithdrawRefusal(true, false, 40, 1, 0).Contains(TEXT("LEVEL")));
    TestTrue(TEXT("A withdrawal into a full backpack says so"), WithdrawRefusal(true, false, 1, 50, 25).Contains(TEXT("FULL")));
    TestTrue(TEXT("A legal withdrawal predicts no refusal"), WithdrawRefusal(true, false, 1, 50, 0).IsEmpty());

    // ---- The stash point goes nowhere ------------------------------------
    // NewObject, never a world spawn, as BreakerTravelPickerTests does: the
    // two overrides read nothing that needs a level.
    ABreakerStashPoint* Point = NewObject<ABreakerStashPoint>();
    TestEqual(TEXT("A stash point offers zero destinations"), Point->GetAvailableDestinations().Num(), 0);
    TestFalse(TEXT("A stash point refuses the gym"), Point->SelectDestination(ABreakerTravelPoint::GymDestinationId, nullptr));
    TestFalse(TEXT("A stash point refuses the hub"), Point->SelectDestination(ABreakerTravelPoint::HubDestinationId, nullptr));
    TestFalse(TEXT("A stash point refuses the rift"), Point->SelectDestination(ABreakerTravelPoint::RiftDestinationId, nullptr));
    // The one new player-facing word (O195): the F prompt.
    TestEqual(TEXT("The prompt is Stash"), Point->GetPromptLabel().ToString(), FString(TEXT("Stash")));
    // It never prints its noun twice: the HUD draws the noun line only when
    // it differs from the prompt, and this point leaves GetDisplayName alone.
    TestEqual(TEXT("The display name is the prompt, so the HUD draws one line"),
        Point->GetDisplayName().ToString(), Point->GetPromptLabel().ToString());
    return true;
}

#endif
