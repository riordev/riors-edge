#include "Game/BreakerLocalMapComponent.h"
#include "Game/BreakerPrototypeDestinations.h"
#include "Game/BreakerContainmentHunt.h"
#include "Combat/BreakerEnemy.h"
#include "Interaction/BreakerFernhallCache.h"
#include "Interaction/BreakerBasinRecorder.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerAlteredEnemy.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerZoneBuilder.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerRiftDoor.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Save/BreakerMissionContent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    const FBreakerMissionBeat* BreakerMapCurrentBeat(const AActor* Owner)
    {
        const auto* Player = Cast<ABreakerCharacter>(Owner);
        if (!Player || !Player->GetQuestJournal()) return nullptr;
        for (const auto& Mission : UBreakerMissionLibrary::GetMissions())
            if (const auto* Beat = UBreakerMissionLibrary::CurrentBeat(Mission, Player->GetQuestJournal()->GetState())) return Beat;
        return nullptr;
    }
}

FText UBreakerLocalMapComponent::GetCampaignObjective() const
{
    if (const auto* Prototype=BreakerPrototypeDestinations::ForWorld(this))
    {
        if (Prototype->Id==TEXT("StationZero"))
        {
            const auto* Player=Cast<ABreakerCharacter>(GetOwner());
            const bool bComplete=Player && Player->GetQuestJournal() && Player->GetQuestJournal()->HasFlag(UBreakerContainmentHunt::CompletionFlag());
            return FText::FromString(bComplete
                ? TEXT("Containment Custodian eliminated. Return to Anchor 13 from either travel gate; supply lockers are optional.")
                : TEXT("Hunt the Containment Custodian in the Containment Laboratory (fixed area 28). Supply lockers are optional."));
        }
        if(Prototype->Id==TEXT("RedBasin"))
        {
            const auto* Player=Cast<ABreakerCharacter>(GetOwner());const auto* Journal=Player?Player->GetQuestJournal():nullptr;
            return FText::FromString(Journal&&Journal->HasFlag(ABreakerBasinRecorder::ExtractedFlag())
                ?TEXT("Survey recorder extracted. Return to Anchor 13 from either travel gate; supply caches are optional.")
                :Journal&&Journal->HasFlag(ABreakerBasinRecorder::RecoveredFlag())
                    ?TEXT("Carry the survey recorder to the Impact Basin extraction relay.")
                    :TEXT("Recover the survey recorder from Burned Homestead."));
        }
        int32 Recovered=0;
        for (TActorIterator<ABreakerFernhallCache> It(GetWorld());It;++It)
            if (It->ActorHasTag(TEXT("PrototypeDestination.Cache")) && It->IsOpened()) ++Recovered;
        return FText::FromString(Recovered==3 ? TEXT("All supplies recovered. Return to Anchor 13 from either travel gate.")
            : FString::Printf(TEXT("%s: recover district supplies (%d/3). Clear each marked pocket, then open its cache."),*Prototype->DisplayName,Recovered));
    }
    const auto* Player = Cast<ABreakerCharacter>(GetOwner());
    const auto* Beat = BreakerMapCurrentBeat(GetOwner());
    return Player && Beat ? FText::FromString(UBreakerMissionLibrary::TrackerLine(*Beat, Player->GetQuestJournal()->GetState())) : FText::GetEmpty();
}

UBreakerLocalMapComponent::UBreakerLocalMapComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = .5f; // O2 discovery polling, not combat timing.
}

FText UBreakerLocalMapComponent::GetRegionName() const
{
    if (const auto* Prototype=BreakerPrototypeDestinations::ForWorld(this)) return FText::FromString(Prototype->DisplayName);
    if (UBreakerGameInstance::IsAnchorMap(this)) return FText::FromString(TEXT("ANCHOR 13"));
    if (UBreakerGameInstance::IsFernhallMap(this)) return FText::FromString(TEXT("FERNHALL APPROACH"));
    if (UBreakerGameInstance::IsErasedEarthMap(this)) return FText::FromString(TEXT("ERASED EARTH"));
    if (UBreakerGameInstance::IsStrippedEarthMap(this)) return FText::FromString(TEXT("STRIPPED EARTH"));
    if (UBreakerGameInstance::IsWinningEarthMap(this)) return FText::FromString(TEXT("WINNING EARTH"));
    return FText::FromString(TEXT("LOCAL AREA"));
}

TArray<FBreakerLocalMapMarker> UBreakerLocalMapComponent::GetMarkers() const
{
    TArray<FBreakerLocalMapMarker> Out;
    UWorld* World = GetWorld();
    if (!World) return Out;
    const auto* Beat = BreakerMapCurrentBeat(GetOwner());
    const auto* Mode = Cast<ABreakerGameMode>(World->GetAuthGameMode());
    const FString Region = UGameplayStatics::GetCurrentLevelName(this, true) + (Mode && Mode->IsRiftInstance() ? TEXT(".instance") : TEXT(""));
    for (TActorIterator<ABreakerTravelPoint> It(World); It; ++It)
    {
        if (!IsValid(*It)) continue;
        auto& Marker = Out.AddDefaulted_GetRef();
        Marker.Location = It->GetActorLocation(); Marker.Label = It->GetDisplayName(); Marker.Detail = It->GetDisplayDetail();
        const auto* Door = Cast<ABreakerRiftDoor>(*It);
        Marker.bRift = Door != nullptr;
        if (Beat)
        {
            if ((Beat->Kind == EBreakerMissionBeatKind::Encounter || Beat->Kind == EBreakerMissionBeatKind::Boss)
                && Door && !Beat->Rift.IsNone()) Marker.bObjective = Door->Rift.EncounterId == Beat->Rift;
            else if (Beat->Kind == EBreakerMissionBeatKind::Travel)
                Marker.bObjective = It->GetAvailableDestinations().ContainsByPredicate([Beat](const auto& Destination)
                    { return Destination.Id == Beat->Destination; });
        }
        // Generic gates have no authored registry id. Their fixed site coordinates
        // distinguish separate gates without depending on transient actor names.
        const FVector Position = It->GetActorLocation();
        const FString Site = Door && !Door->Rift.EncounterId.IsNone() ? Door->Rift.EncounterId.ToString()
            : FString::Printf(TEXT("%s.%d.%d.%d"), *It->GetClass()->GetName(), FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y), FMath::RoundToInt(Position.Z));
        Marker.Id = FName(*(Region + TEXT(".site.") + Site));
    }
    const auto* HuntPlayer=Cast<ABreakerCharacter>(GetOwner());
    const bool bHuntDone=HuntPlayer && HuntPlayer->GetQuestJournal() && HuntPlayer->GetQuestJournal()->HasFlag(UBreakerContainmentHunt::CompletionFlag());
    if (!bHuntDone)
        for (TActorIterator<ABreakerEnemy> It(World);It;++It)
        {
            if (!IsValid(*It) || !It->ActorHasTag(UBreakerContainmentHunt::TargetTag())) continue;
            const auto* Combat=It->FindComponentByClass<UBreakerCombatComponent>();
            if (!Combat || Combat->IsDead()) continue;
            auto& Marker=Out.AddDefaulted_GetRef();Marker.Id=UBreakerContainmentHunt::TargetTag();
            Marker.Label=FText::FromString(TEXT("Containment Custodian"));Marker.Detail=FText::FromString(TEXT("Priority target / fixed area 28"));
            Marker.Location=It->GetActorLocation();Marker.bObjective=true;
        }
    for (TActorIterator<ABreakerFernhallCache> It(World);It;++It)
    {
        if (!IsValid(*It) || !It->ActorHasTag(TEXT("PrototypeDestination.Cache")) || It->IsOpened()) continue;
        FName Site;
        for (FName Tag : It->Tags) if (Tag.ToString().StartsWith(TEXT("Destination.Site."))) { Site=Tag; break; }
        if (Site.IsNone()) continue;
        auto& Marker=Out.AddDefaulted_GetRef(); Marker.Id=Site;
        Marker.Label=It->GetDisplayName(); Marker.Detail=It->GetCachePrompt(); Marker.Location=It->GetActorLocation();
        const auto* Destination=BreakerPrototypeDestinations::ForWorld(this);
        Marker.bObjective=!Destination; // Both prototype supply routes are now optional.
    }
    for(TActorIterator<ABreakerBasinRecorder> It(World);It;++It)
    {
        if(!IsValid(*It)||!It->IsCurrentStep(Cast<ABreakerCharacter>(GetOwner())))continue;
        auto& Marker=Out.AddDefaulted_GetRef();Marker.Id=It->IsExtraction()?FName(TEXT("RedBasin.Extraction")):FName(TEXT("RedBasin.Recovery"));
        Marker.Label=It->GetDisplayName();Marker.Detail=It->GetRecorderPrompt();Marker.Location=It->GetActorLocation();Marker.bObjective=true;
    }
    for (TActorIterator<ABreakerNPC> It(World); It; ++It)
    {
        if (!IsValid(*It) || It->IsA<ABreakerFernhallCache>() || It->IsA<ABreakerBasinRecorder>()) continue;
        auto& Marker = Out.AddDefaulted_GetRef();
        Marker.Location = It->GetActorLocation(); Marker.Label = It->GetDisplayName();
        // NPC appearances can change with the player's body; discovery belongs
        // to the stationary service site, never the mesh or localized label.
        const FVector Position = It->GetActorLocation();
        const FString Site = FString::Printf(TEXT("%s.%d.%d.%d"), *It->GetClass()->GetName(),
            FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y), FMath::RoundToInt(Position.Z));
        Marker.Id = FName(*(Region + TEXT(".npc.") + Site));
        Marker.Detail = FText::FromString(TEXT("SERVICE / CONTACT"));
        Marker.bObjective = Beat && (Beat->Kind == EBreakerMissionBeatKind::Dialogue || Beat->Kind == EBreakerMissionBeatKind::Return)
            && !It->DialogueId.IsNone() && It->DialogueId == Beat->Npc;
    }
    // This authored world encounter already has a dedicated live actor. Guide
    // the current investigation to that actor without inventing discovery or
    // marking every ordinary enemy as a map site.
    if (Beat && Beat->Kind == EBreakerMissionBeatKind::Encounter
        && Beat->WorldEncounter == FName(TEXT("fernhall.altered_contact")))
    {
        ABreakerAlteredEnemy* Contact = nullptr;
        for (TActorIterator<ABreakerAlteredEnemy> It(World); It; ++It)
        {
            if (!IsValid(*It) || !It->Tags.Contains(TEXT("Fernhall.AlteredContact"))) continue;
            const auto* Combat = It->FindComponentByClass<UBreakerCombatComponent>();
            if (!Combat || Combat->IsDead()) continue;
            if (!Contact || It->GetUniqueID() < Contact->GetUniqueID()) Contact = *It;
        }
        if (Contact)
        {
            auto& Marker = Out.AddDefaulted_GetRef();
            Marker.Id = FName(*(Region + TEXT(".encounter.") + Beat->WorldEncounter.ToString()));
            Marker.Location = Contact->GetActorLocation();
            Marker.Label = GetCampaignObjective();
            Marker.Detail = FText::FromString(FString::Printf(TEXT("CONTACT / LEVEL %d"), Contact->GetAreaLevel()));
            Marker.bObjective = true;
        }
    }
    Out.Sort([](const auto& A, const auto& B) { return A.Id.LexicalLess(B.Id); });
    return Out;
}

TArray<FBox2D> UBreakerLocalMapComponent::GetGround() const
{
    TArray<FBox2D> Ground;
    if (GetWorld())
    {
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            if (!It->Tags.Contains(TEXT("BreakerMapGround"))) continue;
            FVector Origin, Extent; It->GetActorBounds(false, Origin, Extent);
            Ground.Emplace(FVector2D(Origin - Extent), FVector2D(Origin + Extent));
        }
    }
    if (!UBreakerGameInstance::IsFernhallMap(this)) return Ground;
    TArray<FBreakerZonePiece> Pieces;
    if (UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(), Pieces))
        for (const auto& Piece : Pieces)
            if (Piece.Name.StartsWith(TEXT("flr_")))
                Ground.Emplace(FVector2D(Piece.Origin - Piece.Extent), FVector2D(Piece.Origin + Piece.Extent));
    return Ground;
}

void UBreakerLocalMapComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, Type, Function);
    auto* Player = Cast<ABreakerCharacter>(GetOwner());
    if (!Player || !Player->HasAuthority() || !Player->HasActorBegunPlay() || Player->GetCombat()->IsDead()) return;
    if (DiscoverNearby(Player->GetActorLocation())) Player->SaveGameState();
}

bool UBreakerLocalMapComponent::DiscoverNearby(FVector Location)
{
    if (Location.ContainsNaN()) return false;
    bool Changed = false;
    for (const auto& Marker : GetMarkers())
        if (!Discovered.Contains(Marker.Id) && FVector::DistSquared(Location, Marker.Location) <= FMath::Square(1800.0f)) // O2: 18m discovery.
        { Discovered.Add(Marker.Id); Changed = true; }
    return Changed;
}

bool UBreakerLocalMapComponent::Track(FName Id)
{
    if (!Id.IsNone() && !GetMarkers().ContainsByPredicate([this, Id](const auto& Marker) { return Marker.Id == Id && IsVisible(Marker); })) return false;
    Tracked = Id;
    if (auto* Player = Cast<ABreakerCharacter>(GetOwner())) Player->SaveGameState();
    return true;
}

bool UBreakerLocalMapComponent::GetTrackedMarker(FBreakerLocalMapMarker& Out) const
{
    if (Tracked.IsNone()) return false;
    for (const auto& Marker : GetMarkers()) if (Marker.Id == Tracked && IsVisible(Marker)) { Out = Marker; return true; }
    return false;
}
