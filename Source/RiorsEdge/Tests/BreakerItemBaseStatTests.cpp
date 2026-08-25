#include "Misc/AutomationTest.h"
#include "Combat/BreakerShieldMath.h"
#include "Items/BreakerItemBaseStats.h"
#include "Items/BreakerLootLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

// The armour archetypes' arithmetic: the base pools an item grants before
// any affix, and the recharge that makes the shield pool worth being the
// smaller one. Pure maths on both sides, so the suite can walk the whole
// ladder and the whole clock.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerItemBaseStatTest,
    "RiorsEdge.Items.BaseStats.ArchetypePoolsAndRecharge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerItemBaseStatTest::RunTest(const FString& Parameters)
{
    // --- Life is the bigger pool, everywhere ---------------------------------
    for (const EBreakerEquipSlot Slot : {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour,
        EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Boots, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Waist})
    {
        for (const int32 Level : {1, 14, 60, 120})
        {
            const float Life = BreakerItemBase::BaseLifeAt(Slot, Level);
            const float Shield = BreakerItemBase::BaseShieldAt(Slot, Level);
            TestTrue(TEXT("Every armour slot carries a base"), Life > 0.0f && Shield > 0.0f);
            TestTrue(TEXT("Life is strictly the bigger pool"), Life > Shield);
        }
        TestTrue(TEXT("The base grows with item level"),
            BreakerItemBase::BaseLifeAt(Slot, 60) > BreakerItemBase::BaseLifeAt(Slot, 1));
    }
    TestEqual(TEXT("Weapons carry no armour base"),
        BreakerItemBase::SlotBaseWeight(EBreakerEquipSlot::Primary), 0.0f);

    // --- The archetype gates the grant ---------------------------------------
    FBreakerItemInstance Piece;
    Piece.Slot = EBreakerEquipSlot::BodyArmour;
    Piece.ItemLevel = 14;
    TestEqual(TEXT("A pre-archetype item grants nothing"), BreakerItemBase::BaseLifeOf(Piece), 0.0f);
    Piece.ArmourArchetype = EBreakerArmourArchetype::Life;
    TestTrue(TEXT("A Life piece grants life and no shield"),
        BreakerItemBase::BaseLifeOf(Piece) > 0.0f && BreakerItemBase::BaseShieldOf(Piece) == 0.0f);
    Piece.ArmourArchetype = EBreakerArmourArchetype::Shield;
    TestTrue(TEXT("A Shield piece grants shield and no life"),
        BreakerItemBase::BaseShieldOf(Piece) > 0.0f && BreakerItemBase::BaseLifeOf(Piece) == 0.0f);

    // --- Rolled armour always commits, and a seed reproduces the commitment --
    // Also: both archetypes actually occur (a stream bug that always rolled
    // one side would pass every per-item test).
    bool bSawLife = false, bSawShield = false;
    for (int32 Seed = 1; Seed <= 32; ++Seed)
    {
        const FBreakerItemInstance Rolled = UBreakerLootLibrary::RollItem(
            NAME_None, EBreakerEquipSlot::Helmet, EBreakerItemRarity::Uncommon, 14, Seed);
        TestTrue(TEXT("Rolled armour is never archetypeless"),
            Rolled.ArmourArchetype != EBreakerArmourArchetype::None);
        const FBreakerItemInstance Again = UBreakerLootLibrary::RollItem(
            NAME_None, EBreakerEquipSlot::Helmet, EBreakerItemRarity::Uncommon, 14, Seed);
        TestTrue(TEXT("A seed reproduces the archetype"), Rolled.ArmourArchetype == Again.ArmourArchetype);
        bSawLife |= Rolled.ArmourArchetype == EBreakerArmourArchetype::Life;
        bSawShield |= Rolled.ArmourArchetype == EBreakerArmourArchetype::Shield;
    }
    TestTrue(TEXT("Both archetypes occur across seeds"), bSawLife && bSawShield);
    TestTrue(TEXT("Rolled weapons stay archetypeless"),
        UBreakerLootLibrary::RollItem(NAME_None, EBreakerEquipSlot::Primary,
            EBreakerItemRarity::Uncommon, 14, 7).ArmourArchetype == EBreakerArmourArchetype::None);

    // --- The recharge clock --------------------------------------------------
    // Nothing before the delay; a linear climb after it; capped at the max;
    // dead pools and dead time are no-ops.
    TestEqual(TEXT("Inside the delay the bar holds"),
        BreakerShield::RechargeStep(10.0f, 100.0f, 3.9f, 0.5f), 10.0f);
    const float Climbed = BreakerShield::RechargeStep(10.0f, 100.0f, 4.1f, 0.5f);
    TestTrue(TEXT("Past the delay the bar climbs by rate x time"),
        FMath::IsNearlyEqual(Climbed, 10.0f + 100.0f * BreakerShield::RechargeFractionPerSecond * 0.5f, 0.01f));
    TestEqual(TEXT("The bar never overshoots its cap"),
        BreakerShield::RechargeStep(99.0f, 100.0f, 60.0f, 10.0f), 100.0f);
    TestEqual(TEXT("A full bar holds"),
        BreakerShield::RechargeStep(100.0f, 100.0f, 60.0f, 0.5f), 100.0f);
    TestEqual(TEXT("No pool, no recharge"),
        BreakerShield::RechargeStep(0.0f, 0.0f, 60.0f, 0.5f), 0.0f);
    return true;
}

#endif
