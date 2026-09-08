#include "Interaction/BreakerFeedstockPickup.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BreakerGameInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Save/BreakerQuestContent.h"
#include "Save/BreakerQuestJournal.h"

namespace
{
    bool BreakerFeedstockLocation(const AActor* Actor)
    {
        if (UBreakerGameInstance::IsFernhallMap(Actor)) return true;
        const auto* Session = Actor && Actor->GetWorld() ? Cast<UBreakerGameInstance>(Actor->GetWorld()->GetGameInstance()) : nullptr;
        return Session && UBreakerGameInstance::IsGymMap(Actor)
            && Session->PendingRift.EncounterId == FName(TEXT("fernhall.entry"));
    }
    bool BreakerFeedstockObjective(const ABreakerCharacter* Player, FBreakerQuestObjective& Out)
    {
        const auto* Journal = Player ? Player->GetQuestJournal() : nullptr;
        FBreakerQuestDefinition Quest;
        if (!Journal || !UBreakerQuestLibrary::FindQuest(TEXT("Quest.KessSalvage"), Quest)
            || UBreakerQuestLibrary::ComputeQuestState(Quest, Journal->GetState()) != EBreakerQuestState::Active) return false;
        for (const auto& Objective : Quest.Objectives)
            if (Objective.ObjectiveId == FName(TEXT("Feedstock"))
                && Objective.ProgressSource == EBreakerQuestProgressSource::FeedstockPickup
                && Objective.RequiredCount > 0 && !Journal->HasFlag(Objective.CompletionFlag))
            {
                Out = Objective;
                return Journal->GetState().GetCounter(Out.ProgressCounter) < Out.RequiredCount;
            }
        return false;
    }
}
ABreakerFeedstockPickup::ABreakerFeedstockPickup()
{
    bReplicates = true;
    bOnlyRelevantToOwner = true;
    SetReplicateMovement(true);
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Feedstock"));
    SetRootComponent(Visual);
    Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Visual->SetRelativeScale3D(FVector(.32f, .24f, .20f));
    Visual->SetCollisionProfileName(TEXT("NoCollision"));
    Visual->SetCanEverAffectNavigation(false);
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        Visual->SetMaterial(0, Material);
    // Compact physical residue, not a rarity-bearing inventory item.
}
void ABreakerFeedstockPickup::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Material = Visual->GetMaterial(0))
    {
        auto* Tint = UMaterialInstanceDynamic::Create(Material, Visual);
        Tint->SetVectorParameterValue(TEXT("Color"), FLinearColor(.24f, .12f, .035f));
        Visual->SetMaterial(0, Tint);
    }
    if (HasAuthority())
        if (auto* Player = Cast<ABreakerCharacter>(GetOwner()))
            if (auto* Journal = Player->GetQuestJournal())
                Journal->OnFlagSet.AddWeakLambda(this, [this](FName)
                {
                    FBreakerQuestObjective Objective;
                    if (!BreakerFeedstockObjective(Cast<ABreakerCharacter>(GetOwner()), Objective)) Destroy();
                });
}
ABreakerFeedstockPickup* ABreakerFeedstockPickup::SpawnForKill(ABreakerCharacter* Player, const FBreakerHitContext& Hit)
{
    auto* Enemy = Cast<ABreakerEnemy>(Hit.Target);
    FBreakerQuestObjective Objective;
    if (!Player || !Player->HasAuthority() || !Enemy || Hit.Instigator != Player || !Hit.Result.bKilled
        || Player->GetWorld() != Enemy->GetWorld() || !Enemy->IsDeadEnemy()
        || Enemy->GetFamily() != EBreakerEnemyFamily::Vestige || !BreakerFeedstockLocation(Player)
        || !BreakerFeedstockObjective(Player, Objective)) return nullptr;
    int32 Outstanding = 0;
    for (TActorIterator<ABreakerFeedstockPickup> It(Player->GetWorld()); It; ++It)
        if (It->GetOwner() == Player && !It->IsActorBeingDestroyed() && !It->bConsumed) ++Outstanding;
    if (Outstanding >= Objective.RequiredCount - Player->GetQuestJournal()->GetState().GetCounter(Objective.ProgressCounter)) return nullptr;
    auto* Claim = Enemy->FindComponentByClass<UBreakerFeedstockDeathClaim>();
    if (Claim && Claim->bClaimed) return nullptr;
    FHitResult Floor;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FeedstockFloor), false, Enemy);
    Query.AddIgnoredActor(Player);
    if (!Player->GetWorld()->LineTraceSingleByObjectType(Floor, Enemy->GetActorLocation() + FVector(0,0,100),
        Enemy->GetActorLocation() - FVector(0,0,1000), FCollisionObjectQueryParams(ECC_WorldStatic), Query)
        || Floor.ImpactNormal.Z < .7f) return nullptr;
    if (!Claim)
    {
        Claim = NewObject<UBreakerFeedstockDeathClaim>(Enemy);
        Enemy->AddInstanceComponent(Claim); Claim->RegisterComponent();
        Enemy->FindComponentByClass<UBreakerCombatComponent>()->OnVitalsRestored.AddDynamic(Claim, &UBreakerFeedstockDeathClaim::ResetClaim);
    }
    Claim->bClaimed = true;
    FActorSpawnParameters Params; Params.Owner = Player;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pickup = Player->GetWorld()->SpawnActor<ABreakerFeedstockPickup>(Floor.ImpactPoint + FVector(0,0,10), FRotator::ZeroRotator, Params);
    if (!Pickup) Claim->bClaimed = false;
    return Pickup;
}
bool ABreakerFeedstockPickup::CanCollect(const ABreakerCharacter* Player) const
{
    FBreakerQuestObjective Objective;
    const bool bEligible = !bConsumed && !IsActorBeingDestroyed() && Player && GetOwner() == Player
        && Player->GetWorld() == GetWorld() && Player->GetCombat() && !Player->GetCombat()->IsDead()
        && FVector::DistSquared(GetActorLocation(), Player->GetActorLocation()) <= FMath::Square(GetInteractionRange())
        && BreakerFeedstockLocation(Player) && BreakerFeedstockObjective(Player, Objective);
    if (!bEligible) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FeedstockCollectVisibility), false, Player);
    Query.AddIgnoredActor(this);
    FHitResult Block;
    return !GetWorld()->LineTraceSingleByObjectType(Block, Player->GetActorLocation(), GetActorLocation(),
        FCollisionObjectQueryParams(ECC_WorldStatic), Query);
}
bool ABreakerFeedstockPickup::TryCollect(ABreakerCharacter* Player)
{
    FBreakerQuestObjective Objective;
    if (!HasAuthority() || !Player || !Player->HasAuthority() || !CanCollect(Player)
        || !BreakerFeedstockObjective(Player, Objective)) return false;
    bConsumed = true; // Claim before callbacks can re-enter collection.
    Player->GetQuestJournal()->AddProgress(Objective.ProgressCounter, 1, Objective.RequiredCount, Objective.CompletionFlag);
    Destroy();
    return true;
}
