#include "Misc/AutomationTest.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDashClassEntitlementTest,
    "RiorsEdge.Movement.DashClassEntitlement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDashClassEntitlementTest::RunTest(const FString& Parameters)
{
    // Exercise the central component entitlement independently of TryDash's
    // world/cooldown gates: a no-world refusal would prove nothing about class.
    auto* Unowned = NewObject<UBreakerCharacterMovementComponent>();
    TestFalse(TEXT("No owner has no dash entitlement"), Unowned->CanUseDash());
    AActor* BareOwner = NewObject<AActor>();
    auto* BareMovement = NewObject<UBreakerCharacterMovementComponent>(BareOwner);
    BareOwner->AddInstanceComponent(BareMovement);
    TestFalse(TEXT("Missing progression refuses dash"), BareMovement->CanUseDash());

    const EBreakerClassId Classes[] = { EBreakerClassId::Swift, EBreakerClassId::Caster,
        EBreakerClassId::Gunsmith, EBreakerClassId::Tank, EBreakerClassId::Support };
    for (const EBreakerClassId ClassId : Classes)
    {
        AActor* Owner = NewObject<AActor>();
        auto* Movement = NewObject<UBreakerCharacterMovementComponent>(Owner);
        auto* Progression = NewObject<UBreakerProgressionComponent>(Owner);
        Owner->AddInstanceComponent(Movement);
        Owner->AddInstanceComponent(Progression);
        TestFalse(TEXT("Unchosen class has no dash"), Movement->CanUseDash());
        if (!TestTrue(TEXT("Fresh class can be chosen"), Progression->ChoosePermanentClassById(ClassId))) return false;
        TestEqual(FString::Printf(TEXT("Only Swift has dash, class %d"), static_cast<int32>(ClassId)),
            Movement->CanUseDash(), ClassId == EBreakerClassId::Swift);
    }
    return true;
}

#endif
