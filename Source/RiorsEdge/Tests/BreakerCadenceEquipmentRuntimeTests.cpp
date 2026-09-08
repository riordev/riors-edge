#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerExperience.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCadenceEquipmentRuntimeTest, "RiorsEdge.Items.Legendary.CadenceEquipmentRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCadenceEquipmentRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    auto* ASC = Player->GetAbilitySystemComponent(); auto* Attributes = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attributes);
    Player->GetCombat()->BindAttributes(Attributes);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attributes);
    Progression->ChoosePermanentClassById(EBreakerClassId::Caster);
    // Earned level entitlement fixture, not a claim of playing ten campaign levels here.
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(10, Progression->ExperienceCurve));
    FText Reason;
    if (!TestTrue(TEXT("Legal Precision gateway"), Progression->PurchaseNode(
        UBreakerProgressionLibrary::GetCoreSliceTree(), TEXT("Core.Precision.Sightline"), Reason))) return false;
    auto* Equipment = Player->GetEquipment(); Equipment->BindAttributes(Attributes);
    auto* Weapon = Player->GetWeapon(); Weapon->BeginPlay();
    auto Secondary = UBreakerLootLibrary::RollItem(TEXT("Cadence.Secondary"), EBreakerEquipSlot::Secondary, EBreakerItemRarity::Standard, 1, 91);
    if (!TestTrue(TEXT("Real secondary equips"), Equipment->EquipItem(Secondary))) return false;
    const auto Cadence = UBreakerLootLibrary::RollLegendary(TEXT("Legendary.Cadence"), 1, 77);
    const auto Preview = Equipment->PreviewEquip(Cadence);
    TestTrue(TEXT("Preview discloses secondary displacement"), Preview.bRuleDisplaces);
    if (!TestTrue(TEXT("Real rolled Cadence equips"), Equipment->EquipItem(Cadence))) return false;
    FBreakerItemInstance Found;
    TestFalse(TEXT("Cadence ejects actual secondary"), Equipment->GetEquippedItem(EBreakerEquipSlot::Secondary, Found));
    // Compare authored aggregation only to isolate the rewrite from the item's other genuine rolls.
    auto Plain = Cadence; Plain.Rule = EBreakerItemRule::None;
    FBreakerAttributeContribution PlainOffer, RuledOffer;
    const auto Conditions = FBreakerBuildConditionState::EvaluateForActor(Player);
    UBreakerEquipmentComponent::AggregateStats({Plain}, &PlainOffer, Conditions);
    UBreakerEquipmentComponent::AggregateStats({Cadence}, &RuledOffer, Conditions);
    const float GearRate = PlainOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::FireRateMultiplier);
    TestTrue(TEXT("Real legendary has positive gear fire rate"), GearRate > 0);
    TestEqual(TEXT("Exactly half the gear rate enters weapon damage"),
        RuledOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier)
        - PlainOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier), GearRate * .5f, .001f);
    const float BeforeDamage = Attributes->GetDamageMultiplier();
    const float BeforeAbility = Attributes->GetAbilityDamageMultiplier();
    const float BeforeRate = Weapon->GetFireRateMultiplier();
    const float BeforeSpendPercent = Progression->GetPointSpendDamagePercent();
    const float BeforeWeaponIncreased = RuledOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier)
        + Progression->GetAttributeContribution().GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier);
    const float BeforeAbilityIncreased = RuledOffer.GetIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier)
        + Progression->GetAttributeContribution().GetIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier);
    if (!TestTrue(TEXT("Actual Cadence rank after gateway"), Progression->PurchaseNode(
        UBreakerProgressionLibrary::GetCoreSliceTree(), TEXT("Core.Precision.Cadence"), Reason))) return false;
    Equipment->TickComponent(0, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Purchased rate reaches actual weapon cadence consumer"), Weapon->GetFireRateMultiplier() > BeforeRate);
    // O27 pays 0.25% shared Increased Damage per spent point. Cadence costs
    // one point: preserve that real floor, while refusing any extra conversion.
    const float SpendDelta = Progression->GetPointSpendDamagePercent() - BeforeSpendPercent;
    TestEqual(TEXT("One-point purchase pays its authored shared floor"), SpendDelta, .25f, .0001f);
    const float ExpectedWeapon = BeforeDamage * (1 + SpendDelta / (100 + BeforeWeaponIncreased));
    const float ExpectedAbility = BeforeAbility * (1 + SpendDelta / (100 + BeforeAbilityIncreased));
    TestEqual(TEXT("Tree fire rate is not converted again by Cadence"), Attributes->GetDamageMultiplier(), ExpectedWeapon, .001f);
    TestEqual(TEXT("Ability lane receives only the normal point-spend floor"), Attributes->GetAbilityDamageMultiplier(), ExpectedAbility, .001f);
    TestTrue(TEXT("Equipping secondary remains reversible"), Equipment->EquipItem(Secondary));
    TestFalse(TEXT("Secondary ejects Cadence symmetrically"), Equipment->GetEquippedItem(EBreakerEquipSlot::Primary, Found));
    return true;
}
#endif
