#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerRocketProjectile.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerWeaponUtilityRuntimeTest, "RiorsEdge.Items.WeaponUtilityRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerWeaponUtilityRuntimeTest::RunTest(const FString& Parameters)
{
    auto RollUtility = [&](FName Id, FBreakerItemInstance& Out)
    {
        for (int32 Seed = 1; Seed <= 8192; ++Seed)
        {
            Out = UBreakerLootLibrary::RollItem(TEXT("Utility.Runtime"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
            if (Out.WeaponArchetype != EBreakerWeaponArchetype::Rifle) continue;
            if (!Out.Affixes.ContainsByPredicate([Id](const FBreakerRolledAffix& A) { return A.AffixId == Id; })) continue;
            // Actual row, tier and roll remain intact. Zero unrelated lines
            // only to isolate its consumer; this is not a second legal roll.
            for (auto& Affix : Out.Affixes) if (Affix.AffixId != Id) Affix.Value = 0;
            return true;
        }
        return false;
    };
    FBreakerItemInstance MagazineItem, RangeItem;
    if (!TestTrue(TEXT("ordinary Rifle can roll magazine capacity"), RollUtility(TEXT("Weapon.MagazineCapacity"), MagazineItem))
        || !TestTrue(TEXT("ordinary Rifle can roll effective range"), RollUtility(TEXT("Weapon.EffectiveRange"), RangeItem))) return false;
    FBreakerItemInstance Control = MagazineItem;
    for (auto& Affix : Control.Affixes) Affix.Value = 0;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated weapon world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real character without save-loading BeginPlay"), Player)) return false;
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    UBreakerEquipmentComponent* Equipment = Player->GetEquipment();
    Equipment->BindAttributes(Player->GetAttributes());
    UBreakerWeaponComponent* Weapon = Player->GetWeapon();
    if (!TestTrue(TEXT("control Primary equips"), Equipment->EquipItem(Control))) return false;
    Weapon->SyncArchetypesToEquipment();
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    UBreakerWeaponDefinition* Definition = Weapon->WeaponDefinition;
    Definition->ReloadDuration = 0.2f;
    Definition->bProjectile = false; Definition->PelletsPerShot = 1;
    Definition->HipSpreadDegrees = 0; Definition->AimSpreadDegrees = 0;
    Definition->BleedChance = 0;
    Definition->FalloffStart = 400; Definition->FalloffEnd = 800;
    Definition->MaximumRange = 2000; Definition->MinimumFalloffMultiplier = 0.5f;
    auto Advance = [&](int32 Steps) { for (int32 Step = 0; Step < Steps; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.05f); } };
    auto Total = [&] { return Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo(); };
    Weapon->ResetAmmunition();
    const int32 BaseCapacity = Weapon->GetEffectiveMagazineSize();
    const int32 InitialTotal = Total();
    if (!TestTrue(TEXT("real magazine row equips"), Equipment->EquipItem(MagazineItem))) return false;
    const int32 Expanded = Weapon->GetEffectiveMagazineSize();
    TestTrue(TEXT("positive capacity roll adds usable rounds"), Expanded > BaseCapacity);
    TestEqual(TEXT("capacity increase never grants ammunition"), Total(), InitialTotal);
    TestEqual(TEXT("existing magazine is not magically topped off"), Weapon->GetMagazineAmmo(), BaseCapacity);
    Weapon->StartReload();
    if (!TestTrue(TEXT("expanded capacity permits a real reload"), Weapon->IsReloading())) return false;
    Advance(6);
    TestFalse(TEXT("actual reload timer completes"), Weapon->IsReloading());
    TestEqual(TEXT("reload fills expanded magazine"), Weapon->GetMagazineAmmo(), Expanded);
    TestEqual(TEXT("expanded reload only transfers reserve"), Total(), InitialTotal);
    if (!TestTrue(TEXT("same-archetype replacement removes capacity"), Equipment->EquipItem(Control))) return false;
    TestEqual(TEXT("replacement immediately clamps active magazine"), Weapon->GetMagazineAmmo(), BaseCapacity);
    TestEqual(TEXT("displaced real rounds return to same reserve"), Total(), InitialTotal);
    if (!TestTrue(TEXT("capacity re-equips"), Equipment->EquipItem(MagazineItem))) return false;
    const FName Temporary(TEXT("Utility.CapacityWindow"));
    Weapon->PushMagazineCapacityOverride(Temporary, 3);
    TestEqual(TEXT("temporary rounds add after percentage capacity"), Weapon->GetEffectiveMagazineSize(), Expanded + 3);
    Weapon->PopMagazineCapacityOverride(Temporary);
    TestEqual(TEXT("temporary pop retains equipment capacity"), Weapon->GetEffectiveMagazineSize(), Expanded);
    Weapon->StartReload(); Advance(6);
    TestEqual(TEXT("Primary is expanded before storing inactive slot"), Weapon->GetMagazineAmmo(), Expanded);
    const FName Paid(TEXT("Utility.PaidWindow"));
    TestEqual(TEXT("real reserve buys three temporary rounds"), Weapon->PushMagazineCapacityOverride(Paid, 3, 2), 3);
    Weapon->EquipSlot(2); Advance(14);
    const int32 SecondaryCapacity = Weapon->GetActiveDefinition()->MagazineSize;
    const int32 SecondaryTotal = Total();
    TestEqual(TEXT("Primary magazine affix cannot expand Secondary"), Weapon->GetEffectiveMagazineSize(), SecondaryCapacity);
    Weapon->PopMagazineCapacityOverride(Paid);
    TestEqual(TEXT("inactive paid pop never refunds into Secondary"), Total(), SecondaryTotal);
    Weapon->EquipSlot(1); Advance(14);
    TestEqual(TEXT("inactive unspent conversion refunds original reserve at purchase ratio"), Total(), InitialTotal);
    TestEqual(TEXT("paid pop restores Primary equipment capacity"), Weapon->GetMagazineAmmo(), Expanded);
    const FName Shrink(TEXT("Utility.ShrinkWindow"));
    Weapon->PushMagazineCapacityOverride(Shrink, -3);
    Weapon->EquipSlot(2); Advance(14);
    const int32 SecondaryMagazine = Weapon->GetMagazineAmmo();
    if (!TestTrue(TEXT("real replacement refreshes equipment during inactive shrink"), Equipment->EquipItem(MagazineItem))) return false;
    TestEqual(TEXT("inactive negative override cannot shrink Secondary on equipment refresh"), Weapon->GetMagazineAmmo(), SecondaryMagazine);
    TestEqual(TEXT("negative override refresh preserves Secondary total"), Total(), SecondaryTotal);
    Weapon->PopMagazineCapacityOverride(Shrink);
    Weapon->EquipSlot(1); Advance(14);
    Weapon->StartReload(); Advance(6);
    TestEqual(TEXT("Primary refills after temporary shrink ends"), Weapon->GetMagazineAmmo(), Expanded);
    Weapon->EquipSlot(2); Advance(14);
    if (!TestTrue(TEXT("inactive Primary really unequips"), Equipment->UnequipSlot(EBreakerEquipSlot::Primary))) return false;
    TestEqual(TEXT("inactive shrink cannot touch Secondary ammunition"), Total(), SecondaryTotal);
    Weapon->EquipSlot(1); Advance(14);
    TestEqual(TEXT("stored Primary overflow does not return on swap"), Weapon->GetMagazineAmmo(), BaseCapacity);
    TestEqual(TEXT("inactive shrink conserves Primary reserve and rounds"), Total(), InitialTotal);

    // A real collision target isolates range/falloff from enemy AI and armour.
    AActor* Target = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("real range collision target"), Target)) return false;
    USphereComponent* Body = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
    Body->SetSphereRadius(10); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    Body->RegisterComponent();
    UBreakerCombatComponent* TargetCombat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(TargetCombat); TargetCombat->RegisterComponent();
    UBreakerAttributeSet* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(1000000); Health->ApplyHealth(1000000); TargetCombat->BindAttributes(Health);
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye, Aim);
    auto Fire = [&]
    {
        Advance(24);
        Player->GetAbilitySystemComponent()->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0);
        Weapon->ResetAmmunition();
        const int32 AmmoBefore = Weapon->GetMagazineAmmo();
        const float Before = Health->GetHealth();
        Weapon->StartFire(); Weapon->StopFire();
        TestTrue(TEXT("actual trigger produces a shot"), Weapon->GetLastShot().bFired);
        TestEqual(TEXT("fresh trigger consumes one actual round"), Weapon->GetMagazineAmmo(), AmmoBefore - 1);
        return Before - Health->GetHealth();
    };
    if (!TestTrue(TEXT("range control equips"), Equipment->EquipItem(Control))) return false;
    Target->SetActorLocation(Eye + Aim.Vector() * 600.0f);
    const float FalloffDamage = Fire();
    if (!TestTrue(TEXT("control actually hits target in falloff band"), FalloffDamage > 0)) return false;
    if (!TestTrue(TEXT("range roll really equips"), Equipment->EquipItem(RangeItem))) return false;
    const float RangeMultiplier = Weapon->GetEffectiveRangeMultiplier();
    TestTrue(TEXT("equipped range has positive multiplier"), RangeMultiplier > 1);
    TestTrue(TEXT("actual hit damage improves at unchanged falloff distance"), Fire() > FalloffDamage);
    const float ExtendedMaximum = Weapon->GetEffectiveMaximumRange();
    const float TargetDistance = (Definition->MaximumRange + ExtendedMaximum) * 0.5f;
    Target->SetActorLocation(Eye + Aim.Vector() * TargetDistance);
    TestTrue(TEXT("range affix reaches beyond ordinary maximum"), Fire() > 0);
    if (!TestTrue(TEXT("range row removal via replacement"), Equipment->EquipItem(Control))) return false;
    TestEqual(TEXT("same target lies beyond unmodified actual trace"), Fire(), 0.0f);
    if (!TestTrue(TEXT("range restored before secondary check"), Equipment->EquipItem(RangeItem))) return false;
    Weapon->EquipSlot(2); Advance(14);
    TestEqual(TEXT("Primary range affix does not extend Secondary"), Weapon->GetEffectiveRangeMultiplier(), 1.0f);
    TestEqual(TEXT("Secondary maximum remains its authored definition"), Weapon->GetEffectiveMaximumRange(), Weapon->GetActiveDefinition()->MaximumRange);
    // Direct archetype selection is an explicit harness setup. The real
    // equipped Primary range row still supplies the production multiplier.
    Weapon->WeaponDefinition = nullptr;
    Weapon->SetSlotArchetype(1, EBreakerWeaponArchetype::Rocket);
    Weapon->EquipSlot(1); Advance(24);
    const UBreakerWeaponDefinition* RocketDefinition = Weapon->GetActiveDefinition();
    if (!TestTrue(TEXT("actual rocket definition selected"), RocketDefinition->bProjectile)) return false;
    const float ExpectedLifetime = Weapon->GetEffectiveMaximumRange() / RocketDefinition->ProjectileSpeed;
    Weapon->StartFire(); Weapon->StopFire();
    ABreakerRocketProjectile* FiredRocket = nullptr;
    int32 RocketCount = 0;
    for (TActorIterator<ABreakerRocketProjectile> It(World); It; ++It)
        if (!It->IsActorBeingDestroyed()) { FiredRocket = *It; ++RocketCount; }
    if (!TestEqual(TEXT("actual weapon trigger spawns one rocket"), RocketCount, 1)) return false;
    FiredRocket->DispatchBeginPlay();
    TestTrue(TEXT("weapon passes equipped effective maximum into real rocket lifetime"),
        FMath::IsNearlyEqual(FiredRocket->GetLifeSpan(), ExpectedLifetime, 0.001f));
    TestTrue(TEXT("range-equipped rocket exceeds ordinary flight budget"),
        FiredRocket->GetLifeSpan() > RocketDefinition->MaximumRange / RocketDefinition->ProjectileSpeed);
    return true;
}
#endif
