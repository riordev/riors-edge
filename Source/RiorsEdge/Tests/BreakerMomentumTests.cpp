#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerMomentumComponent.h"
#include "GameFramework/Actor.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMomentumStateTest,
    "RiorsEdge.Classes.MomentumStates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMomentumStateTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Empty bar is Settled"), UBreakerMomentumComponent::StateForFraction(0.0f), EBreakerMomentumState::Settled);
    TestEqual(TEXT("Below a third stays Settled"), UBreakerMomentumComponent::StateForFraction(0.32f), EBreakerMomentumState::Settled);
    TestEqual(TEXT("The middle third is Running"), UBreakerMomentumComponent::StateForFraction(0.5f), EBreakerMomentumState::Running);
    TestEqual(TEXT("The top third is Redline"), UBreakerMomentumComponent::StateForFraction(0.7f), EBreakerMomentumState::Redline);
    TestEqual(TEXT("A full bar is Redline"), UBreakerMomentumComponent::StateForFraction(1.0f), EBreakerMomentumState::Redline);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMomentumGenerationTest,
    "RiorsEdge.Classes.MomentumGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMomentumGenerationTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Below the sprint threshold generates nothing"), UBreakerMomentumComponent::GroundSpeedRate(700.0f, 750.0f, 1250.0f, 6.0f, 10.0f), 0.0f);
    TestEqual(TEXT("At the threshold generates the sprint rate"), UBreakerMomentumComponent::GroundSpeedRate(750.0f, 750.0f, 1250.0f, 6.0f, 10.0f), 6.0f);
    TestEqual(TEXT("Halfway interpolates linearly"), UBreakerMomentumComponent::GroundSpeedRate(1000.0f, 750.0f, 1250.0f, 6.0f, 10.0f), 8.0f);
    TestEqual(TEXT("Beyond the upper speed clamps"), UBreakerMomentumComponent::GroundSpeedRate(4000.0f, 750.0f, 1250.0f, 6.0f, 10.0f), 10.0f);
    TestEqual(TEXT("Stacked sources clamp to the global cap"), UBreakerMomentumComponent::ClampGeneration(40.0f, 25.0f), 25.0f);
    TestEqual(TEXT("Rates below the cap pass through"), UBreakerMomentumComponent::ClampGeneration(18.0f, 25.0f), 18.0f);
    TestEqual(TEXT("Generation never goes negative"), UBreakerMomentumComponent::ClampGeneration(-5.0f, 25.0f), 0.0f);
    TestEqual(TEXT("Settled speed decays fast"), UBreakerMomentumComponent::DecayRateForSpeed(100.0f, 400.0f, 750.0f, 15.0f, 6.0f), 15.0f);
    TestEqual(TEXT("Walking decays slowly"), UBreakerMomentumComponent::DecayRateForSpeed(500.0f, 400.0f, 750.0f, 15.0f, 6.0f), 6.0f);
    TestEqual(TEXT("Above the threshold never decays"), UBreakerMomentumComponent::DecayRateForSpeed(900.0f, 400.0f, 750.0f, 15.0f, 6.0f), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMomentumInertTest,
    "RiorsEdge.Classes.MomentumInert",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMomentumInertTest::RunTest(const FString& Parameters)
{
    UBreakerMomentumComponent* Momentum = NewObject<UBreakerMomentumComponent>();
    TestFalse(TEXT("A componentless owner is inert"), Momentum->IsActiveForOwner());
    TestEqual(TEXT("Defaults match the Class-Kits global cap"), Momentum->GlobalGenerationCap, 25.0f);
    TestEqual(TEXT("Defaults match the Class-Kits slide rate"), Momentum->SlideRate, 12.0f);
    TestEqual(TEXT("An inert component reports the Settled state"), Momentum->GetMomentumState(), EBreakerMomentumState::Settled);

    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    TestFalse(TEXT("An unselected class is not Swift"), Progression->GetProgressionState().PermanentClass == EBreakerClassId::Swift);
    Progression->ChoosePermanentClassById(EBreakerClassId::Tank);
    TestFalse(TEXT("A Tank never runs the Momentum loop"), Progression->GetProgressionState().PermanentClass == EBreakerClassId::Swift);
    Progression->DevForceClass(EBreakerClassId::Swift);
    TestTrue(TEXT("Swift runs the Momentum loop"), Progression->GetProgressionState().PermanentClass == EBreakerClassId::Swift);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMomentumLoopOverrideTest,
    "RiorsEdge.Classes.MomentumLoopOverride",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMomentumLoopOverrideTest::RunTest(const FString& Parameters)
{
    // Pure expiry rule. A negative expiry is "never expires on its own"; the
    // boundary is inclusive so an override never survives its own end time.
    TestFalse(TEXT("A permanent override never expires"), UBreakerMomentumComponent::IsLoopOverrideExpired(-1.0, 1000.0));
    TestFalse(TEXT("An override before its end time is live"), UBreakerMomentumComponent::IsLoopOverrideExpired(10.0, 9.9));
    TestTrue(TEXT("An override at its end time is expired"), UBreakerMomentumComponent::IsLoopOverrideExpired(10.0, 10.0));
    TestTrue(TEXT("An override past its end time is expired"), UBreakerMomentumComponent::IsLoopOverrideExpired(10.0, 11.0));

    TestEqual(TEXT("No override is a 1x loop"), UBreakerMomentumComponent::ComposeGenerationMultipliers({}), 1.0f);
    TestEqual(TEXT("A single override passes through"), UBreakerMomentumComponent::ComposeGenerationMultipliers({2.0f}), 2.0f);
    TestEqual(TEXT("Overlapping overrides compose multiplicatively"), UBreakerMomentumComponent::ComposeGenerationMultipliers({2.0f, 1.5f}), 3.0f);
    TestEqual(TEXT("Non-positive multipliers are ignored, never inverted"), UBreakerMomentumComponent::ComposeGenerationMultipliers({2.0f, -1.0f, 0.0f}), 2.0f);

    // Push/pop bookkeeping with no world: entries with no world clock are
    // permanent, which is exactly why pop has to work.
    UBreakerMomentumComponent* Momentum = NewObject<UBreakerMomentumComponent>();
    TestFalse(TEXT("A fresh loop suspends nothing"), Momentum->IsDecaySuspended());
    TestEqual(TEXT("A fresh loop generates at 1x"), Momentum->GetGenerationMultiplier(), 1.0f);

    Momentum->PushLoopOverride(NAME_None, true, 2.0f, 8.0f);
    TestEqual(TEXT("An unnamed override is rejected"), Momentum->GetActiveLoopOverrideCount(), 0);

    Momentum->PushLoopOverride(TEXT("Window.Swift.Overdrive"), true, 2.0f, 8.0f);
    TestEqual(TEXT("Overdrive pushes exactly one override"), Momentum->GetActiveLoopOverrideCount(), 1);
    TestTrue(TEXT("Overdrive suspends decay"), Momentum->IsDecaySuspended());
    TestEqual(TEXT("Overdrive doubles generation"), Momentum->GetGenerationMultiplier(), 2.0f);

    // Re-casting refreshes rather than stacking, like the window it mirrors.
    Momentum->PushLoopOverride(TEXT("Window.Swift.Overdrive"), true, 2.0f, 8.0f);
    TestEqual(TEXT("A re-cast refreshes rather than stacks"), Momentum->GetActiveLoopOverrideCount(), 1);
    TestEqual(TEXT("A re-cast does not compound the multiplier"), Momentum->GetGenerationMultiplier(), 2.0f);

    // A generation-only override must not silently suspend decay.
    Momentum->PushLoopOverride(TEXT("Test.GenerationOnly"), false, 1.5f, 0.0f);
    TestEqual(TEXT("Two keys are two overrides"), Momentum->GetActiveLoopOverrideCount(), 2);
    TestEqual(TEXT("Both multipliers compose"), Momentum->GetGenerationMultiplier(), 3.0f);

    Momentum->PopLoopOverride(TEXT("Window.Swift.Overdrive"));
    TestFalse(TEXT("Popping the only decay-suspending override resumes decay"), Momentum->IsDecaySuspended());
    TestEqual(TEXT("The surviving override still generates"), Momentum->GetGenerationMultiplier(), 1.5f);
    Momentum->PopLoopOverride(TEXT("Test.GenerationOnly"));
    TestEqual(TEXT("The loop returns to its unmodified rules"), Momentum->GetActiveLoopOverrideCount(), 0);
    TestEqual(TEXT("An empty stack is 1x"), Momentum->GetGenerationMultiplier(), 1.0f);

    // Phantom Step values are quoted from the node text, not invented.
    TestEqual(TEXT("Phantom Step keeps the node's 2.0s internal cooldown"), Momentum->PhantomStepCooldownSeconds, 2.0f);
    TestTrue(TEXT("The window is strictly shorter than the cooldown"), Momentum->PhantomStepWindowSeconds < Momentum->PhantomStepCooldownSeconds);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerClassLockNotifiesLoopTest,
    "RiorsEdge.Classes.ClassLockNotifiesLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Regression. UBreakerMomentumComponent caches bIsSwift in
// HandleProgressionChanged, which BeginPlay runs exactly once and the
// OnProgressionChanged delegate runs thereafter. For a while none of the three
// class-selection paths broadcast, so a character who locked Swift mid-session
// had a completely inert Momentum loop until some unrelated purchase happened
// to fire the event. It stayed invisible because a save that already carries a
// locked class resolves at BeginPlay.
bool FBreakerClassLockNotifiesLoopTest::RunTest(const FString& Parameters)
{
    AActor* Owner = NewObject<AActor>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    UBreakerMomentumComponent* Momentum = NewObject<UBreakerMomentumComponent>(Owner);
    // The suite has no world, so BeginPlay never runs: bind the way it would.
    Progression->OnProgressionChanged.AddDynamic(Momentum, &UBreakerMomentumComponent::HandleProgressionChanged);

    TestFalse(TEXT("Momentum is inert before any class is locked"), Momentum->IsActiveForOwner());

    TestTrue(TEXT("Locking Swift succeeds"), Progression->ChoosePermanentClassById(EBreakerClassId::Swift));
    TestTrue(TEXT("Locking a class wakes its resource loop without a second event"), Momentum->IsActiveForOwner());

    // The dev swap is the other way in, and it is what a playtest actually uses.
    UBreakerProgressionComponent* DevProgression = NewObject<UBreakerProgressionComponent>(NewObject<AActor>());
    UBreakerMomentumComponent* DevMomentum = NewObject<UBreakerMomentumComponent>(DevProgression->GetOwner());
    DevProgression->OnProgressionChanged.AddDynamic(DevMomentum, &UBreakerMomentumComponent::HandleProgressionChanged);
    DevProgression->DevForceClass(EBreakerClassId::Swift);
    TestTrue(TEXT("A dev swap to Swift wakes the loop"), DevMomentum->IsActiveForOwner());
    DevProgression->DevForceClass(EBreakerClassId::Caster);
    TestFalse(TEXT("A dev swap away from Swift puts the loop back to sleep"), DevMomentum->IsActiveForOwner());
    return true;
}

// THE SAME WORLD-FREE TESTABILITY UBreakerManaComponent::AdvanceLoop ALREADY
// HAS. TickComponent's body is now mechanically split into AdvanceLoop (no
// value or behaviour change) for exactly the reason Mana's already was:
// UActorComponent::TickComponent asserts on an unregistered component, which
// every component built in this suite is, so nothing in the old body was
// reachable from automation at all. This proves AdvanceLoop is real and
// reachable, not merely that it compiles — through the decay branch and the
// decay-suspension branch, both real paths a live playtest actually runs.
//
// NOTE ON A DIVERGENCE DELIBERATELY NOT UNIFIED: unlike Mana's AdvanceLoop,
// this one does NOT poll class ownership every tick as a defensive backstop —
// it relies solely on the bound OnProgressionChanged delegate (proven by
// RiorsEdge.Classes.ClassLockNotifiesLoop above). That is a real mechanism
// difference between the two class-resource loops, left as-is per this
// lane's scope.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMomentumAdvanceLoopTest,
    "RiorsEdge.Classes.MomentumAdvanceLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMomentumAdvanceLoopTest::RunTest(const FString& Parameters)
{
    AActor* Owner = NewObject<AActor>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>(Owner);
    UBreakerMomentumComponent* Momentum = NewObject<UBreakerMomentumComponent>(Owner);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    // Found by AdvanceLoop through Owner->FindComponentByClass, never called
    // directly here — its presence is what lets the ground/decay branches run
    // at all instead of AdvanceLoop bailing out on a null GetBreakerMovement().
    TestNotNull(TEXT("The movement component attaches to the rig"), Movement);

    // Same rig order as the Mana Overcast helpers: lock the class BEFORE
    // binding, so BindAttributes' internal HandleProgressionChanged finds the
    // sibling UBreakerProgressionComponent already holding Swift.
    Progression->DevForceClass(EBreakerClassId::Swift);
    Momentum->BindAttributes(Attributes);
    TestTrue(TEXT("A Swift-locked rig runs the Momentum loop"), Momentum->IsActiveForOwner());

    Attributes->ApplyClassResource(50.0f);
    TestEqual(TEXT("The bank holds what it was given"), Momentum->GetMomentum(), 50.0f);

    // Zero velocity (the movement component's untouched default) is well
    // under SettledSpeed, so AdvanceLoop's decay branch runs once the grace
    // period elapses — exactly the old TickComponent body's behaviour.
    for (int32 Step = 0; Step < 15; ++Step)
    {
        Momentum->AdvanceLoop(0.1f);
    }
    TestTrue(TEXT("AdvanceLoop decays a standing bank with no world"), Momentum->GetMomentum() < 50.0f);

    // A decay-suspending loop override (Overdrive's shape) reaches the SAME
    // branch and turns it off, proving AdvanceLoop reads loop overrides too.
    Attributes->ApplyClassResource(50.0f);
    Momentum->PushLoopOverride(TEXT("Test.SuspendDecay"), /*bSuspendDecay=*/true, /*GenerationMultiplier=*/1.0f, /*Duration=*/-1.0f);
    for (int32 Step = 0; Step < 15; ++Step)
    {
        Momentum->AdvanceLoop(0.1f);
    }
    TestEqual(TEXT("A decay-suspending override reaches AdvanceLoop and holds the bank"), Momentum->GetMomentum(), 50.0f);
    Momentum->PopLoopOverride(TEXT("Test.SuspendDecay"));

    // A non-Swift owner's AdvanceLoop is a no-op, exactly like the old
    // TickComponent's early-out for !IsActiveForOwner().
    AActor* TankOwner = NewObject<AActor>();
    UBreakerProgressionComponent* TankProgression = NewObject<UBreakerProgressionComponent>(TankOwner);
    UBreakerCharacterMovementComponent* TankMovement = NewObject<UBreakerCharacterMovementComponent>(TankOwner);
    UBreakerMomentumComponent* TankMomentum = NewObject<UBreakerMomentumComponent>(TankOwner);
    UBreakerAttributeSet* TankAttributes = NewObject<UBreakerAttributeSet>();
    TestNotNull(TEXT("The Tank rig's movement component attaches too"), TankMovement);
    TankProgression->DevForceClass(EBreakerClassId::Tank);
    TankMomentum->BindAttributes(TankAttributes);
    TankAttributes->ApplyClassResource(30.0f);
    TankMomentum->AdvanceLoop(5.0f);
    TestEqual(TEXT("A Tank's AdvanceLoop touches nothing"), TankMomentum->GetMomentum(), 30.0f);
    return true;
}

#endif
