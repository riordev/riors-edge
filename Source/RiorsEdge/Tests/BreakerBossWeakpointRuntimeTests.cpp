#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerHoldfastEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/PointLightComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerBossWeakpointRuntimeTest,
    "RiorsEdge.Combat.Boss.ApparatusWeakpointRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerBossWeakpointRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto SpawnBoss = [&](UClass* Class, FVector Location)
    {
        ABreakerBossEnemy* Boss = World->SpawnActor<ABreakerBossEnemy>(Class, Location, FRotator::ZeroRotator);
        if (!Boss) return static_cast<ABreakerBossEnemy*>(nullptr);
        auto* Attributes = Cast<UBreakerAttributeSet>(Boss->GetDefaultSubobjectByName(TEXT("Attributes")));
        if (!Attributes) return static_cast<ABreakerBossEnemy*>(nullptr);
        Boss->GetAbilitySystemComponent()->AddAttributeSetSubobject(Attributes);
        Boss->DispatchBeginPlay();
        Boss->SetActorTickEnabled(false);
        return Boss;
    };
    ABreakerBossEnemy* Marshal = SpawnBoss(ABreakerBossEnemy::StaticClass(), FVector::ZeroVector);
    ABreakerBossEnemy* Holdfast = SpawnBoss(ABreakerHoldfastEnemy::StaticClass(), FVector(5000, 0, 0));
    if (!TestNotNull(TEXT("Marshal spawn"), Marshal) || !TestNotNull(TEXT("Holdfast spawn"), Holdfast)) return false;
    auto* Apparatus = Cast<UStaticMeshComponent>(Marshal->GetDefaultSubobjectByName(TEXT("ApparatusVisual")));
    auto* Head = Cast<UPrimitiveComponent>(Marshal->GetDefaultSubobjectByName(TEXT("WeakPoint")));
    auto* Orb = Cast<UPrimitiveComponent>(Marshal->GetDefaultSubobjectByName(TEXT("WeakPointVisual")));
    auto* Named = Cast<USkeletalMeshComponent>(Marshal->GetDefaultSubobjectByName(TEXT("NamedBody")));
    auto* HoldfastHead = Cast<UPrimitiveComponent>(Holdfast->GetDefaultSubobjectByName(TEXT("WeakPoint")));
    if (!TestNotNull(TEXT("Named body component"), Named) || !TestNotNull(TEXT("Orb component"), Orb) || !TestNotNull(TEXT("Holdfast head"), HoldfastHead)) return false;
    if (!TestNotNull(TEXT("Actual apparatus mesh"), Apparatus) || !TestNotNull(TEXT("Inherited head collider"), Head)
        || !TestNotNull(TEXT("Imported Marshal body"), Named->GetSkeletalMeshAsset())) return false;
    TestTrue(TEXT("Apparatus is tagged for actual weapon classification"), Apparatus->ComponentHasTag(TEXT("WeakPoint")));
    if (!TestNotNull(TEXT("Apparatus mesh owns collision data"), Apparatus->GetStaticMesh()->GetBodySetup())) return false;
    TestTrue(TEXT("Shipped cube has simple collision"), Apparatus->GetStaticMesh()->GetBodySetup()->AggGeom.GetElementCount() > 0);
    TestFalse(TEXT("Marshal head starts untargetable"), Head->IsCollisionEnabled());
    TestFalse(TEXT("Marshal orb is hidden"), Orb->IsVisible());
    TestFalse(TEXT("Apparatus begins closed"), Apparatus->IsCollisionEnabled());
    TestFalse(TEXT("Holdfast initial closed state is applied too"), HoldfastHead->IsCollisionEnabled());
    Marshal->ApplyBodyMesh();
    TestFalse(TEXT("Named-body reapply cannot reopen Marshal head"), Head->IsCollisionEnabled());
    Marshal->SetModifierUntargetable(true);
    Marshal->SetModifierUntargetable(false);
    TestFalse(TEXT("Targetable restoration cannot reopen closed head"), Head->IsCollisionEnabled());
    TestFalse(TEXT("Targetable restoration preserves closed apparatus"), Apparatus->IsCollisionEnabled());
    auto BreakFront = [&](ABreakerBossEnemy* Boss)
    {
        UBreakerCombatComponent* Combat = Boss->FindComponentByClass<UBreakerCombatComponent>();
        FBreakerDamageRequest Hit;
        Hit.BaseDamage = Combat->GetFrontShield() + 1;
        Hit.DamageFamily = EBreakerDamageFamily::TrueDamage;
        Hit.bCanCritical = false;
        Hit.bHasSourceLocation = true;
        Hit.SourceLocation = Boss->GetActorLocation() + Boss->GetActorForwardVector() * 1000;
        Combat->ReceiveDamage(Hit);
        return Combat->IsFrontShieldBroken();
    };
    if (!TestTrue(TEXT("Actual frontal hit breaks Marshal pool"), BreakFront(Marshal))) return false;
    TestTrue(TEXT("Actual break exposes hardware"), Marshal->IsApparatusExposed() && Apparatus->IsCollisionEnabled());
    const UPointLightComponent* ApparatusLamp = Cast<UPointLightComponent>(Marshal->GetDefaultSubobjectByName(TEXT("ApparatusLight")));
    TestTrue(TEXT("Resting front-break window visibly lights actual hardware"), ApparatusLamp && ApparatusLamp->Intensity > 0);
    TestFalse(TEXT("Exposing hardware leaves head disabled"), Head->IsCollisionEnabled());
    TestTrue(TEXT("Actual Holdfast break still opens its inherited weakpoint"), BreakFront(Holdfast) && HoldfastHead->IsCollisionEnabled());
    FHitResult Trace;
    const FVector At = Apparatus->GetComponentLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MarshalApparatusTest), false);
    if (!TestTrue(TEXT("Rear weapon trace hits exposed hardware"), World->LineTraceSingleByChannel(Trace,
        At - Marshal->GetActorForwardVector() * 1000, At, ECC_GameTraceChannel2, Query))) return false;
    TestTrue(TEXT("Visible apparatus is the actual query hit"), Trace.GetComponent() == Apparatus);
    Marshal->SetModifierUntargetable(true);
    TestFalse(TEXT("Hidden Marshal hardware cannot catch bullets"), Apparatus->IsCollisionEnabled());
    Marshal->SetModifierUntargetable(false);
    TestTrue(TEXT("Unhidden Marshal restores the still-open window"), Apparatus->IsCollisionEnabled());
    TestFalse(TEXT("Unhidden Marshal never restores head weakpoint"), Head->IsCollisionEnabled());
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    if (!TestNotNull(TEXT("Player spawn"), Player)) return false;
    Player->SetActorTickEnabled(false);
    Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Real player"), Player) || !TestNotNull(TEXT("Real controller"), Controller)) return false;
    Controller->Possess(Player);
    Controller->SetViewTarget(Player);
    UBreakerWeaponComponent* Weapon = Player->GetWeapon();
    Weapon->EquipArchetype(EBreakerWeaponArchetype::Rifle);
    Weapon->WeaponDefinition = DuplicateObject<UBreakerWeaponDefinition>(Weapon->GetActiveDefinition(), Weapon);
    Weapon->WeaponDefinition->HipSpreadDegrees = 0;
    Weapon->WeaponDefinition->AimSpreadDegrees = 0;
    auto FireAt = [&](FVector Target)
    {
        Player->SetActorLocation(Target - FVector(1000, 0, 0));
        Controller->SetInitialLocationAndRotation(Player->GetActorLocation(), (Target - Player->GetActorLocation()).Rotation());
        if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(0.0f);
        FVector Eye; FRotator Rotation; Controller->GetPlayerViewPoint(Eye, Rotation);
        Controller->SetInitialLocationAndRotation(Eye, (Target - Eye).Rotation());
        if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(0.0f);
        Weapon->ResetAmmunition();
        const int32 AmmoBefore = Weapon->GetMagazineAmmo();
        Weapon->StartFire(); Weapon->StopFire();
        TestEqual(TEXT("Each assertion observes a fresh fired shot"), Weapon->GetMagazineAmmo(), AmmoBefore - 1);
    };
    FireAt(At);
    TestTrue(TEXT("Real fired shot hits Marshal"), Weapon->GetLastShot().HitActor == Marshal);
    TestTrue(TEXT("Real fired shot classifies apparatus as weakpoint"), Weapon->GetLastShot().DamageResult.bWeakPoint);
    TGuardValue<uint64> FrameScope(GFrameCounter, GFrameCounter);
    const double BeforeWait = World->GetTimeSeconds();
    for (int32 Step = 0; Step < 8; ++Step) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.05f); }
    TestTrue(TEXT("Weapon refire wait advances actual world clock"), World->GetTimeSeconds() - BeforeWait >= 0.39);
    FireAt(Marshal->GetActorLocation() - FVector(0, 0, 60));
    TestTrue(TEXT("Control shot hits Marshal"), Weapon->GetLastShot().HitActor == Marshal);
    TestTrue(TEXT("Control shot lands on actual boss body"), Weapon->GetLastShot().DamageResult.HealthDamage > 0);
    TestFalse(TEXT("Lower body is outside hardware forgiveness envelope"), Weapon->GetLastShot().DamageResult.bWeakPoint);
    static_cast<AActor*>(Marshal)->Tick(Marshal->PhaseParams.FrontBreakPunishSeconds + 0.1f);
    TestFalse(TEXT("Expired punish window closes actual hardware"), Apparatus->IsCollisionEnabled());
    TestFalse(TEXT("Expired window leaves head disabled"), Head->IsCollisionEnabled());
    Player->SetActorLocation(Marshal->GetActorLocation() + FVector(1000, 0, 0));
    const float RestHeight = Apparatus->GetComponentLocation().Z;
    for (int32 Step = 0; Step < 300 && !Marshal->IsGivingOrder(); ++Step)
        static_cast<AActor*>(Marshal)->Tick(0.1f);
    if (!TestTrue(TEXT("Normal engaged cadence begins an actual order"), Marshal->IsGivingOrder())) return false;
    static_cast<AActor*>(Marshal)->Tick(0.2f);
    TestTrue(TEXT("Actual order raises the hardware"), Apparatus->GetComponentLocation().Z > RestHeight);
    const FVector Raised = Apparatus->GetComponentLocation();
    Query.AddIgnoredActor(Player);
    TestTrue(TEXT("Raised hardware remains an actual weapon trace target"), World->LineTraceSingleByChannel(Trace,
        Raised - Marshal->GetActorForwardVector() * 1000, Raised, ECC_GameTraceChannel2, Query));
    TestTrue(TEXT("Raised trace hits apparatus rather than head"), Trace.GetComponent() == Apparatus);
    // A real lethal request must close every weakpoint before death animation.
    FBreakerDamageRequest Kill;
    Kill.BaseDamage = 10000000; Kill.bCanCritical = false; Kill.bBypassShield = true;
    TestTrue(TEXT("Actual boss death"), Marshal->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Kill).bKilled);
    TestFalse(TEXT("Dead apparatus is untargetable"), Apparatus->IsCollisionEnabled());
    Marshal->ReviveFromPool(FVector::ZeroVector);
    TestFalse(TEXT("Pool body restoration cannot resurrect head collider"), Head->IsCollisionEnabled());
    TestFalse(TEXT("Pool body restoration does not open an exposure window"), Apparatus->IsCollisionEnabled());
    return true;
}
#endif
