#include "Misc/AutomationTest.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "GameFramework/Actor.h"
#include "Characters/BreakerCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSprintFireExclusionTest,
    "RiorsEdge.Input.SprintFireExclusion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSprintFireExclusionTest::RunTest(const FString& Parameters)
{
    // Owned components exercise the shipped entry points without a world or
    // ammo/level grants. Trigger state remains testable even without firing a shot.
    AActor* Owner = NewObject<AActor>();
    auto* Movement = NewObject<UBreakerCharacterMovementComponent>(Owner);
    auto* Weapon = NewObject<UBreakerWeaponComponent>(Owner);
    Owner->AddInstanceComponent(Movement);
    Owner->AddInstanceComponent(Weapon);

    Movement->SetSprinting(true);
    Weapon->StartFire();
    TestFalse(TEXT("A new trigger press cancels sprint"), Movement->IsSprinting());
    TestTrue(TEXT("The trigger takes over"), Weapon->IsTriggerHeld());
    Weapon->StopFire();
    TestFalse(TEXT("Releasing fire does not resume sprint"), Movement->IsSprinting());

    Weapon->StartFire();
    Movement->SetSprinting(true);
    TestTrue(TEXT("A new sprint takes over"), Movement->IsSprinting());
    TestFalse(TEXT("Sprint cancels the held trigger"), Weapon->IsTriggerHeld());
    Movement->SetSprinting(false);
    TestFalse(TEXT("Leaving sprint does not resume held fire"), Weapon->IsTriggerHeld());

    Weapon->StartFire();
    Movement->UpdateFromCompressedFlags(FSavedMove_Character::FLAG_Custom_1);
    TestTrue(TEXT("Server move applies sprint"), Movement->IsSprinting());
    TestFalse(TEXT("Server sprint move cancels firing"), Weapon->IsTriggerHeld());
    Movement->UpdateFromCompressedFlags(0);
    TestFalse(TEXT("Server move clears sprint"), Movement->IsSprinting());
    TestFalse(TEXT("Server sprint release does not resume fire"), Weapon->IsTriggerHeld());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSprintSavedMoveTest,
    "RiorsEdge.Movement.SprintSavedMove",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSprintSavedMoveTest::RunTest(const FString& Parameters)
{
    FBreakerSavedMove_Character Sprint;
    TestEqual(TEXT("Default saved move does not sprint"),
        Sprint.GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_1, 0);
    Sprint.bSavedWantsToSprint = true;
    Sprint.bSavedWantsLedgeTraversal = true;
    TestTrue(TEXT("Sprint flag travels alongside the ledge flag"),
        (Sprint.GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_1) != 0
        && (Sprint.GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_0) != 0);
    FSavedMovePtr Walk(new FBreakerSavedMove_Character());
    static_cast<FBreakerSavedMove_Character*>(Walk.Get())->bSavedWantsLedgeTraversal = true;
    TestFalse(TEXT("Sprint edge cannot combine away"), Sprint.CanCombineWith(Walk, nullptr, 0.1f));
    Sprint.Clear();
    TestEqual(TEXT("Recycled move clears sprint flag"),
        Sprint.GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_1, 0);
    ABreakerCharacter* Character = NewObject<ABreakerCharacter>();
    auto* Movement = Character->GetBreakerMovement();
    auto* Weapon = Character->FindComponentByClass<UBreakerWeaponComponent>();
    if (!TestNotNull(TEXT("Shipped character has weapon"), Weapon)
        || !TestNotNull(TEXT("Shipped character has movement"), Movement)) return false;
    Weapon->StartFire();
    Sprint.bSavedWantsToSprint = true;
    Sprint.PrepMoveFor(Character);
    TestTrue(TEXT("Replay restores historical sprint"), Movement->IsSprinting());
    TestTrue(TEXT("Replay does not cancel a newer trigger press"), Weapon->IsTriggerHeld());
    Weapon->StopFire();
    return true;
}

#endif
