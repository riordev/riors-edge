#include "Interaction/BreakerSurvivor.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Game/BreakerGameInstance.h"
#include "Save/BreakerQuestJournal.h"

ABreakerSurvivor::ABreakerSurvivor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
}

void ABreakerSurvivor::BeginPlay()
{
    Super::BeginPlay();
    const FBreakerDialogueRow* Row = GetDialogueData().Npcs.FindByPredicate(
        [](const FBreakerDialogueRow& Entry) { return Entry.Id == TEXT("Survivor"); });
    if (Row)
    {
        DisplayName = FText::FromString(Row->DisplayName);
        StartNodeId = Row->StartNodeId;
        DialogueNodes = Row->Nodes;
        EntryOverrides = Row->Entries;
    }
    else UE_LOG(LogTemp, Error, TEXT("Survivor has no authored dialogue row"));
}

void ABreakerSurvivor::ConfigureEscort(const FBreakerErasedEarthLayout& Layout)
{
    if (!HasAuthority()) return;
    Shelter = Layout.SurvivorShelter; Extraction = Layout.Extraction;
    Route = Layout.Route; ClearedPockets.Init(false, FMath::Max(0, Layout.PocketCount));
    ResetToShelter();
}

void ABreakerSurvivor::ResetToShelter()
{
    if (!HasAuthority()) return;
    // Explicit attempt reset only. Following never uses teleportation.
    SetActorLocation(Shelter, false, nullptr, ETeleportType::TeleportPhysics);
    RouteIndex = 0; LucidityRemaining = 0;
    EscortPlayer.Reset(); bDialoguePaused = false;
    EscortState = EBreakerSurvivorEscortState::Sheltered;
}

bool ABreakerSurvivor::TryBeginEscort(ABreakerCharacter* Player)
{
    if (!HasAuthority() || !Player || Player->GetWorld() != GetWorld() || !Player->GetCombat()
        || Player->GetCombat()->IsDead() || !Player->GetQuestJournal() || Route.IsEmpty() || ClearedPockets.IsEmpty()
        || !FMath::IsFinite(LuciditySeconds) || LuciditySeconds <= 0
        || EscortState == EBreakerSurvivorEscortState::AtExtraction || IsEscortActive()
        || FVector::Dist(Player->GetActorLocation(), GetActorLocation()) > InteractionRange) return false;
    const auto& Flags = Player->GetQuestJournal()->GetState().Flags;
    if (!UBreakerGameInstance::IsErasedEarthMap(this) || !Flags.Contains(TEXT("Quest.Breach.TurnedIn"))
        || !Flags.Contains(TEXT("Quest.Survivor.Accepted"))
        || Flags.Contains(TEXT("Quest.Survivor.Extracted"))) return false;
    for (const FBreakerSurvivorRoutePoint& Point : Route)
        if (Point.RequiredPocket != INDEX_NONE && !ClearedPockets.IsValidIndex(Point.RequiredPocket)) return false;
    EscortPlayer = Player; RouteIndex = 0;
    LucidityRemaining = FMath::Max(0.0f, LuciditySeconds);
    EscortState = EBreakerSurvivorEscortState::Escorting;
    bDialoguePaused = false;
    return LucidityRemaining > 0.0f;
}

void ABreakerSurvivor::SetPocketCleared(int32 PocketIndex, bool bCleared)
{
    if (HasAuthority() && ClearedPockets.IsValidIndex(PocketIndex)) ClearedPockets[PocketIndex] = bCleared;
}
void ABreakerSurvivor::SetDialoguePaused(bool bPaused)
{
    if (HasAuthority()) bDialoguePaused = bPaused;
}
bool ABreakerSurvivor::AreAllPocketsCleared() const
{
    return !ClearedPockets.IsEmpty() && !ClearedPockets.Contains(false);
}
bool ABreakerSurvivor::IsAtExtraction() const
{
    return !Route.IsEmpty() && FVector::Dist(GetActorLocation(), Extraction) <= ExtractionRadius;
}
void ABreakerSurvivor::FailEscort()
{
    ResetToShelter();
    EscortState = EBreakerSurvivorEscortState::Failed;
    OnEscortFailed.Broadcast(this);
}

bool ABreakerSurvivor::MoveAlongRoute(float DeltaSeconds)
{
    if (!Route.IsValidIndex(RouteIndex) || !GetWorld() || !Body) return false;
    const FBreakerSurvivorRoutePoint& Point = Route[RouteIndex];
    if (FVector::Dist(GetActorLocation(), Point.Location) <= CheckpointRadius)
    {
        if (Point.RequiredPocket != INDEX_NONE && !ClearedPockets[Point.RequiredPocket]) return false;
        ++RouteIndex;
        return false;
    }
    const FVector Direction = (Point.Location - GetActorLocation()).GetSafeNormal2D();
    const float Step = FMath::Min(FMath::Max(0.0f, WalkSpeed) * DeltaSeconds,
        static_cast<float>(FVector::Dist2D(GetActorLocation(), Point.Location)));
    FVector Destination = GetActorLocation() + Direction * Step;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SurvivorRouteGround), false, this);
    Query.AddIgnoredActor(EscortPlayer.Get());
    FHitResult Ground;
    if (!GetWorld()->LineTraceSingleByChannel(Ground, Destination + FVector(0, 0, 30),
        Destination - FVector(0, 0, Body->GetScaledCapsuleHalfHeight() + 50), ECC_Visibility, Query)
        || Ground.ImpactNormal.Z < 0.7f) return false;
    const float GroundedZ = Ground.ImpactPoint.Z + Body->GetScaledCapsuleHalfHeight() + 2.0f;
    if (FMath::Abs(GroundedZ - GetActorLocation().Z) > 35.0f) return false;
    Destination.Z = GroundedZ;
    FHitResult Block;
    SetActorLocation(Destination, true, &Block, ETeleportType::None);
    if (!Direction.IsNearlyZero()) SetActorRotation(Direction.Rotation());
    return !Block.bBlockingHit;
}

void ABreakerSurvivor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || !IsEscortActive() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
    ABreakerCharacter* Player = EscortPlayer.Get();
    if (!Player || Player->GetWorld() != GetWorld() || !Player->GetCombat() || Player->GetCombat()->IsDead())
    {
        FailEscort(); return;
    }
    if (bDialoguePaused) return;
    LucidityRemaining = FMath::Max(0.0f, LucidityRemaining - DeltaSeconds);
    if (LucidityRemaining <= 0) { FailEscort(); return; }
    if (FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) <= FollowRange) MoveAlongRoute(DeltaSeconds);
    if (RouteIndex >= Route.Num() && IsAtExtraction() && AreAllPocketsCleared()
        && FVector::Dist(Player->GetActorLocation(), Extraction) <= ExtractionRadius)
    {
        EscortState = EBreakerSurvivorEscortState::AtExtraction;
        OnEscortReadyForExtraction.Broadcast(this, Player);
    }
}
