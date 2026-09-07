#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Progression/BreakerProgressionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerJointMoreSelectionTest, "RiorsEdge.Attributes.JointMoreSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerJointMoreSelectionTest::RunTest(const FString& Parameters)
{
    using Lane = EBreakerDamageMoreLane;
    using Attr = EBreakerAggregatedAttribute;
    FBreakerAttributeContribution Tree, Gear;
    Tree.AddDamageMoreSource(TEXT("Tree.Shared"), Lane::Shared, 1.20f);
    Tree.AddDamageMoreSource(TEXT("Tree.Weapon"), Lane::Weapon, 1.18f);
    Tree.AddDamageMoreSource(TEXT("Tree.Dot"), Lane::Dot, 1.16f);
    Gear.AddDamageMoreSource(TEXT("00.ReserveSurge"), Lane::Ability, 1.25f);
    FBreakerAttributeAggregator Fold;
    Fold.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
    Fold.SetContribution(EBreakerAttributeContributor::Progression, Tree);
    TestEqual(TEXT("all live sources remain visible"), Fold.GetDamageMoreSourceCount(), 4);
    TestEqual(TEXT("only three static sources selected across four lanes"), Fold.GetSelectedDamageMoreSourceCount(), 3);
    TestEqual(TEXT("shared and weapon survive"), Fold.ComposedMoreProduct(Attr::DamageMultiplier), 1.20f * 1.18f, 0.0001f);
    TestEqual(TEXT("one shared slot benefits ability too"), Fold.ComposedMoreProduct(Attr::AbilityDamageMultiplier), 1.20f * 1.25f, 0.0001f);
    TestEqual(TEXT("weakest DoT source displaced globally"), Fold.ComposedMoreProduct(Attr::DamageOverTimeMultiplier), 1.0f);
    Fold.ClearContribution(EBreakerAttributeContributor::Equipment);
    TestEqual(TEXT("removal restores displaced DoT"), Fold.ComposedMoreProduct(Attr::DamageOverTimeMultiplier), 1.16f, 0.0001f);
    Tree.Reset(); Gear.Reset();
    Tree.AddDamageMoreSource(TEXT("Tree.First"), Lane::Weapon, 1.30f);
    Tree.AddDamageMoreSource(TEXT("Tree.Second"), Lane::Dot, 1.30f);
    Gear.AddDamageMoreSource(TEXT("07.Later"), Lane::Ability, 1.30f);
    Gear.AddDamageMoreSource(TEXT("00.Earlier"), Lane::Shared, 1.30f);
    Fold.SetContribution(EBreakerAttributeContributor::Progression, Tree);
    Fold.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
    const TArray<FBreakerDamageMoreSource> Selected = Fold.GetSelectedDamageMoreSources();
    if (!TestEqual(TEXT("tie selection cardinality"), Selected.Num(), 3)) return false;
    TestEqual(TEXT("tree tie retains authored first"), Selected[0].Key, FName(TEXT("Tree.First")));
    TestEqual(TEXT("tree tie retains authored second"), Selected[1].Key, FName(TEXT("Tree.Second")));
    TestEqual(TEXT("gear tie uses canonical slot order"), Selected[2].Key, FName(TEXT("00.Earlier")));
    Gear.AddDamageMoreSource(TEXT("00.Earlier"), Lane::Ability, 1.80f);
    TestEqual(TEXT("same keyed effect replaces rather than duplicates"), Gear.GetDamageMoreSources().Num(), 2);
    Fold.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
    TestEqual(TEXT("raw strongest wins but individual value caps"), Fold.GetSelectedDamageMoreSources()[0].Key, FName(TEXT("00.Earlier")));
    TestEqual(TEXT("single source ceiling"), Fold.ComposedMoreProduct(Attr::AbilityDamageMultiplier), 1.30f, 0.0001f);
    Gear.AddDamageMoreSource(TEXT("00.Earlier"), Lane::Ability, 1.0f);
    TestEqual(TEXT("inactive keyed source withdraws its slot"), Gear.GetDamageMoreSources().Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerReserveSurgeRuntimeTest, "RiorsEdge.Items.ReserveSurgeRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerReserveSurgeRuntimeTest::RunTest(const FString& Parameters)
{
    const FName Surge(TEXT("Aberrant.ReserveSurge"));
    FBreakerItemInstance Item;
    int32 SurgeIndex = INDEX_NONE;
    for (int32 Seed = 1; Seed <= 4096; ++Seed)
    {
        Item = UBreakerLootLibrary::RollItem(TEXT("Test.ReserveSurge"), EBreakerEquipSlot::Helmet, EBreakerItemRarity::Aberrant, 40, Seed);
        SurgeIndex = Item.Affixes.IndexOfByPredicate([&](const FBreakerRolledAffix& A) { return A.AffixId == Surge; });
        if (SurgeIndex != INDEX_NONE) break;
    }
    if (!TestTrue(TEXT("payoff reachable through real special loot allocation"), SurgeIndex != INDEX_NONE)) return false;
    TestTrue(TEXT("real allocation includes weapon downside"), Item.Affixes.ContainsByPredicate([](const FBreakerRolledAffix& A) { return A.AffixId == FName(TEXT("Downside.Riftburn")) && FMath::IsNearlyEqual(A.Value, -12.0f); }));
    FBreakerBuildConditionState Low; Low.Set(EBreakerBuildCondition::ResourceLow, true);
    FBreakerAttributeContribution Authored, Counterfeit;
    const FBreakerEquipmentStats AuthoredStats = UBreakerEquipmentComponent::AggregateStats({Item}, &Authored, Low);
    FBreakerItemInstance Ordinary = Item; Ordinary.Rarity = EBreakerItemRarity::Exceptional;
    UBreakerEquipmentComponent::AggregateStats({Ordinary}, &Counterfeit, Low);
    TestEqual(TEXT("copied special identity on ordinary gear cannot grant More"), Counterfeit.GetDamageMoreSources().Num(), 0);
    if (!TestEqual(TEXT("one payoff is one source"), Authored.GetDamageMoreSources().Num(), 1)) return false;
    FBreakerItemInstance Control = Item; Control.Affixes[SurgeIndex].Value = 0;
    FBreakerAttributeContribution ControlOffer;
    const FBreakerEquipmentStats ControlStats = UBreakerEquipmentComponent::AggregateStats({Control}, &ControlOffer, Low);
    TestEqual(TEXT("More excluded from legacy active Increased display"), AuthoredStats.ActiveConditionalDamagePercent, ControlStats.ActiveConditionalDamagePercent);
    TestEqual(TEXT("More excluded from legacy potential Increased display"), AuthoredStats.PotentialConditionalDamagePercent, ControlStats.PotentialConditionalDamagePercent);
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated actual equipment world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real player"), Player)) return false;
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetProgression()->BindAttributes(Player->GetAttributes());
    if (!TestTrue(TEXT("real Caster resource context"), Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    Player->GetMana()->BindAttributes(Player->GetAttributes());
    UBreakerEquipmentComponent* Equipment = Player->GetEquipment();
    Equipment->BindAttributes(Player->GetAttributes());
    auto Resource = [&](float Value) { Player->GetAttributes()->ApplyClassResource(Value); Equipment->TickComponent(0.0f, LEVELTICK_All, nullptr); };
    Resource(0);
    TestTrue(TEXT("actual low resource predicate"), FBreakerBuildConditionState::EvaluateForActor(Player).IsActive(EBreakerBuildCondition::ResourceLow));
    AActor* Target = World->SpawnActor<AActor>();
    UBreakerCombatComponent* TargetCombat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(TargetCombat); TargetCombat->RegisterComponent();
    UBreakerAttributeSet* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(100000); Health->ApplyHealth(100000); TargetCombat->BindAttributes(Health);
    auto Direct = [&](EBreakerDamageDelivery Delivery)
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage = 100; Hit.DamageFamily = EBreakerDamageFamily::TrueDamage;
        Hit.bCanCritical = false; Hit.bBypassShield = true; Hit.SetInstigator(Player);
        UBreakerDamageLibrary::FillSourcePools(Player->GetAttributes(), Delivery, Hit);
        Player->GetCombat()->ApplyOutgoingModifiers(Hit);
        return TargetCombat->ReceiveDamage(Hit).HealthDamage;
    };
    // Zero only the tested line for consumer isolation; this is not a second legal roll.
    if (!TestTrue(TEXT("control really equipped"), Equipment->EquipItem(Control))) return false;
    const float BaselineAbility = Direct(EBreakerDamageDelivery::Ability);
    const float BaselineWeapon = Direct(EBreakerDamageDelivery::Weapon);
    const float BaselineDot = UBreakerCombatComponent::ComposeDotSourcePower(Player->GetAttributes(), Player->GetCombat(), EBreakerDamageDelivery::Ability);
    if (!TestTrue(TEXT("actual rolled payoff really equipped"), Equipment->EquipItem(Item))) return false;
    const float Factor = FMath::Min(1.0f + Item.Affixes[SurgeIndex].Value / 100.0f, FBreakerAttributeAggregator::SingleMoreCeiling);
    TestEqual(TEXT("live ability hit pays authored More"), Direct(EBreakerDamageDelivery::Ability), BaselineAbility * Factor, 0.01f);
    TestEqual(TEXT("payoff never multiplies weapon lane"), Direct(EBreakerDamageDelivery::Weapon), BaselineWeapon, 0.01f);
    FBreakerStatusApplicationSpec Snapshot; Snapshot.BaseDamagePerTick = 100;
    Snapshot.Snapshot.SourcePower = UBreakerCombatComponent::ComposeDotSourcePower(Player->GetAttributes(), Player->GetCombat(), EBreakerDamageDelivery::Ability);
    TestEqual(TEXT("ability DoT snapshot includes selected gear More"), Snapshot.Snapshot.SourcePower, BaselineDot * Factor, 0.0001f);
    Resource(Player->GetAttributes()->GetMaxClassResource());
    TestFalse(TEXT("resource recovery switches predicate off"), FBreakerBuildConditionState::EvaluateForActor(Player).IsActive(EBreakerBuildCondition::ResourceLow));
    TestEqual(TEXT("inactive More returns slot immediately"), Player->GetAttributes()->GetAttributeAggregator().GetDamageMoreSourceCount(), 0);
    const float RecoveredDot = UBreakerCombatComponent::ComposeDotSourcePower(Player->GetAttributes(), Player->GetCombat(), EBreakerDamageDelivery::Ability);
    if (!TestTrue(TEXT("high-resource control really equips"), Equipment->EquipItem(Control))) return false;
    TestEqual(TEXT("inactive payoff equals control at same resource state"), RecoveredDot, UBreakerCombatComponent::ComposeDotSourcePower(Player->GetAttributes(), Player->GetCombat(), EBreakerDamageDelivery::Ability), 0.0001f);
    if (!TestTrue(TEXT("actual item restores after high-resource control"), Equipment->EquipItem(Item))) return false;
    FBreakerDamageRequest Tick = UBreakerDamageLibrary::MakeSnapshotDotTick(Snapshot, EBreakerDamageFamily::TrueDamage, 1, Player, FVector::ZeroVector, false);
    TestEqual(TEXT("already applied status preserves old source power"), TargetCombat->ReceiveDamage(Tick).HealthDamage, 100.0f * Snapshot.Snapshot.SourcePower, 0.01f);
    Resource(0);
    TestEqual(TEXT("low resource reactivates without accumulation"), Direct(EBreakerDamageDelivery::Ability), BaselineAbility * Factor, 0.01f);
    // Synthetic source-budget fixture, not a claim that this is a purchased build.
    FBreakerAttributeContribution FullTree;
    FullTree.ComposeSharedMoreDamage(1.30f);
    FullTree.ComposeSharedMoreDamage(1.30f);
    FullTree.ComposeSharedMoreDamage(1.30f);
    Player->GetAttributes()->ApplyAttributeContribution(EBreakerAttributeContributor::Progression, FullTree);
    TestEqual(TEXT("real equipped gear joins three tree source offers"), Player->GetAttributes()->GetAttributeAggregator().GetDamageMoreSourceCount(), 4);
    const float CappedHit = Direct(EBreakerDamageDelivery::Ability);
    Player->GetCombat()->PushOutgoingModifier(TEXT("Test.MoreWindow"), 0, 1.30f, 10);
    TestEqual(TEXT("temporary window cannot exceed full static headroom"), Direct(EBreakerDamageDelivery::Ability), CappedHit, 0.01f);
    Player->GetAttributes()->ApplyAttributeContribution(EBreakerAttributeContributor::Progression, FBreakerAttributeContribution());
    if (!TestTrue(TEXT("real unequip"), Equipment->UnequipSlot(EBreakerEquipSlot::Helmet))) return false;
    TestEqual(TEXT("unequip withdraws source"), Player->GetAttributes()->GetAttributeAggregator().GetDamageMoreSourceCount(), 0);
    return true;
}
#endif
