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
#include "GameFramework/ProjectileMovementComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Weapons/BreakerRocketProjectile.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreWeaponRuntimeTest, "RiorsEdge.Weapons.CoreRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreWeaponRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    auto* Player = World->SpawnActor<ABreakerCharacter>(); if (!Player) return false;
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); auto* Attr = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attr); Attr->SetCriticalChance(0);
    Player->GetCombat()->BindAttributes(Attr);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attr);
    auto* Class = NewObject<UBreakerClassDefinition>(); Class->ClassId = EBreakerClassId::Caster;
    auto* Tree = NewObject<UBreakerProgressionTree>(Class); Tree->TreeId = TEXT("Test.Core.Weapon");
    Tree->Currency = EBreakerPointCurrency::CorePoints; Class->BranchTrees.Add(Tree);
    auto Make = [&](const TCHAR* Id)
    { auto* N = NewObject<UBreakerProgressionNode>(Tree); N->NodeId = Id; N->Currency = Tree->Currency; Tree->Nodes.Add(N); return N; };
    auto Add = [&](UBreakerProgressionNode* N, EBreakerNodeStatTarget Target, float Value, EBreakerNodeStatBucket Bucket)
    { FBreakerNodeEffect E; E.StatTarget = Target; E.ValuePerRank = Value; E.StatBucket = Bucket; N->Effects.Add(E); };
    auto* Pierce = Make(TEXT("Test.Core.Weapon.Pierce"));
    Add(Pierce, EBreakerNodeStatTarget::Pierce, 1, EBreakerNodeStatBucket::Flat);
    auto* Node = Make(TEXT("Test.Core.Weapon.Lanes"));
    // Test-only schema, not the live Core. Buy through an earned wallet.
    for (const auto Pair : {TPair<EBreakerNodeStatTarget,float>(EBreakerNodeStatTarget::WeaponRange,20),
        {EBreakerNodeStatTarget::WeaponFalloffStart,40},{EBreakerNodeStatTarget::ProjectileSpeed,10},
        {EBreakerNodeStatTarget::WeaponSplashArea,100},{EBreakerNodeStatTarget::WeaponReloadSpeed,18},
        {EBreakerNodeStatTarget::WeaponSwapSpeed,12},{EBreakerNodeStatTarget::WeaponMagazineCapacity,20},
        {EBreakerNodeStatTarget::WeaponReserveAmmo,45}})
        Add(Node, Pair.Key, Pair.Value, EBreakerNodeStatBucket::IncreasedPercent);
    Add(Node, EBreakerNodeStatTarget::PierceLossReduction, 15, EBreakerNodeStatBucket::Flat);
    if (!Progression->ChoosePermanentClass(Class)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(5, Progression->ExperienceCurve));
    auto* Weapon = Player->GetWeapon();
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->HipSpreadDegrees = 0; Weapon->WeaponDefinition->AimSpreadDegrees = 0;
    Weapon->WeaponDefinition->Recoil.BloomPerShotDegrees = 0; Weapon->WeaponDefinition->BleedChance = 0;
    Weapon->ResetAmmunition();
    auto Clock = [&](float Seconds)
    { for (float T = 0; T < Seconds; T += .01f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .01f); } };
    auto Fire = [&]()
    {
        Clock(.4f); const int32 Before = Weapon->GetMagazineAmmo();
        Weapon->StartFire(); Weapon->StopFire();
        TestEqual(TEXT("Actual trigger spends a round"), Weapon->GetMagazineAmmo(), Before - 1);
    };
    auto Reload = [&]()
    {
        Weapon->StartReload(); TestTrue(TEXT("Actual reload begins"), Weapon->IsReloading());
        float Elapsed = 0;
        while (Elapsed < 5 && Weapon->IsReloading()) { Clock(.01f); Elapsed += .01f; }
        TestFalse(TEXT("Actual reload completes"), Weapon->IsReloading()); return Elapsed;
    };
    auto Swap = [&](int32 Slot)
    {
        Weapon->EquipSlot(Slot); TestTrue(TEXT("Actual swap begins"), Weapon->IsSwapping());
        float Elapsed = 0;
        while (Elapsed < 5 && Weapon->IsSwapping()) { Clock(.01f); Elapsed += .01f; }
        TestFalse(TEXT("Actual swap completes"), Weapon->IsSwapping()); return Elapsed;
    };
    const int32 BaseMagazine = Weapon->GetEffectiveMagazineSize();
    const int32 BaseReserve = Weapon->GetActiveDefinition()->StartingReserveAmmo;
    Fire(); const float ReloadTime = Reload(); const float SwapTime = Swap(2); Swap(1);
    auto Victim = [&](FVector Location)
    {
        auto* Actor = World->SpawnActor<AActor>(); auto* Shape = NewObject<USphereComponent>(Actor);
        Actor->AddInstanceComponent(Shape); Actor->SetRootComponent(Shape); Shape->SetSphereRadius(40);
        Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
        Shape->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); Shape->RegisterComponent(); Actor->SetActorLocation(Location);
        auto* Combat = NewObject<UBreakerCombatComponent>(Actor); Actor->AddInstanceComponent(Combat); Combat->RegisterComponent();
        auto* Health = NewObject<UBreakerAttributeSet>(Actor); Health->ApplyMaxHealth(10000); Health->ApplyHealth(10000); Combat->BindAttributes(Health);
        return TPair<AActor*, UBreakerAttributeSet*>(Actor, Health);
    };
    FVector Eye; FRotator Aim; Player->GetActorEyesViewPoint(Eye, Aim);
    auto A = Victim(Eye + Aim.Vector() * 500), B = Victim(Eye + Aim.Vector() * 900);
    FText Reason;
    if (!TestTrue(TEXT("Earned point buys actual pierce"), Progression->PurchaseNode(Tree, Pierce->NodeId, Reason))) return false;
    auto PierceRatio = [&]()
    {
        const float BeforeA = A.Value->GetHealth(), BeforeB = B.Value->GetHealth(); Fire();
        const float First = BeforeA - A.Value->GetHealth(), Second = BeforeB - B.Value->GetHealth();
        TestTrue(TEXT("Actual shot hits both pierced targets"), First > 0 && Second > 0);
        return First > 0 ? Second / First : 0;
    };
    TestEqual(TEXT("Original pierced leg keeps seventy percent"), PierceRatio(), .70f, .002f);
    A.Key->SetActorLocation(Eye + Aim.Vector() * 4500); B.Key->SetActorEnableCollision(false);
    const float BeforeRange = A.Value->GetHealth(); Fire();
    TestTrue(TEXT("Baseline distant hit actually falls off"), BeforeRange - A.Value->GetHealth() < Weapon->GetScaledBaseDamage() * Attr->GetDamageMultiplier() * .99f);
    const int32 BeforeBuyAmmo = Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo();
    if (!TestTrue(TEXT("Earned point buys numeric weapon schema"), Progression->PurchaseNode(Tree, Node->NodeId, Reason))) return false;
    TestEqual(TEXT("Capacity purchase creates no free rounds"), Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo(), BeforeBuyAmmo);
    TestEqual(TEXT("Purchased magazine capacity floors whole rounds"), Weapon->GetEffectiveMagazineSize(), FMath::FloorToInt(BaseMagazine * 1.2f));
    const float BeforeFull = A.Value->GetHealth(); Fire();
    TestEqual(TEXT("Actual range/start lanes restore full distant hit"), BeforeFull - A.Value->GetHealth(), Weapon->GetScaledBaseDamage() * Attr->GetDamageMultiplier(), .003f);
    A.Key->SetActorLocation(Eye + Aim.Vector() * 500); B.Key->SetActorEnableCollision(true);
    TestEqual(TEXT("Pierced loss is reduced by fifteen percent of loss"), PierceRatio(), .745f, .002f);
    const int32 BeforeReload = Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo();
    TestEqual(TEXT("Real reload accelerates"), Reload(), ReloadTime / 1.18f, .021f);
    TestEqual(TEXT("Real reload fills purchased headroom"), Weapon->GetMagazineAmmo(), Weapon->GetEffectiveMagazineSize());
    TestEqual(TEXT("Reload transfers rather than creates rounds"), Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo(), BeforeReload);
    const int32 BeforeGrant = Weapon->GetReserveAmmo(); Weapon->AddReserveAmmoFraction(.1f);
    TestEqual(TEXT("Actual resupply uses increased reserve base"), Weapon->GetReserveAmmo() - BeforeGrant, FMath::CeilToInt(FMath::FloorToInt(BaseReserve * 1.45f) * .1f));
    TestEqual(TEXT("Real swap accelerates"), Swap(2), SwapTime / 1.12f, .021f); Swap(1);
    const int32 BeforeRespec = Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo();
    if (!Progression->RespecCore(Reason)) return false;
    TestEqual(TEXT("Respec restores magazine capacity"), Weapon->GetEffectiveMagazineSize(), BaseMagazine);
    TestEqual(TEXT("Respec moves excess magazine rounds to reserve"), Weapon->GetMagazineAmmo(), BaseMagazine);
    TestEqual(TEXT("Respec conserves all ammunition"), Weapon->GetMagazineAmmo() + Weapon->GetReserveAmmo(), BeforeRespec);
    if (!Progression->PurchaseNode(Tree, Node->NodeId, Reason)) return false;
    Weapon->WeaponDefinition = nullptr; Weapon->SetSlotArchetype(1, EBreakerWeaponArchetype::Rocket); Weapon->ResetAmmunition();
    const auto* RocketDefinition = Weapon->GetActiveDefinition();
    Fire(); ABreakerRocketProjectile* Rocket = nullptr;
    for (TActorIterator<ABreakerRocketProjectile> It(World); It; ++It) if (!It->HasExploded()) { Rocket = *It; break; }
    if (!TestNotNull(TEXT("Real trigger launches rocket"), Rocket)) return false;
    auto* Flight = Rocket->FindComponentByClass<UProjectileMovementComponent>();
    if (!TestNotNull(TEXT("Rocket has actual flight component"), Flight)) return false;
    TestEqual(TEXT("Actual projectile launches ten percent faster"), static_cast<float>(Flight->Velocity.Size()), RocketDefinition->ProjectileSpeed * 1.1f, .01f);
    Flight->StopMovementImmediately(); Flight->Deactivate(); Rocket->SetActorEnableCollision(false);
    const FVector Explosion(5000, 0, 0);
    A.Key->SetActorLocation(Explosion + FVector(0, RocketDefinition->ExplosionRadius * 1.2f, 0));
    B.Key->SetActorLocation(Explosion + FVector(0, RocketDefinition->ExplosionRadius * 1.5f, 0));
    const float Inside = A.Value->GetHealth(), Outside = B.Value->GetHealth();
    if (!Progression->RespecCore(Reason)) return false;
    // Production impact seam isolates the fire-time area snapshot, not flight collision.
    Rocket->Explode(Explosion);
    TestTrue(TEXT("Fired rocket retains doubled area across respec"), A.Value->GetHealth() < Inside);
    TestEqual(TEXT("Area increase scales radius by square root, not two"), B.Value->GetHealth(), Outside);
    return true;
}
#endif
