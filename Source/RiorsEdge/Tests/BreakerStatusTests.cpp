#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerStatusComponent.h"
#include "GameFramework/Actor.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

namespace
{
    FBreakerStatusApplicationSpec MakeBleedSpec()
    {
        FBreakerStatusApplicationSpec Spec;
        Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false);
        Spec.BaseDamagePerTick = 6.0f;
        Spec.Duration = 3.0f;
        Spec.TickInterval = 0.5f;
        return Spec;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStatusStackingTest,
    "RiorsEdge.Combat.Status.Stacking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStatusStackingTest::RunTest(const FString& Parameters)
{
    AActor* Target = NewObject<AActor>();
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);
    Status->MaximumStacksPerStatus = 3;

    FBreakerStatusApplicationSpec Spec = MakeBleedSpec();
    Spec.Snapshot.bRolledCritical = true;
    Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical);
    TestTrue(TEXT("Bleed is applied"), Status->HasStatus(Spec.StatusTag));
    TestEqual(TEXT("A single application creates one status"), Status->GetActiveStatuses().Num(), 1);
    TestEqual(TEXT("A single application starts at one stack"), Status->GetActiveStatuses()[0].Stacks, 1);

    FBreakerStatusApplicationSpec Reapply = MakeBleedSpec();
    Reapply.Snapshot.bRolledCritical = false;
    for (int32 Index = 0; Index < 5; ++Index) Status->ApplyStatus(Reapply, EBreakerDamageFamily::Physical);
    TestEqual(TEXT("Reapplication never creates a second status"), Status->GetActiveStatuses().Num(), 1);
    TestEqual(TEXT("Stacks are capped"), Status->GetActiveStatuses()[0].Stacks, 3);
    TestTrue(TEXT("Reapplication keeps the original critical snapshot"), Status->GetActiveStatuses()[0].Spec.Snapshot.bRolledCritical);

    FBreakerStatusApplicationSpec Invalid = MakeBleedSpec();
    Invalid.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"), false);
    Invalid.Duration = 0.0f;
    Status->ApplyStatus(Invalid, EBreakerDamageFamily::Physical);
    TestEqual(TEXT("A zero-duration status is rejected"), Status->GetActiveStatuses().Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponBleedDefinitionTest,
    "RiorsEdge.Weapons.BleedApplication",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponBleedDefinitionTest::RunTest(const FString& Parameters)
{
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
    Weapon->EquipArchetype(EBreakerWeaponArchetype::SMG);
    const UBreakerWeaponDefinition* SMG = Weapon->GetActiveDefinition();
    TestNotNull(TEXT("The SMG prototype resolves"), SMG);
    TestEqual(TEXT("The SMG applies bleed a quarter of the time"), SMG->BleedChance, 0.25f);
    TestEqual(TEXT("The SMG bleed ticks for six"), SMG->BleedDamagePerTick, 6.0f);
    TestEqual(TEXT("The SMG bleed lasts three seconds"), SMG->BleedDuration, 3.0f);
    TestEqual(TEXT("The SMG bleed ticks twice a second"), SMG->BleedTickInterval, 0.5f);

    Weapon->EquipArchetype(EBreakerWeaponArchetype::Rifle);
    TestEqual(TEXT("The rifle does not apply bleed"), Weapon->GetActiveDefinition()->BleedChance, 0.0f);
    return true;
}

#endif
