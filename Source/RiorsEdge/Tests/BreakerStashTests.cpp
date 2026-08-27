#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemRequirements.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerAccountSave.h"

// ---------------------------------------------------------------------------
// THE STASH (Part One-X): a capped transfer point on the account save, with
// the two-file journal as the design under test. Every case runs against an
// injected, never-persisting account — the suite must not touch a real slot.
// The crash windows are the point: each one is simulated by replaying the
// half-written state through RestoreState and asserting the reconcile's
// verdict — never a duplicate, never a loss.
// ---------------------------------------------------------------------------

namespace BreakerStashTest
{
    struct FBreakerStashRig
    {
        UBreakerAccountSave* Account = nullptr;
        AActor* Owner = nullptr;
        UBreakerEquipmentComponent* Equipment = nullptr;
        UBreakerProgressionComponent* Progression = nullptr;
    };

    static FBreakerStashRig BreakerMakeStashRig()
    {
        FBreakerStashRig Rig;
        Rig.Account = NewObject<UBreakerAccountSave>();
        Rig.Account->bNeverPersist = true;
        UBreakerAccountSave::InjectForTesting(Rig.Account);
        Rig.Owner = NewObject<AActor>();
        Rig.Equipment = NewObject<UBreakerEquipmentComponent>(Rig.Owner);
        Rig.Progression = NewObject<UBreakerProgressionComponent>(Rig.Owner);
        Rig.Progression->ChoosePermanentClassById(EBreakerClassId::Swift);
        return Rig;
    }

    static FBreakerItemInstance BreakerMakeStashItem(int32 ItemLevel)
    {
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.Slot = EBreakerEquipSlot::Helmet;
        Item.ItemLevel = ItemLevel;
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStashTransferTest,
    "RiorsEdge.Items.Stash.Transfer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStashTransferTest::RunTest(const FString& Parameters)
{
    using namespace BreakerStashTest;
    FBreakerStashRig Rig = BreakerMakeStashRig();

    const FBreakerItemInstance Item = BreakerMakeStashItem(1);
    Rig.Equipment->AddToBackpack(Item);

    // Anchor-only, the RespecAtForge pattern.
    TestFalse(TEXT("a deposit away from the Anchor refuses"), Rig.Equipment->DepositToStash(Item.ItemId, false));
    TestEqual(TEXT("the refused item stays put"), Rig.Equipment->GetBackpack().Num(), 1);

    // The deposit: one account write carrying item AND journal entry, then
    // the runtime copy drops.
    TestTrue(TEXT("an Anchor deposit succeeds"), Rig.Equipment->DepositToStash(Item.ItemId, true));
    TestEqual(TEXT("the stash holds the item"), Rig.Account->StashItems.Num(), 1);
    TestTrue(TEXT("the journal remembers the deposit"), Rig.Account->PendingRemovals.Contains(Item.ItemId));
    TestEqual(TEXT("the backpack no longer does"), Rig.Equipment->GetBackpack().Num(), 0);

    // Withdrawal at level: claim-marked, copy lands in the backpack, the
    // stash copy STAYS until a restore proves the save.
    TestTrue(TEXT("an at-level withdrawal succeeds"), Rig.Equipment->WithdrawFromStash(Item.ItemId, true));
    TestEqual(TEXT("the stash copy stays until proven"), Rig.Account->StashItems.Num(), 1);
    TestTrue(TEXT("the claim marks it"), Rig.Account->PendingWithdrawals.Contains(Item.ItemId));
    TestEqual(TEXT("the backpack holds the working copy"), Rig.Equipment->GetBackpack().Num(), 1);
    TestFalse(TEXT("a second withdrawal of a claimed item refuses — locked, never duplicated"),
        Rig.Equipment->WithdrawFromStash(Item.ItemId, true));

    // BOTH MARKS STAND (deposited, then withdrawn, no restore between), and
    // without character ids a sighting cannot say whose save it is — so the
    // reconcile consumes conservatively, deposit-mark first: the FIRST
    // sighting could be the depositor's stale save, so that copy drops and
    // the removal mark clears while the claim and the stash copy stand
    // (never a duplicate; the withdrawer at worst withdraws again). The
    // SECOND sighting can only be a save the withdrawal reached: the stash
    // copy retires and the claim clears. The character id in the save
    // payload (the fold's SaveVersion bump) collapses this into one step.
    Rig.Equipment->RestoreState({}, {Item});
    TestEqual(TEXT("the first sighting consumes the deposit mark: the copy drops"), Rig.Equipment->GetBackpack().Num(), 0);
    TestFalse(TEXT("the removal mark cleared"), Rig.Account->PendingRemovals.Contains(Item.ItemId));
    TestEqual(TEXT("the stash copy stands behind the claim"), Rig.Account->StashItems.Num(), 1);
    TestTrue(TEXT("the claim stands"), Rig.Account->PendingWithdrawals.Contains(Item.ItemId));

    Rig.Equipment->RestoreState({}, {Item});
    TestEqual(TEXT("the second sighting proves the withdrawal: the stash copy retires"), Rig.Account->StashItems.Num(), 0);
    TestFalse(TEXT("and clears the claim"), Rig.Account->PendingWithdrawals.Contains(Item.ItemId));

    UBreakerAccountSave::ResetCacheForTesting();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStashCrashWindowsTest,
    "RiorsEdge.Items.Stash.CrashWindows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStashCrashWindowsTest::RunTest(const FString& Parameters)
{
    using namespace BreakerStashTest;
    FBreakerStashRig Rig = BreakerMakeStashRig();

    // THE DEPOSIT CRASH: account written, character save never was. The next
    // restore still carries the item — the reconcile drops that copy, keeps
    // the stash copy, clears the journal. One item, not two.
    const FBreakerItemInstance Item = BreakerMakeStashItem(1);
    Rig.Equipment->AddToBackpack(Item);
    Rig.Equipment->DepositToStash(Item.ItemId, true);
    Rig.Equipment->RestoreState({}, {Item});   // the stale save replays
    TestEqual(TEXT("the stale copy drops — the stash is authoritative"), Rig.Equipment->GetBackpack().Num(), 0);
    TestEqual(TEXT("the stash still holds exactly one"), Rig.Account->StashItems.Num(), 1);
    TestFalse(TEXT("the journal entry cleared"), Rig.Account->PendingRemovals.Contains(Item.ItemId));

    // THE WITHDRAWAL CRASH: claim marked, the character save never wrote. An
    // unrelated restore (no sighting) proves nothing: the claim stands, the
    // item stays locked in the stash — never lost, never duplicated.
    Rig.Equipment->WithdrawFromStash(Item.ItemId, true);
    Rig.Equipment->RestoreState({}, {});   // some restore without the item
    TestEqual(TEXT("an unproven claim keeps the stash copy"), Rig.Account->StashItems.Num(), 1);
    TestTrue(TEXT("and the claim stands"), Rig.Account->PendingWithdrawals.Contains(Item.ItemId));

    UBreakerAccountSave::ResetCacheForTesting();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStashRulesTest,
    "RiorsEdge.Items.Stash.CapAndGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStashRulesTest::RunTest(const FString& Parameters)
{
    using namespace BreakerStashTest;
    FBreakerStashRig Rig = BreakerMakeStashRig();

    // The cap makes putting something in a decision: a full stash refuses.
    for (int32 Index = 0; Index < UBreakerAccountSave::StashCapacity; ++Index)
    {
        Rig.Account->StashItems.Add(BreakerMakeStashItem(1));
    }
    const FBreakerItemInstance Overflow = BreakerMakeStashItem(1);
    Rig.Equipment->AddToBackpack(Overflow);
    TestFalse(TEXT("a full stash refuses a deposit"), Rig.Equipment->DepositToStash(Overflow.ItemId, true));
    TestEqual(TEXT("the refused item stays in the backpack"), Rig.Equipment->GetBackpack().Num(), 1);

    // The O182 gate at the roster's second entry: an under-levelled
    // character cannot withdraw above its level.
    const FBreakerItemInstance High = BreakerMakeStashItem(30);
    Rig.Account->StashItems[0] = High;
    const bool bHighStillStashed = Rig.Account->StashItems.ContainsByPredicate(
        [&High](const FBreakerItemInstance& Existing) { return Existing.ItemId == High.ItemId; });
    TestTrue(TEXT("the fixture holds the high item"), bHighStillStashed);
    TestFalse(TEXT("an under-levelled withdrawal refuses (O182)"),
        Rig.Equipment->WithdrawFromStash(High.ItemId, true));
    TestTrue(TEXT("the refused item never left the stash"),
        Rig.Account->StashItems.ContainsByPredicate(
            [&High](const FBreakerItemInstance& Existing) { return Existing.ItemId == High.ItemId; }));

    UBreakerAccountSave::ResetCacheForTesting();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBackpackCapTest,
    "RiorsEdge.Items.Backpack.CapRefusal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBackpackCapTest::RunTest(const FString& Parameters)
{
    using namespace BreakerStashTest;
    FBreakerStashRig Rig = BreakerMakeStashRig();

    // Fill to One-AB's cap through the ordinary entry.
    for (int32 Index = 0; Index < UBreakerEquipmentComponent::BackpackCapacity; ++Index)
    {
        TestTrue(TEXT("adds below the cap land"), Rig.Equipment->AddToBackpack(BreakerMakeStashItem(1), true));
    }
    TestEqual(TEXT("the backpack sits at the cap"),
        Rig.Equipment->GetBackpack().Num(), UBreakerEquipmentComponent::BackpackCapacity);

    // A REFUSABLE entry (the pickup's path) refuses at the cap: the item has
    // somewhere else to be, and nothing is destroyed.
    TestFalse(TEXT("a refusable add at the cap refuses (One-AB)"),
        Rig.Equipment->AddToBackpack(BreakerMakeStashItem(1), true));
    TestEqual(TEXT("the refused add changed nothing"),
        Rig.Equipment->GetBackpack().Num(), UBreakerEquipmentComponent::BackpackCapacity);

    // A GRANT that already charged the player lands even past the cap —
    // refusing it would destroy a paid item, the exact loss the ruling
    // forbids. Each grant source owes its own pre-payment check instead.
    TestTrue(TEXT("a paid grant lands past the cap — never destroyed"),
        Rig.Equipment->AddToBackpack(BreakerMakeStashItem(1)));
    TestEqual(TEXT("the grant is in the bag"),
        Rig.Equipment->GetBackpack().Num(), UBreakerEquipmentComponent::BackpackCapacity + 1);

    // A withdrawal into a full backpack refuses BEFORE any claim mark is
    // written: the item stays stashed and stays claimable.
    const FBreakerItemInstance Stashed = BreakerMakeStashItem(1);
    Rig.Account->StashItems.Add(Stashed);
    TestFalse(TEXT("a withdrawal at the cap refuses (One-AB)"),
        Rig.Equipment->WithdrawFromStash(Stashed.ItemId, true));
    TestFalse(TEXT("no claim mark was written"), Rig.Account->PendingWithdrawals.Contains(Stashed.ItemId));
    TestEqual(TEXT("the item stays stashed"), Rig.Account->StashItems.Num(), 1);

    UBreakerAccountSave::ResetCacheForTesting();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
