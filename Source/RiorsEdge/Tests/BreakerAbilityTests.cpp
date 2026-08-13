#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbility_Skim.h"
#include "Abilities/BreakerGameplayAbility.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityRegistryTest,
    "RiorsEdge.Abilities.FallbackRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityRegistryTest::RunTest(const FString& Parameters)
{
    const TArray<UBreakerAbilityDefinition*>& Registry = UBreakerAbilityDefinition::GetFallbackRegistry();
    TestTrue(TEXT("The fallback registry is populated"), Registry.Num() >= 3);

    TSet<FName> SeenIds;
    for (const UBreakerAbilityDefinition* Definition : Registry)
    {
        if (!Definition)
        {
            AddError(TEXT("A null entry is in the fallback registry"));
            continue;
        }
        TestFalse(TEXT("Every fallback entry has an id"), Definition->AbilityId.IsNone());
        TestFalse(TEXT("Ids are unique"), SeenIds.Contains(Definition->AbilityId));
        SeenIds.Add(Definition->AbilityId);
        TestTrue(TEXT("Every entry has a class"), Definition->ClassId != EBreakerClassId::None);
        TestTrue(TEXT("Every entry has an ability tag"), Definition->AbilityTag.IsValid());
        TestTrue(TEXT("Costs are non-negative"), Definition->ResourceCost >= 0.0f);
        TestTrue(TEXT("Cooldowns are non-negative"), Definition->CooldownSeconds >= 0.0f);
        // A cooldown without a tag cannot be queried by the HUD, and a tag
        // without a cooldown is dead data.
        TestEqual(TEXT("Cooldown seconds and cooldown tag agree"), Definition->HasCooldown(), Definition->CooldownTag.IsValid());
        TestTrue(TEXT("Every registry entry resolves by id"), UBreakerAbilityDefinition::FindFallback(Definition->AbilityId) == Definition);
    }

    TestNull(TEXT("An unknown id resolves to nothing"), UBreakerAbilityDefinition::FindFallback(TEXT("Swift.NotAnAbility")));
    TestNull(TEXT("None resolves to nothing"), UBreakerAbilityDefinition::FindFallback(NAME_None));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityDefinitionValuesTest,
    "RiorsEdge.Abilities.DefinitionValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityDefinitionValuesTest::RunTest(const FString& Parameters)
{
    // Values quoted from Docs/Design/Class-Kits.md §1.2.
    const UBreakerAbilityDefinition* Skim = UBreakerAbilityDefinition::FindFallback(TEXT("Swift.Skim"));
    if (!Skim)
    {
        AddError(TEXT("Skim is missing from the fallback registry"));
        return false;
    }
    TestEqual(TEXT("Skim costs 15 Momentum"), Skim->GetResourceCost(), 15.0f);
    TestEqual(TEXT("Skim has a 3s cooldown"), Skim->GetCooldownSeconds(), 3.0f);
    TestTrue(TEXT("Skim is the one implemented proof ability"), Skim->IsImplemented());
    TestTrue(TEXT("Skim's ability class derives from the Breaker base"), Skim->AbilityClass->IsChildOf(UBreakerGameplayAbility::StaticClass()));

    const UBreakerAbilityDefinition* Lead = UBreakerAbilityDefinition::FindFallback(TEXT("Swift.Lead"));
    if (!Lead)
    {
        AddError(TEXT("Lead is missing from the fallback registry"));
        return false;
    }
    TestEqual(TEXT("Lead costs 40 Momentum"), Lead->GetResourceCost(), 40.0f);
    TestEqual(TEXT("Lead has a 10s cooldown"), Lead->GetCooldownSeconds(), 10.0f);
    TestFalse(TEXT("Lead is not implemented yet"), Lead->IsImplemented());

    const UBreakerAbilityDefinition* Overdrive = UBreakerAbilityDefinition::FindFallback(TEXT("Swift.Overdrive"));
    if (!Overdrive)
    {
        AddError(TEXT("Overdrive is missing from the fallback registry"));
        return false;
    }
    TestEqual(TEXT("Overdrive costs a full bar"), Overdrive->GetResourceCost(), 100.0f);
    TestFalse(TEXT("Overdrive is cost-gated, not cooldown-gated"), Overdrive->HasCooldown());
    TestTrue(TEXT("Overdrive is the ultimate"), Overdrive->IsUltimate());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilitySlotAffinityTest,
    "RiorsEdge.Abilities.SlotAffinity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilitySlotAffinityTest::RunTest(const FString& Parameters)
{
    UBreakerAbilityDefinition* ClassAbility = NewObject<UBreakerAbilityDefinition>();
    ClassAbility->SlotAffinity = EBreakerAbilitySlot::ClassAbilityOne;
    TestTrue(TEXT("A class ability fits slot one"), ClassAbility->CanOccupySlot(EBreakerAbilitySlot::ClassAbilityOne));
    TestTrue(TEXT("A class ability fits slot two"), ClassAbility->CanOccupySlot(EBreakerAbilitySlot::ClassAbilityTwo));
    TestFalse(TEXT("A class ability may not sit in the ultimate slot"), ClassAbility->CanOccupySlot(EBreakerAbilitySlot::Ultimate));

    UBreakerAbilityDefinition* Ultimate = NewObject<UBreakerAbilityDefinition>();
    Ultimate->SlotAffinity = EBreakerAbilitySlot::Ultimate;
    TestTrue(TEXT("An ultimate fits the ultimate slot"), Ultimate->CanOccupySlot(EBreakerAbilitySlot::Ultimate));
    TestFalse(TEXT("An ultimate may not sit in a class slot"), Ultimate->CanOccupySlot(EBreakerAbilitySlot::ClassAbilityOne));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityResolutionTest,
    "RiorsEdge.Abilities.SlotResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityResolutionTest::RunTest(const FString& Parameters)
{
    // An empty loadout falls back to the class default so the slice is
    // playable before the loadout UI writes anything.
    const UBreakerAbilityDefinition* SlotOne = UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, EBreakerAbilitySlot::ClassAbilityOne, NAME_None);
    TestNotNull(TEXT("An empty Swift slot one defaults to Skim"), SlotOne);
    if (SlotOne)
    {
        TestEqual(TEXT("The default is Skim"), SlotOne->AbilityId, FName(TEXT("Swift.Skim")));
    }

    const UBreakerAbilityDefinition* Ultimate = UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, EBreakerAbilitySlot::Ultimate, NAME_None);
    TestNotNull(TEXT("An empty Swift ultimate defaults to Overdrive"), Ultimate);

    // A class with no kit authored yet grants nothing rather than borrowing Swift's.
    TestNull(TEXT("Caster has no fallback kit yet"), UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Caster, EBreakerAbilitySlot::ClassAbilityOne, NAME_None));
    // An explicitly equipped ability that does not fit the slot is refused.
    TestNull(TEXT("Overdrive cannot be equipped into a class slot"), UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, EBreakerAbilitySlot::ClassAbilityOne, TEXT("Swift.Overdrive")));
    TestNull(TEXT("An unknown equipped id resolves to nothing"), UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, EBreakerAbilitySlot::ClassAbilityOne, TEXT("Swift.Nonsense")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityCostRuleTest,
    "RiorsEdge.Abilities.CostRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityCostRuleTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Exactly enough resource affords the ability"), UBreakerGameplayAbility::IsAffordable(15.0f, 15.0f));
    TestFalse(TEXT("One short does not afford it"), UBreakerGameplayAbility::IsAffordable(14.99f, 15.0f));
    TestTrue(TEXT("A free ability is always affordable"), UBreakerGameplayAbility::IsAffordable(0.0f, 0.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilitySkimDirectionTest,
    "RiorsEdge.Abilities.SkimDirection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilitySkimDirectionTest::RunTest(const FString& Parameters)
{
    // Skim is a horizontal verb: looking straight down must not aim it downward.
    const FVector LookingDown = UBreakerAbility_Skim::HorizontalDirectionForView(FRotator(-89.0, 0.0, 0.0));
    TestTrue(TEXT("The impulse is strictly horizontal"), FMath::IsNearlyZero(LookingDown.Z));
    TestTrue(TEXT("The impulse is normalized"), FMath::IsNearlyEqual(LookingDown.Size(), 1.0f, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Pitch does not change the heading"), LookingDown.Equals(FVector::ForwardVector, KINDA_SMALL_NUMBER));

    const FVector Yawed = UBreakerAbility_Skim::HorizontalDirectionForView(FRotator(0.0, 90.0, 0.0));
    TestTrue(TEXT("Yaw drives the heading"), Yawed.Equals(FVector::RightVector, KINDA_SMALL_NUMBER));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityComponentBookkeepingTest,
    "RiorsEdge.Abilities.ComponentBookkeeping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityComponentBookkeepingTest::RunTest(const FString& Parameters)
{
    // No owner, no ability system: every query must answer safely rather than
    // crash, because the HUD reads these before BeginPlay on a fresh pawn.
    UBreakerAbilityComponent* Component = NewObject<UBreakerAbilityComponent>();
    Component->RefreshGrants();
    TestEqual(TEXT("Nothing is granted without an owner"), Component->GetGrantedCount(), 0);
    TestEqual(TEXT("No active cooldowns without an owner"), Component->GetActiveCooldownCount(), 0);
    TestEqual(TEXT("An ungranted slot has no id"), Component->GetAbilityIdForSlot(EBreakerAbilitySlot::ClassAbilityOne), FName(NAME_None));
    TestNull(TEXT("An ungranted slot has no definition"), Component->GetDefinitionForSlot(EBreakerAbilitySlot::ClassAbilityOne));
    TestFalse(TEXT("An ungranted slot is not implemented"), Component->IsSlotImplemented(EBreakerAbilitySlot::ClassAbilityOne));
    TestFalse(TEXT("An ungranted slot is not granted"), Component->IsSlotGranted(EBreakerAbilitySlot::ClassAbilityOne));
    TestEqual(TEXT("An ungranted slot costs nothing"), Component->GetCost(EBreakerAbilitySlot::Ultimate), 0.0f);
    TestEqual(TEXT("An ungranted slot has no cooldown remaining"), Component->GetCooldownRemaining(EBreakerAbilitySlot::Ultimate), 0.0f);
    TestEqual(TEXT("An ungranted slot has no cooldown duration"), Component->GetCooldownDuration(EBreakerAbilitySlot::Ultimate), 0.0f);
    TestFalse(TEXT("An ungranted slot reports no cooldown at all"), Component->SlotHasCooldown(EBreakerAbilitySlot::Ultimate));
    TestFalse(TEXT("Activating an ungranted slot fails cleanly"), Component->TryActivateSlot(EBreakerAbilitySlot::ClassAbilityOne));
    return true;
}

#endif
