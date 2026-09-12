#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Weapons/BreakerRocketProjectile.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDamageRampRuntimeTest, "RiorsEdge.Items.DamageRampRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDamageRampRuntimeTest::RunTest(const FString& Parameters)
{
    FBreakerItemInstance Item;
    for (int32 Seed = 1; Seed <= 4096; ++Seed)
    {
        Item = UBreakerLootLibrary::RollItem(TEXT("Ramp.Primary"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
        if (Item.Affixes.ContainsByPredicate([](const FBreakerRolledAffix& A) { return A.AffixId == FName(TEXT("Weapon.DamageRamp")); })) break;
    }
    if (!TestTrue(TEXT("ordinary level-one Primary really rolls Damage Ramp"), Item.Affixes.ContainsByPredicate([](const FBreakerRolledAffix& A) { return A.AffixId == FName(TEXT("Weapon.DamageRamp")); }))) return false;
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated runtime world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("real player"), Player)) return false;
    // No Character BeginPlay/save hooks. Actual world time advances for weapon cadence.
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetEquipment()->BindAttributes(Player->GetAttributes());
    if (!TestTrue(TEXT("real rolled Primary equips"), Player->GetEquipment()->EquipItem(Item))) return false;
    UBreakerWeaponComponent* Weapon = Player->GetWeapon();
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->bProjectile = false;
    Weapon->WeaponDefinition->PelletsPerShot = 1;
    Weapon->WeaponDefinition->HipSpreadDegrees = 0;
    Weapon->WeaponDefinition->AimSpreadDegrees = 0;
    Weapon->WeaponDefinition->BleedChance = 0;
    const float PerStack = Weapon->GetDamageRampPerStack();
    TestTrue(TEXT("live equipped affix has a visible per-stack value"), PerStack > 0);
    AActor* Victim = World->SpawnActor<AActor>();
    USphereComponent* Body = NewObject<USphereComponent>(Victim);
    Victim->AddInstanceComponent(Body); Victim->SetRootComponent(Body);
    Body->SetSphereRadius(200);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    Body->RegisterComponent(); Victim->SetActorLocation(FVector(1000, 0, 100));
    UBreakerAttributeSet* Health = NewObject<UBreakerAttributeSet>(Victim);
    Health->ApplyMaxHealth(1000000); Health->ApplyHealth(1000000);
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Victim);
    Victim->AddInstanceComponent(Combat); Combat->RegisterComponent(); Combat->BindAttributes(Health);
    TOptional<bool> HeldRedlineBand;
    auto Fire = [&]()
    {
        const double BeforeTime = World->GetTimeSeconds();
        for (int32 Step = 0; Step < 30; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.05f); }
        TestTrue(TEXT("actual cadence clock advanced"), World->GetTimeSeconds() - BeforeTime > 1.4);
        if (HeldRedlineBand.IsSet())
        {
            // Isolate the ramp's band consumer from passive decay during cadence waits.
            UBreakerMomentumComponent* Loop = Player->FindComponentByClass<UBreakerMomentumComponent>();
            Player->GetAttributes()->ApplyClassResource(HeldRedlineBand.GetValue() ? Player->GetAttributes()->GetMaxClassResource() : 0);
            Loop->BindAttributes(Player->GetAttributes());
            TestEqual(TEXT("actual Momentum band at shot"), Loop->GetMomentumState(), HeldRedlineBand.GetValue() ? EBreakerMomentumState::Redline : EBreakerMomentumState::Settled);
        }
        Player->GetAbilitySystemComponent()->SetNumericAttributeBase(UBreakerAttributeSet::GetCriticalChanceAttribute(), 0);
        Weapon->ResetAmmunition(); Weapon->StartFire(); Weapon->StopFire();
        return Weapon->GetLastShot();
    };
    const float BeforeMultiplier = Player->GetAttributes()->GetDamageMultiplier();
    const float BeforeAbility = Player->GetAttributes()->GetAbilityDamageMultiplier();
    const float BeforeDot = UBreakerCombatComponent::ComposeDotSourcePower(Player->GetAttributes(), Player->GetCombat(), EBreakerDamageDelivery::Weapon);
    const FBreakerShotResult First = Fire();
    TestTrue(TEXT("real first trace deals damage"), First.DamageResult.HealthDamage > 0);
    TestEqual(TEXT("first paid shot earns one stack"), Weapon->GetDamageRampStacks(), 1);
    const float Flat = Player->GetAttributes()->GetAttributeAggregator().ComposedFlatFactor(EBreakerAggregatedAttribute::DamageMultiplier);
    TestEqual(TEXT("ramp joins existing weapon Increased bucket"), Player->GetAttributes()->GetDamageMultiplier(), BeforeMultiplier + Flat * PerStack / 100.0f, 0.0001f);
    TestEqual(TEXT("ability lane isolated"), Player->GetAttributes()->GetAbilityDamageMultiplier(), BeforeAbility, 0.0001f);
    FBreakerStatusApplicationSpec Snapshot; Snapshot.BaseDamagePerTick = 10;
    Snapshot.Snapshot.SourcePower = UBreakerCombatComponent::ComposeDotSourcePower(Player->GetAttributes(), Player->GetCombat(), EBreakerDamageDelivery::Weapon);
    TestEqual(TEXT("weapon DoT captures same additive stack"), Snapshot.Snapshot.SourcePower, BeforeDot + Flat * PerStack / 100.0f, 0.0001f);
    const FBreakerShotResult Second = Fire();
    TestEqual(TEXT("next shot consumes the previous stack"), Second.DamageResult.HealthDamage,
        First.DamageResult.HealthDamage * (BeforeMultiplier + Flat * PerStack / 100.0f) / BeforeMultiplier, 0.01f);
    TestEqual(TEXT("second shot advances once"), Weapon->GetDamageRampStacks(), 2);
    Weapon->WeaponDefinition->PelletsPerShot = 8;
    Fire();
    TestEqual(TEXT("eight-pellet shot advances once"), Weapon->GetDamageRampStacks(), 3);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); Fire();
    TestEqual(TEXT("actual miss resets chain"), Weapon->GetDamageRampStacks(), 0);
    TestEqual(TEXT("miss removes additive contribution"), Player->GetAttributes()->GetDamageMultiplier(), BeforeMultiplier, 0.0001f);
    const FBreakerDamageRequest OldTick = UBreakerDamageLibrary::MakeSnapshotDotTick(Snapshot, EBreakerDamageFamily::TrueDamage, 1, Player, FVector::ZeroVector, false);
    TestEqual(TEXT("prior status retains captured ramp after reset"), Combat->ReceiveDamage(OldTick).HealthDamage, 10.0f * Snapshot.Snapshot.SourcePower, 0.01f);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Fire(); Combat->DodgeChance = 1; Fire(); Combat->DodgeChance = 0;
    TestEqual(TEXT("dodged shot cannot preserve or build chain"), Weapon->GetDamageRampStacks(), 0);

    UBreakerProgressionComponent* Progression = Player->GetProgression();
    Progression->BindAttributes(Player->GetAttributes());
    if (!TestTrue(TEXT("real Swift selection"), Progression->ChoosePermanentClassById(EBreakerClassId::Swift))) return false;
    UBreakerMomentumComponent* Momentum = Player->FindComponentByClass<UBreakerMomentumComponent>();
    HeldRedlineBand = true;
    Player->GetAttributes()->ApplyClassResource(Player->GetAttributes()->GetMaxClassResource()); Momentum->BindAttributes(Player->GetAttributes());
    Fire(); TestEqual(TEXT("Redline alone still grants one"), Weapon->GetDamageRampStacks(), 1);
    // Full authored eight-point wiring fixture; current Act II's four points cannot buy tier four.
    // O272: single-rank pairs — Redline Trigger sits behind Loaded.
    Progression->GrantPlaytestPoints(8, 0);
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetSwiftFrenzyTree();
    FText Failure;
    for (const TCHAR* Id : {TEXT("Swift.Frenzy.Loaded"), TEXT("Swift.Frenzy.RedlineTrigger")})
        if (!TestTrue(FString::Printf(TEXT("actual purchase %s"), Id), Progression->PurchaseNode(Tree, Id, Failure))) return false;
    HeldRedlineBand = false;
    Player->GetAttributes()->ApplyClassResource(0); Momentum->BindAttributes(Player->GetAttributes());
    Fire(); TestEqual(TEXT("owned node below Redline still grants one"), Weapon->GetDamageRampStacks(), 2);
    HeldRedlineBand = true;
    Player->GetAttributes()->ApplyClassResource(Player->GetAttributes()->GetMaxClassResource()); Momentum->BindAttributes(Player->GetAttributes());
    Fire(); TestEqual(TEXT("owned node at Redline grants two per shot"), Weapon->GetDamageRampStacks(), 4);
    for (int32 Shot = 0; Shot < 5; ++Shot) Fire();
    TestEqual(TEXT("authored ten-stack cap"), Weapon->GetDamageRampStacks(), 10);
    Weapon->EquipSlot(2);
    TestEqual(TEXT("swap resets all stacks"), Weapon->GetDamageRampStacks(), 0);
    TestFalse(TEXT("Secondary cannot use Primary affix"), Weapon->IsDamageRampEquipped());
    Weapon->EquipSlot(1);
    // This section validates ordering between single-projectile pulls.
    // The earlier eight-pellet hitscan fixture now also affects rocket emission.
    Weapon->WeaponDefinition->PelletsPerShot = 1;
    Weapon->WeaponDefinition->bProjectile = true;
    auto FireRocket = [&]() -> ABreakerRocketProjectile*
    {
        TSet<ABreakerRocketProjectile*> Existing;
        for (TActorIterator<ABreakerRocketProjectile> It(World); It; ++It) Existing.Add(*It);
        Fire();
        for (TActorIterator<ABreakerRocketProjectile> It(World); It; ++It)
        {
            if (Existing.Contains(*It)) continue;
            // Programmatic-impact fixture: do not let travel race the explicit ordering below.
            if (UProjectileMovementComponent* Movement = It->FindComponentByClass<UProjectileMovementComponent>())
            {
                Movement->StopMovementImmediately(); Movement->Deactivate();
            }
            It->SetActorEnableCollision(false);
            TestFalse(TEXT("new actual rocket has not impacted"), It->HasExploded());
            return *It;
        }
        return nullptr;
    };
    ABreakerRocketProjectile* Rocket = FireRocket();
    if (!TestNotNull(TEXT("actual firing spawns rocket"), Rocket)) return false;
    TestEqual(TEXT("launch alone never accrues"), Weapon->GetDamageRampStacks(), 0);
    // Programmatic production impact seam; this tests actual explosion damage, not flight collision.
    const float HealthBeforeRocket = Health->GetHealth();
    Rocket->Explode(Victim->GetActorLocation());
    TestTrue(TEXT("rocket really damages target"), Health->GetHealth() < HealthBeforeRocket);
    TestEqual(TEXT("successful rocket accrues once"), Weapon->GetDamageRampStacks(), 2);
    Rocket->Explode(Victim->GetActorLocation());
    TestEqual(TEXT("duplicate impact never accrues twice"), Weapon->GetDamageRampStacks(), 2);
    Rocket = FireRocket(); if (!TestNotNull(TEXT("miss rocket"), Rocket)) return false;
    Rocket->Explode(FVector(10000, 0, 0));
    TestEqual(TEXT("real empty explosion resets"), Weapon->GetDamageRampStacks(), 0);
    ABreakerRocketProjectile* EarlierMiss = FireRocket();
    ABreakerRocketProjectile* LaterHit = FireRocket();
    if (!TestNotNull(TEXT("earlier concurrent rocket"), EarlierMiss) || !TestNotNull(TEXT("later concurrent rocket"), LaterHit)) return false;
    TestFalse(TEXT("earlier projectile still awaits explicit impact"), EarlierMiss->HasExploded());
    LaterHit->Explode(Victim->GetActorLocation());
    TestEqual(TEXT("later launch can be first actual hit"), Weapon->GetDamageRampStacks(), 2);
    EarlierMiss->Explode(FVector(10000, 0, 0));
    TestEqual(TEXT("concurrent rockets use actual resolution order"), Weapon->GetDamageRampStacks(), 0);
    Rocket = FireRocket(); if (!TestNotNull(TEXT("old-slot rocket"), Rocket)) return false;
    Weapon->EquipSlot(2); Weapon->EquipSlot(1);
    Rocket->Explode(Victim->GetActorLocation());
    TestEqual(TEXT("late old-slot rocket cannot rebuild chain"), Weapon->GetDamageRampStacks(), 0);
    Weapon->WeaponDefinition->bProjectile = false;
    Fire(); TestTrue(TEXT("chain rebuilds on current weapon"), Weapon->GetDamageRampStacks() > 0);
    Player->GetEquipment()->UnequipSlot(EBreakerEquipSlot::Primary);
    TestEqual(TEXT("unequip resets immediately"), Weapon->GetDamageRampStacks(), 0);
    Player->GetEquipment()->EquipItem(Item); Fire();
    TestTrue(TEXT("actual reacquired weapon builds fresh chain"), Weapon->GetDamageRampStacks() > 0);
    FBreakerDamageRequest Lethal; Lethal.BaseDamage = 100000; Lethal.DamageFamily = EBreakerDamageFamily::TrueDamage; Lethal.bBypassShield = true; Lethal.bCanCritical = false;
    TestTrue(TEXT("actual lethal hit kills owner"), Player->GetCombat()->ReceiveDamage(Lethal).bKilled);
    TestEqual(TEXT("death clears stacks through real callback"), Weapon->GetDamageRampStacks(), 0);
    return true;
}
#endif
