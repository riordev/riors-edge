#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemRequirements.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"

// ---------------------------------------------------------------------------
// Part One-AA: the equip requirement, derived and gated at the player-facing
// entry. Three claims, each pinned: the derivation (min against the cap,
// expiring when levelling does), the gate at EquipFromBackpack refusing both
// an under-levelled character and a level-less rig, and the MECHANISM staying
// ungated — EquipItem composing anything is what thirty-two rig call sites
// and every system path depend on.
//
// THE ENTRY-POINT ROSTER, pinned in prose because a call graph cannot be
// asserted headlessly: the player-facing equip paths are EquipFromBackpack
// (here) and the stash withdrawal (One-X, joins on landing). A new
// player-facing path lands with its gate and joins this list in the same
// commit — a gate a future call site can simply not call is a gate with a
// timer on it.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerItemRequiredLevelTest,
    "RiorsEdge.Items.Requirements.DerivedLevel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerItemRequiredLevelTest::RunTest(const FString& Parameters)
{
    using namespace BreakerItemRequirements;
    const int32 Cap = UBreakerExperienceLibrary::MaxCharacterLevel;

    // Identity while levelling: the number on the door is the level its own
    // drops ask for.
    TestEqual(TEXT("a level-23 item asks for level 23"), RequiredLevelFor(23), 23);
    TestEqual(TEXT("a level-1 item asks for level 1"), RequiredLevelFor(1), 1);
    // The gate expires with levelling: past the cap it is a no-op, because an
    // item level 100 cannot require a level nothing reaches.
    TestEqual(TEXT("an endgame item asks for the cap, not its own level"), RequiredLevelFor(100), Cap);
    TestEqual(TEXT("the ladder's top asks for the cap"), RequiredLevelFor(120), Cap);
    TestTrue(TEXT("a capped character equips anything"), CanEquipAtLevel(120, Cap));
    // Degenerate inputs floor at 1 rather than authoring a level-0 gate.
    TestEqual(TEXT("a degenerate item level floors at 1"), RequiredLevelFor(0), 1);
    // The boundary in both directions.
    TestTrue(TEXT("at-level equips"), CanEquipAtLevel(23, 23));
    TestFalse(TEXT("one level short refuses"), CanEquipAtLevel(23, 22));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEquipGateAtEntryTest,
    "RiorsEdge.Items.Requirements.GateAtEntry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEquipGateAtEntryTest::RunTest(const FString& Parameters)
{
    // A level-1 Swift with a high-level item in the backpack.
    AActor* Owner = NewObject<AActor>();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    TestTrue(TEXT("Swift locks"), Progression->ChoosePermanentClassById(EBreakerClassId::Swift));

    FBreakerItemInstance Item;
    Item.ItemId = FGuid::NewGuid();
    Item.Slot = EBreakerEquipSlot::Helmet;
    Item.ItemLevel = 30;
    Equipment->AddToBackpack(Item);

    // The player-facing path refuses an under-levelled character and the
    // item stays in the backpack — a refusal that eats the item would be the
    // spend-refusal bug wearing armour.
    TestFalse(TEXT("the entry point refuses below the required level"), Equipment->EquipFromBackpack(Item.ItemId));
    TestEqual(TEXT("the refused item stays in the backpack"), Equipment->GetBackpack().Num(), 1);

    // The MECHANISM stays ungated: the same item equips directly, which is
    // what every rig and system path depends on.
    TestTrue(TEXT("EquipItem composes without the gate"), Equipment->EquipItem(Item));

    // A component with no level to read REFUSES rather than passes.
    AActor* Bare = NewObject<AActor>();
    UBreakerEquipmentComponent* BareEquipment = NewObject<UBreakerEquipmentComponent>(Bare);
    FBreakerItemInstance BareItem;
    BareItem.ItemId = FGuid::NewGuid();
    BareItem.Slot = EBreakerEquipSlot::Boots;
    BareItem.ItemLevel = 1;
    BareEquipment->AddToBackpack(BareItem);
    TestFalse(TEXT("no readable level refuses rather than passes"), BareEquipment->EquipFromBackpack(BareItem.ItemId));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
