#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerAbilityFlatPowerTest, "RiorsEdge.Items.AbilityFlatPower",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerAbilityFlatPowerTest::RunTest(const FString& Parameters)
{
    const FName AffixId(TEXT("Ability.AddedPower"));
    const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), AffixId);
    if (!TestNotNull(TEXT("authored ordinary added ability power"), Definition)) return false;
    TestEqual(TEXT("separate flat bucket"), Definition->StatBucket, EBreakerStatBucket::Flat);
    TestEqual(TEXT("ability-only target"), Definition->StatTarget, EBreakerStatTarget::AbilityDamage);
    TestEqual(TEXT("O2 starting anchor"), Definition->ValueAtT12, 1.0f);
    TestEqual(TEXT("O2 top ordinary anchor"), Definition->ValueAtT1, 11.0f);
    FBreakerItemInstance Item;
    int32 PowerIndex = INDEX_NONE;
    for (int32 Seed = 1; Seed <= 2048; ++Seed)
    {
        Item = UBreakerLootLibrary::RollItem(TEXT("Test.AbilityPower"), EBreakerEquipSlot::Helmet, EBreakerItemRarity::Standard, 1, Seed);
        PowerIndex = Item.Affixes.IndexOfByPredicate([&](const FBreakerRolledAffix& Affix) { return Affix.AffixId == AffixId; });
        if (PowerIndex != INDEX_NONE) break;
    }
    if (!TestTrue(TEXT("new prefix actually rolls in ordinary loot"), PowerIndex != INDEX_NONE)) return false;
    const float Added = Item.Affixes[PowerIndex].Value;
    if (!TestTrue(TEXT("rolled power is positive"), Added > 0)) return false;
    // A/B control keeps every other actual rolled line identical. Only this
    // tested line is zeroed, explicitly as a consumer-isolation fixture; the
    // control is not claimed to be another legal loot roll.
    FBreakerItemInstance Control = Item;
    Control.Affixes[PowerIndex].Value = 0;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated equipment combat world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real equipment owner"), Player)) return false;
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    UBreakerEquipmentComponent* Equipment = Player->GetEquipment();
    Equipment->BindAttributes(Player->GetAttributes());
    AActor* Target = World->SpawnActor<AActor>();
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    UBreakerAttributeSet* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(100000); Health->ApplyHealth(100000); Combat->BindAttributes(Health);
    auto Direct = [&](EBreakerDamageDelivery Delivery)
    {
        FBreakerDamageRequest Hit; Hit.BaseDamage = 100; Hit.DamageFamily = EBreakerDamageFamily::TrueDamage;
        Hit.bCanCritical = false; Hit.bBypassShield = true; Hit.SetInstigator(Player);
        UBreakerDamageLibrary::FillSourcePools(Player->GetAttributes(), Delivery, Hit);
        Player->GetCombat()->ApplyOutgoingModifiers(Hit);
        return Combat->ReceiveDamage(Hit).HealthDamage;
    };
    if (!TestTrue(TEXT("control item really equips"), Equipment->EquipItem(Control))) return false;
    const float AbilityBefore = Direct(EBreakerDamageDelivery::Ability);
    const float WeaponBefore = Direct(EBreakerDamageDelivery::Weapon);
    const float AbilityDotBefore = UBreakerCombatComponent::ComposeDotSourcePower(Player->GetAttributes(), Player->GetCombat(), EBreakerDamageDelivery::Ability);
    const float WeaponDotBefore = UBreakerCombatComponent::ComposeDotSourcePower(Player->GetAttributes(), Player->GetCombat(), EBreakerDamageDelivery::Weapon);
    if (!TestTrue(TEXT("actual rolled item really equips"), Equipment->EquipItem(Item))) return false;
    const float FlatFactor = 1.0f + Added / 100.0f;
    TestEqual(TEXT("flat power multiplies existing ability Increased pool in real damage"), Direct(EBreakerDamageDelivery::Ability), AbilityBefore * FlatFactor, 0.01f);
    TestEqual(TEXT("weapon damage is isolated"), Direct(EBreakerDamageDelivery::Weapon), WeaponBefore, 0.01f);
    FBreakerStatusApplicationSpec Snapshot;
    Snapshot.BaseDamagePerTick = 100;
    Snapshot.Snapshot.SourcePower = UBreakerCombatComponent::ComposeDotSourcePower(Player->GetAttributes(), Player->GetCombat(), EBreakerDamageDelivery::Ability);
    TestEqual(TEXT("ability DoT application captures flat power"), Snapshot.Snapshot.SourcePower, AbilityDotBefore * FlatFactor, 0.0001f);
    TestEqual(TEXT("weapon DoT application is isolated"), UBreakerCombatComponent::ComposeDotSourcePower(Player->GetAttributes(), Player->GetCombat(), EBreakerDamageDelivery::Weapon), WeaponDotBefore, 0.0001f);
    auto Tick = [&]()
    {
        FBreakerDamageRequest Hit = UBreakerDamageLibrary::MakeSnapshotDotTick(Snapshot, EBreakerDamageFamily::TrueDamage, 1, Player, FVector::ZeroVector, false);
        return Combat->ReceiveDamage(Hit).HealthDamage;
    };
    const float CapturedTick = Tick();
    TestTrue(TEXT("captured status request actually deals damage"), CapturedTick > 0);
    if (!TestTrue(TEXT("item actually unequips"), Equipment->UnequipSlot(EBreakerEquipSlot::Helmet))) return false;
    TestEqual(TEXT("already captured DoT survives unequip unchanged"), Tick(), CapturedTick, 0.01f);
    if (!TestTrue(TEXT("control re-equips after teardown"), Equipment->EquipItem(Control))) return false;
    TestEqual(TEXT("flat contribution removed without accumulation"), Direct(EBreakerDamageDelivery::Ability), AbilityBefore, 0.01f);
    TestEqual(TEXT("new snapshot no longer contains added power"), UBreakerCombatComponent::ComposeDotSourcePower(Player->GetAttributes(), Player->GetCombat(), EBreakerDamageDelivery::Ability), AbilityDotBefore, 0.0001f);
    return true;
}
#endif
