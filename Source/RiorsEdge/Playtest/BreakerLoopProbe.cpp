#include "Playtest/BreakerLoopProbe.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerHoldfastEnemy.h"
#include "Combat/BreakerBossEnemy.h"
#include "Interaction/BreakerNPC.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestJournal.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerZoneBuilder.h"
#include "Interaction/BreakerRiftDoor.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerRiftRewardMath.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

bool UBreakerLoopProbe::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return Super::ShouldCreateSubsystem(Outer)
        && FParse::Param(FCommandLine::Get(), TEXT("BreakerLoopProbe"));
#endif
}

void UBreakerLoopProbe::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // A custom user directory redirects ALL engine SaveGames, including the
    // account and roster. Refuse before any probe action without isolation.
    FString UserDirectory;
    if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory)
        || UserDirectory.IsEmpty()
        || FPaths::IsRelative(UserDirectory)
        || IFileManager::Get().DirectoryExists(*(FPaths::ProjectSavedDir() / TEXT("SaveGames")))
        || FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(UserDirectory), FPaths::ProjectDir()))
    {
        Finish(false, TEXT("An isolated -UserDir is required"));
        return;
    }
    StartedAt = FPlatformTime::Seconds();
    bActTwo = FParse::Param(FCommandLine::Get(), TEXT("BreakerActTwoLoop"));
    if (bActTwo) UE_LOG(LogTemp, Display, TEXT("[LoopProbe] Act I + II earned-path mode: no seeded flags/points; authored visible dialogue choices, accelerated real kills, real travel. Not a Slate input test."));
    UE_LOG(LogTemp, Display, TEXT("[LoopProbe] isolated saves: %s"), *FPaths::ProjectSavedDir());
    Ticker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UBreakerLoopProbe::TickProbe), 1.0f);
}

void UBreakerLoopProbe::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
    Super::Deinitialize();
}

bool UBreakerLoopProbe::Finish(bool bPassed, const TCHAR* Reason)
{
    UE_LOG(LogTemp, Display, TEXT("[LoopProbe] %s: %s (stage %d, kills %d, XP %d, Riftglass %d)"),
        bPassed ? TEXT("PASS") : TEXT("FAIL"), Reason, Stage, KilledEnemies, PaidXp, PaidGlass);
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
    return false;
}

bool UBreakerLoopProbe::SelectTravel(ABreakerCharacter* Player, FName Destination)
{
    for (TActorIterator<ABreakerTravelPoint> It(Player->GetWorld()); It; ++It)
    {
        if (It->IsA<ABreakerRiftDoor>()) continue;
        for (const FBreakerTravelDestination& Entry : It->GetAvailableDestinations())
        {
            if (Entry.Id != Destination) continue;
            DepartedWorld = Player->GetWorld();
            return It->SelectDestination(Destination, Player);
        }
    }
    return false;
}

bool UBreakerLoopProbe::SelectDialogueFlag(ABreakerCharacter* Player, FName Flag)
{
    UBreakerQuestJournal* Journal = Player->GetQuestJournal();
    if (!Journal) return false;
    // Find a reachable path using the same visibility conditions the menu
    // uses. Only choices on that path are committed through its character API.
    struct FSearch { FName Node; FBreakerQuestFlagSet Flags; TArray<FBreakerDialogueChoice> Path; };
    for (TActorIterator<ABreakerNPC> It(Player->GetWorld()); It; ++It)
    {
        TArray<FSearch> Queue;
        FSearch Start; Start.Node = It->ResolveStartNodeId(Journal->GetState()); Start.Flags = Journal->GetState(); Queue.Add(Start);
        TSet<FName> Visited;
        for (int32 Index = 0; Index < Queue.Num() && Index < 100; ++Index)
        {
            const FSearch Search = Queue[Index];
            if (Visited.Contains(Search.Node)) continue;
            Visited.Add(Search.Node);
            FBreakerDialogueNode Node;
            if (!It->FindDialogueNode(Search.Node, Node)) continue;
            TArray<FBreakerDialogueChoice> Choices; It->GetVisibleChoices(Node, Search.Flags, Choices);
            for (const FBreakerDialogueChoice& Choice : Choices)
            {
                FSearch Next = Search; Next.Path.Add(Choice); Next.Flags.Add(Choice.SetsQuestFlag);
                if (Choice.SetsQuestFlag == Flag)
                {
                    for (const FBreakerDialogueChoice& Selected : Next.Path)
                    {
                        Player->AddQuestFlag(Selected.SetsQuestFlag);
                        UE_LOG(LogTemp, Display, TEXT("[LoopProbe] dialogue %s: %s -> %s"), *It->GetDisplayName().ToString(), *Selected.Text, *Selected.SetsQuestFlag.ToString());
                    }
                    return Journal->HasFlag(Flag);
                }
                if (Choice.Action == EBreakerDialogueAction::None && !Choice.NextNodeId.IsNone())
                { Next.Node = Choice.NextNodeId; Queue.Add(MoveTemp(Next)); }
            }
        }
    }
    return false;
}

bool UBreakerLoopProbe::TickActTwo(ABreakerCharacter* Player, ABreakerGameMode* Mode)
{
    if (PhotoDelay > 0)
    {
        if (--PhotoDelay == 1) FScreenshotRequest::RequestScreenshot(PendingPhoto, true, false);
        return true;
    }
    UWorld* World = Player->GetWorld();
    UBreakerQuestJournal* Journal = Player->GetQuestJournal();
    UBreakerGameInstance* Session = Cast<UBreakerGameInstance>(GetGameInstance());
    if (!Journal || !Session) return Finish(false, TEXT("Campaign journal/session missing"));
    const FBreakerMissionBeat* Beat = nullptr;
    for (const FBreakerMissionDefinition& Mission : UBreakerMissionLibrary::GetMissions())
    {
        if (Mission.Act > 2) continue;
        Beat = UBreakerMissionLibrary::CurrentBeat(Mission, Journal->GetState());
        if (Beat) break;
    }
    if (!Beat)
    {
        const FBreakerProgressionState& Progress = Player->GetProgression()->GetProgressionState();
        const bool bCorrect = UBreakerGameInstance::IsAnchorMap(World) && bSawMarshal && BreachMaximumWave == 4
            && Journal->HasFlag(TEXT("Quest.Breach.TurnedIn"))
            && UBreakerMissionLibrary::DoctrinePointEntitlement(Journal->GetState()) == 4
            && Progress.LevelDoctrinePointsGranted == 4 && Progress.UnspentDoctrinePoints == 4;
        return Finish(bCorrect, TEXT("Earned Act I + II: real dialogue/deaths/travel, dedicated contact, four-wave Field Marshal and exactly four cumulative Doctrine points"));
    }
    if (LastCampaignBeat != Beat->BeatId)
    {
        LastCampaignBeat = Beat->BeatId; ++Stage;
        UE_LOG(LogTemp, Display, TEXT("[LoopProbe] campaign beat %s, earned Doctrine=%d"), *Beat->BeatId.ToString(), Player->GetProgression()->GetProgressionState().LevelDoctrinePointsGranted);
    }
    const bool bDialogue = Beat->Kind == EBreakerMissionBeatKind::Dialogue || Beat->Kind == EBreakerMissionBeatKind::Return;
    if (bDialogue)
    {
        if (!UBreakerGameInstance::IsAnchorMap(World))
            return SelectTravel(Player, ABreakerTravelPoint::HubDestinationId) ? true : Finish(false, TEXT("Campaign cannot return to Anchor"));
        return SelectDialogueFlag(Player, Beat->CompletesOn) ? true : Finish(false, TEXT("Campaign dialogue completion flag is not reachable through visible choices"));
    }
    if (Beat->Kind == EBreakerMissionBeatKind::Travel)
        return SelectTravel(Player, Beat->Destination) ? true : Finish(false, TEXT("Campaign destination unavailable"));
    if (!UBreakerGameInstance::IsFernhallMap(World))
        return SelectTravel(Player, ABreakerTravelPoint::FernhallDestinationId) ? true : Finish(false, TEXT("Campaign cannot reach Fernhall"));
    if (Beat->Kind == EBreakerMissionBeatKind::Boss && !Mode->IsRiftInstance())
    {
        for (TActorIterator<ABreakerRiftDoor> It(World); It; ++It)
            if (It->Rift.EncounterId == Beat->Rift)
            {
                DepartedWorld = World;
                return It->SelectDestination(ABreakerTravelPoint::RiftDestinationId, Player) ? true : Finish(false, TEXT("Campaign boss door refused"));
            }
        return Finish(false, TEXT("Campaign boss door missing"));
    }
    if (Beat->Kind == EBreakerMissionBeatKind::Boss && Mode->IsRiftRunCompleted())
        return Finish(false, TEXT("Actual boss completion did not advance its mission beat"));
    if (Mode->IsRiftInstance() && Session->PendingRift.EncounterId == FName(TEXT("breach.marshalling")))
        BreachMaximumWave = FMath::Max(BreachMaximumWave, Mode->GetCurrentWave());
    TArray<ABreakerEnemy*> Living;
    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        if (It->IsDeadEnemy()) continue;
        if (!Beat->WorldEncounter.IsNone() && !It->ActorHasTag(TEXT("Fernhall.AlteredContact"))) continue;
        Living.Add(*It);
    }
    for (ABreakerEnemy* Enemy : Living)
    {
        const bool bContact = Enemy->ActorHasTag(TEXT("Fernhall.AlteredContact"));
        const bool bMarshal = Session->PendingRift.EncounterId == FName(TEXT("breach.marshalling"))
            && Enemy->GetClass() == ABreakerBossEnemy::ClassForBossName(TEXT("FieldMarshal")).Get();
        if (FParse::Param(FCommandLine::Get(), TEXT("BreakerActTwoPhotos"))
            && ((bContact && !bContactPhotoTaken) || (bMarshal && !bMarshalPhotoTaken)))
        {
            bContactPhotoTaken |= bContact;
            bMarshalPhotoTaken |= bMarshal;
            const FVector Target = Enemy->GetActorLocation();
            const FVector Toward = (Target - Player->GetActorLocation()).GetSafeNormal2D();
            const FVector At = Target - Toward * 900.0f + FVector(0, 0, 70);
            const FRotator Facing = (Target - At).Rotation();
            Player->TeleportTo(At, Facing);
            if (AController* Controller = Player->GetController()) Controller->SetControlRotation(Facing);
            const FString Directory = FPaths::ProjectSavedDir() / TEXT("Screenshots");
            IFileManager::Get().MakeDirectory(*Directory, true);
            PendingPhoto = Directory / (bContact ? TEXT("acttwo-contact.png") : TEXT("acttwo-marshal.png"));
            PhotoDelay = 2;
            return true;
        }
        if (Session->PendingRift.EncounterId == FName(TEXT("breach.marshalling"))
            && Enemy->GetClass() == ABreakerBossEnemy::ClassForBossName(TEXT("FieldMarshal")).Get()) bSawMarshal = true;
        FBreakerDamageRequest Hit; Hit.BaseDamage = 100000000; Hit.bCanCritical = false; Hit.bBypassShield = true; Hit.SetInstigator(Player);
        Enemy->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Hit);
        if (Enemy->IsDeadEnemy()) ++KilledEnemies;
    }
    // Finite field populations legitimately require another visit for some
    // objectives. Travel back out and in; never manufacture elite kills.
    if (Living.IsEmpty() && !Mode->IsRiftInstance() && Beat->WorldEncounter.IsNone())
        return SelectTravel(Player, ABreakerTravelPoint::HubDestinationId) ? true : Finish(false, TEXT("Finite encounter revisit unavailable"));
    return true;
}

bool UBreakerLoopProbe::TickProbe(float DeltaSeconds)
{
    if (FPlatformTime::Seconds() - StartedAt > (bActTwo ? 250.0 : 150.0))
        return Finish(false, TEXT("Travel/combat progression timed out"));
    UBreakerGameInstance* Session = Cast<UBreakerGameInstance>(GetGameInstance());
    UWorld* World = Session ? Session->GetWorld() : nullptr;
    if (!World || !World->HasBegunPlay() || Session->IsArrivalCoverUp()) return true;
    ABreakerGameMode* Mode = World->GetAuthGameMode<ABreakerGameMode>();
    APlayerController* Controller = World->GetFirstPlayerController();
    ABreakerCharacter* Player = Controller ? Cast<ABreakerCharacter>(Controller->GetPawn()) : nullptr;
    if (!Mode || !Player) return true;
    if (DepartedWorld.Get() == World) return true;
    Player->ResumeFromMenu();
    if (bActTwo)
    {
        if (Stage == 0 && !UBreakerGameInstance::IsAnchorMap(World)) return true;
        return TickActTwo(Player, Mode);
    }

    if (Stage == 0)
    {
        if (!UBreakerGameInstance::IsAnchorMap(World)) return true;
        if (!SelectTravel(Player, ABreakerTravelPoint::FernhallDestinationId))
            return Finish(false, TEXT("Anchor has no working Fernhall selection"));
        Stage = 1;
        UE_LOG(LogTemp, Display, TEXT("[LoopProbe] Anchor -> Fernhall selected"));
        return true;
    }
    if (Stage == 1)
    {
        if (!UBreakerGameInstance::IsFernhallMap(World) || Mode->IsRiftInstance())
            return Finish(false, TEXT("Ordinary Fernhall arrival has wrong instance state"));
        for (TActorIterator<ABreakerRiftDoor> It(World); It; ++It)
        {
            if (It->Rift.AreaName.EqualTo(UBreakerZoneBuilder::FernhallRiftFor(TEXT("substation")).AreaName))
            {
                DepartedWorld = World;
                if (!It->SelectDestination(ABreakerTravelPoint::RiftDestinationId, Player))
                    return Finish(false, TEXT("Authored Rift door refused entry"));
                Stage = 2;
                UE_LOG(LogTemp, Display, TEXT("[LoopProbe] Fernhall -> Substation Rift selected"));
                return true;
            }
        }
        return Finish(false, TEXT("Substation Rift door is missing"));
    }
    if (Stage == 2)
    {
        if (!UBreakerGameInstance::IsFernhallMap(World) || !Mode->IsRiftInstance())
            return Finish(false, TEXT("Rift load lost its instance definition"));
        if (!Session->PendingRift.AreaName.EqualTo(UBreakerZoneBuilder::FernhallRiftFor(TEXT("substation")).AreaName))
            return Finish(false, TEXT("Rift load selected the wrong authored encounter"));
        if (!bWatchingCompletion)
        {
            bWatchingCompletion = true;
            // Native multicast dispatches newest bindings first. Installed
            // after player BeginPlay, this sees balances before the existing
            // progression subscription awards the completion purse.
            Mode->OnRiftCompleted.AddWeakLambda(this, [this](const FBreakerRiftDefinition&, APawn* CompletedBy)
            {
                if (ABreakerCharacter* CompletedPlayer = Cast<ABreakerCharacter>(CompletedBy))
                {
                    ++CompletionCount;
                    XpBeforePurse = CompletedPlayer->GetProgression()->GetProgressionState().TotalExperience;
                    GlassBeforePurse = CompletedPlayer->GetEquipment()->GetForgeWallet().Get();
                }
            });
        }
        if (Mode->IsRiftRunCompleted())
        {
            PaidXp = Player->GetProgression()->GetProgressionState().TotalExperience;
            PaidGlass = Player->GetEquipment()->GetForgeWallet().Get();
            const int32 AreaLevel = Session->PendingRift.EffectiveAreaLevel();
            if (CompletionCount != 1 || !bSawHoldfast
                || PaidXp - XpBeforePurse != BreakerRiftReward::XpForCompletion(AreaLevel)
                || PaidGlass - GlassBeforePurse != BreakerRiftReward::RiftglassForCompletion(AreaLevel))
                return Finish(false, TEXT("Holdfast completion did not award its exact first-clear purse"));
            if (!SelectTravel(Player, ABreakerTravelPoint::FernhallDestinationId))
                return Finish(false, TEXT("Completed Rift has no working Fernhall exit"));
            Stage = 3;
            UE_LOG(LogTemp, Display, TEXT("[LoopProbe] Completed Rift -> Fernhall selected"));
            return true;
        }
        // Kill through the real damage/death path. Let game-mode timers move
        // the run forward naturally; never force wave or completion state.
        TArray<ABreakerEnemy*> Living;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
            if (!It->IsDeadEnemy()) Living.Add(*It);
        for (ABreakerEnemy* Enemy : Living)
        {
            bSawHoldfast |= Enemy->IsA<ABreakerHoldfastEnemy>();
            FBreakerDamageRequest Hit;
            Hit.BaseDamage = 100000000.0f;
            Hit.bCanCritical = false;
            Hit.bBypassShield = true;
            Hit.SetInstigator(Player);
            Enemy->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Hit);
            if (Enemy->IsDeadEnemy()) ++KilledEnemies;
        }
        return true;
    }
    if (Session->PendingRift.IsSet() || Mode->IsRiftInstance())
        return Finish(false, TEXT("Leaving Rift retained stale instance state"));
    if (Player->GetProgression()->GetProgressionState().TotalExperience != PaidXp
        || Player->GetEquipment()->GetForgeWallet().Get() != PaidGlass)
        return Finish(false, TEXT("Earned rewards did not survive real map save/load"));
    if (Stage == 3)
    {
        if (!UBreakerGameInstance::IsFernhallMap(World))
            return Finish(false, TEXT("Rift exit loaded the wrong map"));
        if (!SelectTravel(Player, ABreakerTravelPoint::HubDestinationId))
            return Finish(false, TEXT("Fernhall has no working Anchor return"));
        Stage = 4;
        UE_LOG(LogTemp, Display, TEXT("[LoopProbe] Fernhall -> Anchor selected; rewards retained"));
        return true;
    }
    return Finish(UBreakerGameInstance::IsAnchorMap(World),
        TEXT("Real Anchor/Fernhall/Rift/reward/Fernhall/Anchor map loads and reward persistence checked"));
}
