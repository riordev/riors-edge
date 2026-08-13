#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Classes/BreakerManaComponent.h"
#include "Progression/BreakerProgressionComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerManaGenerationTest,
    "RiorsEdge.Classes.ManaGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerManaGenerationTest::RunTest(const FString& Parameters)
{
    // Single-pellet weapons bank the flat weapon-hit gain.
    TestEqual(TEXT("A rifle hit banks the weapon-hit gain"), UBreakerManaComponent::HitGeneration(false, 1, 1, 1.5f, 4.0f, 1.0f), 1.5f);
    TestEqual(TEXT("A rifle weak point replaces the weapon-hit gain"), UBreakerManaComponent::HitGeneration(true, 1, 1, 1.5f, 4.0f, 1.0f), 4.0f);
    TestEqual(TEXT("A miss banks nothing"), UBreakerManaComponent::HitGeneration(false, 0, 1, 1.5f, 4.0f, 1.0f), 0.0f);

    // Anti-Multishot: each pellet is worth 1/n, so a full volley matches a rifle hit.
    TestEqual(TEXT("One pellet of eight banks an eighth"), UBreakerManaComponent::HitGeneration(false, 1, 8, 1.5f, 4.0f, 1.0f), 1.5f / 8.0f);
    TestEqual(TEXT("Half a volley banks half"), UBreakerManaComponent::HitGeneration(false, 4, 8, 1.5f, 4.0f, 1.0f), 0.75f);
    TestEqual(TEXT("A full volley banks exactly one rifle hit"), UBreakerManaComponent::HitGeneration(false, 8, 8, 1.5f, 4.0f, 1.0f), 1.5f);
    TestEqual(TEXT("Landed pellets cannot exceed the volley"), UBreakerManaComponent::HitGeneration(false, 99, 8, 1.5f, 4.0f, 1.0f), 1.5f);

    // A weak point pays for exactly one pellet; the rest stay at the hit rate.
    TestEqual(TEXT("A weak-point volley pays one pellet at the weak-point rate"), UBreakerManaComponent::HitGeneration(true, 8, 8, 1.5f, 4.0f, 1.0f), (4.0f + 1.5f * 7.0f) / 8.0f);
    TestEqual(TEXT("Weak point never stacks with the hit gain"), UBreakerManaComponent::HitGeneration(true, 1, 8, 1.5f, 4.0f, 1.0f), 4.0f / 8.0f);

    // Proc coefficient: a DoT tick carries 0 and must bank nothing.
    TestEqual(TEXT("A DoT tick banks nothing"), UBreakerManaComponent::HitGeneration(false, 1, 1, 1.5f, 4.0f, 0.0f), 0.0f);
    TestEqual(TEXT("A half-coefficient proc banks half"), UBreakerManaComponent::HitGeneration(false, 1, 1, 1.5f, 4.0f, 0.5f), 0.75f);

    TestEqual(TEXT("Generation clamps to the global cap"), UBreakerManaComponent::ClampGeneration(50.0f, 20.0f), 20.0f);
    TestEqual(TEXT("Generation below the cap passes through"), UBreakerManaComponent::ClampGeneration(12.0f, 20.0f), 12.0f);
    TestEqual(TEXT("Generation never goes negative"), UBreakerManaComponent::ClampGeneration(-4.0f, 20.0f), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerManaOvercastTest,
    "RiorsEdge.Classes.ManaOvercast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerManaOvercastTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("An empty bank is not Overcast"), UBreakerManaComponent::IsOvercastValue(0.0f));
    TestFalse(TEXT("A positive bank is not Overcast"), UBreakerManaComponent::IsOvercastValue(35.0f));
    TestTrue(TEXT("A negative bank is Overcast"), UBreakerManaComponent::IsOvercastValue(-5.0f));

    TestEqual(TEXT("Generation is unchanged above zero"), UBreakerManaComponent::GenerationMultiplierForMana(10.0f, 2.0f), 1.0f);
    TestEqual(TEXT("Generation doubles while in debt"), UBreakerManaComponent::GenerationMultiplierForMana(-10.0f, 2.0f), 2.0f);
    TestEqual(TEXT("Doubling stops the instant the debt clears"), UBreakerManaComponent::GenerationMultiplierForMana(0.0f, 2.0f), 1.0f);

    // The bank is bounded by the Overcast floor below and MaxClassResource above.
    TestEqual(TEXT("Spending stops at the Overcast floor"), UBreakerManaComponent::ClampToBank(-45.0f, -20.0f, 100.0f), -20.0f);
    TestEqual(TEXT("Generation stops at the ceiling"), UBreakerManaComponent::ClampToBank(140.0f, -20.0f, 100.0f), 100.0f);
    TestEqual(TEXT("Values inside the bank pass through"), UBreakerManaComponent::ClampToBank(-7.5f, -20.0f, 100.0f), -7.5f);
    TestEqual(TEXT("A positive floor is treated as zero"), UBreakerManaComponent::ClampToBank(-5.0f, 10.0f, 100.0f), 0.0f);

    // Overcast is a debt, not a spiral.
    TestTrue(TEXT("An affordable cast is allowed"), UBreakerManaComponent::CanSpendFrom(40.0f, 30.0f, -20.0f));
    TestTrue(TEXT("A cast may drive the bank negative"), UBreakerManaComponent::CanSpendFrom(10.0f, 30.0f, -20.0f));
    TestTrue(TEXT("A cast may land exactly on the floor"), UBreakerManaComponent::CanSpendFrom(10.0f, 30.0f, -20.0f));
    TestFalse(TEXT("A cast may not pass the floor"), UBreakerManaComponent::CanSpendFrom(10.0f, 31.0f, -20.0f));
    TestFalse(TEXT("Nothing may be cast while already Overcast"), UBreakerManaComponent::CanSpendFrom(-1.0f, 5.0f, -20.0f));
    TestTrue(TEXT("A free cast is always allowed"), UBreakerManaComponent::CanSpendFrom(-1.0f, 0.0f, -20.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerManaInertTest,
    "RiorsEdge.Classes.ManaInert",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerManaInertTest::RunTest(const FString& Parameters)
{
    UBreakerManaComponent* Mana = NewObject<UBreakerManaComponent>();
    TestFalse(TEXT("A componentless owner is inert"), Mana->IsActiveForOwner());
    TestFalse(TEXT("An inert component is never Overcast"), Mana->IsOvercast());
    TestEqual(TEXT("An inert component publishes no damage penalty"), Mana->GetOvercastIncomingDamageTaken(), 0.0f);
    TestFalse(TEXT("An inert component affords nothing"), Mana->CanAffordSpend(1.0f));
    TestEqual(TEXT("Defaults match the Class-Kits weapon-hit gain"), Mana->WeaponHitGain, 1.5f);
    TestEqual(TEXT("Defaults match the Class-Kits weak-point gain"), Mana->WeakPointGain, 4.0f);
    TestEqual(TEXT("Defaults match the Class-Kits global cap"), Mana->GlobalGenerationCap, 20.0f);
    TestEqual(TEXT("Defaults match the Class-Kits Overcast floor"), Mana->GetOvercastFloor(), -20.0f);
    TestEqual(TEXT("Defaults match the Class-Kits Overcast damage penalty"), Mana->OvercastIncomingDamageTaken, 0.15f);

    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    TestFalse(TEXT("An unselected class is not Caster"), Progression->GetProgressionState().PermanentClass == EBreakerClassId::Caster);
    Progression->ChoosePermanentClassById(EBreakerClassId::Swift);
    TestFalse(TEXT("A Swift never runs the Mana loop"), Progression->GetProgressionState().PermanentClass == EBreakerClassId::Caster);
    Progression->DevForceClass(EBreakerClassId::Caster);
    TestTrue(TEXT("Caster runs the Mana loop"), Progression->GetProgressionState().PermanentClass == EBreakerClassId::Caster);
    return true;
}

#endif
