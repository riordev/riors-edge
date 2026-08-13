#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponCadenceTest,
    "RiorsEdge.Weapons.Cadence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponCadenceTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("600 RPM fires every 100 ms"), FBreakerWeaponMath::FireInterval(600.0f), 0.1f);
    TestEqual(TEXT("RPM clamps safely"), FBreakerWeaponMath::FireInterval(0.0f), 60.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponFalloffTest,
    "RiorsEdge.Weapons.Falloff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponFalloffTest::RunTest(const FString& Parameters)
{
    UBreakerWeaponDefinition* Definition = NewObject<UBreakerWeaponDefinition>();
    Definition->FalloffStart = 2000.0f;
    Definition->FalloffEnd = 6000.0f;
    Definition->MinimumFalloffMultiplier = 0.5f;
    TestEqual(TEXT("Full damage before falloff"), FBreakerWeaponMath::DamageMultiplierAtDistance(Definition, 1500.0f), 1.0f);
    TestEqual(TEXT("Halfway distance interpolates damage"), FBreakerWeaponMath::DamageMultiplierAtDistance(Definition, 4000.0f), 0.75f);
    TestEqual(TEXT("Minimum damage after falloff"), FBreakerWeaponMath::DamageMultiplierAtDistance(Definition, 7000.0f), 0.5f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponSpreadTest,
    "RiorsEdge.Weapons.DeterministicSpread",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponSpreadTest::RunTest(const FString& Parameters)
{
    const FVector Direction = FVector::ForwardVector;
    const FVector First = FBreakerWeaponMath::ApplyConeSpread(Direction, 2.0f, 42);
    const FVector Second = FBreakerWeaponMath::ApplyConeSpread(Direction, 2.0f, 42);
    TestTrue(TEXT("Same seed returns same direction"), First.Equals(Second));
    TestTrue(TEXT("Spread remains normalized"), FMath::IsNearlyEqual(First.Size(), 1.0f));
    TestTrue(TEXT("Spread remains inside requested cone"), FMath::RadiansToDegrees(FMath::Acos(Direction.Dot(First))) <= 2.0f + KINDA_SMALL_NUMBER);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponArchetypeTest,
    "RiorsEdge.Weapons.Archetypes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponArchetypeTest::RunTest(const FString& Parameters)
{
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
    TestEqual(TEXT("Rifle is the default archetype"), Weapon->GetArchetype(), EBreakerWeaponArchetype::Rifle);
    Weapon->EquipArchetype(EBreakerWeaponArchetype::Shotgun);
    TestEqual(TEXT("Shotgun can be equipped"), Weapon->GetArchetypeName(), FString(TEXT("SHOTGUN")));
    TestEqual(TEXT("Shotgun receives its eight-round magazine"), Weapon->GetMagazineAmmo(), 8);
    Weapon->EquipArchetype(EBreakerWeaponArchetype::Sniper);
    TestEqual(TEXT("Sniper can be equipped"), Weapon->GetArchetypeName(), FString(TEXT("SNIPER")));
    TestEqual(TEXT("Sniper receives its eight-round magazine"), Weapon->GetMagazineAmmo(), 8);
    Weapon->EquipArchetype(EBreakerWeaponArchetype::SMG);
    TestEqual(TEXT("SMG can be equipped"), Weapon->GetArchetypeName(), FString(TEXT("SMG")));
    TestEqual(TEXT("SMG receives its thirty-five round magazine"), Weapon->GetMagazineAmmo(), 35);
    Weapon->EquipArchetype(EBreakerWeaponArchetype::Rocket);
    TestEqual(TEXT("Rocket launcher can be equipped"), Weapon->GetArchetypeName(), FString(TEXT("ROCKET")));
    TestEqual(TEXT("Rocket receives its four-round magazine"), Weapon->GetMagazineAmmo(), 4);
    UBreakerWeaponComponent* Loadout = NewObject<UBreakerWeaponComponent>();
    Loadout->EquipSlot(2);
    TestEqual(TEXT("Players can equip the secondary slot"), Loadout->GetCurrentSlot(), 2);
    TestEqual(TEXT("The prototype secondary slot carries the shotgun"), Loadout->GetArchetypeName(), FString(TEXT("SHOTGUN")));
    Loadout->SetSlotArchetype(2, EBreakerWeaponArchetype::Rocket);
    TestEqual(TEXT("Slot archetypes are assignable"), Loadout->GetArchetypeName(), FString(TEXT("ROCKET")));
    TestEqual(TEXT("Reassignment grants the new weapon's magazine"), Loadout->GetMagazineAmmo(), 4);
    Loadout->SetSlotArchetype(2, EBreakerWeaponArchetype::Shotgun);
    Loadout->EquipSlot(1);
    TestEqual(TEXT("Players can return to the primary slot"), Loadout->GetCurrentSlot(), 1);
    TestEqual(TEXT("The prototype primary slot carries the rifle"), Loadout->GetArchetypeName(), FString(TEXT("RIFLE")));
    return true;
}

#endif
