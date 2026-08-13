#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Classes/BreakerMomentumComponent.h"
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

#endif
