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

#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusRules.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPierceDurationRuntimeTest, "RiorsEdge.Combat.Status.PierceDurationRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerPierceDurationRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = Frame; };
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector Origin(0,0,100);
    auto* Player = World->SpawnActor<ABreakerCharacter>(ABreakerCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Spawn);
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!Player || !Controller) return false;
    Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->SetAsLocalPlayerController();
    Controller->Possess(Player); Controller->SetViewTarget(Player); Controller->SetInitialLocationAndRotation(Origin, FRotator::ZeroRotator);
    Player->SetActorTickEnabled(false);
    auto* Movement = Player->GetBreakerMovement(); Movement->SetComponentTickEnabled(false);
    auto* ASC = Player->GetAbilitySystemComponent(); auto* Attributes = Player->GetAttributes();
    ASC->InitAbilityActorInfo(Player, Player); ASC->AddAttributeSetSubobject(Attributes);
    Player->GetCombat()->BindAttributes(Attributes);
    auto* Progression = Player->GetProgression(); Progression->BindAttributes(Attributes);
    if (!Progression->ChoosePermanentClassById(EBreakerClassId::Swift)) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(10, Progression->ExperienceCurve));
    FText Reason;
    for (FName Node : {FName(TEXT("Core.Affliction.OpenWound")), FName(TEXT("Core.Affliction.Linger"))})
        if (!TestTrue(TEXT("actual legal Core path buys duration"), Progression->PurchaseNode(UBreakerProgressionLibrary::GetCoreSliceTree(), Node, Reason))) return false;
    const float DurationScale = Progression->GetNodeStats().StatusDurationMultiplier;
    if (!TestTrue(TEXT("purchased duration multiplier is nonidentity"), DurationScale > 1)) return false;
    FBreakerItemInstance Rifle;
    for (int32 Seed = 1; Seed <= 4096; ++Seed)
    {
        Rifle = UBreakerLootLibrary::RollItem(TEXT("Pierce.Duration"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
        if (Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle) break;
    }
    if (!TestTrue(TEXT("actual ordinary rolled Rifle"), Rifle.IsValid() && Rifle.WeaponArchetype == EBreakerWeaponArchetype::Rifle)) return false;
    Player->GetEquipment()->BindAttributes(Attributes);
    if (!TestTrue(TEXT("actual legal equip"), Player->GetEquipment()->EquipItem(Rifle))) return false;
    auto* Weapon = Player->GetWeapon(); Weapon->BeginPlay(); Weapon->SyncArchetypesToEquipment(); Weapon->EquipSlot(1);
    auto* Momentum = Player->GetMomentum(); Momentum->BindAttributes(Attributes); Momentum->SetComponentTickEnabled(false);
    Movement->SetMovementMode(MOVE_Walking); Movement->Velocity = FVector(Movement->WalkSpeed,0,0);
    for (int32 I = 0; I < 40; ++I) { Player->SetActorLocation(Player->GetActorLocation() + Movement->Velocity); Momentum->AdvanceLoop(1); }
    Movement->StopMovementImmediately(); Player->SetActorLocation(Origin);
    // Campaign-reachable innate Redline pierce; the gear affix requires i120.
    if (!TestTrue(TEXT("actual earned Swift band supplies pierce"), Weapon->GetShotChannels().PierceCount > 0)) return false;
    for (int32 I = 0; I < 30; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.05f); }
    TArray<ABreakerEnemy*> Targets;
    for (float X : {600.0f,900.0f})
    {
        auto* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), Origin + FVector(X,0,25), FRotator::ZeroRotator, Spawn);
        if (!Enemy) return false;
        Enemy->ConfigureCrowdProbe(); Enemy->SetAreaLevel(1); Enemy->SetMonsterRank(EBreakerMonsterRank::Boss);
        Enemy->DispatchBeginPlay(); Enemy->SetActorTickEnabled(false);
        if (auto* Move = Enemy->GetMovementComponent()) Move->SetComponentTickEnabled(false);
        Targets.Add(Enemy);
    }
    auto* Source = Targets[0]->FindComponentByClass<UBreakerStatusComponent>();
    auto* Destination = Targets[1]->FindComponentByClass<UBreakerStatusComponent>();
    if (!Source || !Destination) return false;
    // Explicit physical status seed isolates copying; it is not a claim that Swift casts Poison.
    FBreakerStatusApplicationSpec Poison;
    Poison.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"));
    Poison.Duration = 6; Poison.TickInterval = .5f; Poison.BaseDamagePerTick = 8; Poison.ProcCoefficient = 1;
    Source->ApplyStatus(Poison, EBreakerDamageFamily::Physical, Player);
    Source->AdvanceStatuses(1);
    if (!TestEqual(TEXT("one source status"), Source->GetActiveStatuses().Num(),1)) return false;
    const auto Original = Source->GetActiveStatuses()[0];
    TestEqual(TEXT("source duration scales exactly once before aging"), Original.RemainingDuration, 6 * DurationScale - 1, .001f);
    FVector Eye; FRotator Facing; Controller->GetPlayerViewPoint(Eye,Facing);
    Controller->SetControlRotation((Targets[0]->GetActorLocation() - Eye).Rotation());
    if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->UpdateCamera(.05f);
    const int32 Ammo = Weapon->GetMagazineAmmo();
    Weapon->StartFire(); Weapon->StopFire();
    TestEqual(TEXT("actual rifle round spent"), Weapon->GetMagazineAmmo(), Ammo - 1);
    if (!TestEqual(TEXT("actual pierce carries Poison to second body"), Destination->GetActiveStatuses().Num(),1)) return false;
    const auto Copy = Destination->GetActiveStatuses()[0];
    TestEqual(TEXT("copy preserves remaining lifetime without another duration multiplier"), Copy.RemainingDuration, Original.RemainingDuration,.001f);
    TestEqual(TEXT("copy retains normalized tick payload"), Copy.Spec.BaseDamagePerTick, Original.Spec.BaseDamagePerTick * BreakerStatusRules::PierceSpreadPayloadFraction(),.001f);
    TestEqual(TEXT("copy remains depth two"), Copy.Spec.ProcCoefficient,0.0f);
    Destination->AdvanceStatuses(Original.RemainingDuration + .01f);
    TestEqual(TEXT("copy expires with its original remaining budget"), Destination->GetActiveStatuses().Num(),0);
    return true;
}
#endif
