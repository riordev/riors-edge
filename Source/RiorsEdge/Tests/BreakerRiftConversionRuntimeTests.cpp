#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerElementConversionMath.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerRift.h"
#include "Tests/BreakerRiftRuntimeObserver.h"
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
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRiftConversionRuntimeTest, "RiorsEdge.Items.RiftConversionRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRiftConversionRuntimeTest::RunTest(const FString& Parameters)
{
    using BreakerElementConversion::Select;
    TestEqual(TEXT("no conversion preserves physical delivery"), Select(0, 0, 0).Element, EBreakerElement::None);
    TestEqual(TEXT("stronger Rift wins"), Select(.2f, .3f, 0, .4f).Element, EBreakerElement::Rift);
    TestEqual(TEXT("stronger Entropy wins"), Select(.4f, .2f, 0, .3f).Element, EBreakerElement::Entropy);
    TestEqual(TEXT("stronger Void wins"), Select(.2f, .4f, 0, .3f).Element, EBreakerElement::Void);
    TestEqual(TEXT("three-way ties preserve Entropy"), Select(.4f, .4f, 0, .4f).Element, EBreakerElement::Entropy);
    TestEqual(TEXT("Void wins its exact tie with Rift"), Select(0, .4f, 0, .4f).Element, EBreakerElement::Void);
    TestEqual(TEXT("full Attunement overrides full Rift on a tie"), Select(0, 0, 1, 1).Element, EBreakerElement::Entropy);
    TestEqual(TEXT("conversion shares never add together"), Select(.3f, .4f, .6f, .7f).Fraction, .7f);
    TestEqual(TEXT("invalid shares cannot poison selection"), Select(std::numeric_limits<float>::quiet_NaN(), 0,
        std::numeric_limits<float>::infinity(), .2f).Element, EBreakerElement::Rift);
    TestEqual(TEXT("negative shares refuse conversion"), Select(-1, -1, -1, -1).Element, EBreakerElement::None);
    TestEqual(TEXT("oversized share cannot exceed full conversion"), Select(0, 0, 0, 2).Fraction, 1.0f);
    const FName RiftId(TEXT("Weapon.RiftConversion"));
    const FName EntropyId(TEXT("Weapon.EntropyConversion"));
    const auto* Definition = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), RiftId);
    if (!TestNotNull(TEXT("registered ordinary Rift conversion affix"), Definition)) return false;
    TestEqual(TEXT("append-only conversion target"), Definition->StatTarget, EBreakerStatTarget::WeaponRiftConversion);
    TestEqual(TEXT("flat percentage points"), Definition->StatBucket, EBreakerStatBucket::Flat);
    TestEqual(TEXT("O2 low-tier conversion"), UBreakerAffixLibrary::ValueForTier(*Definition, 12), 20.0f);
    TestEqual(TEXT("O2 normal-top conversion"), UBreakerAffixLibrary::ValueForTier(*Definition, 1), 60.0f);
    for (int32 Tier : {0, -1})
    {
        TestEqual(TEXT("tier spike is capped at full conversion"), UBreakerAffixLibrary::ValueForTier(*Definition, Tier), 100.0f);
        TestEqual(TEXT("rolled tier-spike variation cannot exceed full conversion"), UBreakerAffixLibrary::RollValueForTier(*Definition, Tier, 1), 100.0f);
    }
    auto Roll = [&](EBreakerWeaponArchetype Archetype, bool bBoth, FBreakerItemInstance& Item)
    {
        for (int32 Seed = 1; Seed <= 32768; ++Seed)
        {
            Item = UBreakerLootLibrary::RollItem(TEXT("Rift.Runtime"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
            if (Item.WeaponArchetype != Archetype
                || !Item.Affixes.ContainsByPredicate([&](const auto& Row) { return Row.AffixId == RiftId; })
                || (bBoth && !Item.Affixes.ContainsByPredicate([&](const auto& Row) { return Row.AffixId == EntropyId; }))) continue;
            // Keep real conversion row values/tiers; isolate co-rolls only.
            for (auto& Row : Item.Affixes)
                if (Row.AffixId != RiftId && !(bBoth && Row.AffixId == EntropyId)) Row.Value = 0;
            return true;
        }
        return false;
    };
    FBreakerItemInstance Rifle, RocketItem, Mixed;
    if (!TestTrue(TEXT("ordinary level-one rifle can roll Rift"), Roll(EBreakerWeaponArchetype::Rifle, false, Rifle))
        || !TestTrue(TEXT("ordinary level-one rocket can roll Rift"), Roll(EBreakerWeaponArchetype::Rocket, false, RocketItem))
        || !TestTrue(TEXT("ordinary weapon can actually roll both conversions"), Roll(EBreakerWeaponArchetype::Rifle, true, Mixed))) return false;
    const float Rolled = Rifle.Affixes.FindByPredicate([&](const auto& Row) { return Row.AffixId == RiftId; })->Value;
    TestEqual(TEXT("actual conversion aggregates"), UBreakerEquipmentComponent::AggregateStats({Rifle}).PrimaryRiftConversionPercent, Rolled);
    for (auto Slot : { EBreakerEquipSlot::Secondary, EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour,
        EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Boots, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Waist })
    {
        auto Wrong = Rifle; Wrong.Slot = Slot; // Explicit malformed save-copy rejection.
        TestEqual(TEXT("wrong-slot copy cannot convert Primary"), UBreakerEquipmentComponent::AggregateStats({Wrong}).PrimaryRiftConversionPercent, 0.0f);
        TestFalse(TEXT("ordinary roll/treatment eligibility refuses other slots"), UBreakerAffixLibrary::IsEligibleForItem(*Definition, Wrong, 12));
    }
    auto Control = Rifle; for (auto& Row : Control.Affixes) Row.Value = 0;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated weapon world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    if (!Player) return false;
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    auto* Equipment = Player->GetEquipment(); Equipment->BindAttributes(Player->GetAttributes());
    auto* Weapon = Player->GetWeapon();
    if (!TestTrue(TEXT("control item actually equips"), Equipment->EquipItem(Control))) return false;
    Weapon->SyncArchetypesToEquipment();
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    // Exact collision accounting, not extra offense: eliminate spread/crit and
    // an unrelated physical ailment while retaining shipped base damage/ammo.
    Weapon->WeaponDefinition->HipSpreadDegrees = 0; Weapon->WeaponDefinition->AimSpreadDegrees = 0;
    Weapon->WeaponDefinition->Recoil.BloomPerShotDegrees = 0; Weapon->WeaponDefinition->BleedChance = 0;
    ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0);
    Weapon->ResetAmmunition();
    auto* Target = World->SpawnActor<AActor>();
    if (!Target) return false;
    auto* Body = NewObject<USphereComponent>(Target);
    Target->AddInstanceComponent(Body); Target->SetRootComponent(Body);
    Body->SetSphereRadius(80); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    Body->RegisterComponent();
    auto* Combat = NewObject<UBreakerCombatComponent>(Target);
    Target->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* Health = NewObject<UBreakerAttributeSet>(Target);
    Health->ApplyMaxHealth(1000); Health->ApplyHealth(1000); Combat->BindAttributes(Health);
    auto* Status = NewObject<UBreakerStatusComponent>(Target);
    Target->AddInstanceComponent(Status); Status->RegisterComponent();
    Status->RegisterAllComponentTickFunctions(true); Status->SetComponentTickEnabled(true); Status->BeginPlay();
    if (!TestTrue(TEXT("actual status component is registered for world ticking"), Status->PrimaryComponentTick.IsTickFunctionRegistered())) return false;
    auto* Observer = NewObject<UBreakerRiftRuntimeObserver>(Target);
    Observer->Status = Status;
    Combat->OnDamageReceived.AddDynamic(Observer, &UBreakerRiftRuntimeObserver::OnDamage);
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye, Aim);
    Target->SetActorLocation(Eye + Aim.Vector() * 500);
    auto Fire = [&](int32 Steps = 24)
    {
        for (int32 Step = 0; Step < Steps; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); }
        ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0);
        Observer->Results.Reset();
        const int32 Before = Weapon->GetMagazineAmmo();
        Weapon->StartFire(); Weapon->StopFire();
        TestEqual(TEXT("actual trigger spends one ordinary round"), Weapon->GetMagazineAmmo(), Before - 1);
    };
    const float PlainHealth = Health->GetHealth(); Fire();
    const float PlainDamage = PlainHealth - Health->GetHealth();
    if (!TestTrue(TEXT("control actually hits"), PlainDamage > 0)) return false;
    TestEqual(TEXT("control builds no Rift"), Status->GetRiftBuildup(), 0.0f);
    if (!TestTrue(TEXT("actual converted rifle equips"), Equipment->EquipItem(Rifle))) return false;
    const float BeforeRiftShot = Health->GetHealth(); Fire();
    TestEqual(TEXT("conversion does not invent immediate damage"), BeforeRiftShot - Health->GetHealth(), PlainDamage, .001f);
    TestTrue(TEXT("actual converted rifle earns Rift buildup"), Status->GetRiftBuildup() > 0);
    TestEqual(TEXT("Rift-selected hit cannot also build Entropy"), Status->GetEntropyBuildup(), 0.0f);
    TestEqual(TEXT("Rift-selected hit cannot also build Void"), Status->GetVoidBuildup(), 0.0f);
    const FGameplayTag Unstable = FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"));
    // Normal ammunition and world-ticked buildup: the target survives enough
    // actual converted hits to earn Unstable without offense grants.
    for (int32 Shot = 0; Shot < 30 && !Status->HasStatus(Unstable) && Weapon->GetMagazineAmmo() > 0; ++Shot) Fire();
    if (!TestTrue(TEXT("ordinary converted rifle earns Unstable before killing chassis"), Status->HasStatus(Unstable))) return false;
    TestFalse(TEXT("target remains alive after immediate payout"), Combat->IsDead());
    const auto* Earned = Status->GetActiveStatuses().FindByPredicate([Unstable](const auto& Entry) { return Entry.Spec.StatusTag == Unstable; });
    if (!TestNotNull(TEXT("actual earned Unstable marker"), Earned)) return false;
    TestEqual(TEXT("immediately paid marker has no reusable damage"), Earned->UnpaidDamageBudget, 0.0f);
    if (!TestEqual(TEXT("threshold shot dispatches direct damage then one activation"), Observer->Results.Num(), 2)) return false;
    TestEqual(TEXT("first callback is ordinary direct weapon damage"), Observer->Results[0].HealthDamage, PlainDamage, .001f);
    const float Budget = Observer->Results[0].RawDamage * Rolled / 100.0f * BreakerRift::DamageFraction();
    TestEqual(TEXT("second callback pays only earned Rift share"), Observer->Results[1].HealthDamage, Budget, .001f);
    TestTrue(TEXT("callbacks observe direct damage before additional activation"),
        Observer->Results[0].RemainingHealth > Observer->Results[1].RemainingHealth);
    Fire(4);
    TestTrue(TEXT("follow-up paid shot occurs inside active marker"), Status->HasStatus(Unstable));
    if (!TestEqual(TEXT("active marker suppresses repeated activation"), Observer->Results.Num(), 1)) return false;
    TestEqual(TEXT("active marker still permits normal weapon damage"), Observer->Results[0].HealthDamage, PlainDamage, .001f);
    const float BeforeExpiry = Health->GetHealth();
    const double BeforeClock = World->GetTimeSeconds();
    for (int32 Step = 0; Step < 44; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); }
    TestTrue(TEXT("real world advances beyond marker deadline"), World->GetTimeSeconds() - BeforeClock > 2.1);
    TestEqual(TEXT("world-ticked expiry never pays the spent budget twice"), Health->GetHealth(), BeforeExpiry);
    TestFalse(TEXT("world-ticked marker expires"), Status->HasStatus(Unstable));
    Status->ConsumeAllStatuses(); Status->AdvanceStatuses(20); Health->ApplyHealth(1000);
    Weapon->EquipSlot(2);
    if (!TestEqual(TEXT("actually select Secondary"), Weapon->GetCurrentSlot(), 2)) return false;
    Fire();
    TestEqual(TEXT("Primary Rift cannot spill into Secondary"), Status->GetRiftBuildup(), 0.0f);
    if (!TestTrue(TEXT("real mixed item equips"), Equipment->EquipItem(Mixed))) return false;
    Weapon->EquipSlot(1);
    if (!TestEqual(TEXT("actually select Primary"), Weapon->GetCurrentSlot(), 1)) return false;
    const auto MixedStats = Equipment->GetStats();
    const auto Expected = Select(MixedStats.PrimaryEntropyConversionPercent / 100, 0, 0, MixedStats.PrimaryRiftConversionPercent / 100);
    Fire();
    TestTrue(TEXT("mixed weapon applies only selected element"), Expected.Element == EBreakerElement::Rift
        ? Status->GetRiftBuildup() > 0 && Status->GetEntropyBuildup() == 0
        : Status->GetEntropyBuildup() > 0 && Status->GetRiftBuildup() == 0);
    TestEqual(TEXT("two-row selection never invents third-element buildup"), Status->GetVoidBuildup(), 0.0f);
    Status->ConsumeAllStatuses(); Status->AdvanceStatuses(20);
    Health->ApplyMaxHealth(1000000); Health->ApplyHealth(1000000);
    if (!TestTrue(TEXT("real converted rocket equips"), Equipment->EquipItem(RocketItem))) return false;
    Weapon->WeaponDefinition = nullptr; Weapon->SyncArchetypesToEquipment(); Weapon->ResetAmmunition();
    Fire();
    ABreakerRocketProjectile* Rocket = nullptr;
    for (TActorIterator<ABreakerRocketProjectile> It(World); It; ++It)
        if (!It->HasExploded()) { Rocket = *It; break; }
    if (!TestNotNull(TEXT("actual trigger launches rocket"), Rocket)) return false;
    if (auto* Movement = Rocket->FindComponentByClass<UProjectileMovementComponent>())
    { Movement->StopMovementImmediately(); Movement->Deactivate(); }
    Rocket->SetActorEnableCollision(false);
    if (!TestTrue(TEXT("replace gear after actual launch"), Equipment->EquipItem(Control))) return false;
    // Explicit production impact seam isolates fire-time snapshot from flight.
    Rocket->Explode(Target->GetActorLocation());
    TestTrue(TEXT("actual fired projectile retains Rift after gear replacement"), Status->GetRiftBuildup() > 0);
    TestEqual(TEXT("replacement does not retag the projectile"), Status->GetEntropyBuildup(), 0.0f);
    TestEqual(TEXT("fired Rift projectile never builds Void too"), Status->GetVoidBuildup(), 0.0f);
    return true;
}
#endif
