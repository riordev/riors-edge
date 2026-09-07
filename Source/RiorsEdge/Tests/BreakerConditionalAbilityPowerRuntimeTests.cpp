#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerBuildConditions.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerConditionalAbilityPowerRuntimeTest,
    "RiorsEdge.Items.ConditionalAbilityPowerRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerConditionalAbilityPowerRuntimeTest::RunTest(const FString& Parameters)
{
    FBreakerItemInstance AirItem;
    float AirValue = 0;
    for (const TCHAR* Kind : {TEXT("Airborne"), TEXT("Sliding"), TEXT("Redline"), TEXT("Dash"), TEXT("Grounded")})
    {
        const FName Id(*FString::Printf(TEXT("Ability.%sAddedPower"), Kind));
        const FName TwinId(*FString::Printf(TEXT("Offense.%sAddedDamage"), Kind));
        const auto& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        const auto* Definition = UBreakerAffixLibrary::FindAffix(Pool, Id);
        const auto* Twin = UBreakerAffixLibrary::FindAffix(Pool, TwinId);
        if (!TestNotNull(TEXT("registered ordinary ability row"), Definition)
            || !TestNotNull(TEXT("existing weapon counterpart"), Twin)) return false;
        TestEqual(TEXT("existing ability target"), Definition->StatTarget, EBreakerStatTarget::AbilityDamage);
        TestEqual(TEXT("flat, not another multiplier"), Definition->StatBucket, EBreakerStatBucket::Flat);
        TestEqual(TEXT("matching live condition"), Definition->Condition, Twin->Condition);
        TestTrue(TEXT("matching exact slot footprint"), Definition->AllowedSlots == Twin->AllowedSlots);
        TestEqual(TEXT("authored low tier"), Definition->ValueAtT12, 1.5f);
        TestEqual(TEXT("authored normal top tier"), Definition->ValueAtT1, 16.0f);
        TestEqual(TEXT("matching conditional roll weight"), Definition->RollWeight, 45.0f);
        for (int32 Tier : {12, 6, 1, 0, -1})
            TestEqual(TEXT("same shared tier curve including spikes"),
                UBreakerAffixLibrary::ValueForTier(*Definition, Tier), UBreakerAffixLibrary::ValueForTier(*Twin, Tier));
        FBreakerItemInstance Item;
        int32 Index = INDEX_NONE;
        for (int32 Seed = 1; Seed <= 4096; ++Seed)
        {
            Item = UBreakerLootLibrary::RollItem(TEXT("ConditionalPower.Runtime"), Definition->AllowedSlots[0],
                EBreakerItemRarity::Standard, 1, Seed);
            Index = Item.Affixes.IndexOfByPredicate([&](const auto& Row) { return Row.AffixId == Id; });
            if (Index != INDEX_NONE) break;
        }
        if (!TestTrue(TEXT("actual level-one ordinary loot reaches the row"), Index != INDEX_NONE)) return false;
        const float Value = Item.Affixes[Index].Value;
        // Keep the real row, value and tier; isolate its consumer from co-rolls.
        for (auto& Row : Item.Affixes) if (Row.AffixId != Id) Row.Value = 0;
        FBreakerBuildConditionState Active, Inactive;
        Active.Set(Definition->Condition, true);
        FBreakerAttributeContribution Paid, Refused;
        UBreakerEquipmentComponent::AggregateStats({Item}, &Paid, Active);
        UBreakerEquipmentComponent::AggregateStats({Item}, &Refused, Inactive);
        TestEqual(TEXT("active flat power folds before Increased"), Paid.GetFlat(EBreakerAggregatedAttribute::AbilityDamageMultiplier), Value / 100, .00001f);
        TestEqual(TEXT("inactive condition refuses the row"), Refused.GetFlat(EBreakerAggregatedAttribute::AbilityDamageMultiplier), 0.0f);
        TestEqual(TEXT("ability flat cannot spill into weapon damage"), Paid.GetFlat(EBreakerAggregatedAttribute::DamageMultiplier), 0.0f);
        if (FString(Kind) == TEXT("Airborne")) { AirItem = Item; AirValue = Value; }
    }

    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated equip and damage world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    auto* Victim = World->SpawnActor<AActor>();
    if (!Player || !Victim) return false;
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    auto* Movement = Player->GetBreakerMovement(); Movement->SetComponentTickEnabled(false);
    auto* Equipment = Player->GetEquipment(); Equipment->BindAttributes(Player->GetAttributes());
    auto* Combat = NewObject<UBreakerCombatComponent>(Victim);
    Victim->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Victim);
    Health->ApplyMaxHealth(1000); Health->ApplyHealth(1000); Combat->BindAttributes(Health);
    auto* Status = NewObject<UBreakerStatusComponent>(Victim);
    Victim->AddInstanceComponent(Status); Status->RegisterComponent(); Status->BeginPlay(); Status->SetComponentTickEnabled(false);
    auto Hit = [&](EBreakerDamageDelivery Delivery)
    {
        FBreakerDamageRequest Request; Request.BaseDamage = 10; Request.bCanCritical = false; Request.SetInstigator(Player);
        UBreakerDamageLibrary::FillSourcePools(Player->GetAttributes(), Delivery, Request);
        Player->GetCombat()->ApplyOutgoingModifiers(Request);
        return Combat->ReceiveDamage(Request).HealthDamage;
    };
    Movement->SetMovementMode(MOVE_Walking);
    if (!TestTrue(TEXT("equip actual rolled item"), Equipment->EquipItem(AirItem))) return false;
    Equipment->TickComponent(0.0f, LEVELTICK_All, nullptr);
    const float GroundAbility = Hit(EBreakerDamageDelivery::Ability);
    const float GroundWeapon = Hit(EBreakerDamageDelivery::Weapon);
    Movement->SetMovementMode(MOVE_Falling); Equipment->TickComponent(0.0f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("real movement evaluator observes airborne"), FBreakerBuildConditionState::EvaluateForActor(Player).IsActive(EBreakerBuildCondition::Airborne));
    const float Expected = GroundAbility * (1 + AirValue / 100);
    TestEqual(TEXT("equipped conditional raises actual ability hit"), Hit(EBreakerDamageDelivery::Ability), Expected, .0001f);
    TestEqual(TEXT("actual weapon hit is unchanged"), Hit(EBreakerDamageDelivery::Weapon), GroundWeapon, .0001f);
    FBreakerStatusApplicationSpec Dot;
    Dot.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"));
    Dot.Duration = 1; Dot.TickInterval = .5f; Dot.BaseDamagePerTick = 10;
    Dot.Snapshot.SourcePower = UBreakerCombatComponent::ComposeDotSourcePower(Player->GetAttributes(), Player->GetCombat(), EBreakerDamageDelivery::Ability);
    Dot.Snapshot.CriticalChance = 0;
    Status->ApplyStatus(Dot, EBreakerDamageFamily::Physical, Player);
    Movement->SetMovementMode(MOVE_Walking); Equipment->TickComponent(0.0f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("landing immediately removes live ability power"), Hit(EBreakerDamageDelivery::Ability), GroundAbility, .0001f);
    const float BeforeTick = Health->GetHealth(); Status->AdvanceStatuses(.5f);
    TestEqual(TEXT("actual ability DoT keeps its airborne application snapshot"), BeforeTick - Health->GetHealth(), Expected, .0001f);
    return true;
}
#endif
