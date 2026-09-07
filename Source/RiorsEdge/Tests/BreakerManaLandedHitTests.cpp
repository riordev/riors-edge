#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Weapons/BreakerWeaponComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerManaLandedHitTest, "RiorsEdge.Classes.ManaLandedHitRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerManaLandedHitTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated real shot world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector Origin(0, 0, 100);
    auto* Player = World->SpawnActor<ABreakerCharacter>(ABreakerCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Spawn);
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!Player || !Controller) return false;
    // No Character BeginPlay: this fixture must never read or write owner saves.
    Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->SetAsLocalPlayerController();
    Controller->Possess(Player); Controller->SetViewTarget(Player);
    Controller->SetInitialLocationAndRotation(Origin, FRotator::ZeroRotator);
    Player->SetActorTickEnabled(false); Player->GetBreakerMovement()->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    Player->GetProgression()->BindAttributes(Player->GetAttributes());
    if (!TestTrue(TEXT("legal Caster selection"), Player->GetProgression()->ChoosePermanentClassById(EBreakerClassId::Caster))) return false;
    FBreakerItemInstance Rifle;
    for (int32 Seed = 1; Seed <= 4096; ++Seed)
    {
        Rifle = UBreakerLootLibrary::RollItem(TEXT("Mana.LandedHit"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
        if (Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle) break;
    }
    if (!TestTrue(TEXT("real rolled rifle"), Rifle.IsValid() && Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle)) return false;
    for (auto& Affix : Rifle.Affixes) Affix.Value = 0;
    Player->GetEquipment()->BindAttributes(Player->GetAttributes());
    if (!TestTrue(TEXT("legal rifle equip"), Player->GetEquipment()->EquipItem(Rifle))) return false;
    auto* Weapon = Player->GetWeapon();
    Weapon->RegisterAllComponentTickFunctions(true); Weapon->SetComponentTickEnabled(true); Weapon->BeginPlay();
    Weapon->SyncArchetypesToEquipment(); Weapon->EquipSlot(1);
    auto* Mana = Player->GetMana();
    Mana->BindAttributes(Player->GetAttributes());
    Mana->PassiveRegenPerSecond = 0; // Isolate conditional income; no offense or generation gain is added.
    if (!TestTrue(TEXT("make room in the ordinary initial bank"), Mana->TrySpendMana(50))) return false;
    auto Advance = [&]
    {
        ++GFrameCounter; World->Tick(LEVELTICK_All, .05f);
        Mana->AdvanceLoop(.05f); // Explicit component loop; Character remains unbegun.
    };
    for (int32 Step = 0; Step < 30; ++Step) Advance();
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), Origin + FVector(600, 0, 0), FRotator::ZeroRotator, Spawn);
    if (!Enemy) return false;
    Enemy->ConfigureCrowdProbe(); Enemy->SetAreaLevel(1); Enemy->SetMonsterRank(EBreakerMonsterRank::Boss);
    Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
    if (auto* Movement = Enemy->GetMovementComponent()) Movement->SetComponentTickEnabled(false);
    auto* TargetCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
    if (!TargetCombat) return false;
    auto* Wall = World->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box); Wall->AddInstanceComponent(Box);
    Box->SetBoxExtent(FVector(25, 250, 250));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent();
    Wall->SetActorLocation(Origin + FVector(300, 0, 0));
    auto Fire = [&]
    {
        FVector Eye; FRotator Facing; Controller->GetPlayerViewPoint(Eye, Facing);
        Controller->SetControlRotation((Enemy->GetActorLocation() + FVector(0, 0, 25) - Eye).Rotation());
        if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(.05f);
        Weapon->StartFire(); Weapon->StopFire();
        const FBreakerShotResult Shot = Weapon->GetLastShot();
        for (int32 Step = 0; Step < 20; ++Step) Advance();
        return Shot;
    };
    const float InitialMana = Mana->GetMana();
    const auto WallShot = Fire();
    TestTrue(TEXT("real rifle hits wall geometry"), WallShot.bFired && WallShot.bHit && WallShot.HitActor == Wall);
    TestEqual(TEXT("wall shot dealt no combat damage"), WallShot.DamageResult.HealthDamage + WallShot.DamageResult.ShieldDamage, 0.0f);
    TestEqual(TEXT("wall gives no Mana"), Mana->GetMana(), InitialMana);
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TargetCombat->PushIncomingDamageModifier(TEXT("Test.ManaImmunity"), 0);
    const auto ImmuneShot = Fire();
    TestTrue(TEXT("real rifle reaches damage-immune enemy"), ImmuneShot.bFired && ImmuneShot.HitActor == Enemy);
    TestEqual(TEXT("immune target takes zero damage"), ImmuneShot.DamageResult.HealthDamage + ImmuneShot.DamageResult.ShieldDamage, 0.0f);
    TestEqual(TEXT("zero-damage target gives no Mana"), Mana->GetMana(), InitialMana);
    TargetCombat->RemoveIncomingDamageModifier(TEXT("Test.ManaImmunity"));
    const auto LiveShot = Fire();
    TestTrue(TEXT("real accepted shot deals damage"), LiveShot.bFired && LiveShot.DamageResult.HealthDamage + LiveShot.DamageResult.ShieldDamage > 0);
    const float ExpectedGain = LiveShot.bWeakPoint ? Mana->WeakPointGain : Mana->WeaponHitGain;
    AddInfo(FString::Printf(TEXT("LANDED MANA actual%.6f expected%.6f"), Mana->GetMana() - InitialMana, ExpectedGain));
    TestTrue(TEXT("actual accepted shot pays its ordinary hit income"), FMath::IsNearlyEqual(Mana->GetMana() - InitialMana, ExpectedGain, .001f));
    // Supplemental aggregate contract: the last geometric actor is not the
    // whole volley. Replay the accepted result with a final wall pellet; the
    // three cases above, unlike this bookkeeping case, actually fire the rifle.
    FBreakerShotResult Mixed = LiveShot;
    Mixed.HitActor = Wall;
    const float BeforeMixed = Mana->GetMana();
    Weapon->OnShot.Broadcast(Mixed);
    for (int32 Step = 0; Step < 20; ++Step) Advance();
    AddInfo(FString::Printf(TEXT("MIXED MANA actual%.6f expected%.6f"), Mana->GetMana() - BeforeMixed, ExpectedGain));
    TestTrue(TEXT("accepted aggregate still pays when final pellet actor is geometry"), FMath::IsNearlyEqual(Mana->GetMana() - BeforeMixed, ExpectedGain, .001f));
    return true;
}
#endif
