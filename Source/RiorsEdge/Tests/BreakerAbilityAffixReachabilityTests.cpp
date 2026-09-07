#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "GameFramework/Actor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerAbilityAffixReachabilityTest,
    "RiorsEdge.Items.AbilityAffixes.ConditionalReachability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityAffixReachabilityTest::RunTest(const FString& Parameters)
{
    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    const FName Names[] = { TEXT("Ability.DepletedDamage"), TEXT("Ability.LedgeDamage") };
    const FName Twins[] = { TEXT("Offense.DepletedDamage"), TEXT("Offense.LedgeDamage") };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
    {
        const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, Names[Index]);
        const FBreakerAffixDefinition* Twin = UBreakerAffixLibrary::FindAffix(Pool, Twins[Index]);
        if (!TestNotNull(TEXT("Ability affix is in the shipped pool"), Definition)
            || !TestNotNull(TEXT("Weapon counterpart exists"), Twin)) continue;
        TestTrue(TEXT("Slot access matches the existing counterpart"), Definition->AllowedSlots == Twin->AllowedSlots);
        TestEqual(TEXT("The condition is the real existing condition"), Definition->Condition, Twin->Condition);
        TestEqual(TEXT("Tier floor matches its counterpart"), Definition->ValueAtT12, Twin->ValueAtT12);
        TestEqual(TEXT("Tier ceiling matches its counterpart"), Definition->ValueAtT1, Twin->ValueAtT1);

        // Find a real level-one Standard drop, rather than granting an affix
        // combination or tier that cannot roll. Gloves is legal for both rows.
        FBreakerItemInstance Rolled;
        bool bFound = false;
        for (int32 Seed = 0; Seed < 512 && !bFound; ++Seed)
        {
            Rolled = UBreakerLootLibrary::RollItem(TEXT("AbilityAffixCoverage"), EBreakerEquipSlot::Gloves,
                EBreakerItemRarity::Standard, 1, Seed);
            bFound = Rolled.Affixes.ContainsByPredicate([&](const FBreakerRolledAffix& Affix) { return Affix.AffixId == Names[Index]; });
        }
        if (!TestTrue(TEXT("The new affix is reachable through normal level-one loot"), bFound)) continue;
        UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
        UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
        Equipment->BindAttributes(Attributes);
        if (!TestTrue(TEXT("The actual rolled item can be equipped"), Equipment->EquipItem(Rolled))) continue;

        FBreakerBuildConditionState Inactive;
        FBreakerBuildConditionState Active;
        Active.Set(Definition->Condition, true);
        FBreakerAttributeContribution Before, After;
        UBreakerEquipmentComponent::AggregateStats(Equipment->GetEquipped(), &Before, Inactive);
        UBreakerEquipmentComponent::AggregateStats(Equipment->GetEquipped(), &After, Active);
        const FBreakerRolledAffix* Affix = Rolled.Affixes.FindByPredicate(
            [&](const FBreakerRolledAffix& Candidate) { return Candidate.AffixId == Names[Index]; });
        TestEqual(TEXT("Enabling the condition pays the rolled ability value"),
            After.GetIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier)
            - Before.GetIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier), Affix->Value, 0.001f);
        // Isolate this affix from any other conditional line that the same
        // real drop may carry, then prove its delivery lane does not leak.
        FBreakerItemInstance Isolated = Rolled;
        Isolated.Affixes.RemoveAll([&](const FBreakerRolledAffix& Candidate) { return Candidate.AffixId != Names[Index]; });
        FBreakerAttributeContribution IsolatedOffer;
        UBreakerEquipmentComponent::AggregateStats({ Isolated }, &IsolatedOffer, Active);
        TestEqual(TEXT("Ability condition never bids weapon damage"),
            IsolatedOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier), 0.0f);
        TestTrue(TEXT("The rolled item can be removed normally"), Equipment->UnequipSlot(Rolled.Slot));
    }
    return true;
}

#endif
