#include "Playtest/BreakerLoopProbe.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerHoldfastEnemy.h"
#include "Combat/BreakerBossEnemy.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerSurvivor.h"
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
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"

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
    bSurvivorLoop = FParse::Param(FCommandLine::Get(), TEXT("BreakerSurvivorLoop"));
    bActTwo = bSurvivorLoop || FParse::Param(FCommandLine::Get(), TEXT("BreakerActTwoLoop"));
    if (bActTwo) UE_LOG(LogTemp, Display, TEXT("[LoopProbe] Act I + II earned-path mode: no seeded flags/points; authored visible dialogue choices, accelerated real kills, real travel. Not a Slate input test."));
    UE_LOG(LogTemp, Display, TEXT("[LoopProbe] isolated saves: %s"), *FPaths::ProjectSavedDir());
    Ticker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UBreakerLoopProbe::TickProbe), 1.0f);
}

void UBreakerLoopProbe::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
    FTSTicker::GetCoreTicker().RemoveTicker(MarshalPhotoTicker);
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
                        if (Selected.Action == EBreakerDialogueAction::StartSurvivorEscort)
                        {
                            ABreakerSurvivor* Survivor = Cast<ABreakerSurvivor>(*It);
                            if (!Survivor) return false;
                            Player->TeleportTo(Survivor->GetActorLocation() + FVector(180, 0, 0), FRotator::ZeroRotator);
                            if (!Survivor->TryBeginEscort(Player)) return false;
                        }
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
    if (MarshalPhotoStage > 0 && MarshalPhotoStage < 4) return true;
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
        if (Mission.Act > (bSurvivorLoop ? 3 : 2)) continue;
        Beat = UBreakerMissionLibrary::CurrentBeat(Mission, Journal->GetState());
        if (Beat) break;
    }
    if (!Beat)
    {
        const FBreakerProgressionState& Progress = Player->GetProgression()->GetProgressionState();
        const bool bCorrect = UBreakerGameInstance::IsAnchorMap(World) && bSawMarshal && BreachMaximumWave == 4
            && Journal->HasFlag(TEXT("Quest.Breach.TurnedIn"))
            && (!bSurvivorLoop || Journal->HasFlag(TEXT("Quest.Survivor.TurnedIn")))
            && UBreakerMissionLibrary::DoctrinePointEntitlement(Journal->GetState()) == (bSurvivorLoop ? 6 : 4)
            && Progress.LevelDoctrinePointsGranted == (bSurvivorLoop ? 6 : 4)
            && Progress.UnspentDoctrinePoints == (bSurvivorLoop ? 6 : 4);
        return Finish(bCorrect, bSurvivorLoop
            ? TEXT("Earned Act I + II + Survivor: physical escort, actual extraction and Anchor return, exactly six cumulative Doctrine points")
            : TEXT("Earned Act I + II: real dialogue/deaths/travel, dedicated contact, four-wave Field Marshal and exactly four cumulative Doctrine points"));
    }
    if (LastCampaignBeat != Beat->BeatId)
    {
        LastCampaignBeat = Beat->BeatId; ++Stage;
        UE_LOG(LogTemp, Display, TEXT("[LoopProbe] campaign beat %s, earned Doctrine=%d"), *Beat->BeatId.ToString(), Player->GetProgression()->GetProgressionState().LevelDoctrinePointsGranted);
    }
    const bool bDialogue = Beat->Kind == EBreakerMissionBeatKind::Dialogue || Beat->Kind == EBreakerMissionBeatKind::Return;
    if (bDialogue)
    {
        const bool bMeetSurvivor = Beat->CompletesOn == FName(TEXT("Quest.Survivor.Met"));
        if (bMeetSurvivor && !UBreakerGameInstance::IsErasedEarthMap(World))
            return Finish(false, TEXT("Survivor meeting is not in the erased Earth"));
        if (!bMeetSurvivor && !UBreakerGameInstance::IsAnchorMap(World))
            return SelectTravel(Player, ABreakerTravelPoint::HubDestinationId) ? true : Finish(false, TEXT("Campaign cannot return to Anchor"));
        return SelectDialogueFlag(Player, Beat->CompletesOn) ? true : Finish(false, TEXT("Campaign dialogue completion flag is not reachable through visible choices"));
    }
    if (Beat->Kind == EBreakerMissionBeatKind::Travel)
        return SelectTravel(Player, Beat->Destination) ? true : Finish(false, TEXT("Campaign destination unavailable"));
    if (Beat->WorldEncounter == FName(TEXT("earth.survivor_extraction")))
    {
        if (!UBreakerGameInstance::IsErasedEarthMap(World)) return Finish(false, TEXT("Extraction beat lost its actual Earth"));
        ABreakerSurvivor* Survivor = nullptr;
        for (TActorIterator<ABreakerSurvivor> It(World); It; ++It) { Survivor = *It; break; }
        if (!Survivor || !Survivor->IsEscortActive()) return Finish(false, TEXT("Survivor escort did not start or failed"));
        if (SurvivorRouteObserved != Survivor->GetRouteIndex())
        {
            SurvivorRouteObserved = Survivor->GetRouteIndex();
            UE_LOG(LogTemp, Display, TEXT("[LoopProbe] physical Survivor route=%d at=%s lucidity=%.1f"),
                SurvivorRouteObserved, *Survivor->GetActorLocation().ToString(), Survivor->GetLucidityRemaining());
            if (FParse::Param(FCommandLine::Get(), TEXT("BreakerActTwoPhotos"))
                && (SurvivorRouteObserved == 2 || SurvivorRouteObserved == 9))
            {
                const FString Directory = FPaths::ProjectSavedDir() / TEXT("Screenshots");
                IFileManager::Get().MakeDirectory(*Directory, true);
                PendingPhoto = Directory / FString::Printf(TEXT("survivor-route-%d.png"), SurvivorRouteObserved);
                const FVector CameraAt = Survivor->GetActorLocation() + FVector(-280, 150, 100);
                const FRotator Facing = (Survivor->GetActorLocation() + FVector(350, 0, 50) - CameraAt).Rotation();
                Player->TeleportTo(CameraAt, Facing);
                if (AController* Controller = Player->GetController()) Controller->SetControlRotation(Facing);
                PhotoDelay = 2;
                return true;
            }
        }
        // Only the test player follows by relocation. The Survivor traverses
        // every production swept waypoint at the authored walking speed.
        const FVector At = Survivor->GetActorLocation() + FVector(0, 120, 0);
        Player->TeleportTo(At, Survivor->GetActorRotation());
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            if (It->IsDeadEnemy() || FVector::Dist2D(It->GetActorLocation(), At) > 2200) continue;
            FBreakerDamageRequest Hit; Hit.BaseDamage = 100000000; Hit.bCanCritical = false; Hit.bBypassShield = true; Hit.SetInstigator(Player);
            It->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Hit);
            if (It->IsDeadEnemy()) ++KilledEnemies;
        }
        return true;
    }
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
            if (bMarshal)
            {
                PhotoMarshal = Cast<ABreakerBossEnemy>(Enemy);
                MarshalCamera = World->SpawnActor<ACameraActor>();
                MarshalPhotoStage = 1;
                MarshalPhotoStarted = FPlatformTime::Seconds();
                MarshalPhotoTicker = FTSTicker::GetCoreTicker().AddTicker(
                    FTickerDelegate::CreateUObject(this, &UBreakerLoopProbe::TickMarshalPhotos), 0.05f);
                PhotoDelay = 0;
            }
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

bool UBreakerLoopProbe::TickMarshalPhotos(float DeltaSeconds)
{
    ABreakerBossEnemy* Boss = PhotoMarshal.Get();
    ACameraActor* Camera = MarshalCamera.Get();
    APlayerController* Controller = Boss && Boss->GetWorld() ? Boss->GetWorld()->GetFirstPlayerController() : nullptr;
    if (!Boss || !Camera || !Controller || Boss->IsDeadEnemy())
        return Finish(false, TEXT("Marshal exposure photograph lost its live actor/camera"));
    ABreakerCharacter* Player = Cast<ABreakerCharacter>(Controller->GetPawn());
    if (!Player || Player->GetCombat()->IsDead())
        return Finish(false, TEXT("Marshal photograph player died before exposure"));
    // This is a camera inspection, not a stationary damage sponge. Keep the
    // real player in detection range but outside melee while the order clock
    // runs; do not change health, boss cadence or exposure rules.
    if (FVector::DistSquared2D(Player->GetActorLocation(), Boss->GetActorLocation()) < FMath::Square(1800.0f))
    {
        FVector Away = (Player->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
        if (Away.IsNearlyZero()) Away = Boss->GetActorForwardVector();
        bool bRetreated = false;
        const ABreakerGameMode* Mode = Boss->GetWorld()->GetAuthGameMode<ABreakerGameMode>();
        for (const FVector Direction : { Away, -Away, Away.RotateAngleAxis(90, FVector::UpVector), Away.RotateAngleAxis(-90, FVector::UpVector) })
        {
            FVector Retreat = Boss->GetActorLocation() + Direction * 2500.0f;
            if (Mode && Mode->IsInSafeZone(Retreat)) continue;
            FHitResult Floor;
            FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(MarshalPhotoRetreat), false);
            GroundQuery.AddIgnoredActor(Player); GroundQuery.AddIgnoredActor(Boss);
            if (!Boss->GetWorld()->LineTraceSingleByChannel(Floor, Retreat + FVector(0, 0, 500),
                Retreat - FVector(0, 0, 1000), ECC_Visibility, GroundQuery) || Floor.ImpactNormal.Z < 0.6) continue;
            Retreat = Floor.ImpactPoint + FVector(0, 0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2);
            if (Player->TeleportTo(Retreat, (-Direction).Rotation())) { bRetreated = true; break; }
        }
        if (!bRetreated)
            return Finish(false, TEXT("Marshal photo retreat has no safe standing location"));
    }
    const double Now = FPlatformTime::Seconds();
    // Allow the authored full cadence plus raise; the first Deployment order
    // itself takes twenty seconds, so a twenty-second timeout cannot see it.
    const float PhotoTimeout = Boss->PhaseParams.DeployIntervalSeconds + Boss->PhaseParams.DeployRaiseSeconds + 5.0f;
    if (Now - MarshalPhotoStarted > PhotoTimeout)
        return Finish(false, TEXT("Marshal did not present a natural order window for photograph"));
    const FVector Target = Boss->GetActorLocation() + FVector(0, 0, 60);
    const bool bRear = MarshalPhotoStage == 1;
    const FVector At = Target + Boss->GetActorForwardVector() * (bRear ? -550.0f : 650.0f) + FVector(0, 0, 65);
    Camera->SetActorLocation(At);
    Camera->SetActorRotation((Target - At).Rotation());
    Controller->SetViewTarget(Camera);
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Screenshots");
    if (MarshalPhotoStage == 1 && Now - MarshalPhotoStarted > 0.25 && !Boss->IsApparatusExposed())
    {
        FScreenshotRequest::RequestScreenshot(Directory / TEXT("acttwo-marshal-rear.png"), true, false);
        MarshalPhotoStage = 2;
    }
    else if (MarshalPhotoStage == 2)
    {
        if (!Boss->IsGivingOrder()) MarshalOrderSeen = 0;
        else if (MarshalOrderSeen == 0) MarshalOrderSeen = Now;
        else if (Now - MarshalOrderSeen > 0.75f * UBreakerBossPhaseLibrary::GetOrderRaiseSeconds(Boss->GetPhase(), Boss->PhaseParams)
            && Boss->IsApparatusExposed())
        {
            FScreenshotRequest::RequestScreenshot(Directory / TEXT("acttwo-marshal.png"), true, false);
            MarshalPhotoStage = 3;
            MarshalOrderSeen = Now;
        }
    }
    else if (MarshalPhotoStage == 3 && Now - MarshalOrderSeen > 0.25)
    {
        Controller->SetViewTarget(Controller->GetPawn());
        Camera->Destroy();
        MarshalPhotoStage = 4;
        return false;
    }
    return true;
}

bool UBreakerLoopProbe::TickProbe(float DeltaSeconds)
{
    if (FPlatformTime::Seconds() - StartedAt > (bSurvivorLoop ? 400.0 : bActTwo ? 250.0 : 150.0))
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
