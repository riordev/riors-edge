#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerRocketProjectile.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEntropyAffixRuntimeTest, "RiorsEdge.Items.EntropyConversionRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerEntropyAffixRuntimeTest::RunTest(const FString& Parameters)
{
    const FName Id(TEXT("Weapon.EntropyConversion"));
    const auto* Definition = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), Id);
    if (!TestNotNull(TEXT("ordinary registered Entropy conversion affix"), Definition)) return false;
    TestEqual(TEXT("conversion target is append-only explicit type"), Definition->StatTarget, EBreakerStatTarget::WeaponEntropyConversion);
    TestEqual(TEXT("conversion uses flat percentage points"), Definition->StatBucket, EBreakerStatBucket::Flat);
    TestEqual(TEXT("authored low tier converts twenty percent"), UBreakerAffixLibrary::ValueForTier(*Definition, 12), 20.0f);
    TestEqual(TEXT("authored normal top converts sixty percent"), UBreakerAffixLibrary::ValueForTier(*Definition, 1), 60.0f);
    TestEqual(TEXT("T0 cannot print over full conversion"), UBreakerAffixLibrary::ValueForTier(*Definition, 0), 100.0f);
    TestEqual(TEXT("T-1 cannot print over full conversion"), UBreakerAffixLibrary::ValueForTier(*Definition, -1), 100.0f);
    auto Roll = [&](EBreakerWeaponArchetype Archetype, FBreakerItemInstance& Item)
    {
        for (int32 Seed = 1; Seed <= 16384; ++Seed)
        {
            Item = UBreakerLootLibrary::RollItem(TEXT("Entropy.Runtime"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
            if (Item.WeaponArchetype != Archetype || !Item.Affixes.ContainsByPredicate([&](const auto& A) { return A.AffixId == Id; })) continue;
            // Preserve the genuine row/tier/value; isolate this consumer from other affixes.
            for (auto& Affix : Item.Affixes) if (Affix.AffixId != Id) Affix.Value = 0;
            return true;
        }
        return false;
    };
    FBreakerItemInstance Rifle, RocketItem;
    if (!TestTrue(TEXT("actual level-one rifle can roll conversion"), Roll(EBreakerWeaponArchetype::Rifle, Rifle))
        || !TestTrue(TEXT("actual level-one rocket can roll conversion"), Roll(EBreakerWeaponArchetype::Rocket, RocketItem))) return false;
    const float Rolled = Rifle.Affixes.FindByPredicate([&](const auto& A) { return A.AffixId == Id; })->Value;
    TestEqual(TEXT("actual row aggregates percentage"), UBreakerEquipmentComponent::AggregateStats({Rifle}).PrimaryEntropyConversionPercent, Rolled);
    for (auto Slot : { EBreakerEquipSlot::Secondary, EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour,
        EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Boots, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Waist })
    {
        auto Wrong = Rifle; Wrong.Slot = Slot; // Explicit malformed saved copy, not legal loot.
        TestEqual(TEXT("wrong-slot copy cannot convert Primary"), UBreakerEquipmentComponent::AggregateStats({Wrong}).PrimaryEntropyConversionPercent, 0.0f);
    }
    auto Control = Rifle; for (auto& Affix : Control.Affixes) Affix.Value = 0;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    auto* Equipment = Player->GetEquipment(); Equipment->BindAttributes(Player->GetAttributes());
    auto* Weapon = Player->GetWeapon();
    TestTrue(TEXT("real control equipment"), Equipment->EquipItem(Control)); Weapon->SyncArchetypesToEquipment();
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->HipSpreadDegrees = 0; Weapon->WeaponDefinition->AimSpreadDegrees = 0;
    Weapon->WeaponDefinition->Recoil.BloomPerShotDegrees = 0; Weapon->WeaponDefinition->BleedChance = 0;
    auto* Target = World->SpawnActor<AActor>(); if (!Target) return false;
    auto* Body = NewObject<USphereComponent>(Target); Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
    Body->SetSphereRadius(80); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); Body->RegisterComponent();
    auto* Combat = NewObject<UBreakerCombatComponent>(Target); Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target); Health->ApplyMaxHealth(1000000); Health->ApplyHealth(1000000); Combat->BindAttributes(Health);
    auto* Status = NewObject<UBreakerStatusComponent>(Target); Target->AddInstanceComponent(Status); Status->RegisterComponent(); Status->BeginPlay(); Status->SetComponentTickEnabled(false);
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye, Aim); Target->SetActorLocation(Eye + Aim.Vector() * 500);
    auto Advance = [&] { for (int32 I = 0; I < 24; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); } };
    auto Fire = [&]
    {
        Advance(); ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0);
        Weapon->ResetAmmunition(); const int32 Before = Weapon->GetMagazineAmmo();
        Weapon->StartFire(); Weapon->StopFire();
        TestEqual(TEXT("actual weapon trigger spends one round"), Weapon->GetMagazineAmmo(), Before - 1);
    };
    const float PlainHealth = Health->GetHealth(); Fire();
    TestTrue(TEXT("control actually damages collision target"), Health->GetHealth() < PlainHealth);
    TestEqual(TEXT("ordinary shot has no Entropy buildup"), Status->GetEntropyBuildup(), 0.0f);
    TestTrue(TEXT("actual converted rifle equips"), Equipment->EquipItem(Rifle)); Fire();
    const float RifleBuildup = Status->GetEntropyBuildup();
    TestTrue(TEXT("actual converted rifle hit builds Entropy"), RifleBuildup > 0);
    Weapon->EquipSlot(2); Fire();
    TestEqual(TEXT("Primary conversion cannot leak into actual Secondary shot"), Status->GetEntropyBuildup(), RifleBuildup);
    TestTrue(TEXT("real converted rocket item equips"), Equipment->EquipItem(RocketItem));
    Weapon->WeaponDefinition = nullptr; Weapon->SyncArchetypesToEquipment(); Weapon->EquipSlot(1); Fire();
    ABreakerRocketProjectile* Rocket = nullptr;
    for (TActorIterator<ABreakerRocketProjectile> It(World); It; ++It) if (!It->HasExploded()) { Rocket = *It; break; }
    if (!TestNotNull(TEXT("actual converted weapon launches projectile"), Rocket)) return false;
    if (auto* Movement = Rocket->FindComponentByClass<UProjectileMovementComponent>()) { Movement->StopMovementImmediately(); Movement->Deactivate(); }
    Rocket->SetActorEnableCollision(false); // Hold actual fired request for deterministic impact timing.
    TestTrue(TEXT("remove conversion after launch"), Equipment->EquipItem(Control));
    const float BeforeImpact = Status->GetEntropyBuildup(); Rocket->Explode(Target->GetActorLocation());
    TestTrue(TEXT("real projectile retains fire-time Entropy conversion after equipment replacement"), Status->GetEntropyBuildup() > BeforeImpact);
    return true;
}
#endif
