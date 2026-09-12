#include "Game/BreakerGameMode.h"
#include "Game/BreakerRepopulationMath.h"
#include "Game/BreakerPocketRift.h"
#include "TimerManager.h"
#include "Interaction/BreakerSupplyChest.h"
#include "Progression/BreakerProgressionComponent.h"
#include "UI/BreakerRiftDebriefMath.h"
#include "Interaction/BreakerSupplyChestMath.h"
#include "Game/BreakerPrototypeDestinations.h"
#include "Game/BreakerCoopCombatTest.h"
#include "GameFramework/PawnMovementComponent.h"

#include "Game/BreakerHubBuilder.h"
#include "Game/BreakerErasedEarthBuilder.h"
#include "Interaction/BreakerSurvivor.h"
#include "Interaction/BreakerFinaleActor.h"
#include "Interaction/BreakerFernhallCache.h"
#include "Game/BreakerFinaleEarthBuilder.h"
#include "Game/BreakerZoneBuilder.h"
#include "Game/BreakerFernhallCourtyardBuilder.h"
#include "Game/BreakerFernhallCourtyardEncounter.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerDeathBudgetMath.h"
#include "Game/BreakerWorldBasics.h"
#include "GameFramework/PlayerStart.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Interaction/BreakerRiftDoor.h"
#include "Save/BreakerMissionContent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "AbilitySystemComponent.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerTargetDummy.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerHoldfastEnemy.h"
#include "Data/BreakerStrings.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerAlteredEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Interaction/BreakerNPC.h"
#include "Playtest/BreakerKillTelemetryComponent.h"
#include "Playtest/BreakerFeedstockCapture.h"
#include "Playtest/BreakerTellCapture.h"
#include "Playtest/BreakerPlateCapture.h"
#include "Playtest/BreakerPlaytestComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Math/RandomStream.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Combat/BreakerZoneActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/PlatformMisc.h"
#include "RHI.h"
#include "RenderTimer.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerPlaytestHUD.h"
#include "UI/BreakerUIStyle.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "EngineUtils.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

ABreakerGameMode::ABreakerGameMode()
{
    DefaultPawnClass = ABreakerCharacter::StaticClass();
    static const TCHAR* PlayerBlueprintPath =
        TEXT("/Game/ProjectBreaker/Characters/BP_BreakerCharacter.BP_BreakerCharacter_C");
    if (UClass* PlayerBlueprint = StaticLoadClass(
        ABreakerCharacter::StaticClass(), nullptr, PlayerBlueprintPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
    {
        DefaultPawnClass = PlayerBlueprint;
    }
    HUDClass = ABreakerPlaytestHUD::StaticClass();
    // The supply-crate dwell check runs on the game mode tick.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ABreakerGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TickSupplyCrate(DeltaSeconds);
    TickWaveAdvance(DeltaSeconds);
    TickOutdoorRepopulation(DeltaSeconds);
    TickCrowdSampler(DeltaSeconds);
    if (bFernhallMissionReady && GetWorld() && GetWorld()->GetFirstPlayerController())
        BindFernhallMissionJournal(GetWorld()->GetFirstPlayerController()->GetPawn());
    ApplyAlteredContactWound();
    TickSurvivorMission();
    TickFinaleMission();
}

void ABreakerGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessedHandle);
    FTSTicker::GetCoreTicker().RemoveTicker(ScreenshotTickHandle);
    // A console command registered against a game-mode instance outlives the
    // world unless it is unregistered: the next PIE session's `Breaker.Boss`
    // would call through a dangling this.
    if (BossConsoleCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(BossConsoleCommand);
        BossConsoleCommand = nullptr;
    }
    if (EffectProbeConsoleCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(EffectProbeConsoleCommand);
        EffectProbeConsoleCommand = nullptr;
    }
    if (CloseRiftConsoleCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(CloseRiftConsoleCommand);
        CloseRiftConsoleCommand = nullptr;
    }
    if (MarkTerminatorConsoleCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(MarkTerminatorConsoleCommand);
        MarkTerminatorConsoleCommand = nullptr;
    }
    if (PopulationConsoleCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(PopulationConsoleCommand);
        PopulationConsoleCommand = nullptr;
    }
    Super::EndPlay(Reason);
}

void ABreakerGameMode::TeleportPawnToHub(APawn* Pawn)
{
    if (!Pawn) return;
    if (!bHubBuilt)
    {
        // Loud rather than a silent no-op: a PLAY button that appears to do
        // nothing is the failure this whole pass exists to remove. If the hub
        // was never built the player stays where they are and the log says why.
        UE_LOG(LogTemp, Warning,
            TEXT("TeleportPawnToHub: the hub has not been built, so there is nowhere to go. ")
            TEXT("The pawn was left where it was."));
        return;
    }
    // The gate-side arrival spot, never HubOrigin: the origin is the plaza
    // centre, and the centre holds the colliding landmark obelisk — the old
    // +120 teleport put the player INSIDE it.
    Pawn->TeleportTo(HubArrival.GetLocation(), HubArrival.Rotator());
    if (AController* Controller = Pawn->GetController())
    {
        Controller->SetControlRotation(HubArrival.Rotator());
    }
}

void ABreakerGameMode::HandleHubTravelSelected(FName DestinationId, APawn* RequestingPawn)
{
    if (!RequestingPawn) return;
    if (DestinationId == ABreakerTravelPoint::ErasedEarthDestinationId
        && !ABreakerTravelPoint::CanEnterErasedEarth(RequestingPawn)) return;
    if ((DestinationId == ABreakerTravelPoint::StrippedEarthDestinationId || DestinationId == ABreakerTravelPoint::WinningEarthDestinationId)
        && !ABreakerTravelPoint::CanEnterFinaleEarth(DestinationId, RequestingPawn)) return;
    // TRAVEL IS A LEVEL LOAD NOW, not a teleport. It was a teleport because
    // there was one map and both places were in it; with three maps the
    // destination does not exist until it is loaded.
    if (UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>())
    {
        Session->PendingDestinationId = DestinationId;
        // ORDINARY TRAVEL CLEARS THE PENDING RIFT, and this line is load
        // bearing in two places at once. PendingRift is transient travel
        // state, but nothing used to unset it: once a door had written one,
        // leaving the interior carried it onward, so travelling back to
        // Fernhall would raise a deployment briefing naming the rift you had
        // just walked out of, and the yard would build at that rift's area
        // level instead of its own. Walking out of a place is not entering it
        // again.
        Session->PendingRift = FBreakerRiftDefinition();
        // The counter goes with the rift it counted for (O82).
        Session->EndgameDeathsRemaining = UBreakerRiftLibrary::SoloEndgameDeathBudget;
        // THE ENTRY TRANSFORM IS NOT CLEARED HERE FOR A FERNHALL TRAVEL —
        // that travel is the way back from a rift, and the yard's build
        // consumes the transform on arrival (and clears it there). Any OTHER
        // destination drops it: a player who died inside and went to the
        // Anchor, then later walked to Fernhall by the ordinary gate, would
        // otherwise land at the door of a run that ended somewhere else. A
        // Fernhall travel that is NOT a return — the Anchor's gate — has
        // already had the flag dropped by whichever travel took the player
        // out of the yard, so it lands at the PlayerStart as before.
        if (DestinationId != ABreakerTravelPoint::FernhallDestinationId)
        {
            Session->bRiftEntryTransformSet = false;
        }
    }
    if (const auto* Prototype=BreakerPrototypeDestinations::Find(DestinationId))
    {
        if (BreakerPrototypeDestinations::HasMapPackage(*Prototype))
            UBreakerGameInstance::TravelTo(this,FName(*Prototype->MapName));
        return;
    }
    if (DestinationId == ABreakerTravelPoint::HubDestinationId)
    {
        UBreakerGameInstance::TravelTo(this, FName(UBreakerGameInstance::AnchorMapName()));
        return;
    }
    if (DestinationId == ABreakerTravelPoint::GymDestinationId)
    {
        UBreakerGameInstance::TravelTo(this, FName(UBreakerGameInstance::GymMapName()));
        return;
    }
    if (DestinationId == ABreakerTravelPoint::FernhallDestinationId)
    {
        UBreakerGameInstance::TravelTo(this, FName(UBreakerGameInstance::FernhallMapName()));
        return;
    }
    if (DestinationId == ABreakerTravelPoint::ErasedEarthDestinationId)
    {
        UBreakerGameInstance::TravelTo(this, FName(UBreakerGameInstance::ErasedEarthMapName()));
        return;
    }
    if (DestinationId == ABreakerTravelPoint::StrippedEarthDestinationId || DestinationId == ABreakerTravelPoint::WinningEarthDestinationId)
    {
        UBreakerGameInstance::TravelTo(this, FName(DestinationId == ABreakerTravelPoint::StrippedEarthDestinationId
            ? UBreakerGameInstance::StrippedEarthMapName() : UBreakerGameInstance::WinningEarthMapName()));
        return;
    }
    // Any other id is refused rather than guessed at. The old teleport that
    // stood here is gone with the one-map world it belonged to: the gym is a
    // separate level now, so arriving there is a load, and the load is what
    // builds it.
    UE_LOG(LogTemp, Warning, TEXT("HandleHubTravelSelected: no map is registered for destination '%s'."),
        *DestinationId.ToString());
}

void ABreakerGameMode::MarkRiftTerminator(ABreakerEnemy* Enemy)
{
    if (!Enemy) return;
    Enemy->SetRiftTerminator(true);
    // AddUObject, so the binding dies with this game mode. The mark and the
    // binding are cleared together by ReviveFromPool on FIELD's side, which is
    // what stops a pooled body reused in a later wave from completing a rift
    // the player has already left.
    Enemy->OnRiftTerminatorDefeated.AddUObject(this, &ABreakerGameMode::HandleRiftTerminatorDefeated);
    UE_LOG(LogTemp, Display, TEXT("[Rift] terminator marked: %s"), *Enemy->GetName());
}

void ABreakerGameMode::HandleRiftTerminatorDefeated(ABreakerEnemy* Terminator)
{
    const UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>();
    const FName AuthoredBoss = Session ? UBreakerMissionLibrary::BossForRift(Session->PendingRift) : NAME_None;
    const UBreakerCombatComponent* BossCombat = Terminator ? Terminator->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    const bool bConfirmed = Terminator && Terminator == ActiveBoss && BossCombat && BossCombat->IsDead()
        && !AuthoredBoss.IsNone() && Terminator->GetClass() == ABreakerBossEnemy::ClassForBossName(AuthoredBoss).Get();
    TGuardValue<bool> VerifiedDeath(bVerifiedStoryBossDeath, bConfirmed);
    // THE CONSUME SIDE OF O168. FIELD's raise says "the thing holding this open
    // died" and knows nothing about rifts; this is where that becomes a
    // completion. The player is read here rather than carried on the raise,
    // because a terminator's death has no second actor that means anything —
    // agreed with FIELD rather than assumed.
    APawn* Player = GetWorld() && GetWorld()->GetFirstPlayerController()
        ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    UE_LOG(LogTemp, Display, TEXT("[Rift] terminator defeated: %s"),
        Terminator ? *Terminator->GetName() : TEXT("<destroyed>"));
    CompleteRiftRun(Player);
}

void ABreakerGameMode::CompleteRiftRun(APawn* Player)
{
    UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>();
    const FBreakerRiftDefinition Rift = Session ? Session->PendingRift : FBreakerRiftDefinition();

    // THE RULE IS WORLD-FREE AND THIS IS THE THIN CALLER. Both refusals are
    // worth logging rather than swallowing: a completion that did not happen is
    // a payout that did not happen, and LEDGER binds directly on the strength
    // of this being single-fire.
    if (!UBreakerRiftLibrary::CanCompleteRiftRun(bRiftRunCompleted, Rift))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Rift] completion refused: %s"),
            bRiftRunCompleted
                ? TEXT("this run is already complete (one completion, one broadcast)")
                : TEXT("no rift is set; you cannot finish a run you are not in"));
        return;
    }

    // LATCH BEFORE BROADCAST, not after. A listener that re-entered this
    // function — directly, or by anything it triggers — would otherwise pass
    // the guard it was supposed to be stopped by, and LEDGER would be paid
    // twice for one run.
    bRiftRunCompleted = true;
    UE_LOG(LogTemp, Display, TEXT("[Rift] run complete: %s (area level %d, %s)"),
        *Rift.AreaName.ToString(), Rift.EffectiveAreaLevel(),
        Rift.Tier == EBreakerRiftTier::Campaign ? TEXT("campaign") : TEXT("endgame"));
    // THE BOSS SEAM. A Boss beat requires the actual matching boss death,
    // not merely a dev close or an unrelated marked terminator. Written into
    // the player's journal before the broadcast so every
    // listener reads the story where the run left it. The flag is also the
    // Sweep objective's completion flag; SetFlag is monotonic, so whichever
    // of the two sets it first, the other is a no-op.
    if (const ABreakerCharacter* Breaker = bVerifiedStoryBossDeath ? Cast<ABreakerCharacter>(Player) : nullptr)
    {
        if (UBreakerQuestJournal* Journal = Breaker->GetQuestJournal())
        {
            for (const FName& Flag : UBreakerMissionLibrary::RiftCompletionFlagsFor(Rift, Journal->GetState()))
            {
                Journal->SetFlag(Flag);
            }
        }
    }
    // THE EVENT, THEN THE SCREEN — in that order, and the order is the
    // feature. The event ROLLS the offer (O270): LEDGER's listener hands over
    // the purse and rolls three items the rift offers, and holds them on the
    // progression component. It puts NOTHING in the pack. The pack changes
    // only on the claim — the player picks one of the three on the closing
    // card, ClaimRiftCompletionOffer puts that one item into the backpack
    // through the same acquisition funnel a kill drop uses, and the run
    // ledger hears the claim there, exactly as it hears a pickup. The other
    // two are gone. A debrief composed before the broadcast would carry no
    // offer to choose from, so the screen composes after, from the ledger as
    // it stands and the offer as the event rolled it. The ledger is still
    // open while the card is up: the claim lands in it before ReturnFromRift
    // closes anything.
    //
    // The screen used to go up first out of a fear that a listener would
    // travel the player out from under it. No listener travels: every
    // consumer writes travel-surviving state and stays in the world (the
    // header's contract), and the travel out is the debrief's own verb now
    // (ReturnFromRift), taken when the player closes the screen — not
    // something the event does to him.
    OnRiftCompleted.Broadcast(Rift, Player);
    // Composed from the ledger this run kept rather than from a difference
    // between two backpacks, because a backpack difference cannot tell a
    // drop the player took from one they discarded to make room for it. The
    // offer is read from the pawn's progression, which is where the event
    // left it; a pawn with no progression has nothing to choose from.
    if (ABreakerCharacter* Breaker = Cast<ABreakerCharacter>(Player))
    {
        const UBreakerProgressionComponent* Progression = Breaker->GetProgression();
        Breaker->ShowRiftDebrief(BreakerRiftDebrief::Compose(Rift, RiftRunLoot,
            Progression ? Progression->GetRiftCompletionOffer() : TArray<FBreakerItemInstance>(),
            RiftRunRiftglassGained(Player), RiftRunExperienceGained(Player)));
    }
}

// ---------------------------------------------------------------------------
// THE RUN LEDGER. Opened when a player arrives inside a rift instance; the
// ordinary world keeps none, because there is nothing there to debrief.
// ---------------------------------------------------------------------------
void ABreakerGameMode::OpenRiftRunLedger(APawn* Player)
{
    ABreakerCharacter* Breaker = bRiftInstance ? Cast<ABreakerCharacter>(Player) : nullptr;
    if (!Breaker || bRiftRunLedgerOpen) return;
    UBreakerEquipmentComponent* Equipment = Breaker->GetEquipment();
    if (!Equipment) return;
    bRiftRunLedgerOpen = true;
    RiftRunLoot.Reset();
    RiftRunStartRiftglass = Equipment->GetForgeWallet().Get();
    RiftRunStartExperience = Breaker->GetProgression()
        ? Breaker->GetProgression()->GetProgressionState().TotalExperience : 0;
    // AddUnique, because arriving twice in one instance must not double every
    // item the player then picks up.
    Equipment->OnItemAcquired.AddUniqueDynamic(this, &ABreakerGameMode::HandleRiftRunItemAcquired);
}

void ABreakerGameMode::HandleRiftRunItemAcquired(const FBreakerItemInstance& Item)
{
    if (!bRiftRunLedgerOpen || !Item.IsValid()) return;
    RiftRunLoot.Add(Item);
}

int32 ABreakerGameMode::RiftRunRiftglassGained(APawn* Player) const
{
    const ABreakerCharacter* Breaker = Cast<ABreakerCharacter>(Player);
    const UBreakerEquipmentComponent* Equipment = Breaker ? Breaker->GetEquipment() : nullptr;
    return Equipment ? Equipment->GetForgeWallet().Get() - RiftRunStartRiftglass : 0;
}

int32 ABreakerGameMode::RiftRunExperienceGained(APawn* Player) const
{
    const ABreakerCharacter* Breaker = Cast<ABreakerCharacter>(Player);
    const UBreakerProgressionComponent* Progression = Breaker ? Breaker->GetProgression() : nullptr;
    return Progression ? Progression->GetProgressionState().TotalExperience - RiftRunStartExperience : 0;
}

void ABreakerGameMode::HandleRiftEntryRequested(const FBreakerRiftDefinition& Rift, APawn* RequestingPawn)
{
    FText EntryFailure;
    if (!ABreakerRiftDoor::CanEnterRift(Rift, RequestingPawn, EntryFailure))
    {
        UE_LOG(LogTemp, Display, TEXT("[Rift] entry refused: %s"), *EntryFailure.ToString());
        return;
    }
    UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>();
    if (!Session) return;

    // A door with no rift on it is a broken placement, not an empty rift, and
    // travelling anyway would land the player in an interior built at the dev
    // fallback area level with a briefing that has no name to print. Refuse
    // loudly, in the precedent of the zone builder's incomplete marker set.
    if (!Rift.IsSet())
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Rift] a rift door was entered with no definition on it (AreaLevel 0). Refusing travel."));
        return;
    }

    // WHERE HE STOOD, before anything else about the session changes. The
    // interior is this same yard rebuilt, and the way back out is another
    // rebuild; the pawn standing at this door does not survive either, so
    // the place it is standing is recorded on the session for the yard's
    // build to put the returning pawn back on. World space is valid across
    // the reload because the yard is assembled at identity in both builds.
    //
    // Only when the door stands IN Fernhall. Every door today does, but a
    // door in another map would record that map's coordinates and the yard
    // would honour them as its own — a player returning from a rift entered
    // elsewhere is better placed at the yard's PlayerStart than at a point
    // that belongs to a different world. The flag stays false and the yard
    // takes its default landing.
    if (UBreakerGameInstance::IsFernhallMap(this))
    {
        Session->RiftEntryTransform = RequestingPawn->GetActorTransform();
        Session->bRiftEntryTransformSet = true;
    }
    else
    {
        Session->bRiftEntryTransformSet = false;
    }

    // THE WRITE IS THE WHOLE FEATURE. Every consumer downstream already
    // exists and has been waiting for something to set this: the deployment
    // beat gates on PendingRift.IsSet(), the briefing reads the name, line,
    // tier and derived multipliers off it, and the destination's build takes
    // its area level from EffectiveAreaLevel(). None of that changes here.
    Session->PendingRift = Rift;
    Session->PendingDestinationId = ABreakerTravelPoint::RiftDestinationId;
    // THE BUDGET IS SEEDED AT THE DOOR (O82): a fresh entry is a fresh
    // allowance. RetryRift does not pass through here, which is what makes
    // the counter survive a retry.
    Session->EndgameDeathsRemaining = UBreakerRiftLibrary::SoloEndgameDeathBudget;

    // THE INTERIOR IS THE YARD'S OWN GEOMETRY (Part One-Q, ruled). This is the
    // line the placeholder comment said would move, and it moved: the gym was
    // the interior only while nothing better existed, and it put the player in
    // a room with target dummies at the most fiction-breaking moment available.
    //
    // Same map, PendingRift set — which IS a different instance of the same
    // tileset, and is what an instanced rift means. No new map and no new art:
    // the yard is already built, already validated by the grammar, and the wave
    // system spawns around the PLAYER rather than at an authored arena, so it
    // has no dependency on gym geometry at all.
    UBreakerGameInstance::TravelTo(this, FName(UBreakerGameInstance::FernhallMapName()));
}

int32 ABreakerGameMode::SpendDeath()
{
    UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>();
    if (!Session) return UBreakerRiftLibrary::SoloEndgameDeathBudget;
    // THE RULE IS WORLD-FREE AND THIS IS THE THIN CALLER: the tier is the
    // rift's, the boss is this world's, the counter is the session's.
    const EBreakerRiftTier Tier = Session->PendingRift.Tier;
    const int32 Before = Session->EndgameDeathsRemaining;
    Session->EndgameDeathsRemaining = BreakerDeathBudget::SpendDeath(Tier, Before, IsBossAlive());
    UE_LOG(LogTemp, Display, TEXT("[Rift] death %s: %d -> %d of %d (%s%s)"),
        BreakerDeathBudget::IsBudgeted(Tier) ? TEXT("spent") : TEXT("free"),
        Before, Session->EndgameDeathsRemaining, UBreakerRiftLibrary::SoloEndgameDeathBudget,
        Tier == EBreakerRiftTier::Campaign ? TEXT("campaign") : TEXT("endgame"),
        IsBossAlive() ? TEXT(", boss alive") : TEXT(""));
    return Session->EndgameDeathsRemaining;
}

void ABreakerGameMode::RetryRift(APawn* RequestingPawn)
{
    if (!RequestingPawn) return;
    UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>();
    if (!Session) return;
    // HandleRiftEntryRequested minus the seed. PendingRift is not touched:
    // the retry is the same rift at the same tier, and the counter it
    // carries is the point. The refusal is the door's: a retry with no rift
    // set is a death somewhere that is not an instance, and this must not
    // travel on it.
    if (!Session->PendingRift.IsSet())
    {
        UE_LOG(LogTemp, Error, TEXT("[Rift] retry requested with no rift set. Refusing travel."));
        return;
    }
    if (!BreakerDeathBudget::CanRetryRift(Session->PendingRift.Tier, Session->EndgameDeathsRemaining))
    {
        UE_LOG(LogTemp, Display, TEXT("[Rift] retry refused: endgame death allowance exhausted."));
        return;
    }
    Session->PendingDestinationId = ABreakerTravelPoint::RiftDestinationId;
    UE_LOG(LogTemp, Display, TEXT("[Rift] retry: %s (area level %d, %d of %d deaths remain)"),
        *Session->PendingRift.AreaName.ToString(), Session->PendingRift.EffectiveAreaLevel(),
        Session->EndgameDeathsRemaining, UBreakerRiftLibrary::SoloEndgameDeathBudget);
    UBreakerGameInstance::TravelTo(this, FName(UBreakerGameInstance::FernhallMapName()));
}

void ABreakerGameMode::ReturnToAnchor(APawn* RequestingPawn)
{
    HandleHubTravelSelected(ABreakerTravelPoint::HubDestinationId, RequestingPawn);
}

void ABreakerGameMode::ReturnFromRift(APawn* RequestingPawn)
{
    // The same travel the entry plaza's gate makes when Fernhall is chosen
    // from inside a run, and deliberately nothing more: PendingRift is
    // cleared by the travel so the load builds the yard and not another
    // run, and the entry transform the door recorded is left standing so
    // that build can put the player back where he went in. One travel path
    // in the project; this only names it for the completion beat.
    HandleHubTravelSelected(ABreakerTravelPoint::FernhallDestinationId, RequestingPawn);
}

AActor* ABreakerGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    // Select before pawn BeginPlay records its campaign respawn transform.
    // Existing authored shells may retain their former start at the origin.
    if (BreakerPrototypeDestinations::ForWorld(this))
    {
        const FName StartTag(TEXT("PrototypeDestination.Arrival"));
        for (TActorIterator<APlayerStart> It(GetWorld());It;++It)
            if (It->ActorHasTag(StartTag)) return *It;
        auto* Arrival=GetWorld()->SpawnActor<APlayerStart>(BreakerPrototypeDestinations::ArrivalLocation(),FRotator::ZeroRotator);
        if (Arrival) { Arrival->Tags.Add(StartTag); return Arrival; }
        return nullptr;
    }
    // Late guests must use the same authored arrival as the initial host,
    // without re-running HandleStartingNewPlayer's encounter construction.
    if (BreakerCoopCombatTest::IsEnabled(GetWorld()) && UBreakerGameInstance::IsFernhallMap(this))
    {
        const FName StartTag(TEXT("BreakerCoopArrival"));
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
            if (It->Tags.Contains(StartTag)) return *It;
        TArray<FBreakerZonePiece> Pieces;
        FBreakerZoneMarkers Markers;
        if (UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(),Pieces)
            && UBreakerZoneBuilder::ExtractMarkers(Pieces,Markers))
            if (const auto* Start=Markers.Find(EBreakerZoneMarkerRole::PlayerStart))
            {
                // Same capsule placement as the existing Fernhall arrival.
                APlayerStart* Arrival=GetWorld()->SpawnActor<APlayerStart>(Start->Location+FVector(0,0,112),FRotator::ZeroRotator);
                if (Arrival) { Arrival->Tags.Add(StartTag); return Arrival; }
            }
    }
    if (AActor* Authored = Super::ChoosePlayerStart_Implementation(Player))
    {
        return Authored;
    }
    // Z 112: the pawn capsule's half-height is 88, so feet land at 24 — the
    // same "capsule assumption" plane ResolveGroundZ falls back to in a map
    // with no floor, which is exactly the map this branch exists for. The
    // apron / plaza / boot floor all get built at that plane in the same
    // frame, so the pawn stands on ground it arrived with.
    UWorld* World = GetWorld();
    if (!World) return nullptr;
    APlayerStart* Fallback = World->SpawnActor<APlayerStart>(
        APlayerStart::StaticClass(), FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 112.0f)));
    if (Fallback)
    {
        UE_LOG(LogTemp, Log, TEXT("[BreakerMap] no authored PlayerStart; runtime fallback spawned at origin."));
    }
    return Fallback;
}

void ABreakerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
    if (bPlaytestTargetsSpawned || !NewPlayer || !NewPlayer->GetPawn() || !GetWorld()) return;

    // Light to see by, for every map role. Runs before the front-end early
    // return on purpose: the title screen floats over a real (if minimal)
    // world now, not over a black void. A map with an authored directional
    // light (Lvl_FirstPerson) suppresses this entirely.
    UBreakerWorldBasics::EnsureWorldLighting(GetWorld());

    // The rift's run ledger used to open HERE, "the moment its player
    // exists" — and it read bRiftInstance, which the Fernhall branch sets 240
    // lines below. So the ledger opened against false and returned without
    // binding, and every rift ever closed on this build said NOTHING CAME
    // BACK. It now opens beside the flag that decides it.

    // AN UNATTENDED RUN THAT CANNOT EXIT IS A HANG, NOT A FAILURE, and this is
    // the third time that shape has cost someone a cycle: the crowd probe's
    // refused flag left a scripted run sitting forever, FIELD's verify command
    // gave up silently and logged the giving-up as a result, and a headless run
    // with no exit condition simply never returns. A script waiting on a
    // process that will never end has nothing to read and nothing to blame.
    //
    // THREE THINGS END A HEADLESS RUN and if none is armed nobody is told:
    // -BreakerScreenshots exits after its last frame, -BreakerCrowdProbe exits
    // after its summary, and an ExecCmds carrying Quit or SoftQuit exits on its
    // own. Warning rather than forcing an exit, deliberately: a legitimate long
    // unattended run exists (a soak, a profile) and killing it would be a worse
    // failure than the one being prevented. The point is that the operator
    // learns it now instead of after a timeout.
    if (FApp::IsUnattended())
    {
        int32 Shots = 0;
        int32 Crowd = 0;
        const FString CommandLine = FCommandLine::Get();
        const bool bExits =
            (FParse::Value(*CommandLine, TEXT("BreakerScreenshots="), Shots) && Shots > 0)
            || (FParse::Value(*CommandLine, TEXT("BreakerCrowdProbe="), Crowd) && Crowd > 0)
            || CommandLine.Contains(TEXT("Quit"), ESearchCase::IgnoreCase);
        if (!bExits)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[BreakerCapture] -unattended with no exit condition: this run will NOT end on its own. ")
                TEXT("Add -BreakerScreenshots=N, -BreakerCrowdProbe=N, or SoftQuit in -ExecCmds."));
        }
    }

    // WHAT THIS MAP IS FOR. Until tonight there was one map, so this function
    // unconditionally built the entire gym — which is exactly why the owner
    // reported "loading in still takes you to the game": the front end was a
    // widget drawn over a gym that had already been constructed and was
    // already ticking underneath it.
    //
    // The front end builds NOTHING. That is the whole point of the split, and
    // it is why this returns before BuildFieldFrame rather than after: the
    // frame is derived from the pawn and every spawner hangs off it, so an
    // early return here is the one place that guarantees no field exists.
    if (UBreakerGameInstance::IsFrontEndMap(this))
    {
        // "Nothing" still needs a floor: the map is an empty shell and the
        // pawn under the title menu was falling through it. Top surface at
        // the pawn's feet, same plane the fallback PlayerStart assumed.
        if (const APawn* Pawn = NewPlayer->GetPawn())
        {
            UBreakerWorldBasics::EnsureBootFloor(GetWorld(),
                Pawn->GetActorLocation() - FVector(0.0f, 0.0f, 88.0f));
        }
        bPlaytestTargetsSpawned = true;
        // The capture harness works on the front end too. Without this a
        // -BreakerScreenshots run of the shipped boot map never schedules its
        // exit and hangs forever — which is also why no automated run ever
        // photographed the title screen the game actually boots into.
        ScheduleScreenshots();
        UE_LOG(LogTemp, Log, TEXT("[BreakerMap] front end — no field built."));
        return;
    }

    // THE RIFT DEFINITION OWNS THE AREA LEVEL when a travel carried one:
    // adopted once per map build, before anything derives from it. The
    // EditAnywhere GymAreaLevel remains the dev fallback for sessions with
    // no chosen rift — PIE drop-ins, the capture harness — which is the wall
    // between "a tunable on the game mode" and "a property of the place".
    if (const UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>())
    {
        if (Session->PendingRift.IsSet())
        {
            GymAreaLevel = Session->PendingRift.EffectiveAreaLevel();
        }
    }

    BuildFieldFrame(NewPlayer->GetPawn());
    // Before any map branch returns, so the dev instruments reach every map
    // that has a frame rather than only the gym (see ArmDevInstruments).
    ArmDevInstruments(NewPlayer);

    // The Anchor builds the hub and stops. No gym field, no encounter, no
    // waves — a social space with a gate, which is what a hub is.
    if (UBreakerGameInstance::IsAnchorMap(this))
    {
        HubOrigin = Frame.Ground;
        bHubBuilt = true;
        const FTransform HubFrame(Frame.Forward.Rotation(), HubOrigin);
        if (ABreakerTravelPoint* HubTravel = UBreakerHubBuilder::BuildHub(GetWorld(), HubFrame))
        {
            HubTravel->ExcludedDestinationId = ABreakerTravelPoint::HubDestinationId;
            HubTravel->OnDestinationSelected.AddUObject(this, &ABreakerGameMode::HandleHubTravelSelected);
        }
        // THE HUB IS BUILT AROUND THE ARRIVING PAWN, which means the pawn is
        // standing at plaza centre — inside the landmark obelisk BuildHub just
        // spawned there. Move them to the gate-side arrival spot immediately,
        // in the same frame, before physics gets an opinion.
        HubArrival = UBreakerHubBuilder::ArrivalTransform(HubFrame);
        TeleportPawnToHub(NewPlayer->GetPawn());
        bPlaytestTargetsSpawned = true;
        if (FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureTour")))
        {
            // Arrival remains the first capture; subsequent frames move the
            // real pawn through these ground-level hub views.
            struct FHubVantage { FVector Position; FVector LookAt; };
            const FHubVantage Vantages[] =
            {
                // O2 PLACEHOLDER: ground-level framing of street, work areas,
                // pylon and outer settlement; no gameplay spawn changes.
                { FVector(-1460, -140, 175), FVector(1800, 0, 950) },
                { FVector(700, -560, 170), FVector(2500, -930, 560) },
                { FVector(760, 480, 170), FVector(2600, 900, 500) },
                { FVector(0, 250, 170), FVector(-3970, 500, 600) },
                { FVector(-500, -450, 100), FVector(1800, 0, 160) },
            };
            for (const FHubVantage& Vantage : Vantages)
            {
                const FVector Position = HubFrame.TransformPosition(Vantage.Position);
                const FRotator Facing = (HubFrame.TransformPosition(Vantage.LookAt) - Position).Rotation();
                if (ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Position, Facing))
                {
                    if (UCameraComponent* Component = Camera->GetCameraComponent()) Component->SetFieldOfView(90.0f);
                    Camera->SetActorLabel(TEXT("Runtime_AnchorTourCamera"));
                    TourCameras.Add(Camera);
                }
            }
        }
        // Same reason as the front end: a screenshot run of the Anchor must
        // schedule its exit, or the harness can never photograph the hub.
        ScheduleScreenshots();
        UE_LOG(LogTemp, Log, TEXT("[BreakerMap] anchor — hub built, no gym."));
        return;
    }

    if (const auto* Prototype=BreakerPrototypeDestinations::ForWorld(this))
    {
        const auto Layout=BreakerPrototypeDestinations::Build(GetWorld(),Prototype->Id);
        for (auto* Gate : Layout.Gates)
            Gate->OnDestinationSelected.AddUObject(this,&ABreakerGameMode::HandleHubTravelSelected);
        if (APawn* Pawn=NewPlayer->GetPawn())
        {
            Pawn->TeleportTo(Layout.Arrival,Layout.Facing);
            if (AController* Controller=Pawn->GetController()) Controller->SetControlRotation(Layout.Facing);
            BuildFieldFrame(Pawn);
        }
        GymAreaLevel=Prototype->AreaLevels[0];
        bPlaytestTargetsSpawned=true;
        if (FParse::Param(FCommandLine::Get(),TEXT("BreakerCaptureTour")))
            for (int32 District=0;District<Prototype->Districts.Num();++District)
            {
                // O2 PLACEHOLDER: setting review views explicitly frame the
                // scorched farm/crater and research landmarks, not freight.
                const FVector Centre=Prototype->Districts[District];
                const bool bBasin=Prototype->Id==TEXT("RedBasin");
                const FVector BasinViews[]={FVector(-1800,-1300,850),FVector(-500,-800,650),FVector(-500,-800,950)};
                const FVector BasinLooks[]={FVector(300,0,100),FVector(1800,-1750,350),FVector(1800,-1650,100)};
                const FVector ResearchViews[]={FVector(-500,0,500),FVector(500,-500,850),FVector(-500,-500,900)};
                const FVector ResearchLooks[]={FVector(-1500,1750,100),FVector(-2400,1600,450),FVector(1900,-1500,950)};
                const bool bAirport=Prototype->Id==TEXT("PortMeridian");
                // O2 PLACEHOLDER: capture the terminal facade, aircraft wreck
                // and hangar frame; these views do not alter live gameplay.
                const FVector AirportViews[]={FVector(-1800,-800,700),FVector(-300,-600,650),FVector(-2100,-800,650)};
                const FVector AirportLooks[]={FVector(0,1700,420),FVector(1700,-1950,250),FVector(1000,0,900)};
                const bool bCoast=Prototype->Id==TEXT("BrokenCoast");
                // O2 PLACEHOLDER: seaward views frame the boat, pier and beacon.
                const FVector CoastViews[]={FVector(-500,-300,650),FVector(-400,-300,800),FVector(-500,-300,800)};
                const FVector CoastLooks[]={FVector(1800,-1900,250),FVector(1900,-3000,-50),FVector(1900,-1900,1050)};
                const bool bCity=Prototype->Id==TEXT("Shatterpoint");
                // O2 PLACEHOLDER: actual street, civic and underpass silhouettes.
                const FVector CityViews[]={FVector(-1800,-600,550),FVector(-300,-300,700),FVector(-2300,-500,650)};
                const FVector CityLooks[]={FVector(1400,700,1200),FVector(1900,-1900,700),FVector(600,0,650)};
                const FVector At=Centre+(bCity?CityViews[District]:bCoast?CoastViews[District]:bAirport?AirportViews[District]:bBasin?BasinViews[District]:ResearchViews[District]);
                const FRotator Facing=(Centre+(bCity?CityLooks[District]:bCoast?CoastLooks[District]:bAirport?AirportLooks[District]:bBasin?BasinLooks[District]:ResearchLooks[District])-At).Rotation();
                if (auto* Camera=GetWorld()->SpawnActor<ACameraActor>(At,Facing))
                {
                    Camera->GetCameraComponent()->SetFieldOfView(90.f);
#if WITH_EDITOR
                    Camera->SetActorLabel(Prototype->DisplayName+TEXT(" - ")+Prototype->DistrictNames[District]);
#endif
                    UE_LOG(LogTemp,Display,TEXT("[DestinationCapture] %s / %s"),*Prototype->DisplayName,*Prototype->DistrictNames[District]);
                    TourCameras.Add(Camera);
                }
            }
        ScheduleScreenshots();
        return;
    }

    if (UBreakerGameInstance::IsErasedEarthMap(this))
    {
        BuildSurvivorMission(NewPlayer->GetPawn());
        bPlaytestTargetsSpawned = true;
        ScheduleScreenshots();
        return;
    }

    if (UBreakerGameInstance::IsStrippedEarthMap(this) || UBreakerGameInstance::IsWinningEarthMap(this))
    {
        BuildFinaleEarth(NewPlayer->GetPawn(), UBreakerGameInstance::IsWinningEarthMap(this));
        bPlaytestTargetsSpawned = true;
        ScheduleScreenshots();
        return;
    }

    // FERNHALL: the authored zone. Assembled from imported meshes, not
    // generated — UBreakerZoneBuilder spawns the composer's scene at identity
    // and hands back the marker transforms. No targets, no boss key, no waves:
    // the yard is a place, and what fights happen in it will be authored into
    // it (the rift is next). Registered by name in IsGymMapName's exclusion
    // list, which is the only thing standing between this branch and the gym
    // fall-through filling the yard with dummies.
    if (UBreakerGameInstance::IsFernhallMap(this))
    {
        // WHICH BUILD THIS IS, DECIDED BEFORE ANYTHING IS BUILT. ONE MAP, TWO
        // BUILDS: with a PendingRift set this is a RIFT RUN in the yard's
        // geometry; without one it is the yard itself. Reading the session
        // rather than a second map is what makes this an INSTANCE.
        //
        // This used to be resolved sixty lines BELOW the build, which was
        // harmless while both builds read the same meshes and stopped being
        // harmless the moment a rift could read its own: the yard was assembled
        // against a flag that was still false, so a rift always built the living
        // yard and the ruined twin could never appear. Photographing it is what
        // found that — the frames were identical and the spawn count said 107.
        UBreakerGameInstance* RiftSession = GetGameInstance<UBreakerGameInstance>();
        // -BreakerRiftInstance: the harness's way INTO a run. The interior is a
        // mode of this map that only TRAVEL can reach — the door writes
        // PendingRift and travels — and the capture harness cannot press F, so
        // without this the one build that matters is the one build nobody can
        // photograph. A command-line switch, so a shipped build cannot reach it,
        // and it never overwrites a rift a door already authored.
        if (RiftSession && !RiftSession->PendingRift.IsSet()
            && FParse::Param(FCommandLine::Get(), TEXT("BreakerRiftInstance")))
        {
            RiftSession->PendingRift = UBreakerZoneBuilder::FernhallRiftFor(NAME_None);
            UE_LOG(LogTemp, Display, TEXT("[Rift] -BreakerRiftInstance seeded a run; this is a capture, not a door."));
        }
        bRiftInstance = RiftSession && RiftSession->PendingRift.IsSet();
        // THE RUN LEDGER OPENS THE LINE AFTER THE FLAG IT READS. It was
        // called at the top of this function, before bRiftInstance was
        // assigned, so it early-returned in every run: no item bound, no
        // starting purse latched, and the debrief printed the whole wallet
        // and the whole XP total as the run's gain — "nothing came back with
        // you" was the ledger, not the payout.
        OpenRiftRunLedger(NewPlayer->GetPawn());

        FBreakerZoneMarkers Markers;
        if (!UBreakerZoneBuilder::BuildFernhallYard(GetWorld(), Markers, bRiftInstance)) return;
        {
            // The entry yard's frame. A zone has exactly one player start
            // (FBreakerZoneMarkers::IsComplete), and the arrival yard's rift
            // is what the yard points at.
            const FBreakerZoneMarker* StartMarker = Markers.Find(EBreakerZoneMarkerRole::PlayerStart);
            const FBreakerZoneMarker* EntryRift = Markers.Find(EBreakerZoneMarkerRole::Rift);
            const FVector StartAt = StartMarker ? StartMarker->Location : FVector::ZeroVector;
            const FVector YardForward = EntryRift
                ? (EntryRift->Location - StartAt).GetSafeNormal2D()
                : FVector::ForwardVector;

            if (APawn* Pawn = NewPlayer->GetPawn())
            {
                // Feet on the marker's ground plane (capsule half-height 88,
                // same arithmetic as the fallback PlayerStart), facing the
                // rift end of the yard so the first thing seen is somewhere
                // to go.
                FVector StandAt = StartAt + FVector(0.0f, 0.0f, 112.0f);
                FRotator Facing = YardForward.Rotation();
                // UNLESS THIS IS THE WAY BACK. A yard build with the door's
                // recorded entry transform on the session is a player
                // returning from a run, and he comes back to where he stood
                // when the door took him — same feet, same facing — rather
                // than to the start of the yard with the whole lane to walk
                // again. Never inside a run: the instance is the fight, and
                // the transform waits for the build after it. Read once and
                // cleared, so the next ordinary arrival is ordinary.
                if (!bRiftInstance && RiftSession && RiftSession->bRiftEntryTransformSet)
                {
                    StandAt = RiftSession->RiftEntryTransform.GetLocation();
                    Facing = RiftSession->RiftEntryTransform.Rotator();
                    RiftSession->bRiftEntryTransformSet = false;
                    UE_LOG(LogTemp, Display, TEXT("[Rift] returned to the door at %s"), *StandAt.ToString());
                }
                Pawn->TeleportTo(StandAt, Facing);
                if (AController* Controller = Pawn->GetController())
                {
                    Controller->SetControlRotation(Facing);
                }

                // THE FRAME IS REBUILT HERE, AFTER THE TELEPORT, and that is
                // not tidiness. BuildFieldFrame ran before any map branch, so
                // it anchored on wherever the pawn happened to spawn \u2014 the
                // runtime fallback at the world origin \u2014 while the yard's
                // grammar is expressed from the PLAYER START MARKER looking at
                // the rift. Two frames for one yard, and spawn containment
                // solved in the wrong one: it reported a 100 x 50 m yard
                // affording 11285 cm, which is longer than the yard.
                BuildFieldFrame(Pawn);
            }

            // AND THE YARD'S OWN BAND, not the gym's. MakeCoverFieldParams
            // builds the gym field from this actor's properties; measuring
            // Fernhall against it is measuring one place with another's
            // dimensions. This is the same params object the grammar test
            // validates the yard with, so what contains the spawner and what
            // proves the layout legal are one source.
            ActiveFieldParams = UBreakerZoneBuilder::FernhallFieldParams();
            bActiveFieldParamsSet = true;
            // ONE MAP, TWO BUILDS. With a PendingRift set this is a RIFT RUN
            // in the yard's geometry; without one it is the yard itself. The
            // difference is what stands in it: a run has a fight and a way out,
            // the yard has a door and no fight. Which build this is was already
            // decided above, before the yard was assembled, because the yard
            // itself now depends on the answer.

            // The way back. Both builds need one — the yard must not be a trap,
            // and a rift the player cannot leave is worse. Placed off the lane
            // on the entry plaza, in the yard's own marker-derived frame rather
            // than world axes, so a re-exported yard carries its gate with it.
            //
            // FROM INSIDE A RUN THE GATE IS THE WAY OUT, and it needs no
            // special case: travelling anywhere clears PendingRift
            // (HandleHubTravelSelected), so choosing Fernhall from inside a
            // rift lands the player back in the yard rather than in another
            // run. Leaving by the door you came in is exactly O168's
            // no-completion path — no event, no payout.
            const FVector YardRight = FVector::CrossProduct(FVector::UpVector, YardForward);
            const FVector GateAt = StartAt - YardForward * 300.0f + YardRight * 900.0f
                + FVector(0.0f, 0.0f, 100.0f);
            if (ABreakerTravelPoint* Gate = GetWorld()->SpawnActor<ABreakerTravelPoint>(
                ABreakerTravelPoint::StaticClass(),
                FTransform(YardForward.Rotation(), GateAt)))
            {
                Gate->ExcludedDestinationId = bRiftInstance
                    ? ABreakerTravelPoint::RiftDestinationId : ABreakerTravelPoint::FernhallDestinationId;
                Gate->OnDestinationSelected.AddUObject(this, &ABreakerGameMode::HandleHubTravelSelected);
            }

            // A DOOR PER RIFT MARKER, not one door per zone. Today the yard
            // authors one and this loop runs once, so nothing observable
            // changes — but the marker list is what lets five yards each
            // carry their own door without this code moving again, which is
            // the whole reason the marker model was reshaped.
            //
            // NO DOORS INSIDE A RUN. A rift door standing in the rift it opens
            // would offer to enter the place the player is already in, and
            // entering it would write a fresh PendingRift over the live one.
            for (const FBreakerZoneMarker& RiftMarker : bRiftInstance
                    ? TArray<FBreakerZoneMarker>()
                    : Markers.OfRole(EBreakerZoneMarkerRole::Rift))
            {
                // Faces back down the lane toward the player start, so a
                // player walking the length of the yard arrives looking at
                // the door's front rather than its side. Derived per marker
                // rather than from the entry yard's forward, because a door
                // in another yard does not share this one's axis.
                const FVector Approach = (StartAt - RiftMarker.Location).GetSafeNormal2D();
                const FVector DoorAt = RiftMarker.Location + FVector(0.0f, 0.0f, 100.0f);
                ABreakerRiftDoor* Door = GetWorld()->SpawnActor<ABreakerRiftDoor>(
                    ABreakerRiftDoor::StaticClass(),
                    FTransform(Approach.IsNearlyZero() ? FRotator::ZeroRotator : Approach.Rotation(), DoorAt));
                if (!Door) continue;

                // WHICH RIFT THIS DOOR OPENS, authored beside the yard it
                // belongs to rather than as a literal here. Two yards means
                // two doors, and a literal would have made both of them the
                // same place at the same difficulty.
                Door->Rift = UBreakerZoneBuilder::FernhallRiftFor(RiftMarker.Yard);
                Door->OnRiftEntryRequested.AddUObject(this, &ABreakerGameMode::HandleRiftEntryRequested);
            }

            // THE WATCHKEEPER, ON A MARKER THE COMPOSER SHIPPED AND NOTHING
            // READ. marker_npc_contract has been authored, parsed, validated
            // by the piece contract and consumed by no production code — the
            // yard set aside a place to stop and left it empty, while every
            // quest giver in the game stood in the hub. So Fernhall was
            // somewhere you crossed and never somewhere you were sent from.
            //
            // Not in a rift: an instance is the yard emptied of its people,
            // and a contract giver standing in one would offer work inside
            // the place the work is. He faces the player start for the same
            // reason the doors do — arrive looking at a face, not a back.
            if (!bRiftInstance)
            {
                if (const FBreakerZoneMarker* Contract = Markers.Find(EBreakerZoneMarkerRole::NPCContract))
                {
                    const FVector Facing = (StartAt - Contract->Location).GetSafeNormal2D();
                    SpawnFinaleResident(TEXT("Watchkeeper"), Contract->Location,
                        Facing.IsNearlyZero() ? FRotator::ZeroRotator : Facing.Rotation());
                }
            }
        }
        if (!bRiftInstance)
        {
            SpawnFernhallEncounters(Markers);
            FVector2D Origin, Forward;
            if (UBreakerZoneBuilder::YardFrame(Markers, TEXT("substation"), Origin, Forward))
            {
                const FBreakerCoverFieldParams Field = UBreakerZoneBuilder::FernhallFieldParams(TEXT("substation"));
                const FVector Direction(Forward.X, Forward.Y, 0);
                AlteredContactPosition = FVector(Origin.X, Origin.Y, 0)
                    + Direction * FMath::Lerp(Field.BandNearCm, Field.BandFarCm, 0.85f);
                // OFF THE CENTRELINE, BY MORE THAN ONE INTERACTION SPHERE.
                // The substation yard's forward IS the ray from its anchor to
                // its own rift marker, so a Breach door placed on that ray at
                // the contact's depth stood inside the substation door's
                // interaction range: two rifts stacked on top of each other,
                // and nearest-in-range picked the substation when he pressed
                // F on the contract he had just earned. Pushed sideways by
                // more than twice InteractionRange (450) so the two spheres
                // can never overlap. Same idiom as the gate placement above.
                constexpr float BreachDoorLateralCm = 1000.0f; // O2 PLACEHOLDER
                const FVector Right = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
                BreachDoorPosition = AlteredContactPosition + Direction * 450.0f + Right * BreachDoorLateralCm;
                bFernhallMissionReady = true;
                BindFernhallMissionJournal(NewPlayer->GetPawn());
            }
        }
        else
        {
            // THE RUN STARTS ITSELF, AND THEN CARRIES ITSELF. A rift the
            // player has to press a key to populate is a test bench, and the
            // gym already is one; walking through a tear and finding an empty
            // yard is the opposite of a felt loop. Wave 1 arrives here; every
            // later wave arrives on the clear (TickWaveAdvance), as it does in
            // the gym, so F4 only ever shortens a breather.
            //
            // THE BOSS ARRIVES ON RiftBossWave, NOT THE GYM'S TWELVE. Twelve
            // waves is a wave-mode endurance figure and the ruling's success
            // test is one sitting. This is the shortest lever that makes a run
            // a run, and it is O2 PLACEHOLDER: the owner moves it after
            // walking one, which is the only test this has.
            // THE RIFT GETS ITS OWN PARAMS, NOT A LEVER (ruled). Two fields
            // poked into the gym's profile was the lever; this is the whole
            // struct, because nine of its constants are the gym's twelve-wave
            // pacing and a run is three waves. The game mode is per-world, so
            // assigning here IS the rift's own instance — the gym's world has
            // its own game mode carrying the gym's defaults, untouched.
            WaveBudget = UBreakerWaveBudgetLibrary::MakeRiftWaveBudget(RiftBossWave);
            const UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>();
            if (Session && Session->PendingRift.EncounterId == FName(TEXT("breach.marshalling")))
                WaveBudget = UBreakerWaveBudgetLibrary::MakeBreachWaveBudget();
            UE_LOG(LogTemp, Display,
                TEXT("[Rift] instance built: %s, area level %d, boss on wave %d."),
                Session ? *Session->PendingRift.AreaName.ToString() : TEXT("<unnamed>"),
                Session ? Session->PendingRift.EffectiveAreaLevel() : 0,
                WaveBudget.BossWaveInterval);
            StartNextWave();
        }
        bPlaytestTargetsSpawned = true;
        BuildZoneCaptureTour(Markers);
        BreakerScheduleFeedstockCapture(GetWorld());
        BreakerScheduleTellCapture(GetWorld());
        BreakerScheduleBlastCapture(GetWorld());
        BreakerSchedulePocketRiftCapture(GetWorld());
        BreakerScheduleChestCapture(GetWorld());
        BreakerScheduleNpcCapture(GetWorld());
        BreakerScheduleWeakPointCapture(GetWorld());
        ScheduleScreenshots();
        UE_LOG(LogTemp, Log, TEXT("[BreakerMap] fernhall — %s."),
            bRiftInstance ? TEXT("RIFT INSTANCE, waves live") : TEXT("the yard, no gym field"));
        return;
    }

    // Order matters only in one place: the apron has to exist before anything
    // that stands on it, so SpawnExpandedField runs first now. It used to run
    // last, which is harmless for static meshes and was not for the enemies
    // that ground-snap.
    SpawnExpandedField();
    SpawnBreach();
    SpawnSafeZone();
    SpawnAnchorCamp();
    SpawnPlaytestTargets();
    SpawnMovementCourse();
    SpawnJumpGapRun();
    SpawnCombatEncounter();
    SpawnWorldDressing();
    // Recorded HERE rather than inferred from a map name: this is the one
    // place that actually builds a gym field, so it is the only place that can
    // honestly answer whether one exists.
    bGymFieldBuilt = true;
    // THE HUB, and its travel point back to this gym. Placed BEHIND the safe
    // ring, on the opposite side from the encounter, so it cannot overlap the
    // field SpawnExpandedField just built or the arena the combat spawns use.
    //
    // Reachability (O40c): a hub nobody can walk to is the same defect as an
    // ability nobody can equip. This call, and the delegate bind under it, are
    // the whole in-game path — the travel point deliberately does not move
    // anyone itself (it has no idea where the gym is), so the game mode owns
    // the teleport because the game mode is what knows Frame.Ground.
    // NO HUB IN THE GYM ANY MORE. Owner: "the gym is still attatched to the
    // anchor" — and it was, literally: the hub was built 6000 cm from the gym's
    // origin IN THE SAME WORLD, so the two were one continuous space a player
    // could walk between. They are separate maps now, and the gym builds only
    // the gym.
    // The return gate, beside the safe pad where a player who has finished a
    // run is already standing. Travel was one-way without it.
    if (ABreakerTravelPoint* GymTravel = GetWorld()->SpawnActor<ABreakerTravelPoint>(
            ABreakerTravelPoint::StaticClass(), FTransform(Frame.At(600.0f, -600.0f, 0.0f))))
    {
        GymTravel->ExcludedDestinationId = ABreakerTravelPoint::GymDestinationId;
        GymTravel->OnDestinationSelected.AddUObject(this, &ABreakerGameMode::HandleHubTravelSelected);
    }
    LogGymSummary();
    BuildCaptureTour();
    ScheduleScreenshots();

}

void ABreakerGameMode::ArmDevInstruments(APlayerController* NewPlayer)
{
    // EVERY MAP WITH A FRAME, EXCEPT THE ANCHOR, and the exception is the
    // Anchor's own ruling rather than a workaround: the hub is social — no
    // combat, weapon holstered — so arming a crowd of forty enemies in it is
    // wrong on its face.
    //
    // MEASURED, not assumed. The first run with these moved armed the probe in
    // the hub and the session then travelled to Fernhall on its own, with no
    // input: forty bodies in a social space drove the hub into a state it is
    // not built for. The same run without the probe stayed put, which is what
    // named the cause. So this is not "the probe is noisy in the hub" — it is
    // that a combat instrument in a no-combat space produced behaviour nobody
    // asked for, and the instrument armed there for the first time because of
    // the move above.
    if (UBreakerGameInstance::IsAnchorMap(this))
    {
        // AND A PROBE ASKED FOR HERE IS REFUSED OUT LOUD, not silently skipped.
        // Autoplay now lands in the Anchor (Part One-E), so the sweep command
        // every lane already has in its notes stopped arming anything — and
        // because the exit is wired to the probe's summary, an unattended run
        // then HUNG instead of failing. Silence plus a hang is the worst pair
        // this harness can produce, and it is the third time the shape has cost
        // a cycle.
        int32 Requested = 0;
        if (FParse::Value(FCommandLine::Get(), TEXT("BreakerCrowdProbe="), Requested) && Requested > 0)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[BreakerCrowd] the probe does not arm in the Anchor: it is a social map and a crowd ")
                TEXT("in it drove the hub into travelling on its own. Name a combat map, e.g. ")
                TEXT("Lvl_Fernhall or Lvl_Gym, as the first argument."));
            if (FApp::IsUnattended()) FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        return;
    }

    // EVERY DEV INSTRUMENT, ON EVERY MAP THAT HAS A FIELD FRAME.
    //
    // These five used to sit in the gym-only tail, ninety lines past the
    // last map branch. That was invisible and load-bearing: the ruling that
    // moves autoplay into the Anchor does not MOVE them, it stops REACHING
    // them — so -BreakerCrowdProbe, the only instrument that produces the
    // density sweep, would have become unreachable for the lane told to
    // prioritise density. Silently correct at compile time, and broken in a
    // way nobody would think to re-run.
    //
    // Called immediately after BuildFieldFrame, which is the real
    // precondition: SpawnEffectProbe and SpawnCrowdProbe both refuse without
    // bFieldFrameSet, and the front end returns before the frame is built.
    // So "every map with a frame" is exactly the set that can host them.
    //
    // BuildCaptureTour and LogGymSummary deliberately DID NOT move: the
    // tour's eight vantages are authored against gym geometry and the
    // summary describes the gym's own build, so both are gym-DEPENDENT
    // rather than merely gym-located.
    // THE BOSS KEY. F5, because the playtest keys F1-F4 and the F talk key all
    // live on ABreakerCharacter (Characters/), which this lane does not own.
    // Binding onto the PLAYER CONTROLLER's input component reaches the same
    // keyboard without touching that file: the pawn's component sits above the
    // controller's on the input stack, so a key the pawn does not claim falls
    // through to here, and F5 is claimed by nothing.
    if (NewPlayer->InputComponent)
    {
        NewPlayer->InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &ABreakerGameMode::SpawnBossTest);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BreakerGym] no controller input component; the F5 boss key is unavailable. Use Breaker.Boss."));
    }
    // -BreakerBossOnStart spawns a boss during the gym build, so the capture
    // harness can PHOTOGRAPH it. Without this the boss is only reachable by a
    // key press, and a headless run cannot press a key — which would leave
    // the one archetype most worth looking at as the one archetype nobody has
    // looked at. Bare, it is the Field Marshal; =<name> picks a body through
    // ABreakerBossEnemy::ClassForBossName (Holdfast, FieldMarshal), and an
    // unknown name is refused loudly rather than falling back — the same rule
    // -BreakerCrowdLoad keeps. Dev-only by construction: a command-line switch
    // cannot be reached from a shipped build.
    {
        FString BossOnStart;
        const bool bNamed = FParse::Value(FCommandLine::Get(), TEXT("BreakerBossOnStart="), BossOnStart);
        if (bNamed || FParse::Param(FCommandLine::Get(), TEXT("BreakerBossOnStart")))
        {
            const TSubclassOf<ABreakerBossEnemy> BossClass = BossOnStart.IsEmpty()
                ? TSubclassOf<ABreakerBossEnemy>(ABreakerBossEnemy::StaticClass())
                : ABreakerBossEnemy::ClassForBossName(FName(*BossOnStart));
            if (BossClass)
            {
                SpawnBossOfClass(BossClass);
                LogGymSummary();
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[BreakerGym] -BreakerBossOnStart=%s names no boss; nothing spawned. Known: Holdfast, FieldMarshal."), *BossOnStart);
            }
        }
    }
    if (!BossConsoleCommand)
    {
        BossConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("Breaker.Boss"),
            TEXT("Spawns a boss at the elite arena (Encounter-Design 3). Breaker.Boss [Holdfast|FieldMarshal]; bare is the Field Marshal."),
            FConsoleCommandWithArgsDelegate::CreateUObject(this, &ABreakerGameMode::SpawnBossCommand));
    }
    // -BreakerEffectProbe: the Phase B proof. One glow, fixed spot in the
    // spawn view, fixed clock, placed during the gym build so the capture
    // cadence (first frame at 6.0 s, then every 2.0 s) straddles its death:
    // it must be IN the first frame and GONE from the third. Same dev-only
    // construction as -BreakerBossOnStart.
    if (FParse::Param(FCommandLine::Get(), TEXT("BreakerEffectProbe")))
    {
        SpawnEffectProbe();
    }
    // -BreakerCrowdProbe=N: the density instrument (see the header block).
    // -BreakerCrowdLoad=<patrol|engaged> names the scene. An UNRECOGNISED value
    // is refused loudly rather than falling back: the capture harness already
    // has one switch that silently falls back to a different screen, and a
    // performance instrument that quietly measures a scene you did not ask for
    // is exactly the failure this flag exists to close.
    int32 CrowdCount = 0;
    if (FParse::Value(FCommandLine::Get(), TEXT("BreakerCrowdProbe="), CrowdCount) && CrowdCount > 0)
    {
        ECrowdLoad Load = ECrowdLoad::Patrol;
        FString LoadName;
        if (FParse::Value(FCommandLine::Get(), TEXT("BreakerCrowdLoad="), LoadName))
        {
            if (LoadName.Equals(TEXT("engaged"), ESearchCase::IgnoreCase)) Load = ECrowdLoad::Engaged;
            else if (LoadName.Equals(TEXT("patrol"), ESearchCase::IgnoreCase)) Load = ECrowdLoad::Patrol;
            else
            {
                UE_LOG(LogTemp, Error,
                    TEXT("[BreakerCrowd] -BreakerCrowdLoad=%s is not a load. Use patrol or engaged. Probe not armed."),
                    *LoadName);
                CrowdCount = 0;
                // AND THE RUN ENDS. Without this an unattended run sits
                // forever: the exit is wired to the probe's summary, and a
                // refused flag means no probe, so no summary, so no exit. A
                // harness script would have HUNG rather than failed, and the
                // first attempt at this flag did exactly that. Status 1 is
                // requested but MEASURED AS 0 at the process — something on the
                // shutdown path overrides it — so a script must read the log
                // line above, not the exit code, to tell a refusal from a run.
                if (FApp::IsUnattended()) FPlatformMisc::RequestExitWithStatus(false, 1);
            }
        }
        // THE PROBE NEEDS A RUNNING WORLD, and -BreakerAutoPlay is how a
        // headless run gets one. Without it the title menu is up, the menu
        // PAUSES THE WORLD, and TickCrowdSampler rides the game mode's actor
        // tick — so the sampler never advances, no summary is logged, and the
        // exit wired to that summary never fires. The run hangs after printing
        // "probe armed" and "safe ring suppressed", which are the same two
        // lines a working run prints: the failure is indistinguishable from
        // success until the summary does not arrive.
        //
        // NOT FIXED BY MOVING THE SAMPLER TO A CORE TICKER, which is how the
        // capture harness solved the same pause. A core ticker would let the
        // probe sample a PAUSED crowd and report it as a measurement, which is
        // the class of instrument this flag's engaged%% guard exists to refuse.
        // The world has to be running; the requirement is real, so it is
        // stated rather than worked around.
        // BOTH FORMS. Part One-E gave autoplay a value (=Gym / =Fernhall) and
        // told every instrument to use it — and this guard still tested only
        // the bare flag, so the probe has been UNARMABLE since that change:
        // -BreakerAutoPlay=Gym printed this very error and the run played an
        // endless gym fight that read as a hang. FParse::Param sees flags,
        // FParse::Value sees values; an instrument gate must accept whichever
        // the harness table tells people to type.
        FString BreakerAutoPlayValueProbe;
        const bool bBreakerAutoPlayPresent =
            FParse::Param(FCommandLine::Get(), TEXT("BreakerAutoPlay")) ||
            FParse::Value(FCommandLine::Get(), TEXT("BreakerAutoPlay="), BreakerAutoPlayValueProbe);
        if (CrowdCount > 0 && !bBreakerAutoPlayPresent)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[BreakerCrowd] the probe needs -BreakerAutoPlay: without it the title menu is up, ")
                TEXT("the world is paused, and the sampler never runs. Probe not armed."));
            CrowdCount = 0;
            if (FApp::IsUnattended()) FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        if (CrowdCount > 0)
        {
            SpawnCrowdProbe(FMath::Clamp(CrowdCount, 1, 200),
                FParse::Param(FCommandLine::Get(), TEXT("BreakerCrowdSkeletal")), Load);
        }
    }
    // Breaker.CloseRift: the completion seam's producer until FIELD's terminator
    // exists. THE POINT IS THAT THE STATE IS NOT A DEAD API — LEDGER binds
    // OnRiftCompleted and can actually receive it today, and the suite can
    // exercise the latch through a real caller. FIELD's raise becomes a SECOND
    // producer without changing this surface.
    if (!CloseRiftConsoleCommand)
    {
        CloseRiftConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("Breaker.CloseRift"),
            TEXT("Completes the current rift run (O168's seam; refuses outside a rift and refuses twice)."),
            FConsoleCommandDelegate::CreateLambda([this]()
            {
                APawn* Pawn = GetWorld() && GetWorld()->GetFirstPlayerController()
                    ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
                CompleteRiftRun(Pawn);
            }));
    }
    // Breaker.MarkTerminator: marks the nearest live enemy so the WHOLE chain
    // is exercisable today — mark, kill it, completion broadcast — without
    // anyone having decided which body holds a rift open in play. That decision
    // is design and is in the lane's report; this is the instrument.
    if (!MarkTerminatorConsoleCommand)
    {
        MarkTerminatorConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("Breaker.MarkTerminator"),
            TEXT("Marks the nearest live enemy as the rift terminator (O168's seam, dev instrument)."),
            FConsoleCommandDelegate::CreateLambda([this]()
            {
                UWorld* World = GetWorld();
                APawn* Pawn = World && World->GetFirstPlayerController()
                    ? World->GetFirstPlayerController()->GetPawn() : nullptr;
                if (!World || !Pawn)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Rift] no pawn; cannot pick a terminator."));
                    return;
                }
                ABreakerEnemy* Nearest = nullptr;
                float NearestSq = TNumericLimits<float>::Max();
                for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
                {
                    if (It->IsDeadEnemy()) continue;
                    const float DistanceSq = static_cast<float>(
                        FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()));
                    if (DistanceSq < NearestSq) { NearestSq = DistanceSq; Nearest = *It; }
                }
                if (!Nearest)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Rift] nothing alive to mark as a terminator."));
                    return;
                }
                MarkRiftTerminator(Nearest);
            }));
    }
    // Breaker.Rift.Population <n>: respawn the current wave at n bodies, in
    // place, so the owner can walk the same yard at three populations and pick
    // one. It clears what is alive and re-solves at the requested count rather
    // than adding to it — comparing populations means comparing populations,
    // not watching one accumulate.
    if (!PopulationConsoleCommand)
    {
        PopulationConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("Breaker.Rift.Population"),
            TEXT("Re-populates the current wave at N live bodies (walk a yard at three counts and pick)."),
            FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
            {
                const int32 Wanted = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
                if (Wanted <= 0)
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("[Rift] Breaker.Rift.Population <n>: how many live bodies to stand in. ")
                        TEXT("Try 25, 50 and 100 in the same yard and pick the one that reads."));
                    return;
                }
                // THE DENSITY CEILING IS NOT RAISED HERE. 5.3's per-player cap
                // is a readability rule and this command exists to FIND the
                // number inside it, not to argue with it: a run at 300 would
                // answer a question nobody asked and cost a frame nobody has.
                const int32 Capped = FMath::Clamp(Wanted, 1,
                    UBreakerWaveBudgetLibrary::GetMaximumLiveEnemies(1, WaveBudget));
                if (Capped != Wanted)
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("[Rift] %d clamped to %d: 5.3's density ceiling. The question is which number ")
                        TEXT("inside the cap reads as populated, not whether the cap should move."),
                        Wanted, Capped);
                }
                for (const TObjectPtr<ABreakerEnemy>& Enemy : WaveEnemies)
                {
                    if (IsValid(Enemy) && !Enemy->IsDeadEnemy()) Enemy->Destroy();
                }
                WaveEnemies.Reset();
                CurrentWave = FMath::Max(CurrentWave - 1, 0);

                // The density ceiling IS the knob, so it is the knob this
                // turns — borrowed for one solve and handed straight back. The
                // budget curve is untouched: a wave still buys what it can
                // afford, it is simply allowed fewer bodies to buy.
                const int32 Restore = WaveBudget.MaximumLiveEnemiesPerPlayer;
                WaveBudget.MaximumLiveEnemiesPerPlayer = Capped;
                StartNextWave();
                WaveBudget.MaximumLiveEnemiesPerPlayer = Restore;
                UE_LOG(LogTemp, Display, TEXT("[Rift] re-populated at %d live bodies."), Capped);
            }));
    }
    if (!EffectProbeConsoleCommand)
    {
        EffectProbeConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("Breaker.EffectProbe"),
            TEXT("Places one 7 s test glow ahead of the gym spawn (ability-effect renderer probe)."),
            FConsoleCommandDelegate::CreateUObject(this, &ABreakerGameMode::SpawnEffectProbe));
    }
}

void ABreakerGameMode::SpawnEffectProbe()
{
    UWorld* World = GetWorld();
    if (!World || !bFieldFrameSet) return;
    if (!EffectProbeRenderer)
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        EffectProbeRenderer = World->SpawnActor<ABreakerEffectRenderer>(
            ABreakerEffectRenderer::StaticClass(), FTransform::Identity, Params);
    }
    if (!EffectProbeRenderer) return;

    // 5 m ahead of the field origin at chest height: dead centre of the spawn
    // view, close enough that a 50 cm sphere is unmistakably in frame. Cyan,
    // the player-system token — this pool exists for the player's abilities.
    // Duration 7.0 s against the 6.0/8.0/10.0 s capture frames: alive in the
    // first, dying or dead at the second, unarguably gone by the third. All
    // O2 PLACEHOLDER except the 7.0, which is derived from the cadence.
    const FVector ProbeSpot = Frame.At(500.0f, 0.0f, 140.0f);
    BreakerFX::FEffectTiming Timing;
    Timing.DurationSeconds = 7.0f;
    Timing.FadeInSeconds = 0.15f;
    Timing.FadeOutSeconds = 0.5f;
    EffectProbeRenderer->AddGlow(ProbeSpot, 50.0f, BreakerUI::Cyan, 6.0f, Timing);
    // The log half of the proof: a headless reader greps this line for WHERE
    // and UNTIL WHEN, then reads the screenshots for whether the world agreed.
    UE_LOG(LogTemp, Log, TEXT("[BreakerGym] effect probe: glow at (%.0f, %.0f, %.0f) for %.1f s."),
        ProbeSpot.X, ProbeSpot.Y, ProbeSpot.Z, Timing.DurationSeconds);

    // A Rot-shaped zone beside the glow, on the same 7.0 s straddle clock:
    // the zone's own presentation plus the rim the effect renderer draws for
    // it, photographed alive in the first frame and expired out of the later
    // ones. Geometry is Rot's authored footprint (400 cm), payload empty —
    // this is a photograph, not an encounter.
    FBreakerZoneSpec ProbeZone;
    ProbeZone.ZoneTag = FGameplayTag::RequestGameplayTag(TEXT("Zone.EffectProbe"), false);
    ProbeZone.RadiusCm = 400.0f;
    ProbeZone.Duration = Timing.DurationSeconds;
    FActorSpawnParameters ZoneParams;
    ZoneParams.ObjectFlags |= RF_Transient;
    ZoneParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (ABreakerZoneActor* Zone = World->SpawnActor<ABreakerZoneActor>(
            ABreakerZoneActor::StaticClass(), FTransform(Frame.At(900.0f, 250.0f, 2.0f)), ZoneParams))
    {
        Zone->ConfigureZone(ProbeZone, nullptr);
        UE_LOG(LogTemp, Log, TEXT("[BreakerGym] effect probe: zone rim r=%.0f for %.1f s."),
            ProbeZone.RadiusCm, ProbeZone.Duration);
    }
}

const TCHAR* ABreakerGameMode::CrowdLoadName() const
{
    return CrowdLoad == ECrowdLoad::Engaged ? TEXT("engaged") : TEXT("patrol");
}

void ABreakerGameMode::SpawnCrowdProbe(int32 Count, bool bSkeletal, ECrowdLoad Load)
{
    UWorld* World = GetWorld();
    if (!World || !bFieldFrameSet) return;

    // TWO LAYOUTS, ONE PER LOAD, and the geometry is the whole difference.
    //
    // PATROL keeps the historical far grid — ten to a row, 800 cm pitch, from
    // 60 m downfield. The comment that used to sit here said "the pack
    // pursues"; it does not and never did. 6000 cm is nearly three times
    // ABreakerEnemy's 2200 cm DetectionRange, so every body takes the PATROL
    // branch. The layout is kept EXACTLY as it was so the figures already
    // taken with it remain comparable; only its name is corrected.
    //
    // ENGAGED arrays the crowd in frontal rings from 900 to 2100 cm, every
    // one of them inside DetectionRange, and drops the safe ring below. Ring
    // pitch is chosen against the widest body in the project (120 cm): the
    // tightest arc here is the innermost, and it seats its share at over
    // 140 cm apart.
    //
    // Loot and respawn are off in both so the measurement is enemies, not
    // pickups.
    CrowdLoad = Load;
    CrowdRoster.Reset();
    if (Load == ECrowdLoad::Engaged)
    {
        // DECLARED FIRST, so the ordering cannot matter. SpawnSafeZone honours
        // this whenever it runs; the drop below only handles the case where the
        // ring is already up by the time the probe arms.
        bProbeSuppressesSafeZone = true;
    }
    if (Load == ECrowdLoad::Engaged && bSafeZoneSet)
    {
        // A player inside the safe ring is off-limits to EVERY enemy
        // (IsInSafeZone nulls the target before detection is even consulted),
        // so the ring alone would hold the whole crowd in PATROL however close
        // it spawned. Saved and restored at the summary rather than dropped for
        // the process, because a probe must not leave the world altered under
        // a controller-in-hand run that keeps going afterwards.
        //
        // THE FLAG IS CLEARED, NOT THE RADIUS. Setting the radius to zero does
        // not work and the first attempt at this shipped that bug: IsInSafeZone
        // compares with <=, and the pawn spawns AT SafeZoneCenter, so a zero
        // radius still contains it and the whole crowd stayed in PATROL at
        // 600 cm. The probe's own engaged%% guard is what caught it.
        CrowdSavedSafeZoneRadius = SafeZoneRadius;
        bSafeZoneSet = false;
    }
    const int32 Rings = 5;
    const int32 PerRing = FMath::Max(FMath::DivideAndRoundUp(Count, Rings), 1);
    USkeletalMesh* ProbeManny = bSkeletal ? LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")) : nullptr;
    UClass* ProbeAnim = bSkeletal ? LoadClass<UAnimInstance>(nullptr,
        TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")) : nullptr;

    for (int32 Index = 0; Index < Count; ++Index)
    {
        FVector Spot;
        if (Load == ECrowdLoad::Engaged)
        {
            const int32 Ring = FMath::Min(Index / PerRing, Rings - 1);
            const int32 Slot = Index % PerRing;
            const float RadiusCm = 900.0f + Ring * 300.0f;   // 900..2100, inside DetectionRange
            const float Sweep = PerRing > 1 ? (Slot / static_cast<float>(PerRing - 1)) : 0.5f;
            const float AngleRad = FMath::DegreesToRadians(-80.0f + 160.0f * Sweep);
            Spot = Frame.At(RadiusCm * FMath::Cos(AngleRad), RadiusCm * FMath::Sin(AngleRad), 100.0f);
        }
        else
        {
            const int32 Row = Index / 10;
            const int32 Col = Index % 10;
            Spot = Frame.At(6000.0f + Row * 800.0f, (Col - 4.5f) * 800.0f, 100.0f);
        }
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(),
            Spot, (-Frame.Forward).Rotation(), Params);
        if (!Enemy) continue;
        Enemy->ConfigureCrowdProbe();
        Enemy->SetAreaLevel(GymAreaLevel);
        // A sprinkle of rank so the crowd IS the glance test: can you find
        // the gold and the violet in a field of eighty grey-violet trash?
        if (Index % 25 == 0) Enemy->SetMonsterRank(EBreakerMonsterRank::ModifierBearing);
        else if (Index % 10 == 0) Enemy->SetMonsterRank(EBreakerMonsterRank::Elite);
        if (ProbeManny)
        {
            // PROBE-ONLY SURGERY, from outside: hide every primitive part
            // and strap on an animating mannequin, so the skeletal run is
            // the primitive run plus exactly one variable. Not a shipping
            // path — the measurement decides whether one ever exists.
            TArray<UStaticMeshComponent*> Parts;
            Enemy->GetComponents<UStaticMeshComponent>(Parts);
            for (UStaticMeshComponent* Part : Parts) Part->SetVisibility(false);
            USkeletalMeshComponent* Skel = NewObject<USkeletalMeshComponent>(Enemy, TEXT("CrowdProbeBody"));
            Skel->SetupAttachment(Enemy->GetRootComponent());
            Skel->RegisterComponent();
            Skel->SetSkeletalMesh(ProbeManny);
            if (ProbeAnim) Skel->SetAnimInstanceClass(ProbeAnim);
            Skel->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
            Skel->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
            Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        CrowdRoster.Add(Enemy);
        ++CrowdSpawned;
    }
    bCrowdProbeArmed = true;
    UE_LOG(LogTemp, Display,
        TEXT("[BreakerCrowd] probe armed: %d/%d enemies spawned, load=%s, skeletal=%d, area level %d%s."),
        CrowdSpawned, Count, CrowdLoadName(), ProbeManny != nullptr, GymAreaLevel,
        CrowdSavedSafeZoneRadius >= 0.0f ? TEXT(" (safe ring dropped for the run)") : TEXT(""));
}

void ABreakerGameMode::TickCrowdSampler(float DeltaSeconds)
{
    if (!bCrowdProbeArmed) return;
    if (CrowdWarmupRemaining > 0.0f)
    {
        CrowdWarmupRemaining -= DeltaSeconds;
        return;
    }
    if (CrowdSampleRemaining > 0.0f)
    {
        CrowdSampleRemaining -= DeltaSeconds;
        ++CrowdFrames;
        const float FrameMs = DeltaSeconds * 1000.0f;
        CrowdFrameMsSum += FrameMs;
        CrowdFrameMsMax = FMath::Max(CrowdFrameMsMax, FrameMs);
        // The engine's own thread clocks, cycles converted to ms — the
        // dominance answer (game vs render vs GPU) in four numbers.
        CrowdGameMsSum += FPlatformTime::ToMilliseconds(GGameThreadTime);
        CrowdRenderMsSum += FPlatformTime::ToMilliseconds(GRenderThreadTime);
        CrowdGpuMsSum += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());

        // THE SCENE IS MEASURED, NOT ASSUMED. Every roster body's own state
        // label decides whether it counts as engaged, and the nearest body's
        // distance says whether the crowd is where the layout put it. The
        // first version of this probe claimed a load in a comment and measured
        // a different one for its whole life; a claim beside a measurement of
        // the same thing is the only shape that cannot do that again.
        const APawn* ProbePawn = GetWorld() && GetWorld()->GetFirstPlayerController()
            ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
        float NearestSq = TNumericLimits<float>::Max();
        for (const TWeakObjectPtr<ABreakerEnemy>& Weak : CrowdRoster)
        {
            const ABreakerEnemy* Body = Weak.Get();
            if (!Body || Body->IsDeadEnemy()) continue;
            ++CrowdStateReads;
            if (Body->GetEnemyStateLabel() != TEXT("PATROL")) ++CrowdEngagedReads;
            if (ProbePawn)
            {
                NearestSq = FMath::Min(NearestSq,
                    static_cast<float>(FVector::DistSquared2D(Body->GetActorLocation(), ProbePawn->GetActorLocation())));
            }
        }
        if (NearestSq < TNumericLimits<float>::Max())
        {
            CrowdNearestCm = FMath::Sqrt(NearestSq);
        }
        return;
    }
    bCrowdProbeArmed = false;
    const float N = FMath::Max(1, CrowdFrames);
    // THE SUMMARY CARRIES THE REQUESTED LOAD AND THE MEASURED ONE SIDE BY SIDE.
    // "enemies=100" says nothing about what the hundred were doing, and the
    // word alone is what was wrong last time: the flag said crowd and the
    // scene said patrol-at-three-times-detection-range. engaged%% is read off
    // the bodies' own state labels, so a run whose scene disagrees with its
    // flag reports the disagreement in its own summary.
    const float EngagedPct = CrowdStateReads > 0
        ? 100.0f * CrowdEngagedReads / static_cast<float>(CrowdStateReads) : 0.0f;
    UE_LOG(LogTemp, Display,
        TEXT("[BreakerCrowd] SUMMARY load=%s engaged=%.0f%% (measured) nearest=%.0fcm enemies=%d frames=%d avg=%.2fms worst=%.2fms fps=%.0f game=%.2fms render=%.2fms gpu=%.2fms"),
        CrowdLoadName(), EngagedPct, CrowdNearestCm,
        CrowdSpawned, CrowdFrames, CrowdFrameMsSum / N, CrowdFrameMsMax,
        1000.0f / FMath::Max(CrowdFrameMsSum / N, 0.01f),
        CrowdGameMsSum / N, CrowdRenderMsSum / N, CrowdGpuMsSum / N);

    // AN INSTRUMENT THAT RETURNS A FALSE NEGATIVE IS WORSE THAN ONE THAT
    // RETURNS NOTHING, so a scene that does not match the flag is an ERROR in
    // the log rather than a footnote a reader has to notice. Both directions
    // are checked: an engaged run that did not engage is the original defect,
    // and a patrol run that DID engage means the far grid stopped being far.
    if (CrowdLoad == ECrowdLoad::Engaged && EngagedPct < 90.0f)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[BreakerCrowd] load=engaged but only %.0f%% of the crowd engaged (nearest %.0f cm). ")
            TEXT("These numbers are NOT a fighting crowd's — do not read them as one."),
            EngagedPct, CrowdNearestCm);
    }
    else if (CrowdLoad == ECrowdLoad::Patrol && EngagedPct > 5.0f)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[BreakerCrowd] load=patrol but %.0f%% engaged: the far grid is no longer outside detection range, ")
            TEXT("so this run is not comparable with the figures taken before it."),
            EngagedPct);
    }

    // The world is handed back as it was found: a probe must not leave a
    // controller-in-hand run without its safe ring.
    if (CrowdSavedSafeZoneRadius >= 0.0f)
    {
        SafeZoneRadius = CrowdSavedSafeZoneRadius;
        CrowdSavedSafeZoneRadius = -1.0f;
    }
    if (bProbeSuppressesSafeZone)
    {
        bProbeSuppressesSafeZone = false;
        bSafeZoneSet = true;
    }
    if (FApp::IsUnattended())
    {
        // A scripted run has its number; let the script harvest the log.
        FPlatformMisc::RequestExitWithStatus(false, 0);
    }
}

bool ABreakerGameMode::IsBossAlive() const
{
    return IsValid(ActiveBoss) && !ActiveBoss->IsDeadEnemy();
}

void ABreakerGameMode::ResetBossEncounter()
{
    // O82 (amended): a solo death inside a boss encounter resets the
    // encounter rather than spending a budget — the dead boss progress is
    // the death's whole price in campaign.
    //
    // O121, AND WHY IT LIVES HERE TOO: the full reset is fair ONLY because
    // O18 puts a boss at twenty to forty-five seconds — losing that much
    // progress is a beat, not an evening. THE ENCOUNTER'S LENGTH IS WHAT
    // LICENSES THIS RULE. Anyone lengthening a boss fight past a few
    // minutes — more phases, a longer order cadence, a second health bar —
    // is obliged by O121 to replace this reset with a checkpoint, and this
    // comment exists because the TTK figure lives in a different file from
    // the death rule and nothing else connects them.
    if (!IsBossAlive()) return;
    // The SAME boss comes back: the class is read off the body before it is
    // destroyed, so a Holdfast encounter does not reset into a Marshal.
    const TSubclassOf<ABreakerBossEnemy> BossClass = ActiveBoss->GetClass();
    const FVector PreviousCenter = ActiveBoss->GetActorLocation();
    UE_LOG(LogTemp, Display, TEXT("[BreakerGym] boss encounter RESET on player death (O82): %s respawns whole."),
        *BossClass->GetName());
    ActiveBoss->Destroy();
    ActiveBoss = nullptr;
    SpawnBossOfClass(BossClass, bRiftInstance ? TOptional<FVector>(PreviousCenter) : TOptional<FVector>());
    if (bRiftInstance && IsValid(ActiveBoss))
    {
        WaveEnemies.RemoveAll([](const TObjectPtr<ABreakerEnemy>& Enemy) { return !IsValid(Enemy) || Enemy->IsDeadEnemy(); });
        ActiveBoss->ConfigureWave(GymAreaLevel);
        WaveEnemies.Add(ActiveBoss);
        MarkRiftTerminator(ActiveBoss);
    }
}

void ABreakerGameMode::SpawnBossTest()
{
    SpawnBossOfClass(ABreakerBossEnemy::StaticClass());
}

void ABreakerGameMode::SpawnBossCommand(const TArray<FString>& Args)
{
    if (Args.IsEmpty())
    {
        SpawnBossTest();
        return;
    }
    const TSubclassOf<ABreakerBossEnemy> BossClass = ABreakerBossEnemy::ClassForBossName(FName(*Args[0]));
    if (!BossClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[BreakerGym] Breaker.Boss %s names no boss; nothing spawned. Known: Holdfast, FieldMarshal."), *Args[0]);
        return;
    }
    SpawnBossOfClass(BossClass);
}

void ABreakerGameMode::SpawnBossOfClass(TSubclassOf<ABreakerBossEnemy> BossClass, TOptional<FVector> EncounterCenter)
{
    UWorld* World = GetWorld();
    if (!World || !bFieldFrameSet || !BossClass) return;
    if (IsBossAlive())
    {
        UE_LOG(LogTemp, Display, TEXT("[BreakerGym] %s is already alive; refusing a second boss."), *ActiveBoss->GetClass()->GetName());
        return;
    }

    // The elite arena, which is the fourth combat pocket. Level-Design §5 puts
    // it at ArenaDistance with radius CombatPocketRadius (2000), and §5.1
    // notices that doubling that radius is EXACTLY Encounter-Design §3.3's
    // 4000 x 4000 boss room. So the arena is the right size by two independent
    // derivations — but only just: the boss's gallery offsets are ±1900 and the
    // pocket's broken wall arc sits at 1800-2200 cm from centre, so a gallery
    // can land inside a ruin segment. Checked and reported rather than assumed.
    FVector ArenaCentre = EncounterCenter.IsSet() ? EncounterCenter.GetValue() : Frame.At(ArenaDistance, 0.0f, 140.0f);
    if (EncounterCenter.IsSet())
    {
        FHitResult Floor;
        const FVector TraceTop = ArenaCentre + FVector(0, 0, 1000);
        const FVector TraceBottom = ArenaCentre - FVector(0, 0, 2000);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(RiftBossPlacement), false);
        if (!World->LineTraceSingleByObjectType(Floor, TraceTop, TraceBottom,
            FCollisionObjectQueryParams(ECC_WorldStatic), Query))
        {
            UE_LOG(LogTemp, Error, TEXT("[Rift] boss spawn refused: no floor at %s."), *ArenaCentre.ToString());
            return;
        }
        ArenaCentre.Z = Floor.ImpactPoint.Z + 140.0f;
    }
    if (CombatPocketRadius < BossArenaClearanceCm)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[BreakerGym] arena radius %.0f cm is under the boss's %.0f cm gallery reach; orders will point into geometry."),
            CombatPocketRadius, BossArenaClearanceCm);
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    // Facing back down the field, so a player arriving from the camp meets its
    // FRONT — which is the armoured side, and the whole fight is the decision
    // to stop being there.
    ABreakerBossEnemy* Boss = World->SpawnActor<ABreakerBossEnemy>(
        BossClass, ArenaCentre, (-Frame.Forward).Rotation(), Params);
    if (!Boss) return;

    Boss->ConfigureEncounter(Boss->GetActorLocation(), 0.0f);
    Boss->SetAreaLevel(GymAreaLevel);
    // Rank Boss is authored in the class and must NOT be overwritten here;
    // SetAreaLevel rebuilds the chassis against whatever rank the archetype
    // set, which is exactly the one-source-of-truth rule O27 installed.
    UBreakerKillTelemetryComponent::AttachTo(Boss);
    Boss->OnBossDefeated.AddDynamic(this, &ABreakerGameMode::HandleBossDefeated);
    ActiveBoss = Boss;

    UE_LOG(LogTemp, Display,
        TEXT("[BreakerBoss] %s spawned at %s (%s), area level %d, %.0f health."),
        *Boss->GetClass()->GetName(), *Boss->GetActorLocation().ToString(),
        EncounterCenter.IsSet() ? TEXT("rift field") : TEXT("gym arena"), GymAreaLevel, Boss->GetMonsterMaxHealth());
}

void ABreakerGameMode::HandleBossDefeated()
{
    // O18 puts the boss band at 20-45s and Encounter-Design §3.2 records a
    // DIVERGENCE that is still open: the order cadence imposes a script floor
    // independent of health, so even a player who bursts a phase down waits on
    // the phases. Whether the composition lands inside the band is a
    // MEASUREMENT, and it is the boss TTK bucket that carries it — filed by the
    // kill-telemetry component from the boss's own rank, not from here.
    RefillPlayerAmmo();
    // THE BOSS THAT FELL, NOT THE FIRST BOSS THAT EXISTED. This printed the
    // literal FIELD MARSHAL for every boss, so the Holdfast's death was
    // logged as the Marshal's. The name comes from the same string keys the
    // enemy bar draws over the body (BreakerEnemyHealthBars' name source is
    // file-static, so the two-class map is repeated here rather than reached).
    const ABreakerBossEnemy* Fallen = ActiveBoss;
    const FString BossName = !Fallen ? FString(TEXT("Boss"))
        : BreakerStrings::Get(Fallen->IsA<ABreakerHoldfastEnemy>()
            ? EBreakerStringKey::EnemyHoldfast : EBreakerStringKey::EnemyFieldMarshal);
    UE_LOG(LogTemp, Display, TEXT("[BreakerGym] %s down. Boss TTK sample recorded; F2 copies the report."), *BossName);
}

float ABreakerGameMode::ResolveGroundZ(const APawn* Pawn, bool* bOutFoundFloor) const
{
    if (bOutFoundFloor) *bOutFoundFloor = false;
    if (bUseGroundZOverride) return GroundZOverride;
    const UWorld* World = GetWorld();
    if (!World || !Pawn) return Pawn ? Pawn->GetActorLocation().Z - 88.0f : 0.0f;

    // Probe a ring rather than straight down. Straight down from the
    // PlayerStart in Lvl_FirstPerson lands on the template's 210 cm central
    // plinth, which is exactly the mistake the old "location minus 88" made.
    // The LOWEST hit on a ring outside the plinth is the floor the field
    // should be built on.
    const FVector Centre = Pawn->GetActorLocation();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerGroundProbe), false, Pawn);
    float Lowest = TNumericLimits<float>::Max();
    for (int32 Probe = 0; Probe < 8; ++Probe)
    {
        const FVector Start = Centre + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Probe * 45.0f, FVector::UpVector) * GroundProbeRadius;
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, Start + FVector(0, 0, 500.0f), Start - FVector(0, 0, 5000.0f), ECC_Visibility, Params))
        {
            Lowest = FMath::Min(Lowest, static_cast<float>(Hit.ImpactPoint.Z));
        }
    }
    // No hits at all means an open map with no authored floor; the capsule
    // assumption is the only thing left and is correct in that case.
    const bool bFoundFloor = Lowest != TNumericLimits<float>::Max();
    if (bOutFoundFloor) *bOutFoundFloor = bFoundFloor;
    return bFoundFloor ? Lowest : Centre.Z - 88.0f;
}

// THE FIELD HAS NO FIXED WORLD POSITION, and that surprises people twice.
// Ground, forward and right are all derived from the possessed pawn, so the
// field is built relative to wherever the player happened to spawn and facing
// however they happened to face. Two PIE sessions started from different
// viewport camera positions produce the SAME field at different world
// coordinates. Consequences: comparing absolute coordinates between two
// sessions' screenshots or logs is meaningless, and every distance in this
// file is field-relative through Frame.At(), never a world vector.
void ABreakerGameMode::BuildFieldFrame(const APawn* Pawn)
{
    if (!Pawn) return;
    Frame.Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    Frame.Right = Pawn->GetActorRightVector().GetSafeNormal2D();
    Frame.SpawnZ = Pawn->GetActorLocation().Z;
    bool bFoundFloor = false;
    const float GroundZ = ResolveGroundZ(Pawn, &bFoundFloor);
    Frame.bAuthoredFloor = bFoundFloor;
    Frame.Ground = FVector(Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, GroundZ);
    bFieldFrameSet = true;
    UE_LOG(LogTemp, Display, TEXT("[BreakerGym] field frame: ground z %.0f, spawn z %.0f (%.0f cm of plinth), forward (%.2f, %.2f)"),
        GroundZ, Frame.SpawnZ, Frame.SpawnZ - 88.0f - GroundZ, Frame.Forward.X, Frame.Forward.Y);
}

void ABreakerGameMode::ScheduleScreenshots()
{
    int32 Count = 0;
    // -BreakerCaptureArrival photographs the arrival itself: breaker_00 is
    // the first frame after the arrival cover leaves, breaker_01 one second
    // later. Two frames unless -BreakerScreenshots=N says otherwise.
    bCaptureAwaitsArrival = FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureArrival"));
    if (!FParse::Value(FCommandLine::Get(), TEXT("BreakerScreenshots="), Count) && bCaptureAwaitsArrival) Count = 2; // O2 PLACEHOLDER
    if (Count <= 0) return;
    BreakerSchedulePlateCapture(GetWorld());
    ScreenshotsRemaining = FMath::Clamp(Count, 1, 60);
    ScreenshotIndex = 0;
    if (bCaptureAwaitsArrival)
    {
        ScreenshotIntervalSeconds = 1.0f; // O2 PLACEHOLDER
        NextScreenshotTime = 0.0;
        const FBreakerArrivalHold& Hold = UBreakerGameInstance::ShippedArrivalHold();
        UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] %d screenshots, first when the arrival cover leaves (>= %.2fs and %d frames after load, then a %.2fs fade), every %.1fs after."),
            ScreenshotsRemaining, Hold.MinHoldSeconds, Hold.MinSettleFrames, Hold.FadeInSeconds, ScreenshotIntervalSeconds);
    }
    else
    {
        // THE CADENCE IS A SWITCH. Two seconds between frames photographs a
        // yard; it cannot photograph a quarter-second ability burst, and the
        // ability probe fires three of those. -BreakerScreenshotFirst=<s> and
        // -BreakerScreenshotInterval=<s> override the defaults for a run.
        FParse::Value(FCommandLine::Get(), TEXT("BreakerScreenshotFirst="), ScreenshotFirstDelaySeconds);
        FParse::Value(FCommandLine::Get(), TEXT("BreakerScreenshotInterval="), ScreenshotIntervalSeconds);
        ScreenshotIntervalSeconds = FMath::Max(0.05f, ScreenshotIntervalSeconds);
        NextScreenshotTime = FPlatformTime::Seconds() + FMath::Max(0.1f, ScreenshotFirstDelaySeconds);
        UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] %d screenshots, first at %.1fs, every %.1fs after."),
            ScreenshotsRemaining, ScreenshotFirstDelaySeconds, ScreenshotIntervalSeconds);
    }

    // Shot 0 is the SPAWN EYE VIEW even on a tour: it is the one frame that
    // answers "what does a player see when the level loads", which is the
    // question the owner's complaint is about. The tour starts at shot 1.
    if (TourCameras.Num() > 0)
    {
        if (UWorld* World = GetWorld())
        {
            if (APlayerController* PC = World->GetFirstPlayerController())
            {
                if (APawn* Pawn = PC->GetPawn()) PC->SetViewTarget(Pawn);
            }
        }
    }

    // A CORE ticker, not a world timer. Opening the front end calls
    // SetPause(true), which stops world timers dead, so a world timer here
    // captured nothing at all on any menu screen -- and the menus are half of
    // what needs looking at.
    ScreenshotTickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
        {
            if (bCaptureAwaitsArrival)
            {
                const UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>();
                if (Session && Session->IsArrivalCoverUp()) return true;
            }
            if (!bScreenshotPending && FPlatformTime::Seconds() >= NextScreenshotTime)
            {
                CaptureScreenshot();
            }
            return ScreenshotsRemaining > 0;
        }), 0.0f);
}

void ABreakerGameMode::BuildZoneCaptureTour(const FBreakerZoneMarkers& Markers)
{
    if (!GetWorld()) return;
    if (!FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureTour"))) return;

    const FBreakerZoneMarker* Start = Markers.Find(EBreakerZoneMarkerRole::PlayerStart);
    const FBreakerZoneMarker* EntryRift = Markers.Find(EBreakerZoneMarkerRole::Rift);
    if (!Start || !EntryRift) return;

    // The zone's own axes, from its own markers.
    const FVector A = Start->Location;
    const FVector B = EntryRift->Location;
    const FVector Fwd = (B - A).GetSafeNormal2D();
    const FVector Rgt = FVector::CrossProduct(FVector::UpVector, Fwd);
    const float LaneLength = static_cast<float>(FVector::Dist2D(A, B));
    const float Yaw = Fwd.Rotation().Yaw;

    struct FVantage { FVector Location; FRotator Rotation; };
    TArray<FVantage> Vantages;

    // 1. PLAN VIEW over the whole zone. The one frame that answers "what shape
    //    is this place", which is the question two yards and a seam raise.
    Vantages.Add({ A + Fwd * (LaneLength * 0.5f) + FVector(0.0f, 0.0f, 22000.0f),
        FRotator(-89.9f, Yaw, 0.0f) });

    // 2. Behind the player start, down the entry lane: the arrival composition.
    Vantages.Add({ A - Fwd * 1800.0f + FVector(0.0f, 0.0f, 1400.0f), FRotator(-9.0f, Yaw, 0.0f) });

    if (FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureEnemyNames")))
    {
        int32 CapturePocket = 0;
        FParse::Value(FCommandLine::Get(), TEXT("BreakerCapturePocket="), CapturePocket);
        const FName PocketTag(*FString::Printf(TEXT("Fernhall.Outdoor.%d"), FMath::Clamp(CapturePocket, 0, 2)));
        for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It)
        {
            if (!It->Tags.Contains(PocketTag)) continue;
            const FVector Eye = It->GetActorLocation() - Fwd * 1500.0f + FVector(0, 0, 100);
            Vantages.Reset();
            Vantages.Add({ Eye, (It->GetActorLocation() - Eye).Rotation() });
            break;
        }
    }

    // 3. The entry yard's rift door, from the ground a player approaches it on.
    Vantages.Add({ B - Fwd * 2200.0f + FVector(0.0f, 0.0f, 200.0f), FRotator(-2.0f, Yaw, 0.0f) });

    // The substation yard, if this zone has one. Derived, so a zone with only
    // an entry yard simply gets the three vantages above.
    for (const FBreakerZoneMarker& Marker : Markers.All)
    {
        if (Marker.Role != EBreakerZoneMarkerRole::Yard) continue;

        const FBreakerZoneMarker* FarRift = Markers.Find(EBreakerZoneMarkerRole::Rift, Marker.Yard);
        const FVector YA = Marker.Location;
        const FVector YFwd = FarRift ? (FarRift->Location - YA).GetSafeNormal2D() : Fwd;
        const float YYaw = YFwd.Rotation().Yaw;

        // 4. THE BOUNDARY BETWEEN THE TWO YARDS from above — and this vantage
        //    is NOT the seam, which is the finding rather than the intent.
        //
        //    It was written as "the seam from above" and it cannot be: a
        //    connection has NO MARKER, so nothing in code can locate one. Its
        //    geometry lives in the composer and its terms live in
        //    FernhallConnections, with nothing tying the two together. The
        //    midpoint of the entry rift and the far yard's anchor lands at
        //    x 81 while the seam spans x 101-118, so this camera photographed
        //    the wall between the yards and called it the connection.
        //
        //    Relabelled rather than moved, because moving it would mean
        //    hardcoding a world coordinate the composer owns — the same
        //    unanchored-number shape that produced this. The fix is a marker
        //    for a connection's mouths, which is in the lane's report.
        const FVector Boundary = (B + YA) * 0.5f;
        Vantages.Add({ Boundary + FVector(0.0f, 0.0f, 7000.0f), FRotator(-89.9f, Yaw, 0.0f) });

        // 5. Standing in the seam's far mouth, looking into the yard it opens.
        Vantages.Add({ YA - YFwd * 1200.0f + FVector(0.0f, 0.0f, 200.0f), FRotator(-3.0f, YYaw, 0.0f) });

        // 6. The far yard's own rift door, the thing the walk is for.
        if (FarRift)
        {
            Vantages.Add({ FarRift->Location - YFwd * 2200.0f + FVector(0.0f, 0.0f, 200.0f),
                FRotator(-2.0f, YYaw, 0.0f) });
        }
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureCourtyard")))
    {
        TArray<FBreakerZonePiece> Pieces;
        BreakerFernhallCourtyard::FPlan Plan;
        FString Error;
        if (UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(), Pieces)
            && BreakerFernhallCourtyard::MakePlan(Pieces, Plan, Error))
        {
            Vantages.Reset();
            for (const FVector& Eye : {Plan.At(-650,0,100), Plan.At(1600,2050,100), Plan.At(3300,5500,100)})
            {
                const FVector Target = Eye.Equals(Plan.At(-650,0,100)) ? Plan.At(1600,0,100) : Plan.At(3000,3900,150);
                Vantages.Add({Eye, (Target - Eye).Rotation()});
            }
        }
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BreakerCapturePerimeter")))
    {
        FString UserDirectory;
        if (!FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureScenery"))
            || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory)
            || UserDirectory.IsEmpty() || FPaths::IsRelative(UserDirectory)
            || FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(UserDirectory), FPaths::ProjectDir())
            || IFileManager::Get().DirectoryExists(*(FPaths::ProjectSavedDir() / TEXT("SaveGames"))))
        {
            UE_LOG(LogTemp, Warning, TEXT("[BreakerCapture] Perimeter refused: Scenery and fresh isolated absolute UserDir required."));
            return;
        }
        TArray<FBreakerZonePiece> Pieces;
        BreakerFernhallCourtyard::FPlan Plan;
        FString Error;
        if (!UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(), Pieces)
            || !BreakerFernhallCourtyard::MakePlan(Pieces, Plan, Error)) return;
        auto Bounds = [&](const TCHAR* Name, FBox& Out)
        {
            const auto* Piece = Pieces.FindByPredicate([&](const FBreakerZonePiece& P) { return P.Name == Name; });
            auto* Mesh = Piece ? Cast<UStaticMesh>(Piece->MeshPath.TryLoad()) : nullptr;
            if (!Mesh) return false;
            Out = Mesh->GetBoundingBox(); return true;
        };
        FBox Entry, Sub;
        if (!Bounds(TEXT("flr_yard"), Entry) || !Bounds(TEXT("flr_yard_sub"), Sub)) return;
        const float Side = Entry.GetCenter().Y >= Sub.GetCenter().Y ? 1.f : -1.f;
        const auto* Defaults = GetDefault<ABreakerCharacter>();
        const auto* Capsule = Defaults->GetCapsuleComponent();
        const auto* Camera = Defaults->FindComponentByClass<UCameraComponent>();
        if (!Capsule || !Camera) return;
        Vantages.Reset();
        auto StandingView = [&](const TCHAR* Label, FVector Ground, const FVector& Target)
        {
            // O2 PLACEHOLDER capture offsets. Existing floor and capsule clearance
            // determine standing height; no geometry is moved to serve the shot.
            FCollisionQueryParams Query(SCENE_QUERY_STAT(PerimeterCaptureFloor), false);
            if (auto* PC = GetWorld()->GetFirstPlayerController()) Query.AddIgnoredActor(PC->GetPawn());
            FHitResult Floor;
            if (!GetWorld()->LineTraceSingleByObjectType(Floor, Ground+FVector(0,0,200), Ground-FVector(0,0,200),
                FCollisionObjectQueryParams(ECC_WorldStatic), Query)) return false;
            const FVector At = Floor.ImpactPoint+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+2.f);
            if (GetWorld()->OverlapBlockingTestByChannel(At,FQuat::Identity,ECC_Pawn,
                FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query)) return false;
            FRotator Aim=(Target-At).Rotation();
            for (int32 Pass=0;Pass<2;++Pass)
                Aim=(Target-(At+Aim.RotateVector(Camera->GetRelativeLocation()))).Rotation();
            Vantages.Add({At,Aim});
            UE_LOG(LogTemp,Display,TEXT("[BreakerCapture] perimeter %s standing=%s target=%s"),Label,*At.ToString(),*Target.ToString());
            return true;
        };
        FVector EntryGround=Start->Location; EntryGround.Z=Entry.Max.Z;
        FVector GroveGround=Entry.GetCenter()+FVector(-Entry.GetExtent().X*.5f,0,0); GroveGround.Z=Entry.Max.Z;
        FVector PipesGround=Sub.GetCenter()+FVector(-Sub.GetExtent().X*.4f,0,0); PipesGround.Z=Sub.Max.Z;
        const bool bEntry=StandingView(TEXT("entry-grove"),EntryGround,Entry.GetCenter()+FVector(0,Side*Entry.GetExtent().Y,700));
        const bool bGrove=StandingView(TEXT("grove-medium"),GroveGround,Entry.GetCenter()+FVector(0,Side*(Entry.GetExtent().Y+1700),1000));
        const bool bPipes=StandingView(TEXT("pipeworks-medium"),PipesGround,Sub.GetCenter()+FVector(0,-Side*(Sub.GetExtent().Y+2400),1600));
        // Aim at the real first turn, not through the wall across the dogleg.
        const bool bCourt=Plan.RoutePoints.Num()>=2 && StandingView(TEXT("courtyard-entrance"),Plan.RoutePoints[0],Plan.RoutePoints[1]+FVector(0,0,100));
        if (!bEntry || !bGrove || !bPipes || !bCourt)
        {
            UE_LOG(LogTemp,Warning,TEXT("[BreakerCapture] Perimeter refused incomplete standing route: entry=%d grove=%d pipes=%d courtyard=%d."),bEntry,bGrove,bPipes,bCourt);
            return;
        }
    }
    for (const FVantage& Vantage : Vantages)
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        if (ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
            Vantage.Location, Vantage.Rotation, Params))
        {
            TourCameras.Add(Camera);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] zone tour: %d vantages from %d markers."),
        TourCameras.Num(), Markers.All.Num());
}

void ABreakerGameMode::BuildCaptureTour()
{
    // -BreakerCaptureTour points the capture at the field instead of at the
    // player's eyes. The spawn view is one composition and a LAYOUT is not
    // visible from inside it; this pass changes the layout, so the harness has
    // to be able to see the layout or it verifies nothing. Vantage points are
    // derived from the same station constants the field is built from, so they
    // move when the field moves.
    if (!GetWorld() || !bFieldFrameSet) return;
    if (!FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureTour"))) return;

    struct FVantage { FVector Location; FRotator Rotation; };
    const float Mid = FieldForwardExtent * 0.4f;
    const TArray<FVantage> Vantages =
    {
        // 1. Straight down over the middle of the field: the plan view.
        { Frame.At(Mid, 0.0f, 16000.0f), FRotator(-89.9f, Frame.Forward.Rotation().Yaw, 0.0f) },
        // 2. Behind and above the camp looking out along the forward axis —
        //    the whole route, camp to arena, in one frame.
        { Frame.At(-FieldRearExtent - 2000.0f, 0.0f, 5200.0f), FRotator(-17.0f, Frame.Forward.Rotation().Yaw, 0.0f) },
        // 3. Standing on the breach crest, the vista the ramp exists to buy.
        { Frame.At(2100.0f, 0.0f, BreachCrestHeight + 170.0f), FRotator(-4.0f, Frame.Forward.Rotation().Yaw, 0.0f) },
        // 4. Oblique over the encounter pocket, to read pocket radius against
        //    the enemies actually standing in it.
        { Frame.At(EncounterPocketDistance - CombatPocketRadius * 2.0f, -CombatPocketRadius, 2600.0f), FRotator(-26.0f, Frame.Forward.Rotation().Yaw + 38.0f, 0.0f) },
        // 5. Down the wall-ride corridor at ride height.
        { Frame.At(EncounterPocketDistance - 3200.0f, FieldHalfExtent * 0.62f, 320.0f), FRotator(-3.0f, Frame.Forward.Rotation().Yaw, 0.0f) },
        // 6. Along the sniper lane from the firing line.
        { Frame.At(RangeFiringLineDistance - 1200.0f, -FieldHalfExtent * 0.62f, 260.0f), FRotator(-2.0f, Frame.Forward.Rotation().Yaw, 0.0f) },
        // 7. Oblique over the ELITE ARENA. Added because the boss lives there
        //    and nothing pointed at it: the Field Marshal's galleries reach
        //    ±1900 cm against a 2000 cm pocket radius, and whether its orders
        //    point at open ground or into the pocket's ruin arc is a question
        //    only a picture answers.
        { Frame.At(ArenaDistance - CombatPocketRadius * 1.6f, -CombatPocketRadius * 1.3f, 2200.0f), FRotator(-24.0f, Frame.Forward.Rotation().Yaw + 34.0f, 0.0f) },
        // 8. THE GROUND ITSELF, at a grazing angle (owner: "a lot of the
        //    textures on the ground were tearing"). Z-fighting is invisible in a
        //    plan view and invisible from head height facing a wall; it needs a
        //    shallow angle across a large flat, and it gets worse with distance
        //    as depth precision falls off. This vantage stands over the jump-gap
        //    trench — whose floor was authored at the SAME top height as the
        //    apron under it — and looks out along 150 m of tint-patched apron,
        //    so both coplanar populations are in one frame.
        { Frame.At(EncounterPocketDistance + CombatPocketRadius + 2400.0f, 0.0f, 700.0f), FRotator(-11.0f, Frame.Forward.Rotation().Yaw, 0.0f) },
    };

    for (const FVantage& Vantage : Vantages)
    {
        if (ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Vantage.Location, Vantage.Rotation))
        {
            if (UCameraComponent* Component = Camera->GetCameraComponent())
            {
                Component->SetFieldOfView(90.0f);
            }
            Camera->SetActorLabel(TEXT("Runtime_TourCamera"));
            TourCameras.Add(Camera);
        }
    }
}

void ABreakerGameMode::CaptureScreenshot()
{
    // FScreenshotRequest rather than the HighResShot console command: under
    // -unattended the console exec produced no file and no error, which is the
    // worst possible outcome for a verification tool -- it would have reported
    // success while capturing nothing. bShowUI TRUE is the load-bearing
    // argument; without it the capture omits Slate, and the menus are half of
    // what needs looking at.
    const FString Path = FPaths::ProjectSavedDir() / TEXT("Screenshots") /
        FString::Printf(TEXT("breaker_%02d.png"), ScreenshotIndex);
    bScreenshotPending = true;
    ScreenshotProcessedHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddWeakLambda(this, [this]()
    {
        FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessedHandle);
        ScreenshotProcessedHandle.Reset();
        FinishScreenshot();
    });
    FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
    UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] shot %d -> %s"), ScreenshotIndex, *Path);
}

void ABreakerGameMode::FinishScreenshot()
{
    // The viewport has consumed this request. Moving before this callback can
    // put the next viewpoint into the current file despite correct log indexes.
    bScreenshotPending = false;
    NextScreenshotTime = FPlatformTime::Seconds() + FMath::Max(0.1f, ScreenshotIntervalSeconds);
    ++ScreenshotIndex;

    if (--ScreenshotsRemaining > 0)
    {
        // Move only after the saved frame, then allow a full settling interval.
        if (TourCameras.Num() > 0)
        {
            if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
            {
                int32 StartVantage = 0;
                FParse::Value(FCommandLine::Get(), TEXT("BreakerCaptureTourStart="), StartVantage);
                const int32 VantageIndex = (FMath::Max(0, StartVantage) + ScreenshotIndex - 1) % TourCameras.Num();
                AActor* Vantage = TourCameras[VantageIndex];

                // THE TOUR MOVES THE PAWN, NOT A FREE CAMERA (ruled). It used
                // to SetViewTarget onto the vantage actor and leave the pawn
                // behind, which broke an invariant the shipping game relies on:
                // the enemy bar culls at 50 m FROM THE PAWN, and in the real
                // game the pawn and the camera are the same point
                // (FirstPersonCamera is a component on the character). A
                // vantage standing among enemies with the pawn 60 m away
                // therefore culled every bar, and the capture read as a bar
                // defect that did not exist.
                //
                // The alternative was widening the cull to camera-space, which
                // would be changing SHIPPING behaviour to serve an instrument
                // — the mirror of never narrowing an instrument to make a cycle
                // pass. Moving the pawn keeps the invariant true everywhere,
                // including inside the thing that was breaking it.
                if (APawn* Pawn = PC->GetPawn())
                {
                    Pawn->TeleportTo(Vantage->GetActorLocation(), Vantage->GetActorRotation());
                    if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) Movement->StopMovementImmediately();
                    PC->SetControlRotation(Vantage->GetActorRotation());
                    PC->SetViewTarget(Pawn);
                    // Frozen scenery tours still need a fresh camera cache after moving the pawn.
                    if (FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureScenery")) && PC->PlayerCameraManager)
                        PC->PlayerCameraManager->UpdateCamera(0.0f);
                }
                else
                {
                    // No pawn is a capture of a menu or the front end, where a
                    // vantage means nothing anyway; fall back rather than skip
                    // the shot silently.
                    PC->SetViewTarget(Vantage);
                }
                UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] next shot %d from vantage %d (pawn moved)"),
                    ScreenshotIndex, VantageIndex);
            }
        }
        return;
    }

    // Quit on a real-time delay so the last shot finishes writing. Real time
    // again, for the same pause reason.
    const double QuitAt = FPlatformTime::Seconds() + 2.5;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([QuitAt](float) -> bool
    {
        if (FPlatformTime::Seconds() < QuitAt) return true;
        UE_LOG(LogTemp, Display, TEXT("[BreakerCapture] done."));
        FPlatformMisc::RequestExit(false);
        return false;
    }), 0.0f);
}

// One line stating what the gym actually built. This exists for the
// headless smoke run (-BreakerAutoPlay): a log with no gym line means the
// encounter never spawned, which is otherwise indistinguishable from a
// system that simply logs nothing. It is also the fastest way for the owner
// to confirm the area level a session was actually played at.
void ABreakerGameMode::LogGymSummary() const
{
    const UWorld* World = GetWorld();
    if (!World) return;
    // Counted by CLASS as well as by telemetry bucket. The summary line used to
    // say "melee N | ranged N", which was true and useless the moment four
    // archetypes existed: a Warden and a Skitter are both "melee" and a
    // Skirmisher and a Lattice are both "ranged", so the one line that proves
    // the gym spawned what it meant to could not tell them apart. A headless
    // smoke run reads this line to confirm the integration, so it has to name
    // every archetype it is asserting.
    int32 Melee = 0;
    int32 Ranged = 0;
    int32 Wardens = 0;
    int32 Skirmishers = 0;
    int32 Drudges = 0;
    int32 Lattices = 0;
    int32 Bosses = 0;
    int32 ModifierBearing = 0;
    int32 Elites = 0;
    int32 ModifierTotal = 0;
    TArray<FVector> AnchorLocations;
    TArray<FVector> RangedLocations;
    for (TActorIterator<ABreakerEnemy> It(const_cast<UWorld*>(World)); It; ++It)
    {
        if (It->IsRangedForTelemetry()) ++Ranged; else ++Melee;
        // Boss first: it SUBCLASSES the Warden (§3.1 "a Warden that commands
        // the other three archetypes"), so an unordered cast chain would count
        // the Field Marshal as a Warden and silently break the §5.3 cap check.
        if (It->IsA<ABreakerBossEnemy>()) { ++Bosses; AnchorLocations.Add(It->GetActorLocation()); }
        else if (It->IsA<ABreakerWardenEnemy>()) { ++Wardens; AnchorLocations.Add(It->GetActorLocation()); }
        else if (It->IsA<ABreakerSkirmisherEnemy>()) { ++Skirmishers; RangedLocations.Add(It->GetActorLocation()); }
        else if (It->IsA<ABreakerRangedEnemy>()) { ++Lattices; RangedLocations.Add(It->GetActorLocation()); }
        // Counted by name because it is otherwise indistinguishable from a
        // Skitter in this line, and "is the new archetype actually in the
        // world" is exactly the question a headless smoke run asks. Not a
        // Warden-class anchor: it has no frontal armour and no shield, so it
        // does not enter the 5.3 anchor cap.
        else if (It->IsA<ABreakerAlteredEnemy>()) { ++Drudges; }
        if (It->IsElite()) ++Elites;
        if (const UBreakerEnemyModifierComponent* Modifiers = It->GetModifierComponent();
            Modifiers && Modifiers->GetModifierCount() > 0)
        {
            ++ModifierBearing;
            ModifierTotal += Modifiers->GetModifierCount();
        }
    }
    int32 Targets = 0;
    for (TActorIterator<ABreakerTargetDummy> It(const_cast<UWorld*>(World)); It; ++It) ++Targets;
    UE_LOG(LogTemp, Display,
        TEXT("[BreakerGym] area level %d | melee %d | ranged %d | skitter/other %d | lattice %d | warden %d | skirmisher %d | drudge %d | boss %d | elite %d | modifier-bearing %d (%d modifiers) | target dummies %d"),
        GymAreaLevel, Melee, Ranged,
        Melee + Ranged - Lattices - Wardens - Skirmishers - Bosses - Drudges,
        Lattices, Wardens, Skirmishers, Drudges, Bosses, Elites, ModifierBearing, ModifierTotal, Targets);
    // The §5.3 caps, asserted rather than assumed. They are the difference
    // between "dense" and "unplayable", and this check has already earned its
    // keep once: it caught two Skirmishers standing alongside two Lattices in
    // the encounter, which is four converging projectile sources against a cap
    // of three.
    //
    // Counted PER ENCOUNTER, not per world. The caps are about what is in one
    // fight — the field holds a standing encounter at 8500 cm and an arena at
    // 17000, and a Warden in one plus the Field Marshal in the other is two
    // separate fights, not an illegal one. Proximity is the only definition of
    // "one fight" available here, and one combat pocket's diameter is the
    // honest radius for it.
    const float EncounterRadius = CombatPocketRadius * 2.0f;
    auto WarnOnCrowding = [&](const TArray<FVector>& Locations, int32 Cap, const TCHAR* What, const TCHAR* Reason)
    {
        for (const FVector& Centre : Locations)
        {
            int32 Nearby = 0;
            for (const FVector& Other : Locations)
            {
                if (FVector::DistSquared2D(Centre, Other) <= FMath::Square(EncounterRadius)) ++Nearby;
            }
            if (Nearby > Cap)
            {
                UE_LOG(LogTemp, Warning, TEXT("[BreakerGym] %d %s within one encounter; Encounter-Design 5.3 caps them at %d — %s"),
                    Nearby, What, Cap, Reason);
                return;
            }
        }
    };
    WarnOnCrowding(AnchorLocations, 1, TEXT("Warden-class anchors"),
        TEXT("overlapping frontal-armour anchors create unsolvable geometry."));
    WarnOnCrowding(RangedLocations, 3, TEXT("ranged sources"),
        TEXT("four converging projectile sources removes all safe ground."));

    // The stock First Person template geometry is the other half of the "map
    // scope" complaint and it can only be removed in the editor. Measuring it
    // from here is what turns "the template crowds the field" into an
    // actionable delete list: every non-runtime static mesh actor in the map,
    // with the combined footprint it occupies around the spawn.
    FBox TemplateBounds(ForceInit);
    int32 TemplateActors = 0;
    for (TActorIterator<AStaticMeshActor> It(const_cast<UWorld*>(World)); It; ++It)
    {
        if (It->GetActorLabel().StartsWith(TEXT("Runtime_"))) continue;
        const FBox Box = It->GetComponentsBoundingBox(true);
        // Skybox/backdrop meshes are effectively infinite and would swallow the
        // measurement; only the playable shell is interesting here.
        if (Box.GetSize().GetMax() > 50000.0f) continue;
        UE_LOG(LogTemp, Display, TEXT("[BreakerGymTemplate] %s | min (%.0f %.0f %.0f) max (%.0f %.0f %.0f)"),
            *It->GetActorLabel(), Box.Min.X, Box.Min.Y, Box.Min.Z, Box.Max.X, Box.Max.Y, Box.Max.Z);
        TemplateBounds += Box;
        ++TemplateActors;
    }
    if (TemplateActors > 0)
    {
        const FVector Size = TemplateBounds.GetSize();
        const FVector Centre = TemplateBounds.GetCenter();
        UE_LOG(LogTemp, Display,
            TEXT("[BreakerGym] pre-placed (template) static meshes: %d | bounds %.0f x %.0f x %.0f cm | centre (%.0f, %.0f, %.0f) | min (%.0f, %.0f, %.0f) max (%.0f, %.0f, %.0f)"),
            TemplateActors, Size.X, Size.Y, Size.Z, Centre.X, Centre.Y, Centre.Z,
            TemplateBounds.Min.X, TemplateBounds.Min.Y, TemplateBounds.Min.Z,
            TemplateBounds.Max.X, TemplateBounds.Max.Y, TemplateBounds.Max.Z);
    }
    if (const APawn* Pawn = World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr)
    {
        const FVector P = Pawn->GetActorLocation();
        UE_LOG(LogTemp, Display, TEXT("[BreakerGym] spawn (%.0f, %.0f, %.0f) facing (%.2f, %.2f)"),
            P.X, P.Y, P.Z, Pawn->GetActorForwardVector().X, Pawn->GetActorForwardVector().Y);
    }
}

namespace
{
    // --- Overgrown-Earth palette (O24) -------------------------------------
    // Ground/course blocks: mossy greens and desaturated earth.
    // Ruins/walls: weathered concrete grey-greens.
    // Tech props: dim amber and off-white.
    // Saturated teal is RESERVED for rift/suppression OBJECTS only
    // ("saturated teal is a property of objects, not of damage").
    const FLinearColor PaletteMoss       (0.14f, 0.26f, 0.11f);
    const FLinearColor PaletteFoliage    (0.09f, 0.19f, 0.09f);
    const FLinearColor PaletteDryGrass   (0.30f, 0.32f, 0.15f);
    const FLinearColor PaletteEarth      (0.20f, 0.16f, 0.11f);
    const FLinearColor PaletteConcrete   (0.33f, 0.35f, 0.30f);
    const FLinearColor PaletteStone      (0.24f, 0.26f, 0.23f);
    const FLinearColor PaletteRust       (0.34f, 0.20f, 0.09f);
    const FLinearColor PaletteAmber      (0.46f, 0.29f, 0.08f);
    const FLinearColor PaletteOffWhite   (0.58f, 0.57f, 0.51f);
    const FLinearColor PaletteRiftTeal   (0.03f, 0.72f, 0.66f);   // reserved

    const TCHAR* ShapeCube     = TEXT("/Engine/BasicShapes/Cube.Cube");
    const TCHAR* ShapeSphere   = TEXT("/Engine/BasicShapes/Sphere.Sphere");
    const TCHAR* ShapeCylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
    const TCHAR* ShapeCone     = TEXT("/Engine/BasicShapes/Cone.Cone");
    // A single upward quad with NO side walls. Ground tinting has to be laid
    // above the apron to avoid coplanarity, and a lifted CUBE pays for that
    // with a vertical lip whose shaded face is sub-pixel at field distances and
    // aliases into a dashed dark line tracing every patch outline — which is
    // most of what the ground-tearing report was looking at. A plane has no lip
    // to alias.
    const TCHAR* ShapePlane    = TEXT("/Engine/BasicShapes/Plane.Plane");

    // The stock basic-shape material exposes a single "Color" vector param, so
    // one dynamic instance per primitive is all the palette needs — no assets.
    void ApplyShapeColor(UStaticMeshComponent* Mesh, const FLinearColor& Color)
    {
        if (!Mesh) return;
        UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!BaseMaterial) return;
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
            Mesh->SetMaterial(0, Dynamic);
        }
    }

    AStaticMeshActor* SpawnShape(UWorld* World, const TCHAR* ShapePath, const FVector& Location, const FVector& Scale,
        const FRotator& Rotation, const FLinearColor& Color, bool bCollides, const TCHAR* Label)
    {
        if (!World) return nullptr;
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, Rotation);
        if (!Actor) return nullptr;
        UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, ShapePath));
        Mesh->SetWorldScale3D(Scale);
        ApplyShapeColor(Mesh, Color);
        if (!bCollides) Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // Dressing never needs to tick or update after placement.
        Mesh->SetMobility(EComponentMobility::Static);
        Actor->SetActorEnableCollision(bCollides);
        Actor->SetActorTickEnabled(false);
        Actor->SetActorLabel(Label);
        return Actor;
    }

    AStaticMeshActor* SpawnGymBlock(UWorld* World, const FVector& Location, const FVector& Scale,
        const FRotator& Rotation = FRotator::ZeroRotator, const FLinearColor& Color = PaletteConcrete)
    {
        return SpawnShape(World, ShapeCube, Location, Scale, Rotation, Color, true, TEXT("Runtime_PlaytestFacility"));
    }

    // A rectangle of ground or wall authored in FIELD units — forward/right
    // extents in cm and a top surface height — rather than in mesh scale.
    // Level-design numbers are dimensions, and a spawner that takes 0.3 when
    // the design says "30 cm thick" is how a derived grammar stops being
    // checkable against the code.
    AStaticMeshActor* SpawnFieldSlab(UWorld* World, const ABreakerGameMode::FFieldFrame& Frame,
        float FwdMin, float FwdMax, float RgtMin, float RgtMax,
        float TopZ, float Thickness, const FLinearColor& Color, const TCHAR* Label, bool bCollides = true)
    {
        const float FwdSize = FMath::Max(FwdMax - FwdMin, 1.0f);
        const float RgtSize = FMath::Max(RgtMax - RgtMin, 1.0f);
        const FVector Centre = Frame.At((FwdMin + FwdMax) * 0.5f, (RgtMin + RgtMax) * 0.5f, TopZ - Thickness * 0.5f);
        return SpawnShape(World, ShapeCube, Centre,
            FVector(FwdSize / 100.0f, RgtSize / 100.0f, Thickness / 100.0f),
            Frame.Forward.Rotation(), Color, bCollides, Label);
    }

    // An inclined slab whose TOP SURFACE runs from (FwdA, HeightA) to
    // (FwdB, HeightB). Ramps are the one piece of level geometry where getting
    // the trigonometry slightly wrong produces a step the player trips on, so
    // the caller states the two endpoints and never a pitch.
    AStaticMeshActor* SpawnFieldRamp(UWorld* World, const ABreakerGameMode::FFieldFrame& Frame,
        float FwdA, float HeightA, float FwdB, float HeightB, float RgtCentre, float Width,
        float Thickness, const FLinearColor& Color, const TCHAR* Label)
    {
        const float Run = FwdB - FwdA;
        const float Rise = HeightB - HeightA;
        const float Length = FMath::Sqrt(Run * Run + Rise * Rise);
        const float PitchDegrees = FMath::RadiansToDegrees(FMath::Atan2(Rise, Run));
        const FRotator Rotation = FRotator(PitchDegrees, Frame.Forward.Rotation().Yaw, 0.0f);
        // Drop the centre by half the thickness along the slab's own normal so
        // the TOP lands on the authored line rather than the mid-plane.
        const FVector Up = Rotation.RotateVector(FVector::UpVector);
        const FVector Centre = Frame.At((FwdA + FwdB) * 0.5f, RgtCentre, (HeightA + HeightB) * 0.5f) - Up * (Thickness * 0.5f);
        return SpawnShape(World, ShapeCube, Centre,
            FVector(Length / 100.0f, Width / 100.0f, Thickness / 100.0f), Rotation, Color, true, Label);
    }

    // Small warm/cool point light bolted onto a prop. Movable because runtime
    // spawns cannot participate in baked lighting; radius and intensity are
    // kept low so the six-light budget stays cheap.
    void AttachPropLight(AActor* Owner, const FVector& RelativeOffset, const FLinearColor& Color, float Intensity, float Radius)
    {
        if (!Owner) return;
        UPointLightComponent* Light = NewObject<UPointLightComponent>(Owner);
        if (!Light) return;
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetupAttachment(Owner->GetRootComponent());
        Light->SetRelativeLocation(RelativeOffset);
        Light->SetLightColor(Color);
        Light->SetIntensity(Intensity);
        Light->SetAttenuationRadius(Radius);
        Light->SetCastShadows(false);
        Light->RegisterComponent();
    }
}

void ABreakerGameMode::SpawnPlaytestTargets()
{
    if (!GetWorld() || !bFieldFrameSet) return;

    // The range moved OUT of the template courtyard. It used to start 1200 cm
    // from the spawn, which put the first two dummies behind the template's own
    // ramps and the third inside the perimeter wall — the "HEALTH 12m" label in
    // the before-shot is pointing at geometry the player cannot shoot through.
    // The firing line is now past the breach, and the four ranges are unchanged
    // relative to it so every falloff reading taken so far still compares.
    const float Line = RangeFiringLineDistance;
    const float Ranges[] = { 1200.0f, 2400.0f, 4500.0f, 2100.0f };
    const float Laterals[] = { -300.0f, 350.0f, 0.0f, -850.0f };
    const EBreakerTargetProfile Profiles[] =
    {
        EBreakerTargetProfile::Health,
        EBreakerTargetProfile::Shielded,
        EBreakerTargetProfile::Armored,
        EBreakerTargetProfile::Moving
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ranges); ++Index)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        const FVector Location = Frame.At(Line + Ranges[Index], Laterals[Index], 120.0f);
        if (ABreakerTargetDummy* Target = GetWorld()->SpawnActor<ABreakerTargetDummy>(ABreakerTargetDummy::StaticClass(), Location, FRotator::ZeroRotator, Params))
        {
            Target->ConfigureProfile(Profiles[Index]);
        }
    }
    bPlaytestTargetsSpawned = true;
}

void ABreakerGameMode::SpawnMovementCourse()
{
    if (!GetWorld() || !bFieldFrameSet) return;
    UWorld* World = GetWorld();

    // --- The rubble stair: the redundant way out of the courtyard ----------
    // Steps a hand under MantleStepHeight so the whole climb is mantle-able
    // and needs no jump at all (master sheet 5.4: the conventional route is
    // never punished). THE RISER SITS 10 CM OFF THE CEILING (D4, owner-ruled):
    // a riser authored AT the exact 145 made every step a float coin-flip
    // between mantle and wall — the measured height lands either side of the
    // edge by transform noise — so authored tops now stay clear of the band
    // edges and the verb is decided by design, not by the trace's float error.
    // Four 135 cm risers reach 540 cm, which still clears the template's
    // 400 cm parapet with the top step landing ON it. Placed to the left of
    // the breach and climbing along the same forward axis, so the courtyard
    // offers two ways out that read differently: a ramp you keep speed on and
    // a stair you climb.
    const float MantleRiser = MantleStepHeight - 10.0f;   // decisively inside the band (D4)
    const float StairRight = -1200.0f;
    for (int32 Step = 0; Step < 4; ++Step)
    {
        const float Top = MantleRiser * (Step + 1);
        SpawnFieldSlab(World, Frame, 900.0f + Step * 250.0f, 1150.0f + Step * 250.0f,
            StairRight - 450.0f, StairRight + 450.0f, Top, Top, PaletteEarth, TEXT("Runtime_RubbleStair"));
    }
    // Outer side, sloping back down to the apron. Starts past the wall's inner
    // face so the descent clears the 400 cm crest: at X 1950 it is still at 522.
    SpawnFieldRamp(World, Frame, 1900.0f, MantleRiser * 4.0f, 3400.0f, 0.0f,
        StairRight, 900.0f, 60.0f, PaletteEarth, TEXT("Runtime_RubbleStair"));

    // --- Dash reach markers -------------------------------------------------
    // Posts every 500 cm along a clear lane. What they measure is how far one
    // dash carries: a dash floors horizontal speed at 1700 cm/s and holds it
    // while input is held, so the honest reading is posts-per-second, not a
    // fixed distance. Nine posts covers 4000 cm, just under one
    // DashRefreshDistance, so the lane is exactly "how much ground one dash
    // window buys you".
    const float DashLaneRight = -DashCorridorWidth * 1.6f;
    for (int32 Marker = 0; Marker <= 8; ++Marker)
    {
        const float Fwd = RangeFiringLineDistance - 800.0f + Marker * 500.0f;
        SpawnShape(World, ShapeCylinder, Frame.At(Fwd, DashLaneRight, 90.0f),
            FVector(0.12f, 0.12f, 1.8f), FRotator::ZeroRotator,
            Marker % 2 == 0 ? PaletteOffWhite : PaletteStone, false, TEXT("Runtime_DashMarker"));
    }

    // --- Wall-ride corridor -------------------------------------------------
    // Two pairs down the right flank. Length WallRideWallLength (three full
    // 935 cm rides), gap WallRideCorridorWidth. The gap is the fix: the shipped
    // field used 700 cm, and a wall jump leaving at 650 cm/s needs 1.08 s to
    // cross that against roughly 0.85 s of usable air, so the gym's own wall
    // lane could not be chained on the gym's own numbers.
    const float WallLaneRight = FieldHalfExtent * 0.62f;
    for (int32 Pair = 0; Pair < 2; ++Pair)
    {
        // Pairs are separated by one DashRefreshDistance so getting from the
        // end of one ride to the start of the next is a dash decision.
        const float Base = EncounterPocketDistance - 4200.0f + Pair * (WallRideWallLength + DashRefreshDistance);
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const float Lateral = WallLaneRight + (Side == 0 ? -WallRideCorridorWidth : WallRideCorridorWidth) * 0.5f;
            SpawnFieldSlab(World, Frame, Base, Base + WallRideWallLength,
                Lateral - 15.0f, Lateral + 15.0f, WallRideWallHeight, WallRideWallHeight,
                PaletteConcrete, TEXT("Runtime_WallRideWall"));
        }
        // A run-up approach: the entry gate is 450 cm/s of ALONG-WALL speed, so
        // the player needs room to be at sprint before the first wall.
        SpawnFieldSlab(World, Frame, Base - 1400.0f, Base, WallLaneRight - 500.0f, WallLaneRight + 500.0f,
            8.0f, 24.0f, PaletteDryGrass, TEXT("Runtime_WallRideApproach"));
    }

    // --- Flat slide lane ----------------------------------------------------
    // A slide is duration-capped at SlideMaxDuration 1.0 s and entered at
    // sprint, so it covers roughly 1000 cm before it drops under SlideExitSpeed.
    // The lane is exactly that long with a stripe at the midpoint, so the
    // player can see where their slide actually ended instead of guessing.
    const float SlideLaneRight = -FieldHalfExtent * 0.30f;
    SpawnFieldSlab(World, Frame, RangeFiringLineDistance - 1500.0f, RangeFiringLineDistance - 500.0f,
        SlideLaneRight - SprintCorridorWidth * 0.5f, SlideLaneRight + SprintCorridorWidth * 0.5f,
        14.0f, 28.0f, PaletteEarth, TEXT("Runtime_SlideLane"));
    SpawnFieldSlab(World, Frame, RangeFiringLineDistance - 1010.0f, RangeFiringLineDistance - 990.0f,
        SlideLaneRight - SprintCorridorWidth * 0.5f, SlideLaneRight + SprintCorridorWidth * 0.5f,
        16.0f, 28.0f, PaletteAmber, TEXT("Runtime_SlideLane"));

    // --- Watchtowers --------------------------------------------------------
    // Moved out of the courtyard (they used to sit inside the template wall)
    // and onto the range shoulders, where they are what they were named for:
    // a firing perch overlooking the target line, reachable by two jumps off
    // the stack under them.
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const float Lateral = (Side == 0 ? -1.0f : 1.0f) * (CombatPocketRadius + 1000.0f);
        const float Height = 240.0f + Side * 100.0f;
        SpawnFieldSlab(World, Frame, RangeFiringLineDistance + 200.0f, RangeFiringLineDistance + 800.0f,
            Lateral - 300.0f, Lateral + 300.0f, Height, 30.0f, PaletteConcrete, TEXT("Runtime_Watchtower"));
        SpawnFieldSlab(World, Frame, RangeFiringLineDistance + 400.0f, RangeFiringLineDistance + 600.0f,
            Lateral - 100.0f, Lateral + 100.0f, Height - 30.0f, Height - 30.0f, PaletteStone, TEXT("Runtime_Watchtower"));
        // Mantle-height stack so the perch has a conventional route up — the
        // same 10-under-the-ceiling riser as the rubble stair (D4): a step
        // authored AT the 145 edge was a mantle-or-wall coin flip.
        for (int32 Step = 0; Step < 2; ++Step)
        {
            const float Top = (MantleStepHeight - 10.0f) * (Step + 1);
            SpawnFieldSlab(World, Frame, RangeFiringLineDistance - 100.0f - Step * 250.0f, RangeFiringLineDistance + 150.0f - Step * 250.0f,
                Lateral - 250.0f, Lateral + 250.0f, Top, Top, PaletteEarth, TEXT("Runtime_Watchtower"));
        }
    }
}

void ABreakerGameMode::SpawnJumpGapRun()
{
    if (!GetWorld() || !bFieldFrameSet) return;
    UWorld* World = GetWorld();

    // Three crossings of one trench, each sized so the verb it needs is the
    // only verb that clears it. Pips in the near kerb count the jumps: one,
    // two, three. This is the piece of the field that makes the derivation
    // FALSIFIABLE — if OneJumpGap cannot be cleared with one jump, the
    // arithmetic in the header is wrong and the doc says so out loud.
    const float TrenchFwd = EncounterPocketDistance + CombatPocketRadius + 2200.0f;
    const float Gaps[] = { OneJumpGap, TwoJumpGap, SwiftThreeJumpGap };
    const float LandingDepth = 1600.0f;
    // Platforms are DashCorridorWidth wide so a player can arrive at speed
    // without threading a needle; a landing narrower than the turn radius is
    // where a gap stops being a jump and starts being a coin flip.
    const float PlatformWidth = DashCorridorWidth;

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Gaps); ++Index)
    {
        // Lanes are separated by two platform widths so an overshoot lands in
        // dirt, not in the neighbouring gap.
        const float Lateral = -PlatformWidth * 1.6f + Index * PlatformWidth * 1.6f;

        // Take-off platform, raised so the trench below reads as a trench.
        SpawnFieldSlab(World, Frame, TrenchFwd - LandingDepth, TrenchFwd,
            Lateral - PlatformWidth * 0.5f, Lateral + PlatformWidth * 0.5f,
            220.0f, 220.0f, PaletteEarth, TEXT("Runtime_JumpGap"));
        // Landing platform at the same height: a flat-to-flat gap is the only
        // one the airtime arithmetic actually describes.
        SpawnFieldSlab(World, Frame, TrenchFwd + Gaps[Index], TrenchFwd + Gaps[Index] + LandingDepth,
            Lateral - PlatformWidth * 0.5f, Lateral + PlatformWidth * 0.5f,
            220.0f, 220.0f, PaletteEarth, TEXT("Runtime_JumpGap"));
        // Amber lips on both edges. A gap you cannot see the edge of is a
        // reaction test, and the whole point of this run is that it is an
        // arithmetic test.
        SpawnFieldSlab(World, Frame, TrenchFwd - 40.0f, TrenchFwd,
            Lateral - PlatformWidth * 0.5f, Lateral + PlatformWidth * 0.5f, 226.0f, 12.0f,
            PaletteAmber, TEXT("Runtime_JumpGap"), false);
        SpawnFieldSlab(World, Frame, TrenchFwd + Gaps[Index], TrenchFwd + Gaps[Index] + 40.0f,
            Lateral - PlatformWidth * 0.5f, Lateral + PlatformWidth * 0.5f, 226.0f, 12.0f,
            PaletteAmber, TEXT("Runtime_JumpGap"), false);
        // Pip stones on the take-off lip: 1 / 2 / 3 jumps.
        for (int32 Pip = 0; Pip <= Index; ++Pip)
        {
            SpawnShape(World, ShapeCube,
                Frame.At(TrenchFwd - 180.0f, Lateral + (Pip - Index * 0.5f) * 140.0f, 260.0f),
                FVector(0.5f, 0.5f, 0.4f), FRotator::ZeroRotator, PaletteAmber, false, TEXT("Runtime_JumpGap"));
        }
    }
    // The trench floor. Deliberately a floor and not a pit: the drop is 220 cm,
    // well under the LandingHeavyFallSpeed threshold, so a failed jump costs
    // the climb back out and nothing else. Falling out of the world is not a
    // teaching tool.
    // Top at GroundOverlayLift, NOT at 0. It was 0, which is exactly the apron's
    // top height, and this slab sits ON the apron — two coplanar surfaces over
    // the whole trench, i.e. guaranteed z-fighting (owner: "a lot of the
    // textures on the ground were tearing"). The lift is centimetres: the drop
    // off the take-off platform goes 220 -> 214 cm, which changes no jump and
    // no landing band.
    SpawnFieldSlab(World, Frame, TrenchFwd, TrenchFwd + SwiftThreeJumpGap,
        -PlatformWidth * 2.6f, PlatformWidth * 2.6f, GroundOverlayLift, 30.0f, PaletteStone, TEXT("Runtime_JumpGap"));
    // Ramp out of the trench so a miss is recoverable without a jump.
    SpawnFieldRamp(World, Frame, TrenchFwd + SwiftThreeJumpGap, 0.0f, TrenchFwd + SwiftThreeJumpGap + 900.0f, 220.0f,
        PlatformWidth * 2.0f, 700.0f, 40.0f, PaletteEarth, TEXT("Runtime_JumpGap"));
}

void ABreakerGameMode::SpawnBreach()
{
    if (!GetWorld() || !bFieldFrameSet || !bSpawnBreachRamp) return;
    UWorld* World = GetWorld();

    // MEASURED, not assumed (LogGymSummary prints it): Lvl_FirstPerson is a
    // sealed 4000 x 4000 cm courtyard with a continuous parapet — a 200 cm
    // inner course from X 1800 to 2000 and a 200 cm upper course from 1900 to
    // 2000, topping out at 400. There is no doorway. Two base-kit jumps reach
    // 355 cm, so before this the only way into the field the game spawns was
    // to discover a two-stage wall climb, and the field is 85 m of it.
    //
    // A collapsed embankment over the wall is the runtime answer, and it is
    // the right READ as well as the expedient one: overgrown Earth (O24) is
    // exactly a world where the compound wall has been breached and grown over.
    // The proper fix is deleting the wall in the editor — recorded in
    // Docs/Design/Level-Design.md.
    const float Width = SprintCorridorWidth;

    // Ascent. Ends past the wall's outer face at 2100 so the ramp SURFACE
    // clears the 400 cm crest where the crest exists: the upper wall course
    // only spans X 1900-2000, and over that band the ramp runs 433 to 476.
    // Below 1900 the wall is 200 cm and anything clears it.
    // The pitch is atan(520/1200) = 23.4 degrees, well inside the engine's
    // 44.76-degree walkable limit, so it is a run-up and not a climb.
    // Starts at X 900, where the template's own plinth ramp (SM_Ramp11, 500-900)
    // reaches the floor, so the two meet flush instead of one poking through
    // the other.
    SpawnFieldRamp(World, Frame, 900.0f, 0.0f, 2100.0f, BreachCrestHeight, 0.0f, Width, 90.0f,
        PaletteEarth, TEXT("Runtime_Breach"));
    // Descent. 2000 cm of run for 500 cm of drop is 14 degrees, which
    // SlideSlopeAcceleration turns into a genuine downhill slide lane — the
    // gym's old sloped lane, relocated to the one place every route passes
    // through.
    SpawnFieldRamp(World, Frame, 2100.0f, BreachCrestHeight, 4100.0f, 0.0f, 0.0f, Width, 90.0f,
        PaletteEarth, TEXT("Runtime_Breach"));
    // Crest landing, so the top is a place to stand and look rather than a
    // ridge to trip over. This is the vista: the whole field is legible from
    // here, which is what a mouth is FOR.
    SpawnFieldSlab(World, Frame, 2000.0f, 2200.0f, -Width * 0.5f, Width * 0.5f,
        BreachCrestHeight, 120.0f, PaletteStone, TEXT("Runtime_Breach"));
    // Stone edging down both flanks of the ascent. Without it the ramp is a
    // featureless beige wedge filling the spawn view with no depth cue at all —
    // it read as a wall in the first capture, which is the opposite of what a
    // mouth is supposed to say.
    for (int32 Edge = 0; Edge < 2; ++Edge)
    {
        const float Lateral = (Edge == 0 ? -1.0f : 1.0f) * (Width * 0.5f + 30.0f);
        SpawnFieldRamp(World, Frame, 900.0f, 80.0f, 2100.0f, BreachCrestHeight + 80.0f, Lateral, 90.0f, 60.0f,
            PaletteStone, TEXT("Runtime_Breach"));
        SpawnFieldRamp(World, Frame, 2100.0f, BreachCrestHeight + 80.0f, 4100.0f, 80.0f, Lateral, 90.0f, 60.0f,
            PaletteStone, TEXT("Runtime_Breach"));
    }
    // Spill of rubble either side of the crest, so the breach reads as damage
    // rather than as a ramp asset dropped on a wall.
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const float Lateral = (Side == 0 ? -1.0f : 1.0f) * (Width * 0.5f + 200.0f);
        SpawnShape(World, ShapeCube, Frame.At(2050.0f, Lateral, BreachCrestHeight * 0.55f),
            FVector(3.0f, 2.2f, BreachCrestHeight / 100.0f * 0.9f),
            FRotator(0.0f, Side == 0 ? 17.0f : -21.0f, Side == 0 ? 9.0f : -8.0f),
            PaletteConcrete, true, TEXT("Runtime_Breach"));
    }
}

void ABreakerGameMode::SpawnAnchorCamp()
{
    if (!GetWorld() || !bFieldFrameSet) return;
    UWorld* World = GetWorld();

    // The camp now sits ON the courtyard floor rather than 212 cm above it,
    // and it lost its own back wall: the template's parapet already is one, and
    // two walls 200 cm apart is the kind of clutter that makes a 40 m room feel
    // like a 20 m one. Camp centre pulled in from -1400 to -1100 so the 1400 cm
    // plaza fits inside the wall face at -1800 instead of intersecting it.
    const float CampFwd = -1100.0f;
    SpawnFieldSlab(World, Frame, CampFwd - 700.0f, CampFwd + 700.0f, -700.0f, 700.0f,
        16.0f, 32.0f, PaletteEarth, TEXT("Runtime_CampPlaza"));

    if (AStaticMeshActor* Forge = SpawnShape(World, ShapeCube, Frame.At(CampFwd - 400.0f, -450.0f, 110.0f),
        FVector(1.6f, 1.6f, 2.2f), FRotator::ZeroRotator, PaletteRust, true, TEXT("Runtime_Forge")))
    {
        AttachPropLight(Forge, FVector(0, 0, 40.0f), FLinearColor(1.0f, 0.62f, 0.26f), 900.0f, 700.0f);  // Forge glow (light 1/6)
    }
    SpawnShape(World, ShapeCube, Frame.At(CampFwd - 400.0f, 480.0f, 60.0f),
        FVector(2.4f, 1.2f, 1.2f), FRotator::ZeroRotator, PaletteOffWhite, true, TEXT("Runtime_CampProp"));

    // Ammo resupply crate: amber cube beside the quartermaster. Purely a
    // distance trigger evaluated in Tick — no interaction/NPC plumbing.
    SupplyCrateLocation = Frame.At(CampFwd - 200.0f, 480.0f, 0.0f);
    bSupplyCrateSet = true;
    if (AStaticMeshActor* Crate = SpawnShape(World, ShapeCube, SupplyCrateLocation + FVector(0, 0, 55.0f),
        FVector(1.1f, 1.1f, 1.1f), FRotator(0.0f, 12.0f, 0.0f), PaletteAmber, true, TEXT("Runtime_SupplyCrate")))
    {
        AttachPropLight(Crate, FVector(0, 0, 90.0f), FLinearColor(1.0f, 0.68f, 0.28f), 700.0f, 600.0f);  // light 6/6
    }

    // Kess and the Quartermaster no longer spawn here (owner ruling
    // 2026-08-16, A10 -> O48: the Anchor hub is their ONLY home —
    // BreakerHubBuilder.cpp spawns them, this camp does not). The camp keeps
    // its physical props (forge, crate, supply trigger) as set dressing.
    // Consequence, deliberate: quest offer and turn-in now require travelling
    // to the Anchor — including from a PIE drop-in on the template map, where
    // there is no vendor at all until you take the travel point to the hub.

    // Arena boundary ring, at CombatPocketRadius rather than the old 1400.
    // 1400 was under the 1976 cm a dash-speed orbit needs, so the markers were
    // describing a circle the player could not actually run.
    for (int32 Marker = 0; Marker < 12; ++Marker)
    {
        const float Angle = Marker * 30.0f;
        const FVector Offset = Frame.Forward.RotateAngleAxis(Angle, FVector::UpVector) * CombatPocketRadius;
        SpawnShape(World, ShapeCylinder, Frame.At(ArenaDistance, 0.0f, 130.0f) + Offset,
            FVector(0.22f, 0.22f, 2.6f), FRotator::ZeroRotator, PaletteStone, true, TEXT("Runtime_ArenaMarker"));
    }
}

void ABreakerGameMode::SpawnSafeZone()
{
    if (!GetWorld() || !bFieldFrameSet) return;
    SafeZoneCenter = Frame.Ground;
    // THE PROBE'S DECLARATION WINS. An engaged crowd probe needs no safe ring
    // and cannot drop one that does not exist yet; establishing it here anyway
    // would null every enemy's target before detection is consulted and hold
    // the whole crowd in PATROL, which is precisely the measurement the probe
    // exists to refuse. The ring's visual furniture below still builds — only
    // the RULE is suppressed — so a capture of an engaged run still looks like
    // the gym it is.
    bSafeZoneSet = !bProbeSuppressesSafeZone;
    if (bProbeSuppressesSafeZone)
    {
        UE_LOG(LogTemp, Display,
            TEXT("[BreakerCrowd] safe ring suppressed for the engaged probe (declared at arm time)."));
    }

    // Owner feedback: the full-radius teal disc swallowed the spawn area.
    // The boundary now reads as a modest center pad plus a ring of short
    // teal posts at the radius — teal stays on suppression OBJECTS, the
    // ground stays ground.
    AStaticMeshActor* Pad = GetWorld()->SpawnActor<AStaticMeshActor>(SafeZoneCenter + FVector(0, 0, 2.0f), FRotator::ZeroRotator);
    if (Pad)
    {
        UStaticMeshComponent* Mesh = Pad->GetStaticMeshComponent();
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
        Mesh->SetWorldScale3D(FVector(4.0f, 4.0f, 0.04f));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ApplyShapeColor(Mesh, PaletteRiftTeal * 0.5f);
        Mesh->SetMobility(EComponentMobility::Static);
        Pad->SetActorLabel(TEXT("Runtime_SafeZone"));
    }
    for (int32 Post = 0; Post < 12; ++Post)
    {
        const FVector PostLocation = SafeZoneCenter
            + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Post * 30.0f, FVector::UpVector) * SafeZoneRadius
            + FVector(0, 0, 40.0f);
        SpawnShape(GetWorld(), ShapeCylinder, PostLocation, FVector(0.08f, 0.08f, 0.8f), FRotator::ZeroRotator,
            PaletteRiftTeal, false, TEXT("Runtime_SafeZone"));
    }

    // Suppression pylon inside the zone: the second and last teal object.
    if (AStaticMeshActor* Pylon = SpawnShape(GetWorld(), ShapeCylinder,
        SafeZoneCenter + FVector(0, 0, 260.0f), FVector(0.18f, 0.18f, 2.6f), FRotator::ZeroRotator,
        PaletteRiftTeal, false, TEXT("Runtime_SuppressionPylon")))
    {
        AttachPropLight(Pylon, FVector(0, 0, 150.0f), FLinearColor(0.10f, 0.90f, 0.85f), 1400.0f, 900.0f);  // light 2/6
    }
}

bool ABreakerGameMode::IsInSafeZone(const FVector& Location) const
{
    return bSafeZoneSet && FVector::DistSquared2D(Location, SafeZoneCenter) <= FMath::Square(SafeZoneRadius);
}

void ABreakerGameMode::SpawnCombatEncounter()
{
    if (!GetWorld() || !bFieldFrameSet) return;
    UWorld* World = GetWorld();

    // The standing encounter moved from 3500 cm out to the first combat
    // pocket at EncounterPocketDistance. Two reasons, both dimensional:
    //   1. At 3500 the melee pack stood on top of the breach descent — and
    //      before the breach existed, inside a wall the player could not pass.
    //   2. 8500 cm is just under two DashRefreshDistances from the camp, so
    //      the approach is a route with two dash decisions in it rather than a
    //      four-second sprint. The whole complaint was that nothing in this
    //      field is far enough away to be a decision.
    const float LateralOffsets[] = { -450.0f, 0.0f, 450.0f };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        // Inside the pocket, spread across it. The pack occupies the near half
        // so there is CombatPocketRadius of circling room behind them.
        const FVector SpawnLocation = Frame.At(
            EncounterPocketDistance - CombatPocketRadius * 0.5f + Index * 400.0f, LateralOffsets[Index], 120.0f);
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params))
        {
            Enemy->ConfigureEncounter(SpawnLocation, Index * 1.7f);
            // The standing encounter is an area-level-GymAreaLevel area.
            Enemy->SetAreaLevel(GymAreaLevel);
            UBreakerKillTelemetryComponent::AttachTo(Enemy);
        }
    }

    // NON-ELITE MODIFIER CARRIERS (O27's kill-bucket producer). Until this,
    // modifiers only ever landed on the elite below, and GrantModifiers
    // restores the authored rank afterwards — correct for an elite, since
    // ModifierBearing (x2.5) would otherwise DEMOTE it from Elite (x3.0) — so
    // rank ModifierBearing never existed at kill time. Playtest/
    // BreakerKillBuckets.h calls that bucket "the one number that says
    // whether [O27] worked"; it was structurally empty. These plain trash
    // bodies KEEP the promotion instead (O9 keeps Rank and Modifiers
    // separate, so this does not contradict the elite's own tell).
    const float CarrierLateralOffsets[] = { -250.0f, 250.0f };
    for (int32 Index = 0; Index < GymModifierCarrierCount; ++Index)
    {
        const FVector SpawnLocation = Frame.At(
            EncounterPocketDistance - CombatPocketRadius * 0.2f,
            CarrierLateralOffsets[Index % 2], 120.0f);
        FActorSpawnParameters CarrierParams;
        CarrierParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerEnemy* Carrier = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, CarrierParams))
        {
            Carrier->ConfigureEncounter(SpawnLocation, 2.1f + Index * 0.6f);
            Carrier->SetAreaLevel(GymAreaLevel);
            // Offset well clear of the elite's own ModifierSeedBase draw so
            // the two rolls never share a stream position.
            GrantModifierCarrier(Carrier, ModifierSeedBase + 500 + Index * 97);
            UBreakerKillTelemetryComponent::AttachTo(Carrier);
        }
    }

    // One elite anchors the back of the pack: tougher, harder-hitting, and
    // guaranteed Exceptional-or-better drops. It is also the first enemy in the
    // gym to CARRY MODIFIERS — O27 puts difficulty in modifiers rather than
    // trash health, and until this call existed that ruling was implemented in
    // Combat/ and unreachable from a controller.
    const FVector EliteLocation = Frame.At(EncounterPocketDistance + CombatPocketRadius * 0.5f, 0.0f, 120.0f);
    FActorSpawnParameters EliteParams;
    EliteParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if (ABreakerEnemy* Elite = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), EliteLocation, FRotator::ZeroRotator, EliteParams))
    {
        Elite->ConfigureEncounter(EliteLocation, 0.9f);
        Elite->SetAreaLevel(GymAreaLevel);
        Elite->ConfigureElite();
        GrantModifiers(Elite, ModifierSeedBase);
        UBreakerKillTelemetryComponent::AttachTo(Elite);
    }

    // Two LATTICE ranged enemies (Encounter-Design §2.2) flank the pack wide.
    // Placed off to the sides rather than behind the melee so their fire lanes
    // CROSS the ground route the chasers push the player along: the melee
    // enemies deny standing still, the ranged pair deny running in a straight
    // line, and neither problem is solved by the answer to the other.
    //
    // The lateral offset is now CombatPocketRadius rather than a flat 1500, so
    // the pair sits ON the pocket rim: a player entering the pocket is at
    // 2000 cm from each, inside the 900-1900 band's outer edge with the
    // approach still in front of them. RangedSightlineDepth of clear ground
    // behind each one is what lets the retreat gear actually fire.
    const float RangedLateral[] = { -CombatPocketRadius, CombatPocketRadius };
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const FVector SpawnLocation = Frame.At(EncounterPocketDistance - CombatPocketRadius * 0.5f, RangedLateral[Index], 120.0f);
        FActorSpawnParameters RangedParams;
        RangedParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerRangedEnemy* Ranged = World->SpawnActor<ABreakerRangedEnemy>(
            ABreakerRangedEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, RangedParams))
        {
            Ranged->ConfigureEncounter(SpawnLocation, 0.4f + Index * 1.1f);
            Ranged->SetAreaLevel(GymAreaLevel);
            UBreakerKillTelemetryComponent::AttachTo(Ranged);
        }
    }

    // ONE Warden, front and centre (Encounter-Design §5.3: live Wardens per
    // player = 1, "frontal-armour anchors overlapping create unsolvable
    // geometry"). It stands in FRONT of the pack rather than behind it, which
    // is the point of the archetype: §2.4's third axis is "Wardens punish
    // approaching from the front", so the player meets it first and has to
    // decide to go around something instead of through it.
    const FVector WardenLocation = Frame.At(EncounterPocketDistance - CombatPocketRadius * 0.85f, 0.0f, 120.0f);
    FActorSpawnParameters WardenParams;
    WardenParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if (ABreakerWardenEnemy* Warden = World->SpawnActor<ABreakerWardenEnemy>(
        ABreakerWardenEnemy::StaticClass(), WardenLocation, FRotator::ZeroRotator, WardenParams))
    {
        Warden->ConfigureEncounter(WardenLocation, 1.4f);
        Warden->SetAreaLevel(GymAreaLevel);
        UBreakerKillTelemetryComponent::AttachTo(Warden);
    }

    // THE SEVERED DRUDGE (O40c reachability). ABreakerAlteredEnemy shipped and
    // was tested and was spawned by NOTHING, which is the exact defect O40(c)
    // exists to prevent: content in the codebase that no player can reach.
    //
    // ENCOUNTER-DESIGN GATE, stated rather than skirted: the document gates all
    // Altered content behind the Act II turn beat. The gym is a playtest
    // instrument, not campaign content, so this is legitimate — and the Drudge
    // must not be added to anything that reads as campaign progression until
    // that beat exists.
    //
    // Placed on the pocket's OPEN side, off the corridor axis, and then moved to
    // whatever ground near there carries no hard cover: its weak point is a
    // dorsal ridge at 129 cm rather than a head, so the answer to it is a circle,
    // and a 2.0x-health body with its back to a slab has no answer at all. It is
    // also a MELEE body against 5.3's live-enemy ceiling — one, not two, keeps
    // the standing encounter at eleven live against a cap of twelve.
    for (int32 Index = 0; Index < GymDrudgeCount; ++Index)
    {
        const FVector DrudgeAround = Frame.At(EncounterPocketDistance - CombatPocketRadius * 0.25f,
            (Index % 2 == 0 ? -1.0f : 1.0f) * (CombatPocketRadius * 0.45f), 120.0f);
        SpawnDrudge(DrudgeAround, 2.4f + Index * 0.8f, GymAreaLevel);
    }

    // ONE Skirmisher, and its placement is the whole point. It goes AT the
    // pocket's cover ring, not at an arbitrary bearing: the pocket's four cover
    // blocks sit on a CoverPitchMax ring around the pocket centre, and a
    // Skirmisher that starts beside one of them is behind cover on frame one.
    // Spawned in the open it is a plain shooter with a longer telegraph than a
    // Lattice, which is strictly worse than a Lattice and teaches nothing.
    //
    // ONE, not two, and the cap check in LogGymSummary is what caught it: two
    // Skirmishers alongside the two flanking Lattices is FOUR converging
    // projectile sources, and §5.3 holds that at three at any party size
    // because "four converging projectile sources removes all safe ground —
    // this is the single most dangerous scaling knob". Wave mode introduces
    // more of them, inside the same budget solver that enforces the same cap.
    const FVector PocketCentre = Frame.At(EncounterPocketDistance, 0.0f, 0.0f);
    const FVector PlayerApproach = Frame.At(EncounterPocketDistance - CombatPocketRadius * 2.0f, 0.0f, 0.0f);
    for (int32 Index = 0; Index < 1; ++Index)
    {
        // Two different bearings off the pocket centre so they resolve to two
        // different cover blocks rather than crowding one.
        const FVector Bearing = Frame.Forward.RotateAngleAxis(Index == 0 ? 55.0f : -55.0f, FVector::UpVector);
        if (ABreakerSkirmisherEnemy* Skirmisher = SpawnSkirmisherNearCover(
            PocketCentre + Bearing * (CoverPitchMax * 0.5f), PlayerApproach, 0.6f + Index * 1.3f))
        {
            Skirmisher->SetAreaLevel(GymAreaLevel);
        }
    }
}

void ABreakerGameMode::GrantModifiers(ABreakerEnemy* Enemy, int32 Seed) const
{
    if (!bGrantModifiers || !Enemy) return;

    // The rank the CONTENT authored. ConfigureWithModifiers overwrites it with
    // ModifierBearing, which is a demotion for anything ranked above that, so
    // it is captured and put back. Rank is the single source of truth for what
    // an elite is worth (O27); a modifier roll must not become a second one.
    const EBreakerMonsterRank AuthoredRank = Enemy->GetMonsterRank();
    if (Enemy->ConfigureWithModifiers(Seed) <= 0) return;

    if (Enemy->GetMonsterRank() != AuthoredRank)
    {
        Enemy->SetMonsterRank(AuthoredRank);
        // SetMonsterRank rebuilt the chassis, so max health moved, so the
        // Warded ward is now sized against a number that no longer exists.
        // Re-publishing the same set re-runs ApplyPersistentModifiers against
        // the new health. Copied into a local first because SetModifiers
        // assigns over the very array it would otherwise be reading.
        if (UBreakerEnemyModifierComponent* Modifiers = Enemy->GetModifierComponent())
        {
            const TArray<EBreakerEnemyModifier> Granted = Modifiers->GetModifiers();
            Modifiers->SetModifiers(Granted);
        }
    }
}

void ABreakerGameMode::GrantModifierCarrier(ABreakerEnemy* Enemy, int32 Seed) const
{
    if (!bGrantModifiers || !Enemy) return;

    // No capture-and-restore: ConfigureWithModifiers's unconditional
    // promotion to rank ModifierBearing is exactly what a carrier is for —
    // see Playtest/BreakerKillBuckets.h. If the roll grants zero (a legal
    // outcome of RollAndApplyModifiers for a pathological family/params
    // combination), the body simply stays rank Trash; nothing else to do.
    Enemy->ConfigureWithModifiers(Seed);
}

void ABreakerGameMode::SpawnWorldDressing()
{
    if (!GetWorld() || !bFieldFrameSet) return;

    // One seed drives every dressing decision so the gym looks identical each
    // run and screenshots stay comparable between playtests.
    FRandomStream Stream(20260812);
    SpawnRuins(Stream);
    SpawnScatteredTech(Stream);
    SpawnOvergrowth(Stream);
}

void ABreakerGameMode::SpawnOvergrowth(FRandomStream& Stream)
{
    // Vegetation clusters: squashed spheres read as bushes, thin tall cones as
    // reeds and saplings. All non-colliding, so movement tests are unaffected.
    //
    // Anchors follow the STATIONS now instead of a hand-picked list that was
    // written when the whole gym fitted in 75 x 50 m. Every one sits on a lane
    // shoulder, because dressing on a shoulder gives the eye something to read
    // speed against and dressing in the middle of a corridor narrows it.
    const FVector Right = Frame.Right;
    const FVector ClusterAnchors[] =
    {
        Frame.At(-1100.0f, -1300.0f),                                        // camp edge
        Frame.At(-1600.0f, 1200.0f),                                         // camp edge
        Frame.At(3000.0f, -SprintCorridorWidth * 1.4f),                      // breach shoulder
        Frame.At(3200.0f, SprintCorridorWidth * 1.5f),                       // breach shoulder
        Frame.At(RangeFiringLineDistance + 900.0f, -CombatPocketRadius),     // range shoulder
        Frame.At(RangeFiringLineDistance + 2600.0f, CombatPocketRadius),     // range shoulder
        Frame.At(EncounterPocketDistance - CombatPocketRadius, -CombatPocketRadius * 1.4f),
        Frame.At(EncounterPocketDistance + CombatPocketRadius, CombatPocketRadius * 1.3f),
        Frame.At(EncounterPocketDistance + CombatPocketRadius + 2200.0f, -DashCorridorWidth * 3.4f),
        Frame.At(ArenaDistance - CombatPocketRadius * 1.5f, CombatPocketRadius * 1.2f),
        Frame.At(ArenaDistance + CombatPocketRadius * 1.4f, -CombatPocketRadius * 1.1f),
        Frame.At(RangeFiringLineDistance, -FieldHalfExtent * 0.62f - 1400.0f) // sniper lane shoulder
    };

    for (const FVector& Anchor : ClusterAnchors)
    {
        const int32 Pieces = Stream.RandRange(5, 7);
        for (int32 Index = 0; Index < Pieces; ++Index)
        {
            const FVector Offset(Stream.FRandRange(-320.0f, 320.0f), Stream.FRandRange(-320.0f, 320.0f), 0.0f);
            const float Yaw = Stream.FRandRange(0.0f, 360.0f);
            const bool bReed = Stream.FRand() < 0.4f;
            const FLinearColor Tint = FMath::Lerp(PaletteFoliage, Stream.FRand() < 0.25f ? PaletteDryGrass : PaletteMoss, Stream.FRand());
            if (bReed)
            {
                const float Height = Stream.FRandRange(1.1f, 2.3f);
                SpawnShape(GetWorld(), ShapeCone, Anchor + Offset + FVector(0, 0, Height * 40.0f),
                    FVector(Stream.FRandRange(0.16f, 0.30f), Stream.FRandRange(0.16f, 0.30f), Height),
                    FRotator(Stream.FRandRange(-9.0f, 9.0f), Yaw, Stream.FRandRange(-9.0f, 9.0f)),
                    Tint, false, TEXT("Runtime_Overgrowth"));
            }
            else
            {
                const float Spread = Stream.FRandRange(0.7f, 1.6f);
                SpawnShape(GetWorld(), ShapeSphere, Anchor + Offset + FVector(0, 0, Stream.FRandRange(6.0f, 26.0f)),
                    FVector(Spread, Spread * Stream.FRandRange(0.75f, 1.2f), Spread * Stream.FRandRange(0.32f, 0.55f)),
                    FRotator(Stream.FRandRange(-7.0f, 7.0f), Yaw, 0.0f),
                    Tint, false, TEXT("Runtime_Overgrowth"));
            }
        }
    }
}

void ABreakerGameMode::SpawnRuins(FRandomStream& Stream)
{
    const FVector Forward = Frame.Forward;
    const FVector Right = Frame.Right;

    // Broken walls: overlapping offset boxes at odd angles, partially sunken,
    // weathered palette. These DO collide — they are playable cover.
    //
    // Spacing is the design content here, not the shapes. The chain from the
    // breach to the encounter pocket is laid out at CoverPitchMax so a player
    // crossing at sprint always has a next piece of cover reachable inside one
    // telegraph-plus-flight window (0.85 + 0.82 = 1.67 s, 1837 cm). Ground with
    // no answer to a telegraph is the failure mode O1 creates by making
    // movement the only active defence.
    const FVector WallAnchors[] =
    {
        Frame.At(4600.0f, -CoverPitchMax * 0.9f),
        Frame.At(4600.0f + CoverPitchMax, CoverPitchMax * 0.7f),
        Frame.At(4600.0f + CoverPitchMax * 2.0f, -CoverPitchMax * 0.5f),
        Frame.At(EncounterPocketDistance - CombatPocketRadius - 400.0f, CoverPitchMax * 0.8f),
        Frame.At(-1900.0f, -900.0f)
    };
    for (int32 Wall = 0; Wall < UE_ARRAY_COUNT(WallAnchors); ++Wall)
    {
        const float BaseYaw = Stream.FRandRange(0.0f, 180.0f);
        for (int32 Segment = 0; Segment < 3; ++Segment)
        {
            const float Height = Stream.FRandRange(1.4f, 3.0f);
            const FVector Slide = Right.RotateAngleAxis(BaseYaw, FVector::UpVector) * (Segment * 320.0f - 320.0f);
            SpawnShape(GetWorld(), ShapeCube,
                WallAnchors[Wall] + Slide + FVector(0, 0, Height * 50.0f - 40.0f),
                FVector(Stream.FRandRange(2.2f, 3.4f), 0.35f, Height),
                FRotator(0.0f, BaseYaw + Stream.FRandRange(-14.0f, 14.0f), Stream.FRandRange(-7.0f, 7.0f)),
                Segment == 1 ? PaletteStone : PaletteConcrete, true, TEXT("Runtime_Ruin"));
        }
    }

    // Collapsed arch: two leaning legs and a fallen span across them. Placed
    // on the forward axis just past the breach, where it is the first thing
    // the field puts in front of the player — a scale reference at 45 m, in
    // the 15-40 m band the art plan says every asset is judged in.
    const FVector ArchBase = Frame.At(4500.0f, -300.0f);
    SpawnShape(GetWorld(), ShapeCube, ArchBase - Right * 400.0f + FVector(0, 0, 200.0f), FVector(0.5f, 0.5f, 4.0f),
        FRotator(0.0f, 0.0f, 11.0f), PaletteConcrete, true, TEXT("Runtime_Ruin"));
    SpawnShape(GetWorld(), ShapeCube, ArchBase + Right * 400.0f + FVector(0, 0, 170.0f), FVector(0.5f, 0.5f, 3.4f),
        FRotator(0.0f, 0.0f, -16.0f), PaletteConcrete, true, TEXT("Runtime_Ruin"));
    SpawnShape(GetWorld(), ShapeCube, ArchBase + FVector(0, 0, 380.0f), FVector(4.6f, 0.6f, 0.45f),
        FRotator(6.0f, 0.0f, -9.0f), PaletteStone, true, TEXT("Runtime_Ruin"));

    // Cracked platform slabs strewn near the arena: low, tilted, mantle-able.
    for (int32 Slab = 0; Slab < 5; ++Slab)
    {
        const float Angle = 34.0f + Slab * 61.0f;
        // Inside the arena rim (CombatPocketRadius) rather than on it: the
        // slabs are footing inside the circle, not a second wall around it.
        const FVector Offset = Forward.RotateAngleAxis(Angle, FVector::UpVector)
            * Stream.FRandRange(CombatPocketRadius * 0.45f, CombatPocketRadius * 0.8f);
        SpawnShape(GetWorld(), ShapeCube,
            Frame.At(ArenaDistance, 0.0f, Stream.FRandRange(10.0f, 40.0f)) + Offset,
            FVector(Stream.FRandRange(2.0f, 3.6f), Stream.FRandRange(1.6f, 2.8f), 0.22f),
            FRotator(Stream.FRandRange(-8.0f, 8.0f), Angle, Stream.FRandRange(-8.0f, 8.0f)),
            Stream.FRand() < 0.5f ? PaletteStone : PaletteConcrete, true, TEXT("Runtime_Ruin"));
    }
}

void ABreakerGameMode::SpawnScatteredTech(FRandomStream& Stream)
{
    // O24's "slight sci-fi": functional, weathered, out of place. Amber and
    // off-white only — teal stays reserved for the pad and the pylon.
    const FVector Forward = Frame.Forward;
    const FVector Right = Frame.Right;

    // 1. Leaning monolith panel at the camp mouth. It is the tallest thing in
    //    the courtyard, so it is the landmark that says which way the mouth is.
    SpawnShape(GetWorld(), ShapeCube, Frame.At(-300.0f, -900.0f, 210.0f),
        FVector(1.6f, 0.22f, 4.2f), FRotator(0.0f, 24.0f, -13.0f), PaletteOffWhite, true, TEXT("Runtime_TechProp"));

    // 2. Generator cylinder with a low amber lamp.
    if (AStaticMeshActor* Generator = SpawnShape(GetWorld(), ShapeCylinder, Frame.At(-500.0f, 900.0f, 70.0f),
        FVector(0.9f, 0.9f, 1.4f), FRotator(0.0f, 0.0f, 4.0f), PaletteRust, true, TEXT("Runtime_TechProp")))
    {
        AttachPropLight(Generator, FVector(0, 0, 90.0f), FLinearColor(1.0f, 0.66f, 0.30f), 800.0f, 650.0f);  // light 3/6
    }

    // 3. Crate stack on the far side of the breach, where the field opens.
    SpawnShape(GetWorld(), ShapeCube, Frame.At(4300.0f, SprintCorridorWidth * 0.9f, 55.0f),
        FVector(1.1f, 1.1f, 1.1f), FRotator(0.0f, 18.0f, 0.0f), PaletteOffWhite, true, TEXT("Runtime_TechProp"));
    SpawnShape(GetWorld(), ShapeCube, Frame.At(4360.0f, SprintCorridorWidth * 0.85f, 150.0f),
        FVector(0.8f, 0.8f, 0.8f), FRotator(0.0f, -31.0f, 5.0f), PaletteRust, true, TEXT("Runtime_TechProp"));

    // 4. Broken antenna ring: half-buried torus faked from tilted cylinders.
    const FVector RingBase = Frame.At(RangeFiringLineDistance + 2200.0f, -CombatPocketRadius * 1.6f);
    for (int32 Segment = 0; Segment < 4; ++Segment)
    {
        SpawnShape(GetWorld(), ShapeCylinder, RingBase + FVector(0, 0, 120.0f + Segment * 18.0f),
            FVector(0.14f, 0.14f, 2.4f), FRotator(0.0f, Segment * 45.0f, 62.0f + Segment * 6.0f),
            PaletteOffWhite, true, TEXT("Runtime_TechProp"));
    }
    if (AStaticMeshActor* Mast = SpawnShape(GetWorld(), ShapeCylinder, RingBase + FVector(0, 0, 200.0f),
        FVector(0.22f, 0.22f, 3.6f), FRotator(0.0f, 0.0f, 9.0f), PaletteRust, true, TEXT("Runtime_TechProp")))
    {
        AttachPropLight(Mast, FVector(0, 0, 160.0f), FLinearColor(1.0f, 0.72f, 0.34f), 600.0f, 550.0f);  // light 4/6
    }

    // 5. Toppled fuel drums near the ruins.
    for (int32 Drum = 0; Drum < 3; ++Drum)
    {
        SpawnShape(GetWorld(), ShapeCylinder,
            Frame.At(4600.0f + CoverPitchMax, -(700.0f + Drum * 180.0f), 40.0f),
            FVector(0.5f, 0.5f, 0.8f), FRotator(Stream.FRandRange(70.0f, 96.0f), Stream.FRandRange(0.0f, 360.0f), 0.0f),
            Drum == 1 ? PaletteOffWhite : PaletteRust, true, TEXT("Runtime_TechProp"));
    }

    // 6. Half-sunken relay cone beside the arena approach, one dim lamp.
    if (AStaticMeshActor* Relay = SpawnShape(GetWorld(), ShapeCone, Frame.At(ArenaDistance - CombatPocketRadius * 1.8f, 950.0f, 60.0f),
        FVector(1.4f, 1.4f, 1.6f), FRotator(0.0f, 0.0f, 17.0f), PaletteAmber, true, TEXT("Runtime_TechProp")))
    {
        AttachPropLight(Relay, FVector(0, 0, 110.0f), FLinearColor(1.0f, 0.70f, 0.32f), 500.0f, 500.0f);  // light 5/6
    }
}

void ABreakerGameMode::RefillPlayerAmmo()
{
    if (!GetWorld()) return;
    APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    if (UBreakerWeaponComponent* Weapon = PlayerPawn ? PlayerPawn->FindComponentByClass<UBreakerWeaponComponent>() : nullptr)
    {
        Weapon->ResetAmmunition();
    }
}

void ABreakerGameMode::TickWaveAdvance(float DeltaSeconds)
{
    // Every mode advances itself once a wave has gone live; F4 (StartWave)
    // skips the breather, and the live wave that follows resets the countdown
    // below. A completed run is over: the latch is O168's, and a wave after
    // it would be a run that cannot end.
    if (bRiftRunCompleted || CurrentWave <= 0) return;
    if (IsWaveActive())
    {
        // A live wave, including one Breaker.Rift.Population just re-solved
        // mid-breather: the countdown belongs to a clear that is no longer
        // the current one.
        WaveAdvanceCountdown = -1.0f;
        return;
    }
    if (WaveAdvanceCountdown < 0.0f)
    {
        float Delay = 0.0f;
        if (!UBreakerWaveBudgetLibrary::GetAutoAdvanceDelay(CurrentWave, WaveBudget, Delay)) return;
        WaveAdvanceCountdown = Delay;
        UE_LOG(LogTemp, Display, TEXT("[BreakerWaves] wave %d cleared; wave %d in %.0fs (F4 skips)."),
            CurrentWave, CurrentWave + 1, Delay);
    }
    WaveAdvanceCountdown -= DeltaSeconds;
    if (WaveAdvanceCountdown <= 0.0f)
    {
        WaveAdvanceCountdown = -1.0f;
        StartNextWave();
    }
}

void ABreakerGameMode::TickSupplyCrate(float DeltaSeconds)
{
    if (!bSupplyCrateSet || !GetWorld()) return;
    APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    if (!PlayerPawn) return;

    const bool bAtCrate = FVector::DistSquared2D(PlayerPawn->GetActorLocation(), SupplyCrateLocation)
        <= FMath::Square(SupplyCrateRadius);
    if (!bAtCrate)
    {
        SupplyCrateDwell = 0.0f;
        return;
    }
    if (GetWorld()->GetTimeSeconds() - LastSupplyCrateUseTime < SupplyCrateCooldownSeconds) return;

    SupplyCrateDwell += DeltaSeconds;
    if (SupplyCrateDwell >= SupplyCrateDwellSeconds)
    {
        SupplyCrateDwell = 0.0f;
        LastSupplyCrateUseTime = GetWorld()->GetTimeSeconds();
        RefillPlayerAmmo();
    }
}

void ABreakerGameMode::SpawnCombatPocket(float Fwd, float Rgt, FRandomStream& Stream, bool bRimRuins)
{
    UWorld* World = GetWorld();
    if (!World) return;

    // A pocket is a place a movement build can CIRCLE, and that is a
    // measurement, not a mood. Minimum turn radius is v^2/MaxAcceleration:
    // 288 cm at sprint, 688 cm at dash speed. An orbit at exactly the minimum
    // radius is a rail; CombatPocketRadius is twice the dash radius plus body
    // and cover clearance, so the orbit is a choice of line rather than the
    // only line. Everything below is placed as a fraction of that radius, so
    // retuning the radius retunes the pocket instead of breaking it.
    const float BaseYaw = Stream.FRandRange(0.0f, 360.0f);

    // Broken wall arc on the rim. It sweeps ~150 degrees and is deliberately
    // NOT a closed ring: a closed pocket is an arena, and an arena the player
    // cannot leave at speed removes the route choice the pocket exists to
    // create.
    // bRimRuins false is the ELITE ARENA. The arc sits 1800-2200 cm from centre
    // and the Field Marshal's gallery offsets are +/-1900 with alcoves at
    // +/-1700, so in the arena this arc is geometry the boss gives orders into.
    // SpawnBossTest has warned about exactly that since it was written; leaving
    // the arc out is the fix rather than the warning.
    for (int32 Segment = 0; bRimRuins && Segment < 5; ++Segment)
    {
        const float Angle = BaseYaw + Segment * 37.0f;
        const FVector Radial = Frame.Forward.RotateAngleAxis(Angle, FVector::UpVector);
        const float Height = Stream.FRandRange(1.6f, 3.2f);
        SpawnShape(World, ShapeCube,
            Frame.At(Fwd, Rgt, Height * 50.0f - 40.0f) + Radial * Stream.FRandRange(CombatPocketRadius * 0.9f, CombatPocketRadius * 1.1f),
            FVector(Stream.FRandRange(2.4f, 3.6f), 0.35f, Height),
            FRotator(0.0f, Angle + 90.0f + Stream.FRandRange(-12.0f, 12.0f), Stream.FRandRange(-6.0f, 6.0f)),
            Segment % 2 == 0 ? PaletteConcrete : PaletteStone, true, TEXT("Runtime_PocketRuin"));
    }

    // THE POCKET'S COVER MOVED OUT OF HERE. It used to be four blocks and a
    // pillar authored inline, registered as bare positions, and invisible to
    // anything that wanted to reason about the field: nothing could tell the
    // 500 cm pillar from the 110 cm blocks, and nothing outside the pocket had
    // any cover at all. Both are now the cover field's job
    // (Game/BreakerCoverRegistry.h, SpawnCoverField below), which places the
    // same cluster at every pocket centre plus an outer ring, classifies each
    // piece, and can be measured. What is left in this function is dressing:
    // the broken rim arc, the overgrowth and the fallen prop.

    for (int32 Bush = 0; Bush < 6; ++Bush)
    {
        const FVector Offset(Stream.FRandRange(-1300.0f, 1300.0f), Stream.FRandRange(-1300.0f, 1300.0f), 0.0f);
        const float Spread = Stream.FRandRange(0.8f, 1.7f);
        SpawnShape(World, ShapeSphere, Frame.At(Fwd, Rgt, Stream.FRandRange(8.0f, 28.0f)) + Offset,
            FVector(Spread, Spread * Stream.FRandRange(0.75f, 1.2f), Spread * Stream.FRandRange(0.32f, 0.55f)),
            FRotator(0.0f, Stream.FRandRange(0.0f, 360.0f), 0.0f),
            FMath::Lerp(PaletteFoliage, PaletteMoss, Stream.FRand()), false, TEXT("Runtime_PocketOvergrowth"));
    }
    SpawnShape(World, ShapeCylinder, Frame.At(Fwd, Rgt + 400.0f, 40.0f),
        FVector(0.5f, 0.5f, 0.8f), FRotator(Stream.FRandRange(70.0f, 96.0f), Stream.FRandRange(0.0f, 360.0f), 0.0f),
        PaletteRust, true, TEXT("Runtime_PocketProp"));
}

void ABreakerGameMode::SpawnExpandedField()
{
    if (!GetWorld() || !bFieldFrameSet) return;
    UWorld* World = GetWorld();

    // THE FIELD, laid out against Docs/Design/Level-Design.md.
    //
    // What was wrong with the previous version, stated plainly because it is
    // the thing the owner has been reporting for weeks:
    //
    //  * The playable room was Lvl_FirstPerson's 4000 x 4000 cm courtyard.
    //    Sprint crosses that in 3.6 s and dash in 2.4 s against a 4.0 s dash
    //    cooldown, so inside the only room the player could reach, the dash
    //    was structurally incapable of being a traversal choice.
    //  * The apron and everything on it was built 212 cm above the real floor,
    //    because the ground plane was taken as "spawn minus a capsule" and the
    //    spawn is on a 210 cm plinth.
    //  * The additions were islands: three pockets, a lane and a wall pair
    //    scattered across 180 m of featureless flat with nothing between them,
    //    so the field read as a small box next to a car park.
    //
    // What replaces it is a route with STATIONS, each at least one
    // DashRefreshDistance from the last, on the real floor, entered through a
    // breach in the courtyard wall. Numbers come from the constants in the
    // header; nothing here is a literal that is not either a fraction of one
    // of them or a piece of dressing.
    //
    // Its own seed stream, offset from the dressing seed, so both stay
    // deterministic and independent.
    FRandomStream Stream(20260812 + 101);

    // --- 1. Ground --------------------------------------------------------
    // Four big slabs, not a tile grid. The template Floor already covers
    // +/-2000 and its top is exactly at the ground plane, so the apron is
    // authored as the rectangle AROUND it: abutting, never overlapping, no
    // coplanar z-fighting and no step at the seam. It also drops the actor
    // count from 81 tiles to 4.
    const float Back = -FieldRearExtent;
    const float Front = FieldForwardExtent;
    const float Side = FieldHalfExtent;
    const float Shell = 2000.0f;   // the template courtyard half-extent, measured
    SpawnFieldSlab(World, Frame, Back, -Shell, -Side, Side, 0.0f, 40.0f, PaletteEarth, TEXT("Runtime_FieldApron"));
    SpawnFieldSlab(World, Frame, Shell, Front, -Side, Side, 0.0f, 40.0f, PaletteEarth, TEXT("Runtime_FieldApron"));
    SpawnFieldSlab(World, Frame, -Shell, Shell, -Side, -Shell, 0.0f, 40.0f, PaletteEarth, TEXT("Runtime_FieldApron"));
    SpawnFieldSlab(World, Frame, -Shell, Shell, Shell, Side, 0.0f, 40.0f, PaletteEarth, TEXT("Runtime_FieldApron"));
    // THE FIFTH SLAB, only when the courtyard the four above abut DOES NOT
    // EXIST. The shipped Lvl_Gym is an empty asset — no authored floor at all
    // — so the ground probe found nothing and the rectangle-around-the-shell
    // authoring left a 4000 x 4000 hole exactly under the arriving pawn
    // ("theres no floor to the gym"). In Lvl_FirstPerson the probe hits the
    // template Floor, this slab is skipped, and the abutting-never-overlapping
    // rule (no coplanar z-fighting) is preserved untouched.
    if (!Frame.bAuthoredFloor)
    {
        SpawnFieldSlab(World, Frame, -Shell, Shell, -Shell, Shell, 0.0f, 40.0f, PaletteEarth, TEXT("Runtime_FieldApron"));
        UE_LOG(LogTemp, Display, TEXT("[BreakerGym] no authored floor under the shell — centre apron slab spawned."));
    }

    // Tint patches: non-colliding flat plates that break 250 x 220 m of one
    // colour. Purely so the eye has something to judge speed against — a
    // featureless plane is a large part of why a big field can still read as
    // nothing (O24 dressing, no gameplay meaning).
    // Deliberately SMALL and numerous. The first version used 900-2600 cm
    // plates and the plan view read as farmland — a patch the size of a combat
    // pocket is a landmark, not a texture, and it lies about the scale of the
    // thing next to it. Scrub-sized plates give the eye optical flow to judge
    // speed against without ever being mistaken for geometry.
    //
    // TWO THINGS HERE ARE BUG FIXES, not dressing (owner: "a lot of the
    // textures on the ground were tearing"). Both were the same mistake in two
    // forms — a flat plate laid on a flat plane with nothing separating them:
    //
    //  1. The patches were placed by pure rejection-free random sampling, so
    //     they OVERLAPPED each other, and every patch is authored at the same
    //     height. Two overlapping plates whose top faces are at exactly the
    //     same z are coplanar, and coplanar is z-fighting by construction: the
    //     depth test has no winner and the pair stipples. At ~18% area coverage
    //     over 200 plates that is dozens of overlapping pairs scattered across
    //     the whole field, which is exactly what "a lot of" describes. The
    //     footprints are now tracked and an overlapping placement is REJECTED,
    //     so no two patches ever share a surface. Rejection also removes the
    //     double-tinted blotches, which is a second, smaller win.
    //  2. The patches were 4 cm-thick CUBES that CAST SHADOWS. At 150-200 m
    //     both the shadow of that lip and the lip's own shaded side face are
    //     sub-pixel, and both alias into a stippled dashed line tracing the
    //     patch outline — the dark dotted seams along every patch edge in the
    //     before-capture. Killing the shadow removed most of it and left the
    //     side face still drawing a fainter one, which is measured, not
    //     assumed: it is visible in the intermediate capture. So the patches are
    //     now PLANES — one upward quad, no lip to shade and none to alias —
    //     lifted clear of the apron, casting nothing.
    //
    // The attempt count is raised because rejection now throws placements away;
    // it is attempts, not patches, and the field settles at rather fewer.
    struct FPatchFootprint { float MinFwd, MaxFwd, MinRgt, MaxRgt; };
    TArray<FPatchFootprint> Placed;
    Placed.Reserve(FieldPatchAttempts);
    for (int32 Patch = 0; Patch < FieldPatchAttempts; ++Patch)
    {
        const float Fwd = Stream.FRandRange(Back, Front);
        const float Rgt = Stream.FRandRange(-Side, Side);
        if (FMath::Abs(Fwd) < Shell && FMath::Abs(Rgt) < Shell) continue;
        const float SizeX = Stream.FRandRange(320.0f, 1100.0f);
        const float SizeY = Stream.FRandRange(320.0f, 1100.0f);
        const float Yaw = Stream.FRandRange(0.0f, 360.0f);
        // Axis-aligned bound of the ROTATED plate, so the rejection is a true
        // separation test rather than one that passes on a corner overlap.
        const float CosYaw = FMath::Abs(FMath::Cos(FMath::DegreesToRadians(Yaw)));
        const float SinYaw = FMath::Abs(FMath::Sin(FMath::DegreesToRadians(Yaw)));
        const float HalfFwd = 0.5f * (SizeX * CosYaw + SizeY * SinYaw);
        const float HalfRgt = 0.5f * (SizeX * SinYaw + SizeY * CosYaw);
        const FPatchFootprint Footprint{ Fwd - HalfFwd, Fwd + HalfFwd, Rgt - HalfRgt, Rgt + HalfRgt };
        bool bOverlaps = false;
        for (const FPatchFootprint& Other : Placed)
        {
            if (Footprint.MinFwd < Other.MaxFwd && Footprint.MaxFwd > Other.MinFwd &&
                Footprint.MinRgt < Other.MaxRgt && Footprint.MaxRgt > Other.MinRgt)
            {
                bOverlaps = true;
                break;
            }
        }
        if (bOverlaps) continue;
        Placed.Add(Footprint);
        AStaticMeshActor* PatchActor = SpawnShape(World, ShapePlane, Frame.At(Fwd, Rgt, GroundOverlayLift * 0.5f),
            FVector(SizeX / 100.0f, SizeY / 100.0f, 1.0f),
            FRotator(0.0f, Yaw, 0.0f),
            FMath::Lerp(PaletteEarth, Stream.FRand() < 0.3f ? PaletteDryGrass : PaletteMoss, Stream.FRandRange(0.35f, 1.0f)),
            false, TEXT("Runtime_FieldPatch"));
        if (PatchActor)
        {
            PatchActor->GetStaticMeshComponent()->SetCastShadow(false);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("[BreakerGym] tint patches: %d placed from %d attempts (overlaps rejected; a coplanar pair is z-fighting by construction)"),
        Placed.Num(), FieldPatchAttempts);

    // --- 2. The forward route ---------------------------------------------
    // Shoulder ruins flanking the axis from the breach exit to the arena. They
    // are placed at DashCorridorWidth * 1.5 from the centreline on each side,
    // so the main route is 4800 cm wide: three sprint corridors, or one dash
    // corridor with a corridor of clear ground either side of it. A route this
    // wide is not a corridor at all, which is deliberate — the corridors in
    // this field are the SIDE lanes, and the spine is open ground you choose a
    // line across.
    const float SpineHalf = DashCorridorWidth * 1.5f;
    for (int32 Marker = 0; Marker < 9; ++Marker)
    {
        const float Fwd = 4600.0f + Marker * DashRefreshDistance * 0.45f;
        if (Fwd > ArenaDistance - CombatPocketRadius) break;
        for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
        {
            const float Lateral = (SideIndex == 0 ? -1.0f : 1.0f) * SpineHalf;
            const float Height = Stream.FRandRange(1.2f, 2.6f);
            SpawnShape(World, ShapeCube, Frame.At(Fwd + Stream.FRandRange(-500.0f, 500.0f), Lateral + Stream.FRandRange(-300.0f, 300.0f), Height * 50.0f - 30.0f),
                FVector(Stream.FRandRange(2.0f, 3.4f), 0.4f, Height),
                FRotator(0.0f, Stream.FRandRange(0.0f, 180.0f), Stream.FRandRange(-8.0f, 8.0f)),
                Marker % 2 == 0 ? PaletteConcrete : PaletteStone, true, TEXT("Runtime_SpineRuin"));
        }
    }

    // --- 3. Combat pockets -------------------------------------------------
    // Three, all at least one DashRefreshDistance apart so moving between them
    // is a route decision. The first is the standing encounter's ground.
    SpawnCombatPocket(EncounterPocketDistance, 0.0f, Stream);
    SpawnCombatPocket(EncounterPocketDistance + DashRefreshDistance, FieldHalfExtent * 0.55f, Stream);
    SpawnCombatPocket(RangeFiringLineDistance + DashRefreshDistance, -FieldHalfExtent * 0.55f, Stream);
    // Fourth pocket IS the elite arena: same radius, same grammar, marked with
    // the ring in SpawnAnchorCamp and reused by wave mode.
    // bRimRuins false: see SpawnCombatPocket. The arena's cover is authored to
    // Encounter-Design 3.3 by the cover field instead.
    SpawnCombatPocket(ArenaDistance, 0.0f, Stream, false);

    // --- 4. Sniper sightline lane -----------------------------------------
    // Runs down the left flank with three distance markers. Its width is
    // DashCorridorWidth (a lane the player is expected to move fast down) and
    // its markers sit at 30 / 60 / 90 m from the firing line. It starts at the
    // range's firing line rather than at the spawn, so the numbers on the
    // posts are the numbers the target dummies are at.
    const float LaneRight = -FieldHalfExtent * 0.62f;
    const float LaneStart = RangeFiringLineDistance - 1500.0f;
    const float MarkerDistances[] = { 3000.0f, 6000.0f, 9000.0f };   // 30 / 60 / 90 m
    for (int32 Marker = 0; Marker < UE_ARRAY_COUNT(MarkerDistances); ++Marker)
    {
        const float Fwd = LaneStart + MarkerDistances[Marker];
        // Post height scales with range so the far marker still subtends
        // something readable through a scope.
        const float PostHeight = 3.0f + Marker * 1.0f;
        SpawnShape(World, ShapeCylinder, Frame.At(Fwd, LaneRight, PostHeight * 50.0f),
            FVector(0.3f, 0.3f, PostHeight), FRotator::ZeroRotator, PaletteOffWhite, true, TEXT("Runtime_RangeMarker"));
        SpawnShape(World, ShapeCube, Frame.At(Fwd, LaneRight, PostHeight * 100.0f + 30.0f),
            FVector(1.6f, 0.3f, 0.6f), FRotator::ZeroRotator, PaletteAmber, true, TEXT("Runtime_RangeMarker"));
        for (int32 Pip = 0; Pip <= Marker; ++Pip)
        {
            SpawnShape(World, ShapeCube, Frame.At(Fwd, LaneRight + Pip * 120.0f - 60.0f, 25.0f),
                FVector(0.7f, 0.7f, 0.5f), FRotator::ZeroRotator, PaletteAmber, true, TEXT("Runtime_RangeMarker"));
        }
    }
    // Kerbs at DashCorridorWidth so the lane reads as a lane. Low enough that
    // they never block a shot down it, which is the whole point of a sightline.
    // 35, NOT 45 (D4, owner-ruled): the kerb sat exactly ON the MaxStepHeight
    // boundary, so whether a running player stepped it or stubbed against it
    // was a float coin-flip in the engine's own step-up test. Ten under the
    // boundary, it is decisively WALKED — the verb the kerb was always meant
    // to have (One-V: "the kerb walks, the crate vaults").
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Lateral = LaneRight + (SideIndex == 0 ? -1.0f : 1.0f) * DashCorridorWidth * 0.5f;
        SpawnFieldSlab(World, Frame, LaneStart, LaneStart + 10000.0f,
            Lateral - 30.0f, Lateral + 30.0f, 35.0f, 35.0f, PaletteStone, TEXT("Runtime_SniperLaneKerb"));
    }
    // The lane's one piece of hard cover — RangedSightlineDepth of clear ground
    // behind it, the minimum a LATTICE needs to use its whole 900-1900 band
    // instead of backing into a kerb — is now placed by the cover field, which
    // is also what knows to keep every other piece off this lane.

    // --- 5. THE COVER FIELD ------------------------------------------------
    // Last, because it measures the band it is filling and the exclusions it
    // measures against are the stations built above.
    SpawnCoverField();
}

FBreakerCoverFieldParams ABreakerGameMode::MakeCoverFieldParams() const
{
    // EVERY grammar number is transported, never re-authored. Level-Design 3 is
    // the single source of truth for CoverPitchMax, CombatPocketRadius,
    // DashCorridorWidth, RangedSightlineDepth, SafeZoneRadius and ArenaDistance,
    // and the cover field is derived from them — so retuning the grammar
    // retunes the cover with it instead of leaving two numbers to drift apart.
    FBreakerCoverFieldParams Params;
    Params.CoverPitchMaxCm = CoverPitchMax;
    Params.DashCorridorWidthCm = DashCorridorWidth;
    Params.CombatPocketRadiusCm = CombatPocketRadius;
    Params.RangedSightlineDepthCm = RangedSightlineDepth;
    Params.SafeZoneRadiusCm = SafeZoneRadius;
    Params.ArenaDistanceCm = ArenaDistance;

    Params.ClusterPitchCm = CoverClusterPitch;
    Params.ClusterRingRadiusCm = CoverClusterRingRadius;
    Params.ChestHeightCm = CoverChestHeight;
    Params.FullHeightCm = CoverFullHeight;
    Params.PocketPillarHeightCm = FMath::Max(CoverFullHeight, 500.0f);
    Params.PocketInnerRingRadiusCm = CoverPitchMax * 0.5f;
    Params.WidestEnemyBodyCm = 120.0f;   // the SEVERED DRUDGE's overridden capsule

    // The contested band: from the target range's firing line to past the elite
    // arena, stopping short of the sniper lane and the wall-ride corridor.
    Params.BandNearCm = RangeFiringLineDistance - 600.0f;
    Params.BandFarCm = ArenaDistance + CombatPocketRadius;
    Params.BandHalfWidthCm = CoverBandHalfWidth;

    // The instrument corridor: the firing line, its four dummies (laterals
    // -850 .. +350) and the player's line in to the standing encounter.
    Params.CorridorNearCm = RangeFiringLineDistance - 600.0f;
    Params.CorridorFarCm = EncounterPocketDistance + CombatPocketRadius;
    Params.CorridorShoulderPitchCm = CoverPitchMax;

    // THE POCKETS. SpawnExpandedField builds four; the first is the standing
    // encounter and sits ON the corridor, so it gets flank line breaks rather
    // than a cluster, and the fourth IS the elite arena, which is authored to
    // Encounter-Design 3.3 separately.
    Params.CorridorPocketCentres.Add(FVector2D(EncounterPocketDistance, 0.0f));
    Params.PocketCentres.Add(FVector2D(EncounterPocketDistance + DashRefreshDistance, FieldHalfExtent * 0.55f));
    Params.PocketCentres.Add(FVector2D(RangeFiringLineDistance + DashRefreshDistance, -FieldHalfExtent * 0.55f));
    Params.EncounterFlankOffsetCm = FMath::Min(CoverPitchMax - 100.0f, 1600.0f);

    // THE JUMP-GAP RUN, taken from SpawnJumpGapRun's own arithmetic rather than
    // restated: trench at EncounterPocketDistance + CombatPocketRadius + 2200,
    // platforms DashCorridorWidth wide with the take-off 1600 cm behind it and
    // the trench floor 2.6 platform widths to each side.
    const float TrenchFwd = EncounterPocketDistance + CombatPocketRadius + 2200.0f;
    Params.JumpRunNearCm = TrenchFwd - 2000.0f;
    // +1200 rather than the run's full 1600 cm landing depth plus its ramp: the
    // elite arena's marker ring starts at 15000 and the third jump lane's
    // landing ends at 16400, so the two stations PHYSICALLY OVERLAP in the field
    // this pass inherited. A box drawn to the run's true far edge swallows the
    // arena's own §3.3 pillars. 16000 covers the trench, the take-offs and the
    // first two landings, and the third landing is protected instead by the
    // arena exclusion, which reaches 3904 cm from the arena centre and therefore
    // covers everything from 13096 forward. Reported to the owner as a station
    // collision rather than papered over.
    Params.JumpRunFarCm = TrenchFwd + SwiftThreeJumpGap + 1200.0f;
    Params.JumpRunHalfWidthCm = DashCorridorWidth * 2.6f;

    // THE MOVEMENT LANES, likewise taken from the spawners that build them.
    Params.SniperLaneRightCm = -FieldHalfExtent * 0.62f;
    Params.SniperLaneHalfWidthCm = DashCorridorWidth * 0.5f;
    Params.WallLaneRightCm = FieldHalfExtent * 0.62f;
    Params.WallLaneHalfWidthCm = WallRideCorridorWidth * 0.5f + 125.0f;

    // The lane's own hard cover, in the position the shipped field gave it.
    Params.LaneCoverForwardCm = RangeFiringLineDistance - 1500.0f + RangedSightlineDepth;
    Params.LaneCoverRightCm = Params.SniperLaneRightCm + 500.0f;

    // The layout is the rift's, not the session's. Unset (the gym, the
    // ordinary field) keeps the authored base so an instrument never shifts
    // under measurement; a rift mixes its own identity and level in.
    const UBreakerGameInstance* SeedSession = GetGameInstance<UBreakerGameInstance>();
    Params.Seed = SeedSession ? SeedSession->PendingRift.LayoutSeed(ModifierSeedBase) : ModifierSeedBase;
    return Params;
}

void ABreakerGameMode::SpawnCoverField()
{
    UWorld* World = GetWorld();
    if (!World || !bFieldFrameSet || !bBuildCoverField) return;

    const FBreakerCoverFieldParams Params = MakeCoverFieldParams();
    const TArray<FBreakerCoverPiece> Pieces = UBreakerCoverLayoutLibrary::BuildCoverField(Params);

    for (const FBreakerCoverPiece& Piece : Pieces)
    {
        // Sunk 5 cm so the bottom face is never coplanar with the apron. The
        // face is hidden either way, but the field has already shipped one
        // z-fighting report and the fix costs nothing. A chest-high piece is
        // still 115 cm proud, under MantleStepHeight 145, so it stays climbable.
        const float CentreZ = Piece.HeightCm * 0.5f - 5.0f;
        const FVector Location = Frame.At(Piece.Forward, Piece.Right, CentreZ);
        SpawnShape(World, ShapeCube, Location,
            FVector(Piece.HalfLengthCm * 2.0f / 100.0f, Piece.HalfDepthCm * 2.0f / 100.0f, Piece.HeightCm / 100.0f),
            FRotator(0.0f, Frame.Forward.Rotation().Yaw + Piece.YawDegrees, 0.0f),
            Piece.Class == EBreakerCoverClass::FullHeight ? PaletteConcrete : PaletteStone,
            true, Piece.Class == EBreakerCoverClass::FullHeight ? TEXT("Runtime_CoverFull") : TEXT("Runtime_CoverChest"));
        RegisterCoverAnchor(Location, Piece.Class, Piece.HeightCm);
    }

    UE_LOG(LogTemp, Display, TEXT("[BreakerGym] %s"),
        *UBreakerCoverLayoutLibrary::DescribeCoverField(Pieces, Params));
    FString Reason;
    if (!UBreakerCoverLayoutLibrary::IsLayoutLegal(Pieces, Params, Reason))
    {
        // Loud, never silent. Every rule named here has a reason written beside
        // it in Level-Design 3/4 or Encounter-Design 3.3, and a field that
        // breaks one is a field that measures the wrong thing.
        UE_LOG(LogTemp, Warning, TEXT("[BreakerGym] COVER FIELD IS ILLEGAL: %s"), *Reason);
    }
}

FVector ABreakerGameMode::FindFlankableGround(const FVector& Around, float SearchRadius) const
{
    // A SEVERED DRUDGE is answered by getting BEHIND it: its weak point is a
    // dorsal ridge at 129 cm, not a head, so the whole archetype is a circle.
    // Spawning one with its back against a 400 cm slab deletes the answer and
    // leaves a 2.0x-health body with no counterplay, which is the opposite of
    // what this archetype is for. This walks a ring of candidates and returns
    // the first whose flanking circle is clear of hard cover.
    FBreakerCoverAnchor Anchor;
    if (!CoverRegistry.FindNearest(Around, DrudgeFlankClearanceCm, Anchor)) return Around;
    for (int32 Step = 1; Step <= 12; ++Step)
    {
        // A spiral rather than a ring: bearing turns by the golden angle and the
        // radius grows, so twelve candidates cover the neighbourhood evenly
        // instead of all landing on one arc.
        const float Bearing = Step * 137.5f;
        const float Radius = DrudgeFlankClearanceCm * (0.6f + 0.5f * Step);
        if (Radius > SearchRadius) break;
        const FVector Candidate = Around
            + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Bearing, FVector::UpVector) * Radius;
        if (!CoverRegistry.FindNearest(Candidate, DrudgeFlankClearanceCm, Anchor)) return Candidate;
    }
    // No clear circle anywhere nearby. Honest answer: put it where it was asked
    // for and say so, rather than silently walking it across the field.
    UE_LOG(LogTemp, Display,
        TEXT("[BreakerGym] no cover-free circle of %.0f cm within %.0f cm of (%.0f, %.0f); the Drudge there will be harder to flank."),
        DrudgeFlankClearanceCm, SearchRadius, Around.X, Around.Y);
    return Around;
}

ABreakerAlteredEnemy* ABreakerGameMode::SpawnDrudge(const FVector& Around, float PatrolPhase, int32 AreaLevel)
{
    UWorld* World = GetWorld();
    if (!World || !bSpawnDrudges) return nullptr;

    // Z is the CALLER'S. The standing encounter builds its point on the field
    // frame; wave mode builds its arena around wherever the player is standing,
    // which may not be the frame's ground plane at all. Forcing the frame's Z
    // here would drop a wave Drudge through a platform the playtester is fighting
    // on. Enemies ground-snap every tick regardless.
    const FVector SpawnLocation = FindFlankableGround(Around, CombatPocketRadius);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    ABreakerAlteredEnemy* Drudge = World->SpawnActor<ABreakerAlteredEnemy>(
        ABreakerAlteredEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params);
    if (!Drudge) return nullptr;
    Drudge->ConfigureEncounter(SpawnLocation, PatrolPhase);
    Drudge->SetAreaLevel(AreaLevel);
    UBreakerKillTelemetryComponent::AttachTo(Drudge);
    return Drudge;
}

void ABreakerGameMode::RegisterCoverAnchor(const FVector& WorldLocation, EBreakerCoverClass Class, float HeightCm)
{
    CoverRegistry.Add(WorldLocation, Class, HeightCm);
}

bool ABreakerGameMode::FindCoverAnchorNear(const FVector& Around, float MaxDistance, FVector& OutAnchor) const
{
    // The 2D-ness and the "no cover here is a real answer" contract both live in
    // FBreakerCoverRegistry now, which is where they can be tested.
    FBreakerCoverAnchor Anchor;
    if (!CoverRegistry.FindNearest(Around, MaxDistance, Anchor)) return false;
    OutAnchor = Anchor.Location;
    return true;
}

ABreakerSkirmisherEnemy* ABreakerGameMode::SpawnSkirmisherNearCover(const FVector& Around,
    const FVector& ThreatLocation, float PatrolPhase)
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    // Stand it just BEHIND the cover relative to the threat. Its opening state
    // is Relocating, so the first thing it does is look for a point whose line
    // from the threat is blocked; starting on the blocked side means it finds
    // one on frame one instead of walking across open ground to get there.
    // FULL-HEIGHT FIRST. A 120 cm block does not break a line of sight — the
    // Skirmisher's own cover search traces from the threat and the trace is
    // real — so ducking behind chest-high cover leaves it visible and it is a
    // plain shooter again. Chest-high is the fallback rather than the answer,
    // and the log below says which one it got.
    const auto* Defaults = GetDefault<ABreakerSkirmisherEnemy>();
    const auto* Capsule = Defaults->FindComponentByClass<UCapsuleComponent>();
    if (!Capsule) return nullptr;
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    auto ResolveSafePoint = [&](const FVector& Desired, FVector& Out)
    {
        if (bFieldFrameSet)
        {
            const FBreakerCoverFieldParams Field = bActiveFieldParamsSet ? ActiveFieldParams : MakeCoverFieldParams();
            const FVector Offset = Desired - Frame.Ground;
            const float F = FVector::DotProduct(Offset, Frame.Forward);
            const float R = FVector::DotProduct(Offset, Frame.Right);
            if (F - Radius < Field.BandNearCm || F + Radius > Field.BandFarCm
                || FMath::Abs(R) + Radius > Field.BandHalfWidthCm) return false;
        }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SkirmisherSpawnFloor), false);
        FHitResult Floor;
        if (!World->LineTraceSingleByObjectType(Floor, Desired + FVector(0, 0, 3000),
            Desired - FVector(0, 0, 3000), FCollisionObjectQueryParams(ECC_WorldStatic), Query)
            || Floor.ImpactNormal.Z < .7f) return false;
        const FVector At = Floor.ImpactPoint + FVector(0, 0, HalfHeight + 2);
        if (World->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(Radius, HalfHeight), Query)) return false;
        Out = At;
        return true;
    };
    TArray<FBreakerCoverAnchor> Candidates;
    for (const FBreakerCoverAnchor& Candidate : CoverRegistry.Anchors)
        if (FVector::DistSquared2D(Candidate.Location, Around) <= FMath::Square(CoverPitchMax))
            Candidates.Add(Candidate);
    Candidates.StableSort([&](const FBreakerCoverAnchor& A, const FBreakerCoverAnchor& B)
    {
        const bool FullA = A.Class == EBreakerCoverClass::FullHeight;
        const bool FullB = B.Class == EBreakerCoverClass::FullHeight;
        if (FullA != FullB) return FullA;
        return FVector::DistSquared2D(A.Location, Around) < FVector::DistSquared2D(B.Location, Around);
    });
    FBreakerCoverAnchor Chosen;
    FVector SpawnLocation;
    bool bHasCover = false;
    for (const FBreakerCoverAnchor& Candidate : Candidates)
    {
        // Preserve the existing rear-side placement; choose another actual
        // anchor when this one has no safe space, never clamp through its wall.
        const FVector Behind = Candidate.Location + (Candidate.Location - ThreatLocation).GetSafeNormal2D() * 260.0f;
        if (!ResolveSafePoint(Behind, SpawnLocation)) continue;
        Chosen = Candidate;
        bHasCover = true;
        break;
    }
    if (!bHasCover && !ResolveSafePoint(Around, SpawnLocation))
    {
        UE_LOG(LogTemp, Error, TEXT("[BreakerWave] Skirmisher has no safe cover or fallback placement at %s."), *Around.ToString());
        return nullptr;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
    ABreakerSkirmisherEnemy* Skirmisher = World->SpawnActor<ABreakerSkirmisherEnemy>(
        ABreakerSkirmisherEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params);
    if (!Skirmisher) return nullptr;
    Skirmisher->ConfigureEncounter(SpawnLocation, PatrolPhase);
    UBreakerKillTelemetryComponent::AttachTo(Skirmisher);
    if (!bHasCover)
    {
        // Loud, because a Skirmisher with nothing to hide behind is a plain
        // shooter and the whole archetype has quietly stopped existing. That is
        // exactly the failure its own class note warns about.
        UE_LOG(LogTemp, Warning,
            TEXT("[BreakerGym] Skirmisher spawned with NO cover anchor within %.0f cm of (%.0f, %.0f) — it will degrade to an open-ground shooter."),
            CoverPitchMax, Around.X, Around.Y);
    }
    else if (Chosen.Class != EBreakerCoverClass::FullHeight)
    {
        // Not a warning: chest-high cover is legitimate ground for it, and the
        // instrument corridor carries nothing else on purpose. It is recorded
        // because a run whose Skirmishers all found chest-high cover is a
        // different measurement from one where they found line breaks.
        UE_LOG(LogTemp, Display,
            TEXT("[BreakerGym] Skirmisher anchored on CHEST-HIGH cover (%.0f cm) at (%.0f, %.0f); no line break within %.0f cm."),
            Chosen.HeightCm, Chosen.Location.X, Chosen.Location.Y, CoverPitchMax);
    }
    return Skirmisher;
}


void ABreakerGameMode::BindFernhallMissionJournal(APawn* Pawn)
{
    ABreakerCharacter* Character = Cast<ABreakerCharacter>(Pawn);
    UBreakerQuestJournal* Journal = Character ? Character->GetQuestJournal() : nullptr;
    if (!Journal) return;
    if (Journal == FernhallMissionJournal.Get())
    {
        // Save restoration intentionally emits no new-flag events. Startup
        // may bind before Character BeginPlay restores this same journal.
        if (FernhallObservedFlagCount != Journal->GetFlags().Num()) RefreshFernhallMission();
        return;
    }
    if (FernhallMissionJournal.IsValid()) FernhallMissionJournal->OnFlagSet.RemoveAll(this);
    FernhallMissionJournal = Journal;
    Journal->OnFlagSet.AddUObject(this, &ABreakerGameMode::RefreshFernhallMission);
    RefreshFernhallMission();
}

void ABreakerGameMode::RefreshFernhallMission(FName ChangedFlag)
{
    UBreakerQuestJournal* Journal = FernhallMissionJournal.Get();
    UWorld* World = GetWorld();
    if (!bFernhallMissionReady || !Journal || !World || !HasAuthority()) return;
    FernhallObservedFlagCount = Journal->GetFlags().Num();
    const bool bContactCurrent = Journal->HasFlag(TEXT("Quest.Deeper.TurnedIn"))
        && !UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(
            TEXT("fernhall.altered_contact"), Journal->GetState()).IsEmpty();
    if (bContactCurrent && !AlteredContact.IsValid() && !bAlteredContactDeathConsumed)
    {
        FHitResult Floor;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(AlteredContactFloor), false);
        if (World->LineTraceSingleByObjectType(Floor, AlteredContactPosition + FVector(0, 0, 3000),
            AlteredContactPosition - FVector(0, 0, 3000), FCollisionObjectQueryParams(ECC_WorldStatic), Query)
            && Floor.ImpactNormal.Z >= 0.7f)
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ABreakerAlteredEnemy* Contact = World->SpawnActor<ABreakerAlteredEnemy>(
                ABreakerAlteredEnemy::StaticClass(), Floor.ImpactPoint + FVector(0, 0, 200), FRotator::ZeroRotator, Parameters);
            if (Contact)
            {
                UCapsuleComponent* Capsule = Contact->FindComponentByClass<UCapsuleComponent>();
                if (!Capsule) { Contact->Destroy(); return; }
                const FVector At = Floor.ImpactPoint + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 2);
                Query.AddIgnoredActor(Contact);
                if (World->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn,
                    FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query))
                    Contact->Destroy();
                else
                {
                    Contact->SetActorLocation(At);
                    Contact->ConfigureWave(16); // O2 Act II contact band; finite, outside the wave controller.
                    Contact->ConfigureEncounter(At, 0);
                    Contact->Tags.Add(TEXT("Fernhall.AlteredContact"));
                    AlteredContact = Contact;
                    UE_LOG(LogTemp, Display, TEXT("[Mission] dedicated Fernhall contact spawned at %s"), *At.ToString());
                    bAlteredContactWoundApplied = false;
                    ApplyAlteredContactWound();
                    if (UBreakerCombatComponent* Combat = Contact->FindComponentByClass<UBreakerCombatComponent>())
                        Combat->OnDeath.AddDynamic(this, &ABreakerGameMode::HandleAlteredContactDeath);
                }
            }
        }
    }
    if (!BreachDoor.IsValid() && Journal->HasFlag(TEXT("Quest.AlteredContact.TurnedIn"))
        && Journal->HasFlag(TEXT("Quest.Breach.Accepted")))
    {
        FHitResult Floor;
        if (World->LineTraceSingleByObjectType(Floor, BreachDoorPosition + FVector(0, 0, 3000),
            BreachDoorPosition - FVector(0, 0, 3000), FCollisionObjectQueryParams(ECC_WorldStatic))
            && Floor.ImpactNormal.Z >= 0.7f)
        {
            ABreakerRiftDoor* Door = World->SpawnActor<ABreakerRiftDoor>(ABreakerRiftDoor::StaticClass(),
                FTransform(FRotator::ZeroRotator, Floor.ImpactPoint + FVector(0, 0, 100)));
            if (Door)
            {
                Door->Rift = UBreakerZoneBuilder::FernhallRiftFor(TEXT("breach"));
                Door->OnRiftEntryRequested.AddUObject(this, &ABreakerGameMode::HandleRiftEntryRequested);
                BreachDoor = Door;
                UE_LOG(LogTemp, Display, TEXT("[Mission] earned Breach entrance restored at %s"), *Door->GetActorLocation().ToString());
            }
        }
    }
}

void ABreakerGameMode::ApplyAlteredContactWound()
{
    ABreakerAlteredEnemy* Contact = AlteredContact.Get();
    if (!Contact || !Contact->HasActorBegunPlay() || bAlteredContactWoundApplied || bAlteredContactDeathConsumed) return;
    // Initial map construction can precede BeginPlay, which restores the
    // chassis. Apply once after that restore; never heal subsequent damage.
    if (UAbilitySystemComponent* ASC = Contact->GetAbilitySystemComponent())
    {
        bAlteredContactWoundApplied = true;
        const float Health = ASC->GetNumericAttribute(UBreakerAttributeSet::GetHealthAttribute());
        ASC->SetNumericAttributeBase(UBreakerAttributeSet::GetHealthAttribute(),
            FMath::Min(Health, Contact->GetMonsterMaxHealth() * 0.35f));
    }
}

void ABreakerGameMode::HandleAlteredContactDeath()
{
    UBreakerQuestJournal* Journal = FernhallMissionJournal.Get();
    const ABreakerAlteredEnemy* Contact = AlteredContact.Get();
    const UBreakerCombatComponent* Combat = Contact ? Contact->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!HasAuthority() || !Journal || bAlteredContactDeathConsumed || !Combat || !Combat->IsDead()) return;
    bAlteredContactDeathConsumed = true;
    for (FName Flag : UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(
        TEXT("fernhall.altered_contact"), Journal->GetState())) Journal->SetFlag(Flag);
}

ABreakerNPC* ABreakerGameMode::SpawnFinaleResident(FName RowId, const FVector& At, const FRotator& Facing)
{
    const FBreakerDialogueRow* Row = ABreakerNPC::GetDialogueData().Npcs.FindByPredicate(
        [RowId](const FBreakerDialogueRow& Entry) { return Entry.Id == RowId; });
    if (!Row || !GetWorld()) return nullptr;
    const FTransform Transform(Facing, At);
    ABreakerNPC* NPC = GetWorld()->SpawnActorDeferred<ABreakerNPC>(ABreakerNPC::StaticClass(), Transform,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!NPC) return nullptr;
    NPC->DialogueId = Row->Id;
    NPC->DisplayName = FText::FromString(Row->DisplayName);
    NPC->StartNodeId = Row->StartNodeId; NPC->DialogueNodes = Row->Nodes; NPC->EntryOverrides = Row->Entries;
    NPC->Tags.Add(RowId);
    // A PERSON, NOT A CUBE WITH A SPHERE ON TOP. Owner-asked: "replace the
    // random npc in fernhall with one of the human assets we have for the time
    // being". Everyone who arrives through this path is somebody the player
    // walks up to and talks to, so everyone gets a body and a talking idle;
    // the two named Anchor spawners already did, and this generic one was the
    // reason the one person in the field did not.
    //
    // The male base rather than the female one, because Kess wears the female
    // base in the hub and two people ought to be told apart at a glance.
    // AlternateSelf overrides both below - it is the PLAYER, and wears what
    // the player is wearing.
    NPC->ApplyHumanBody(EBreakerNPCBody::Male, EBreakerNPCIdle::Talking);
    if (RowId == FName(TEXT("AlternateSelf")))
    {
        // The same currently equipped character mesh, presented as a living
        // person. This actor has no combat component or enemy controller.
        const ACharacter* Player = GetWorld()->GetFirstPlayerController()
            ? Cast<ACharacter>(GetWorld()->GetFirstPlayerController()->GetPawn()) : nullptr;
        if (Player && Player->GetMesh() && Player->GetMesh()->GetSkeletalMeshAsset())
            NPC->BodyMeshAsset = FSoftObjectPath(Player->GetMesh()->GetSkeletalMeshAsset());
        else NPC->BodyMeshAsset = FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
        // ApplyCharacterBody uses this mannequin family. A plain NPC cannot
        // drive the Character-owned animation blueprint, so use its unarmed idle.
        NPC->BodyIdleAnimation = FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
        NPC->BodyMeshRotation = FRotator(0, -90, 0);
    }
    NPC->FinishSpawning(Transform);
    return NPC;
}

void ABreakerGameMode::BuildFinaleEarth(APawn* Player, bool bWinning)
{
    UWorld* World = GetWorld();
    if (!World || !Player) return;
    const FBreakerFinaleEarthLayout Layout = bWinning ? UBreakerFinaleEarthBuilder::BuildWon(World)
        : UBreakerFinaleEarthBuilder::BuildStripped(World);
    Player->TeleportTo(Layout.PlayerArrival, Layout.ArrivalFacing);
    if (AController* Controller = Player->GetController()) Controller->SetControlRotation(Layout.ArrivalFacing);
    if (bWinning)
    {
        ABreakerNPC* Alternate = SpawnFinaleResident(TEXT("AlternateSelf"), Layout.InteractionLocation, Layout.InteractionFacing);
        if (Alternate && FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureAlternate")))
        {
            const FVector At = Alternate->GetActorLocation() + Alternate->GetActorForwardVector() * 450 + FVector(0, 0, 10);
            const FRotator Facing = (Alternate->GetActorLocation() - At).Rotation();
            Player->TeleportTo(At, Facing);
            if (AController* Controller = Player->GetController()) Controller->SetControlRotation(Facing);
        }
    }
    else
    {
        const FTransform Transform(Layout.InteractionFacing, Layout.InteractionLocation);
        ABreakerFinaleActor* Fragment = World->SpawnActorDeferred<ABreakerFinaleActor>(ABreakerFinaleActor::StaticClass(),
            Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (Fragment) { Fragment->ConfigureFragment(Layout.PocketCount); Fragment->FinishSpawning(Transform); }
        FinaleFragment = Fragment;
        bFinaleRosterValid = Fragment && Layout.PocketCount > 0 && !Layout.Enemies.IsEmpty();
        for (const FBreakerFinaleEnemySpawn& Spawn : Layout.Enemies)
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(Spawn.EnemyClass, Spawn.Location, FRotator::ZeroRotator, Parameters);
            if (!Enemy) { bFinaleRosterValid = false; continue; }
            Enemy->ConfigureWave(40); // O2 first finale recovery area.
            UCapsuleComponent* Capsule = Enemy->FindComponentByClass<UCapsuleComponent>();
            FCollisionQueryParams Query(SCENE_QUERY_STAT(FinaleEnemyFloor), false, Enemy);
            FHitResult Floor;
            if (!Capsule || !World->LineTraceSingleByObjectType(Floor, Spawn.Location + FVector(0, 0, 1000),
                Spawn.Location - FVector(0, 0, 1000), FCollisionObjectQueryParams(ECC_WorldStatic), Query) || Floor.ImpactNormal.Z < 0.7f)
            { Enemy->Destroy(); bFinaleRosterValid = false; continue; }
            const FVector At = Floor.ImpactPoint + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 2);
            if (World->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query))
            { Enemy->Destroy(); bFinaleRosterValid = false; continue; }
            Enemy->SetActorLocation(At); Enemy->ConfigureEncounter(At, FinaleEnemies.Num() * 1.3f);
            Enemy->Tags.Add(TEXT("Finale.FragmentGuard"));
            UBreakerKillTelemetryComponent::AttachTo(Enemy);
            FinaleEnemies.Add(Enemy); FinaleEnemyPockets.Add(Spawn.PocketIndex); FinaleEnemyDeaths.Add(false);
        }
    }
    for (const FVector At : {Layout.PlayerArrival + FVector(0, 450, 0), Layout.InteractionLocation + FVector(0, 500, 0)})
    {
        if (ABreakerTravelPoint* Gate = World->SpawnActor<ABreakerTravelPoint>(At, FRotator::ZeroRotator))
        {
            Gate->ExcludedDestinationId = bWinning ? ABreakerTravelPoint::WinningEarthDestinationId : ABreakerTravelPoint::StrippedEarthDestinationId;
            Gate->OnDestinationSelected.AddUObject(this, &ABreakerGameMode::HandleHubTravelSelected);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("[Finale] %s built: %d finite enemies, rosterValid=%d"),
        bWinning ? TEXT("Winning Earth") : TEXT("Stripped Earth"), FinaleEnemies.Num(), bFinaleRosterValid);
}

void ABreakerGameMode::TickFinaleMission()
{
    UWorld* World = GetWorld();
    ABreakerCharacter* Player = World && World->GetFirstPlayerController()
        ? Cast<ABreakerCharacter>(World->GetFirstPlayerController()->GetPawn()) : nullptr;
    UBreakerQuestJournal* Journal = Player ? Player->GetQuestJournal() : nullptr;
    if (!HasAuthority() || !Journal) return;
    if (UBreakerGameInstance::IsAnchorMap(this))
    {
        if (!bResearcherSpawned && Journal->HasFlag(TEXT("Quest.Survivor.TurnedIn")))
            bResearcherSpawned = SpawnFinaleResident(TEXT("Researcher"), HubOrigin - Frame.Right * 650
                + Frame.Forward * 350 + FVector(0, 0, 90), (-Frame.Forward).Rotation()) != nullptr;
        if (!bFinaleDeviceSpawned && Journal->HasFlag(TEXT("Quest.Finale.ReturnedFromWon")))
        {
            const FTransform Transform((-Frame.Forward).Rotation(), HubOrigin - Frame.Right * 650
                + Frame.Forward * 700 + FVector(0, 0, 90));
            ABreakerFinaleActor* Device = World->SpawnActorDeferred<ABreakerFinaleActor>(ABreakerFinaleActor::StaticClass(),
                Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
            if (Device) { Device->ConfigureDevice(); Device->FinishSpawning(Transform); bFinaleDeviceSpawned = true; }
        }
    }
    ABreakerFinaleActor* Fragment = FinaleFragment.Get();
    if (!Fragment || !bFinaleRosterValid) return;
    for (int32 Index = 0; Index < FinaleEnemies.Num(); ++Index)
    {
        if (FinaleEnemyDeaths[Index]) continue;
        ABreakerEnemy* Enemy = FinaleEnemies[Index].Get();
        const UBreakerCombatComponent* Combat = Enemy ? Enemy->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (Combat && Combat->IsDead()) FinaleEnemyDeaths[Index] = true;
        else if (!Enemy) bFinaleRosterValid = false;
    }
    TSet<int32> Pockets;
    for (int32 Pocket : FinaleEnemyPockets) Pockets.Add(Pocket);
    for (int32 Pocket : Pockets)
    {
        bool bCleared = bFinaleRosterValid;
        for (int32 Index = 0; Index < FinaleEnemies.Num(); ++Index)
            if (FinaleEnemyPockets[Index] == Pocket) bCleared &= FinaleEnemyDeaths[Index];
        Fragment->SetPocketCleared(Pocket, bCleared);
    }
}

void ABreakerGameMode::BuildSurvivorMission(APawn* Player)
{
    UWorld* World = GetWorld();
    if (!World || !Player || MissionSurvivor.IsValid()) return;
    const FBreakerErasedEarthLayout Layout = UBreakerErasedEarthBuilder::Build(World);
    SurvivorExtraction = Layout.Extraction;
    Player->TeleportTo(Layout.PlayerArrival, Layout.ArrivalFacing);
    if (AController* Controller = Player->GetController()) Controller->SetControlRotation(Layout.ArrivalFacing);
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABreakerSurvivor* Survivor = World->SpawnActor<ABreakerSurvivor>(Layout.SurvivorShelter, Layout.ArrivalFacing, Parameters);
    MissionSurvivor = Survivor;
    if (Survivor)
    {
        Survivor->ConfigureEscort(Layout);
        Survivor->OnEscortReadyForExtraction.AddUObject(this, &ABreakerGameMode::HandleSurvivorExtraction);
    }
    bSurvivorRosterValid = Survivor && Layout.PocketCount == 3 && !Layout.Enemies.IsEmpty();
    for (const FBreakerErasedEarthEnemySpawn& Spawn : Layout.Enemies)
    {
        ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(Spawn.EnemyClass, Spawn.Location, FRotator::ZeroRotator, Parameters);
        if (!Enemy) { bSurvivorRosterValid = false; continue; }
        Enemy->ConfigureWave(30); // O2: Act III's first erased Earth.
        UCapsuleComponent* Capsule = Enemy->FindComponentByClass<UCapsuleComponent>();
        FHitResult Floor;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SurvivorEnemyFloor), false, Enemy);
        const bool bFloor = World->LineTraceSingleByObjectType(Floor, Spawn.Location + FVector(0, 0, 1000),
            Spawn.Location - FVector(0, 0, 1000), FCollisionObjectQueryParams(ECC_WorldStatic), Query);
        if (!Capsule || !bFloor || Floor.ImpactNormal.Z < 0.7f)
        { Enemy->Destroy(); bSurvivorRosterValid = false; continue; }
        const FVector At = Floor.ImpactPoint + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 2);
        if (World->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query))
        { Enemy->Destroy(); bSurvivorRosterValid = false; continue; }
        Enemy->SetActorLocation(At);
        Enemy->ConfigureEncounter(At, SurvivorEnemies.Num() * 1.3f);
        Enemy->Tags.Add(FName(*FString::Printf(TEXT("Survivor.Pocket.%d"), Spawn.PocketIndex)));
        UBreakerKillTelemetryComponent::AttachTo(Enemy);
        SurvivorEnemies.Add(Enemy);
        SurvivorEnemyPockets.Add(Spawn.PocketIndex);
        SurvivorEnemyDeaths.Add(false);
    }
    for (const FVector At : {Layout.PlayerArrival + FVector(0, 450, 0), Layout.Extraction + FVector(0, 450, 0)})
    {
        if (ABreakerTravelPoint* Gate = World->SpawnActor<ABreakerTravelPoint>(At, FRotator::ZeroRotator, Parameters))
        {
            Gate->ExcludedDestinationId = ABreakerTravelPoint::ErasedEarthDestinationId;
            Gate->OnDestinationSelected.AddUObject(this, &ABreakerGameMode::HandleHubTravelSelected);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("[Survivor] Quiet Earth built: %d finite Vestiges, rosterValid=%d."), SurvivorEnemies.Num(), bSurvivorRosterValid);
}

void ABreakerGameMode::TickSurvivorMission()
{
    UWorld* World = GetWorld();
    ABreakerCharacter* Player = World && World->GetFirstPlayerController()
        ? Cast<ABreakerCharacter>(World->GetFirstPlayerController()->GetPawn()) : nullptr;
    UBreakerQuestJournal* Journal = Player ? Player->GetQuestJournal() : nullptr;
    if (!HasAuthority() || !Journal) return;
    if (UBreakerGameInstance::IsAnchorMap(this) && !bAnchorSurvivorSpawned
        && Journal->HasFlag(TEXT("Quest.Survivor.ReachedAnchor")))
    {
        FActorSpawnParameters Parameters;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        // The rescued person remains a physical resident after the reward.
        const FVector At = HubOrigin + Frame.Right * 650 + Frame.Forward * 350 + FVector(0, 0, 90);
        bAnchorSurvivorSpawned = World->SpawnActor<ABreakerSurvivor>(At, (-Frame.Forward).Rotation(), Parameters) != nullptr;
    }
    ABreakerSurvivor* Survivor = MissionSurvivor.Get();
    if (!Survivor || !bSurvivorRosterValid) return;
    for (int32 Index = 0; Index < SurvivorEnemies.Num(); ++Index)
    {
        if (SurvivorEnemyDeaths[Index]) continue;
        ABreakerEnemy* Enemy = SurvivorEnemies[Index].Get();
        const UBreakerCombatComponent* Combat = Enemy ? Enemy->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (Combat && Combat->IsDead()) SurvivorEnemyDeaths[Index] = true;
        else if (!Enemy) bSurvivorRosterValid = false; // Despawn is not a kill.
    }
    for (int32 Pocket = 0; Pocket < 3; ++Pocket)
    {
        bool bHasMembers = false, bCleared = bSurvivorRosterValid;
        for (int32 Index = 0; Index < SurvivorEnemies.Num(); ++Index)
            if (SurvivorEnemyPockets[Index] == Pocket)
            { bHasMembers = true; bCleared &= SurvivorEnemyDeaths[Index]; }
        Survivor->SetPocketCleared(Pocket, bHasMembers && bCleared);
    }
}

void ABreakerGameMode::HandleSurvivorExtraction(ABreakerSurvivor* Survivor, ABreakerCharacter* Player)
{
    UBreakerQuestJournal* Journal = Player ? Player->GetQuestJournal() : nullptr;
    const UBreakerCombatComponent* Combat = Player ? Player->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!HasAuthority() || !UBreakerGameInstance::IsErasedEarthMap(this) || !Journal || !Combat || Combat->IsDead()
        || Survivor != MissionSurvivor.Get() || !Survivor || Survivor->GetEscortPlayer() != Player
        || !Survivor->IsAtExtraction() || !Survivor->AreAllPocketsCleared() || !bSurvivorRosterValid
        || SurvivorEnemyDeaths.Contains(false)
        || FVector::Dist(Player->GetActorLocation(), SurvivorExtraction) > Survivor->ExtractionRadius
        || FVector::Dist(Survivor->GetActorLocation(), SurvivorExtraction) > Survivor->ExtractionRadius) return;
    for (FName Flag : UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(
        TEXT("earth.survivor_extraction"), Journal->GetState())) Journal->SetFlag(Flag);
    UE_LOG(LogTemp, Display, TEXT("[Survivor] Physical extraction verified; return to Anchor remains required."));
}

// ---------------------------------------------------------------------------
// REPOPULATION. The ordinary world has patrols and they come back; a rift is an
// instance with a completion condition and stays finite. Owner-ruled, and it
// REPLACES the reasoning of the older roam ruling rather than merely reversing
// it: finiteness used to be what made the roam and the rift feel like different
// verbs, because they are the same ground. That work now falls to the rift's
// objective and its dilapidated dressing, which separate the two far harder
// than a population count did.
//
// Deliberately NOT the per-body self-respawn that already ships
// (ABreakerEnemy::bRespawns, live today on the gym's standing encounter): that
// path returns a body three seconds after death wherever it fell, with no
// distance gate and no pace. The mode owns this clock because the gate and the
// cadence ARE the feature.
// ---------------------------------------------------------------------------
void ABreakerGameMode::TickOutdoorRepopulation(float DeltaSeconds)
{
    if (bRiftInstance || OutdoorSlots.IsEmpty()) return;
    UWorld* World = GetWorld();
    if (!World || !HasAuthority()) return;

    OutdoorRepopulationCountdown = FMath::Max(0.0f, OutdoorRepopulationCountdown - DeltaSeconds);

    const APlayerController* Viewer = World->GetFirstPlayerController();
    const APawn* Player = Viewer ? Viewer->GetPawn() : nullptr;
    if (!Player) return;
    const FVector PlayerAt = Player->GetActorLocation();

    // A slot is empty when its occupant is gone OR dead: a corpse lying in the
    // pocket has already stopped being a fight.
    FTimerManager& Timers = World->GetTimerManager();
    TArray<float, TInlineAllocator<32>> Candidates;
    Candidates.Reserve(OutdoorSlots.Num());
    for (FBreakerOutdoorSlot& Slot : OutdoorSlots)
    {
        const ABreakerEnemy* Standing = Slot.Occupant.Get();
        const bool bHeld = Standing && !Standing->IsDeadEnemy();
        // A slot whose body is still coming through its tear is not held and
        // not a candidate: the wait it has accrued stands, so a refused arrival
        // returns it to the queue at the head rather than the tail.
        const bool bArriving = Timers.IsTimerActive(Slot.ArrivalTimer);
        Slot.EmptySeconds = bHeld ? 0.0f : Slot.EmptySeconds + DeltaSeconds;
        // A slot the player is standing near is no candidate this frame, but it
        // KEEPS the wait it has accrued — walking away must not restart a clock
        // that has already run, or a patrolled route could never recover.
        const bool bClear = BreakerRepopulation::IsClearOfPlayer(
            FVector::DistSquared(Slot.AppearsAt(), PlayerAt), OutdoorRepopulationClearanceCm);
        Candidates.Add(bHeld || bArriving || !bClear ? 0.0f : Slot.EmptySeconds);
    }

    if (OutdoorRepopulationCountdown > 0.0f) return;
    const int32 Due = BreakerRepopulation::NextDueSlot(Candidates, OutdoorRepopulationDelaySeconds);
    if (Due == INDEX_NONE) return;
    if (RefillOutdoorSlot(Due))
    {
        OutdoorRepopulationCountdown = OutdoorRepopulationDelaySeconds;
    }
}

bool ABreakerGameMode::RefillOutdoorSlot(int32 SlotIndex)
{
    UWorld* World = GetWorld();
    if (!World || !OutdoorSlots.IsValidIndex(SlotIndex)) return false;
    FBreakerOutdoorSlot& Slot = OutdoorSlots[SlotIndex];
    if (!Slot.Class) return false;
    // A TEAR IS AN EVENT (O274): it opens where a patrol is about to return,
    // and the body comes through once it has opened. So the claim is two
    // steps — open now, arrive AppearSeconds later — and an open tear is a
    // promise the arrival keeps or, if the ground has been taken in the
    // meantime, closes again without a body. A pocket with no tear keeps the
    // O268 arrival exactly: the body appears at once.
    ABreakerPocketRift* Tear = Slot.Rift.Get();
    const float Wait = Tear ? ABreakerPocketRift::AppearSeconds : 0.0f;
    if (Tear) Tear->Open();
    if (Wait <= 0.0f)
    {
        // No timer for a zero wait: the body exists before this returns, so a
        // caller reading the slot sees the same thing a timer would have left.
        return ArriveAtOutdoorSlot(SlotIndex) != nullptr;
    }
    World->GetTimerManager().SetTimer(Slot.ArrivalTimer,
        FTimerDelegate::CreateWeakLambda(this, [this, SlotIndex]()
        {
            ArriveAtOutdoorSlot(SlotIndex);
        }), Wait, false);
    return true;
}

ABreakerEnemy* ABreakerGameMode::ArriveAtOutdoorSlot(int32 SlotIndex)
{
    UWorld* World = GetWorld();
    if (!World || !OutdoorSlots.IsValidIndex(SlotIndex)) return nullptr;
    FBreakerOutdoorSlot& Slot = OutdoorSlots[SlotIndex];
    ABreakerPocketRift* Tear = Slot.Rift.Get();
    if (!Slot.Class) return nullptr;
    // The floor under this slot was traced when the area was built and has not
    // moved, but the world has had a fight in it since: a corpse, a deployable
    // or another body can be standing here now. Refuse and try again rather
    // than pushing a patrol into something. Tested HERE, at the moment of the
    // spawn rather than at the claim, because the world had AppearSeconds to
    // change in between and the body is placed into the world as it is now.
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FernhallRepopulation), false);
    if (ABreakerEnemy* Fallen = Slot.Occupant.Get()) Query.AddIgnoredActor(Fallen);
    const ABreakerEnemy* Template = GetDefault<ABreakerEnemy>(Slot.Class);
    const UCapsuleComponent* Body = Template ? Template->FindComponentByClass<UCapsuleComponent>() : nullptr;
    const FVector Appears = Slot.AppearsAt();
    if (Body && World->OverlapBlockingTestByChannel(Appears, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(Body->GetScaledCapsuleRadius(), Body->GetScaledCapsuleHalfHeight()), Query))
    {
        // THE PROMISE IS NOT KEPT, SO THE TEAR SAYS SO. Unless another body is
        // still on its way through the same tear, in which case it stays open
        // for that one; a tear that shut in front of a pending arrival would
        // have to tear open again a moment later.
        if (Tear)
        {
            bool bOthersArriving = false;
            for (int32 Other = 0; Other < OutdoorSlots.Num() && !bOthersArriving; ++Other)
            {
                bOthersArriving = Other != SlotIndex && OutdoorSlots[Other].Rift.Get() == Tear
                    && World->GetTimerManager().IsTimerActive(OutdoorSlots[Other].ArrivalTimer);
            }
            if (!bOthersArriving) Tear->Close();
        }
        return nullptr;
    }
    // A body that walks in through a door FACES ITS ROUTE, not the facing its
    // post was authored with — it is arriving, not standing. A body appearing
    // at its post keeps the authored facing, because that is the placement.
    const FVector ToPost = (Slot.Home - Appears).GetSafeNormal2D();
    const FRotator Facing = Slot.bHasArrival && !ToPost.IsNearlyZero() ? ToPost.Rotation() : Slot.Facing;
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABreakerEnemy* Patrol = World->SpawnActor<ABreakerEnemy>(Slot.Class, Appears, Facing, Parameters);
    if (!Patrol)
    {
        if (Tear) Tear->CloseAfter(OutdoorTearCloseAfterSeconds);
        return nullptr;
    }
    // Exactly what the authored placement gave this slot, including the elite
    // that a quest's elite-gated objective counts. ConfigureWave keeps the
    // per-body self-respawn off: this clock owns the return, not the corpse.
    Patrol->ConfigureWave(Slot.AreaLevel);
    if (Slot.bElite) Patrol->ConfigureElite();
    // THE POST IS THE LEASH, AND THAT IS WHAT MAKES THE BODY WALK. The patrol
    // target is derived from the leash origin, so a body that appeared at the
    // doorway with its post as its origin walks to the post on the shipped
    // patrol path — no new movement code, no scripted route, and it fights
    // from wherever it has got to if the player interrupts the walk.
    Patrol->ConfigureEncounter(Slot.Home, Slot.PatrolPhase);
    if (!Slot.PocketTag.IsNone())
    {
        Patrol->Tags.Add(Slot.PocketTag);
    }
    UBreakerKillTelemetryComponent::AttachTo(Patrol);
    // The same protection every other arrival gets: undeletable and not yet
    // hunting for EmergenceProtectedSeconds — the body emerges before it hunts.
    Patrol->GrantEmergenceWindow();
    // AND THE TEAR ANSWERS. The flare is fired here rather than at the claim,
    // because this is the first line at which a body definitely exists: the
    // overlap test above refuses and returns, and a rift that flashed for a
    // refusal would be lying about what came through. Then the close is armed
    // — or pushed back, if another body came through a moment ago — so the
    // tear closes after the LAST arrival and the world at rest shows no tears.
    if (Tear)
    {
        Tear->Flare();
        Tear->CloseAfter(OutdoorTearCloseAfterSeconds);
    }
    Slot.Occupant = Patrol;
    Slot.EmptySeconds = 0.0f;
    return Patrol;
}

void ABreakerGameMode::SpawnFernhallEncounters(const FBreakerZoneMarkers& Markers)
{
    UWorld* World = GetWorld();
    if (!World || bRiftInstance) return;
    // Reuse the shipped pre-boss roster, divided across ground rather than
    // arriving as a wave: eight melee (one elite), a Lattice, a Skirmisher,
    // and a Warden. Existing detection/leash behavior starts each encounter;
    // the visible patrols exist before the player rounds the corner.
    const FBreakerWaveComposition Roster = UBreakerWaveBudgetLibrary::SolveWave(
        2, 1, UBreakerWaveBudgetLibrary::MakeRiftWaveBudget(3));
    // A SECOND SOLVED WAVE, for the two pockets the yards did not have. The
    // roster stays SOLVED rather than authored — hand-placing bodies to make a
    // place feel busy is how a destination stops agreeing with the budget the
    // rest of the game is measured against.
    //
    // WAVE ONE, not three. Three is the boss wave of this budget and solves to
    // no trash at all, which is exactly what the first attempt placed: two
    // empty pockets and a yard that looked identical. Wave one is twelve plain
    // Skitters with no Lattice, Skirmisher, Warden or elite in it — the right
    // shape for the ground between the set pieces, and the reason the two
    // fights that carry a rank stay pockets 1 and 2.
    const FBreakerWaveComposition Second = UBreakerWaveBudgetLibrary::SolveWave(
        1, 1, UBreakerWaveBudgetLibrary::MakeRiftWaveBudget(3));
    // A THIRD SOLVED WAVE FOR THE SIDING (O276), because the second is spent:
    // its twelve stand in the four quiet pockets already. Wave one again —
    // trash only, no rank, no Lattice — for the same reason the quiet pockets
    // took it: the fourth yard is ground between the plaza and its own door,
    // and a set piece there is the owner's to ask for once he has walked it.
    const FBreakerWaveComposition Siding = UBreakerWaveBudgetLibrary::SolveWave(
        1, 1, UBreakerWaveBudgetLibrary::MakeRiftWaveBudget(3));
    // FIVE POCKETS, THREE IN THE ENTRY YARD AND TWO IN THE SUBSTATION. Before
    // this the whole persistent world held fifteen bodies and a player crossed
    // two 106 m yards meeting three fights; the far half of each yard was
    // walked and never contested. Fractions are spread across the validated
    // band rather than clustered, so the ground between them is the reason to
    // keep moving. All O2 PLACEHOLDER.
    //
    // EIGHT NOW, BECAUSE THERE ARE THREE YARDS. The DEPOT is the third place in
    // the world and it opens with three pockets of its own: two flanking
    // fights of plain melee and, in the middle, the rank the deepest yard has
    // to carry. Owner-asked — "expand the size a lot as well and add more
    // pockets" — and the expansion is a whole yard rather than a wider one,
    // because the composer's own note records what a yard authored by eye does
    // to the cover grammar.
    //
    // NOTHING WAS ADDED TO THE ENTRY YARD, and that is the XP economy rather
    // than taste: a cleared entry yard has to stay under the 279 that reaches
    // level two, or the first contract's turn-in stops being what levels the
    // player. The depot is two seams away and the player arrives there long
    // past that moment.
    //
    // ELEVEN WITH THE SIDING (O276). The fourth yard hangs off the entry
    // plaza's west flank and opens with three pockets on the ENTRY yard's
    // pattern — two on the lane at 0.25 and 0.70, one off it at 0.45 — each a
    // quarter of the siding's own solved wave: nine plain Skitters at level 6,
    // no rank among them. WHAT THIS ADDS TO THE 279 LINE IS NOT NOTHING, and
    // it is recorded here rather than hidden: the entry yard itself is
    // unchanged and still clears under the line, but the siding is ONE seam
    // from the plaza, not two, so a player who clears it before turning in
    // the first contract carries nine level-6 kills into that moment. Whether
    // that is the side room paying for itself or the contract arriving late
    // is the owner's to feel; the numbers are not tuned here. O2 PLACEHOLDER.
    const FName Yards[] = { NAME_None, NAME_None, FName(TEXT("substation")),
                            NAME_None, FName(TEXT("substation")),
                            FName(TEXT("depot")), FName(TEXT("depot")), FName(TEXT("depot")),
                            FName(TEXT("siding")), FName(TEXT("siding")), FName(TEXT("siding")) };
    const float Fractions[] = { 0.25f, 0.70f, 0.50f, 0.45f, 0.80f, 0.25f, 0.55f, 0.85f,
                                0.25f, 0.70f, 0.45f };
    // OFF THE LANE, and that is the point of the two new ones. The original
    // three sit on the yard's centreline, so the whole fight of Fernhall
    // happened in a strip down the middle and the flanks were scenery you ran
    // past. A yard band is 4000 cm wide against a 1800 cm dash corridor, so
    // 1400 puts a pocket clear of the corridor and still well inside the band
    // — genuinely different ground, reached by leaving the lane.
    //
    // It also buys the separation that depth alone could not: the entry band
    // is 7500 cm with fights already at 0.25 and 0.70, so a third between
    // them can never be more than ~1690 cm from both. Lateral offset makes
    // that distance two-dimensional. All O2 PLACEHOLDER.
    // The siding's off-lane pocket takes the OTHER flank from the entry yard's
    // (pocket 3 at +1400), so the two side rooms a player meets first do not
    // read as one layout mirrored. O2 PLACEHOLDER.
    const float Laterals[] = { 0.0f, 0.0f, 0.0f, 1400.0f, -1400.0f, 1400.0f, -1500.0f, 900.0f,
                               0.0f, 0.0f, -1400.0f };
    constexpr int32 PocketCount = 11;
    static_assert(UE_ARRAY_COUNT(Yards) == PocketCount && UE_ARRAY_COUNT(Fractions) == PocketCount
        && UE_ARRAY_COUNT(Laterals) == PocketCount, "every pocket has a yard, a fraction and a lateral");
    TArray<FBreakerZonePiece> YardPieces;
    UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(), YardPieces);
    int32 Spawned = 0;
    for (int32 Pocket = 0; Pocket < PocketCount; ++Pocket)
    {
        FVector2D Origin2D, Forward2D;
        if (!UBreakerZoneBuilder::YardFrame(Markers, Yards[Pocket], Origin2D, Forward2D))
        {
            UE_LOG(LogTemp, Error, TEXT("[Fernhall] outdoor pocket %d has no yard frame."), Pocket);
            continue;
        }
        const FVector Forward(Forward2D.X, Forward2D.Y, 0);
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
        const FBreakerCoverFieldParams Field = UBreakerZoneBuilder::FernhallFieldParams(Yards[Pocket]);
        const FVector Center = FVector(Origin2D.X, Origin2D.Y, 0)
            + Forward * FMath::Lerp(Field.BandNearCm, Field.BandFarCm, Fractions[Pocket])
            + Right * Laterals[Pocket];
        // THE YARD'S OWN LEVEL, not a rift's. The depot has no door, so asking
        // FernhallRiftFor for its level would have quietly handed back the
        // entry yard's — a third yard two seams deep populated at level five.
        const int32 AreaLevel = UBreakerZoneBuilder::FernhallYardAreaLevel(Yards[Pocket]);
        TArray<ABreakerEnemy*> PocketMembers;
        // Where this pocket's slots start, so the tear placed below can reach
        // exactly the bodies it is the source of and no others.
        const int32 SlotFirst = OutdoorSlots.Num();
        int32 Placement = 0;
        auto Spawn = [&](TSubclassOf<ABreakerEnemy> Class, bool bElite)
        {
            const int32 Index = Placement++;
            // Compact formations stay in the authored clear corridor. Offsets
            // separate capsules while leaving the shoulder cover usable.
            FVector Desired = Center + Forward * ((Index / 3) * 300.0f - 150.0f)
                + Right * ((Index % 3 - 1) * 300.0f);
            if (Pocket == 2 || Pocket == 6)
            {
                // Warden holds the approach; two melee bodies pressure its
                // flanks while the Skirmisher uses an authored line break.
                // The depot's set piece borrows the same formation and simply
                // has no Skirmisher, so that branch never fires for it.
                Desired = Center - Forward * 200.0f;
                if (Class == ABreakerEnemy::StaticClass())
                    Desired += Right * (Index == 1 ? -550.0f : 550.0f) - Forward * 250.0f;
                if (Class == ABreakerSkirmisherEnemy::StaticClass())
                {
                    const FBreakerZonePiece* Cover = nullptr;
                    double BestDistance = TNumericLimits<double>::Max();
                    for (const FBreakerZonePiece& Piece : YardPieces)
                    {
                        if (!Piece.Name.StartsWith(TEXT("blk_full_"))
                            || UBreakerZoneBuilder::YardForPoint(Markers, Piece.Origin) != Yards[Pocket]) continue;
                        const double Distance = FVector::DistSquared2D(Piece.Origin, Center);
                        if (Distance < BestDistance) { Cover = &Piece; BestDistance = Distance; }
                    }
                    if (!Cover)
                    {
                        UE_LOG(LogTemp, Error, TEXT("[Fernhall] Substation Skirmisher requires authored full-height cover."));
                        return;
                    }
                    // Imported bounds are baked world placement. Clear the
                    // complete footprint, instead of assuming gym block size.
                    const float Depth = FMath::Abs(Forward.X) * Cover->Extent.X + FMath::Abs(Forward.Y) * Cover->Extent.Y;
                    Desired = Cover->Origin + Forward * (Depth + 180.0f);
                }
            }
            FHitResult Floor;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(FernhallOutdoorFloor), false);
            if (!World->LineTraceSingleByObjectType(Floor, Desired + FVector(0, 0, 3000), Desired - FVector(0, 0, 3000),
                FCollisionObjectQueryParams(ECC_WorldStatic), Query) || Floor.ImpactNormal.Z < 0.7f)
            {
                UE_LOG(LogTemp, Error, TEXT("[Fernhall] outdoor pocket %d has no walkable floor at %s."), Pocket, *Desired.ToString());
                return;
            }
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ABreakerEnemy* Enemy = World->SpawnActor<ABreakerEnemy>(Class,
                Floor.ImpactPoint + FVector(0, 0, 200), (-Forward).Rotation(), Parameters);
            if (!Enemy) return;
            Enemy->ConfigureWave(AreaLevel); // Finite life; does not enroll in the wave controller.
            if (bElite) Enemy->ConfigureElite();
            UCapsuleComponent* Body = Enemy->FindComponentByClass<UCapsuleComponent>();
            if (!Body) { Enemy->Destroy(); return; }
            const FVector At = Floor.ImpactPoint + FVector(0, 0, Body->GetScaledCapsuleHalfHeight() + 2.0f);
            Query.AddIgnoredActor(Enemy);
            if (World->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Body->GetScaledCapsuleRadius(), Body->GetScaledCapsuleHalfHeight()), Query))
            {
                UE_LOG(LogTemp, Error, TEXT("[Fernhall] outdoor pocket %d capsule obstructed at %s."), Pocket, *At.ToString());
                Enemy->Destroy();
                return;
            }
            Enemy->SetActorLocation(At);
            Enemy->ConfigureEncounter(At, Index * 1.3f);
            Enemy->Tags.Add(FName(*FString::Printf(TEXT("Fernhall.Outdoor.%d"), Pocket)));
            // THE SLOT, recorded at the moment the placement is known good:
            // floor traced, capsule clear, formation resolved. A patrol that
            // returns takes this back rather than having its position derived a
            // second time from the formation maths above — one authored layout,
            // one source of truth for where a body stands.
            FBreakerOutdoorSlot& Slot = OutdoorSlots.AddDefaulted_GetRef();
            Slot.Class = Class;
            Slot.Home = At;
            Slot.Facing = (-Forward).Rotation();
            Slot.PatrolPhase = Index * 1.3f;
            Slot.AreaLevel = AreaLevel;
            Slot.PocketTag = FName(*FString::Printf(TEXT("Fernhall.Outdoor.%d"), Pocket));
            Slot.bElite = bElite;
            Slot.Occupant = Enemy;
            PocketMembers.Add(Enemy);
            UBreakerKillTelemetryComponent::AttachTo(Enemy);
            ++Spawned;
        };
        const int32 FirstMelee = Roster.Skitters / 2;
        if (Pocket == 0)
            for (int32 Index = 0; Index < FirstMelee; ++Index) Spawn(ABreakerEnemy::StaticClass(), false);
        if (Pocket == 1)
        {
            for (int32 Index = FirstMelee; Index < Roster.Skitters - 2; ++Index)
                Spawn(ABreakerEnemy::StaticClass(), Index - FirstMelee < Roster.Elites);
            for (int32 Index = 0; Index < Roster.Lattices; ++Index) Spawn(ABreakerRangedEnemy::StaticClass(), false);
        }
        if (Pocket == 2)
        {
            for (int32 Index = 0; Index < Roster.Wardens; ++Index) Spawn(ABreakerWardenEnemy::StaticClass(), false);
            // ONE OF THE TWO FLANKERS CARRIES A RANK. See the note at pocket 4:
            // Quest.Pattern asks for three marked kills and the world stood two.
            for (int32 Index = 0; Index < 2; ++Index) Spawn(ABreakerEnemy::StaticClass(), Index == 0);
            for (int32 Index = 0; Index < Roster.Skirmishers; ++Index) Spawn(ABreakerSkirmisherEnemy::StaticClass(), false);
        }
        // The second wave's melee, split between the two yards' quiet halves.
        // Deliberately no Warden and no elite out here: the two fights that
        // carry a rank already stand at pockets 1 and 2, and a destination
        // whose every pocket is a set piece has no shape to it.
        // THREE EACH, NOT SIX, AND THE REASON IS THE XP ECONOMY. Wave one
        // solves to twelve Skitters and placing all of them took a cleared
        // entry yard from 159 XP to 375 — past the 279 that reaches level two,
        // so the first contract stopped being the thing that levels you and
        // became a reward for something you had already outgrown. Half the
        // wave keeps the pacing the campaign was tuned against while still
        // nearly doubling what stands in the world. Owner-ruled. O2.
        const int32 PerQuietPocket = Second.Skitters / 4;
        // QUEST.PATTERN ASKS FOR THREE MARKED KILLS AND THE WORLD STOOD TWO.
        // That is the owner's "quest marked enemies are still bugged", and the
        // desk's earlier reading — that he was mistaking ordinary elites for
        // quest ones — was only half of it. Elites ARE the quest ones (the
        // objective is elite-gated) and they ARE marked: the rank paint puts an
        // amber wash on them and they carry a bar above trash. The defect is
        // arithmetic. One elite stood in the entry yard and one in the depot,
        // so the counter could reach 2 of 3 and stop, which reads exactly like
        // a broken quest.
        //
        // Two more, both in the SUBSTATION: it is the yard between the two, so
        // three are met before the depot and a fourth waits past it. Nothing
        // went into the ENTRY yard, because a cleared entry yard has to stay
        // under the 279 XP that reaches level two and an elite is worth more
        // than a Skitter.
        if (Pocket == 3 || Pocket == 4 || Pocket == 5 || Pocket == 7)
            for (int32 Index = 0; Index < PerQuietPocket; ++Index)
                Spawn(ABreakerEnemy::StaticClass(), Pocket == 4 && Index == 0);
        if (Pocket == 6)
        {
            // THE DEPOT'S SET PIECE. A yard reached through two seams that held
            // nothing but trash would be the longest walk in the game for the
            // least reason, so the middle pocket carries a Warden and an elite
            // — the same shape pocket 2 uses, which is the one fight in the
            // world already proved to work at range and in close.
            //
            // A WARDEN AND NOT A LATTICE, deliberately: the Lattice is the one
            // archetype with no body mesh (owner-ruled, 2026-08-29), and a
            // fresh yard is the worst place to put the thing that reads worst.
            for (int32 Index = 0; Index < Roster.Wardens; ++Index)
                Spawn(ABreakerWardenEnemy::StaticClass(), false);
            for (int32 Index = 0; Index < 2; ++Index)
                Spawn(ABreakerEnemy::StaticClass(), Index == 0);
        }
        // THE SIDING'S THREE (O276): a quarter of its own solved wave in each,
        // the same share the quiet pockets take of theirs. Plain melee and no
        // rank — see the note above the yard table for what that adds to the
        // first contract's XP line and why it is recorded rather than tuned.
        if (Pocket >= 8)
            for (int32 Index = 0; Index < Siding.Skitters / 4; ++Index)
                Spawn(ABreakerEnemy::StaticClass(), false);
        if (Pocket == 3)
        {
            // Existing yard-frame fraction; no marker or existing site moves.
            const FVector Desired = Center - Forward * 550.f; // O2 PLACEHOLDER, approach offset.
            FHitResult Floor;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(FernhallCacheFloor), false);
            if (World->LineTraceSingleByObjectType(Floor, Desired + FVector(0,0,3000), Desired - FVector(0,0,3000),
                FCollisionObjectQueryParams(ECC_WorldStatic), Query) && Floor.ImpactNormal.Z >= .7f)
            {
                const auto* Body = GetDefault<ABreakerFernhallCache>()->FindComponentByClass<UCapsuleComponent>();
                const float Height = Body->GetScaledCapsuleHalfHeight();
                const FVector At = Floor.ImpactPoint + FVector(0,0,Height + 2.f);
                if (!World->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn,
                    FCollisionShape::MakeCapsule(Body->GetScaledCapsuleRadius(),Height), Query))
                {
                    auto* Cache = World->SpawnActor<ABreakerFernhallCache>(At, (-Forward).Rotation());
                    if (Cache) Cache->Configure(AreaLevel, PocketMembers, PerQuietPocket);
                }
                else UE_LOG(LogTemp, Error, TEXT("[Fernhall] cache approach is obstructed."));
            }
            else UE_LOG(LogTemp, Error, TEXT("[Fernhall] cache has no walkable floor."));
        }

        // ---- WHERE THIS POCKET'S PATROLS COME BACK FROM --------------------
        // O268 gave the world a return and left it ILLEGIBLE: a body resolved
        // into existence standing on its post. O274 answers it twice: the
        // composer authors openings (`marker_spawn_*` — bay mouths, dock
        // faces, seam mouths) and a pocket within reach of one returns through
        // it; a pocket the composer gave no opening returns through a TEAR,
        // which needs no geometry, works on any ground, and is a better
        // sentence about Fernhall than a door nobody drew would be.
        //
        // SPAWNED CLOSED (O274). A tear is an event, not a fixture: the actor
        // placed here draws nothing until the repopulation clock opens it for a
        // return, the pocket's body comes through once it has opened, and it
        // closes after the last arrival. A player crossing a yard at rest sees
        // no tears; one that is open is a promise that something is coming out
        // of it. The initial population below never uses it — those bodies are
        // the world as found, placed on their posts, and only a RETURN tears.
        //
        // BEHIND THE FORMATION, not beside it. The bodies are placed facing
        // -Forward because that is where the player comes from, so +Forward is
        // the far side of the fight: a patrol steps out of the tear with its
        // back to open ground and walks toward the post, which is the walk the
        // player is supposed to see. The offset clears the widest formation
        // (three rows at 300 cm) with room left.
        //
        // NO TEAR, NO ARRIVAL, DELIBERATELY. If the ground fails its trace or
        // the actor cannot spawn, the slots keep the shipped O268 behaviour and
        // the body appears at its post at once. An arrival point with nothing
        // opening at it would be strictly WORSE than that — a body popping into
        // open ground away from its post — so the two are coupled.
        //
        // AUTHORED GROUND FIRST (O274). Where the composer put an opening
        // within reach of this pocket — a bay mouth, a dock face, a seam
        // mouth, as a `marker_spawn_*` — the patrols come back through THAT,
        // exactly as the courtyard's come back through its bay, and no tear
        // is placed for the pocket. A tear is for ground the composer left
        // blank, not a second door beside an authored one. The nearest site
        // wins and the reach is the builder's one number, so a re-export that
        // moves a mouth moves the pocket's return with it and the suite reads
        // the same rule the mode does.
        //
        // THE CLOCK IS THE SAME EITHER WAY. A slot with no tear claims and
        // arrives in one step (RefillOutdoorSlot's zero wait), the body is
        // granted its emergence window on arrival, and it walks to its post
        // from wherever it appeared: only WHERE differs, and where is the
        // authored thing.
        if (OutdoorSlots.Num() > SlotFirst)
        {
            const FBreakerZoneMarker* Site = UBreakerZoneBuilder::NearestSpawnSite(
                Markers, Yards[Pocket], Center, UBreakerZoneBuilder::FernhallSpawnSiteReachCm);
            bool bArrivesAtSite = false;
            if (Site)
            {
                // From just above the marker's own floor, not from the sky: a
                // bay mouth has a roof over it, and a trace dropped through
                // the roof would stand the arrival on top of the bay. Same
                // reach the courtyard mouth uses.
                FHitResult SiteFloor;
                FCollisionQueryParams SiteQuery(SCENE_QUERY_STAT(FernhallSpawnSiteFloor), false);
                const FVector SiteAbove = Site->Location + FVector(0.0f, 0.0f, 400.0f);
                if (World->LineTraceSingleByObjectType(SiteFloor, SiteAbove,
                        SiteAbove - FVector(0.0f, 0.0f, 900.0f), FCollisionObjectQueryParams(ECC_WorldStatic), SiteQuery)
                    && SiteFloor.ImpactNormal.Z >= 0.7f)
                {
                    for (int32 Index = SlotFirst; Index < OutdoorSlots.Num(); ++Index)
                    {
                        FBreakerOutdoorSlot& Slot = OutdoorSlots[Index];
                        const ABreakerEnemy* Template = Slot.Class ? GetDefault<ABreakerEnemy>(Slot.Class) : nullptr;
                        const UCapsuleComponent* Capsule = Template
                            ? Template->FindComponentByClass<UCapsuleComponent>() : nullptr;
                        if (!Capsule) continue;
                        // A capsule centre, for the same reason the tear's is.
                        Slot.Arrival = SiteFloor.ImpactPoint
                            + FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight() + 2.0f);
                        Slot.bHasArrival = true;
                        bArrivesAtSite = true;
                    }
                    UE_LOG(LogTemp, Display,
                        TEXT("[Fernhall] pocket %d returns through authored site %d of yard '%s' (%.0f cm from its centre); no tear."),
                        Pocket, Site->Index, Site->Yard.IsNone() ? TEXT("<entry>") : *Site->Yard.ToString(),
                        FVector::Dist2D(Site->Location, Center));
                }
                else
                {
                    // An authored opening with no floor under it is a broken
                    // export, not a blank one — said so, then treated as blank
                    // so the pocket still has a return rather than none.
                    UE_LOG(LogTemp, Error,
                        TEXT("[Fernhall] spawn site %d of yard '%s' has no walkable floor at %s; pocket %d falls back to a tear."),
                        Site->Index, Site->Yard.IsNone() ? TEXT("<entry>") : *Site->Yard.ToString(),
                        *Site->Location.ToString(), Pocket);
                }
            }
            constexpr float PocketRiftOffsetCm = 750.0f;   // O2 PLACEHOLDER
            const FVector Desired = Center + Forward * PocketRiftOffsetCm;
            FHitResult Ground;
            FCollisionQueryParams RiftQuery(SCENE_QUERY_STAT(FernhallPocketRiftFloor), false);
            ABreakerPocketRift* Tear = nullptr;
            if (!bArrivesAtSite
                && World->LineTraceSingleByObjectType(Ground, Desired + FVector(0, 0, 3000),
                    Desired - FVector(0, 0, 3000), FCollisionObjectQueryParams(ECC_WorldStatic), RiftQuery)
                && Ground.ImpactNormal.Z >= 0.7f)
            {
                FActorSpawnParameters RiftParameters;
                RiftParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                // Facing the way a body leaving it walks, which is also the way
                // the player arrives: the tear is flat, so it has a front.
                Tear = World->SpawnActor<ABreakerPocketRift>(ABreakerPocketRift::StaticClass(),
                    Ground.ImpactPoint, (-Forward).Rotation(), RiftParameters);
            }
            if (Tear)
            {
                for (int32 Index = SlotFirst; Index < OutdoorSlots.Num(); ++Index)
                {
                    FBreakerOutdoorSlot& Slot = OutdoorSlots[Index];
                    const ABreakerEnemy* Template = Slot.Class ? GetDefault<ABreakerEnemy>(Slot.Class) : nullptr;
                    const UCapsuleComponent* Capsule = Template
                        ? Template->FindComponentByClass<UCapsuleComponent>() : nullptr;
                    if (!Capsule) continue;
                    // A capsule CENTRE, matching what Home holds — the refill
                    // spawns at this point and overlap-tests a capsule around
                    // it, so a floor point here would bury every arrival.
                    Slot.Arrival = Ground.ImpactPoint
                        + FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight() + 2.0f);
                    Slot.bHasArrival = true;
                    Slot.Rift = Tear;
                }
            }
            else if (!bArrivesAtSite)
            {
                UE_LOG(LogTemp, Display,
                    TEXT("[Fernhall] pocket %d has no ground for a tear; its patrols return to their posts."), Pocket);
            }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("[Fernhall] %d finite outdoor enemies placed across %d pockets; no wave controller."), Spawned, PocketCount);
    // ---- SUPPLY CHESTS, ONE ROLL PER SESSION --------------------------------
    // O275: a chest stands behind full-height cover or inside a bay, never on
    // open lane ground. The yard's own pieces offer the SITES — every
    // blk_full_* far face, every bay interior — and the seed picks which of
    // them carry a chest this session. The seed never invents a coordinate.
    //
    // RANDOM PER SESSION, NOT PER AUTHORED PLACEMENT. What the owner asked for
    // is a route that is not identical the third time it is walked, and one
    // seed drawn here gives every chest in the world a site and a content that
    // a test can reproduce from that seed alone.
    //
    // NOT GATED ON A FIGHT, which is the whole difference from the cache
    // standing at pocket 3. What one pays is in BreakerSupplyChestMath.h and
    // the chest's own TryOpen, where a bare test can read it.
    {
        // NOT FMath::Rand, AND THE REASON IS THE SUITE. That function advances
        // a PROCESS-GLOBAL stream which every test in the run shares, so a
        // session roll drawn from it silently changes the seeds every later
        // fixture sees — and this one is drawn once per Fernhall build, which
        // several runtime tests do. A GUID gives per-session variation without
        // touching anything else's arithmetic.
        const int32 ChestSeed = static_cast<int32>(GetTypeHash(FGuid::NewGuid()));
        const FName ChestYards[] = { NAME_None, FName(TEXT("substation")), FName(TEXT("depot")),
                                     FName(TEXT("siding")) };
        const ABreakerSupplyChest* ChestTemplate = GetDefault<ABreakerSupplyChest>();
        const UCapsuleComponent* ChestBody = ChestTemplate
            ? ChestTemplate->FindComponentByClass<UCapsuleComponent>() : nullptr;
        // The body a patrol stands in, which is the thing a chest must not be
        // able to share ground with.
        const ABreakerEnemy* EnemyTemplate = GetDefault<ABreakerEnemy>();
        const UCapsuleComponent* EnemyTemplateBody = EnemyTemplate
            ? EnemyTemplate->FindComponentByClass<UCapsuleComponent>() : nullptr;
        // Room to walk between the two rather than merely not intersecting: a
        // chest touching a patrol is reachable and unreadable. O2 PLACEHOLDER.
        constexpr float ChestBodyMarginCm = 90.0f;
        int32 ChestsPlaced = 0;
        for (int32 YardIndex = 0; YardIndex < UE_ARRAY_COUNT(ChestYards) && ChestBody; ++YardIndex)
        {
            FVector2D Origin2D, Forward2D;
            if (!UBreakerZoneBuilder::YardFrame(Markers, ChestYards[YardIndex], Origin2D, Forward2D)) continue;
            const FVector Forward(Forward2D.X, Forward2D.Y, 0);
            const FVector YardOrigin(Origin2D.X, Origin2D.Y, 0);
            const int32 AreaLevel = UBreakerZoneBuilder::FernhallYardAreaLevel(ChestYards[YardIndex]);
            // THE YARD'S OWN PIECES, answered the way the Skirmisher's cover
            // is: by which anchor each piece stands nearest.
            TArray<FBreakerZonePiece> OwnPieces;
            for (const FBreakerZonePiece& Piece : YardPieces)
            {
                if (UBreakerZoneBuilder::YardForPoint(Markers, Piece.Origin) == ChestYards[YardIndex])
                    OwnPieces.Add(Piece);
            }
            TArray<BreakerSupplyChest::FBreakerChestSite> Sites;
            BreakerSupplyChest::CollectChestSites(OwnPieces, YardOrigin, Forward,
                ChestBody->GetScaledCapsuleRadius(), Sites);
            if (Sites.Num() < BreakerSupplyChest::ChestsPerYard)
            {
                UE_LOG(LogTemp, Display,
                    TEXT("[Fernhall] yard '%s' offers %d chest sites for %d chests; placing what it has."),
                    *ChestYards[YardIndex].ToString(), Sites.Num(), BreakerSupplyChest::ChestsPerYard);
            }
            const TArray<BreakerSupplyChest::FBreakerChestSite> Picked = BreakerSupplyChest::PickChestSites(
                Sites, static_cast<int32>(BreakerSupplyChest::Mix(ChestSeed, YardIndex)),
                BreakerSupplyChest::ChestsPerYard);
            for (int32 Index = 0; Index < Picked.Num(); ++Index)
            {
                const int32 Salt = YardIndex * 16 + Index;
                const FVector Desired = Picked[Index].Location;
                FHitResult Floor;
                FCollisionQueryParams Query(SCENE_QUERY_STAT(FernhallChestFloor), false);
                // From just above the site's ground, not from the sky: a bay
                // has a roof, and a trace dropped through it would stand the
                // chest on top of the bay.
                if (!World->LineTraceSingleByObjectType(Floor,
                        Desired + FVector(0, 0, BreakerSupplyChest::SiteTraceHeightCm),
                        Desired - FVector(0, 0, 3000), FCollisionObjectQueryParams(ECC_WorldStatic), Query)
                    || Floor.ImpactNormal.Z < 0.7f)
                {
                    UE_LOG(LogTemp, Display, TEXT("[Fernhall] chest site behind '%s' has no walkable floor at %s."),
                        *Picked[Index].Cover.ToString(), *Desired.ToString());
                    continue;   // that chest is simply not there
                }
                const float Height = ChestBody->GetScaledCapsuleHalfHeight();
                const FVector At = Floor.ImpactPoint + FVector(0, 0, Height + 2.0f);
                // CLEAR OF THE WORLD, and then clear of the BODIES, and the
                // two are separate questions for a reason the suite had to
                // teach twice. First a chest is narrower than an enemy, so a
                // spot the chest fits in can still be inside a patrol. Then —
                // and this is the part a wider overlap shape did NOT fix — an
                // enemy capsule does not block the Pawn channel the way the
                // chest does, so widening the query still could not see one.
                // The encounter test's "every body is clear of world props"
                // assertion went red on a random roll, which is worse than the
                // overlap itself because it makes a shipped-configuration test
                // depend on a dice throw.
                //
                // So the bodies are asked about DIRECTLY rather than through a
                // channel whose responses this lane does not own.
                if (World->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn,
                    FCollisionShape::MakeCapsule(ChestBody->GetScaledCapsuleRadius(), Height), Query))
                {
                    continue;
                }
                const float BodyClearanceCm = ChestBody->GetScaledCapsuleRadius()
                    + (EnemyTemplateBody ? EnemyTemplateBody->GetScaledCapsuleRadius() : 0.0f)
                    + ChestBodyMarginCm;
                bool bCrowded = false;
                for (TActorIterator<ABreakerEnemy> It(World); It && !bCrowded; ++It)
                {
                    bCrowded = FVector::DistSquared2D(It->GetActorLocation(), At)
                        < FMath::Square(BodyClearanceCm);
                }
                if (bCrowded) continue;
                // Facing back down the yard, so the player meets its front and
                // its reward lands on the side they walked in from.
                ABreakerSupplyChest* Chest = World->SpawnActor<ABreakerSupplyChest>(At, (-Forward).Rotation());
                if (!Chest) continue;
                Chest->Configure(AreaLevel, BreakerSupplyChest::Mix(ChestSeed, Salt * 101 + 7));
                ++ChestsPlaced;
            }
        }
        UE_LOG(LogTemp, Display, TEXT("[Fernhall] %d supply chests placed from seed %d."), ChestsPlaced, ChestSeed);
    }

    BreakerFernhallCourtyard::FPlan Courtyard;
    FString CourtyardError;
    if (BreakerFernhallCourtyard::MakePlan(YardPieces, Courtyard, CourtyardError))
    {
        // THE COURTYARD ROSTER WAS DISCARDED HERE. The encounter returns the
        // bodies it placed and nobody took them, so the one pocket in the yard
        // with an authored doorway was also the one pocket that never came
        // back — cleared once and empty forever, while the five with nothing to
        // arrive from all repopulated.
        const TArray<ABreakerEnemy*> Placed = BreakerSpawnFernhallCourtyardEncounter(World, Courtyard);

        // THE ARRIVAL POINT: the bay mouth, which is the only authored opening
        // in the whole composed yard. Its transform comes from the same pure
        // plan the builder built the doorway from, so the point cannot drift
        // from the geometry. Just inside the entrance floor, on the route's own
        // centre line, and traced to the floor rather than assumed onto it.
        //
        // A FAILED TRACE IS NOT A FAILED POCKET. Without an arrival point the
        // slots still register and still repopulate — the body simply appears
        // at its post, which is exactly what the other five pockets do. The
        // doorway is the fiction, not the mechanism.
        const float MouthOutward = FMath::Max(Courtyard.EntranceFloorStart + 200.0f, 300.0f);
        const FVector MouthAbove = Courtyard.At(MouthOutward, 0.0f, 400.0f);
        FHitResult MouthFloor;
        FCollisionQueryParams MouthQuery(SCENE_QUERY_STAT(CourtyardArrivalFloor), false);
        const bool bMouth = World->LineTraceSingleByObjectType(MouthFloor, MouthAbove,
            MouthAbove - FVector(0.0f, 0.0f, 900.0f), FCollisionObjectQueryParams(ECC_WorldStatic), MouthQuery)
            && MouthFloor.ImpactNormal.Z >= 0.7f;

        for (int32 Index = 0; Index < Placed.Num(); ++Index)
        {
            ABreakerEnemy* Body = Placed[Index];
            if (!IsValid(Body)) continue;
            const UCapsuleComponent* Capsule = Body->FindComponentByClass<UCapsuleComponent>();
            FBreakerOutdoorSlot& Slot = OutdoorSlots.AddDefaulted_GetRef();
            Slot.Class = Body->GetClass();
            Slot.Home = Body->GetActorLocation();
            Slot.Facing = Body->GetActorRotation();
            // The phase the encounter itself hands each body as it places them,
            // so a returning patrol keeps its own leg of the formation's sweep
            // rather than falling in step with the rest of the pocket.
            Slot.PatrolPhase = Index * 1.3f;
            Slot.AreaLevel = UBreakerZoneBuilder::FernhallRiftFor(NAME_None).EffectiveAreaLevel();
            Slot.PocketTag = TEXT("Fernhall.Outdoor.Courtyard");
            Slot.Occupant = Body;
            if (bMouth && Capsule)
            {
                Slot.Arrival = MouthFloor.ImpactPoint
                    + FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight() + 2.0f);
                Slot.bHasArrival = true;
            }
        }
        UE_LOG(LogTemp, Display,
            TEXT("[Fernhall] courtyard registered %d repopulation slots%s."),
            Placed.Num(), bMouth ? TEXT(", arriving through the bay doorway") : TEXT(" (no doorway floor; arriving at post)"));
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[Fernhall] no courtyard: %s"), *CourtyardError);
    }
}
void ABreakerGameMode::StartNextWave()
{
    if (!GetWorld() || bRiftRunCompleted || IsWaveActive()) return;
    APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    if (!PlayerPawn) return;

    // Wave clear = full restock. Reaching here with a wave already started
    // means the previous wave is empty (IsWaveActive() gated above), so the
    // player begins every wave topped up. Answers "no way to regain ammo"
    // at the coarse grain; kill drops and the camp crate cover the rest.
    if (CurrentWave > 0) RefillPlayerAmmo();

    ++CurrentWave;
    // Dead entries pruned too, not just stale pointers: a parked pool body is
    // a VALID actor still sitting in last wave's list, and reviving it into
    // this wave would give it two entries — one enemy counted twice by every
    // walker of this array.
    WaveEnemies.RemoveAll([](const TObjectPtr<ABreakerEnemy>& Enemy) { return !IsValid(Enemy) || Enemy->IsDeadEnemy(); });

    const FBreakerWaveComposition Composition = GetWaveComposition(CurrentWave);
    const FVector Origin = PlayerPawn->GetActorLocation();
    const FVector Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
    // Reserve the actual fixed formation in field coordinates, including its
    // capsules. The former 900cm margin omitted even the melee pack's extent.
    float EncounterRadius = WaveSpawnPackRadiusCm;
    TArray<FVector> MeleeOffsets, LatticeOffsets, WardenOffsets;
    auto ReserveOffset = [&](const FVector& Offset, float BodyRadius)
    {
        const FVector AxisF = bFieldFrameSet ? Frame.Forward : FVector::ForwardVector;
        const FVector AxisR = bFieldFrameSet ? Frame.Right : FVector::RightVector;
        EncounterRadius = FMath::Max(EncounterRadius, BodyRadius + FMath::Max(
            FMath::Abs(static_cast<float>(FVector::DotProduct(Offset, AxisF))),
            FMath::Abs(static_cast<float>(FVector::DotProduct(Offset, AxisR)))));
    };
    const float MeleeRadius = bSpawnDrudges
        ? FMath::Max(GetDefault<ABreakerEnemy>()->GetBodyCapsuleRadius(), GetDefault<ABreakerAlteredEnemy>()->GetBodyCapsuleRadius())
        : GetDefault<ABreakerEnemy>()->GetBodyCapsuleRadius();
    for (int32 Index = 0; Index < Composition.Skitters; ++Index)
    {
        const float Angle = 360.0f * (Index / 4) / FMath::Max(1, (Composition.Skitters + 3) / 4);
        MeleeOffsets.Add(FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector) * (CombatPocketRadius * .55f)
            + FVector::ForwardVector.RotateAngleAxis(Index * 90.0f, FVector::UpVector) * 160.0f);
        // Elite modifiers restore the authored Elite rank; ordinary carriers
        // may promote to ModifierBearing. Reserve that possible final size.
        const EBreakerMonsterRank SpawnRank = Index < Composition.Elites
            ? EBreakerMonsterRank::Elite
            : (bGrantModifiers && Index < Composition.Elites + Composition.ModifierCarriers
                ? EBreakerMonsterRank::ModifierBearing : EBreakerMonsterRank::Trash);
        ReserveOffset(MeleeOffsets.Last(), MeleeRadius
            * UBreakerMonsterChassisLibrary::GetRankScaleMultiplier(SpawnRank));
    }
    for (int32 Index = 0; Index < Composition.Lattices; ++Index)
    {
        LatticeOffsets.Add(FVector::ForwardVector.RotateAngleAxis(360.0f * Index / FMath::Max(1, Composition.Lattices) + 45.0f,
            FVector::UpVector) * CombatPocketRadius);
        ReserveOffset(LatticeOffsets.Last(), GetDefault<ABreakerRangedEnemy>()->GetBodyCapsuleRadius());
    }
    for (int32 Index = 0; Index < Composition.Wardens; ++Index)
    {
        WardenOffsets.Add(-Forward * (CombatPocketRadius * .6f)
            + FVector::ForwardVector.RotateAngleAxis(Index * 90.0f, FVector::UpVector) * 300.0f);
        ReserveOffset(WardenOffsets.Last(), GetDefault<ABreakerWardenEnemy>()->GetBodyCapsuleRadius());
    }
    // Skirmishers select cover anchors separately; do not move them blindly
    // away from cover to make a fixed-ring containment claim.
    if (bRiftInstance && Composition.bBoss)
        EncounterRadius = FMath::Max(EncounterRadius, BossArenaClearanceCm);
    // Wave mode deliberately spawns around the PLAYER rather than at the
    // authored arena: the instrument has to work wherever a playtest happens
    // to be standing. THAT IS CORRECT FOR AN INSTRUMENT IN AN OPEN FIELD AND
    // WRONG FOR A WALLED YARD — the owner watched enemies spawn outside the
    // tileset and walk in, because 44 m from a player facing across a 50 m
    // width lands outside it. The missing concept is that this spawner has no
    // idea a boundary exists; containment is GROUND's and is reported before
    // any number is authored for it. What is fixed HERE is the narrower defect
    // underneath it.
    //
    // THE VALUE AND ITS OWN JUSTIFICATION DISAGREED. The comment cited
    // Encounter-Design 5.2's 1500-4000 cm band and then used 4400, 400 cm above
    // the top of the band it named in the same sentence. The band wins, for the
    // reason the wave budget's caps beat its curve: it is the one with a
    // document behind it. So the distance is DERIVED rather than either number
    // being quietly rewritten — DashRefreshDistance still expresses the intent
    // ("one dash-cooldown of ground away") and the clamp stops it leaving the
    // band again the next time movement is retuned.
    //
    // Lowering the constant to 4000 would have satisfied the band and still
    // spawned outside a 50 m yard, which is why this is not the fix for what
    // the owner saw. It is the fix for a justification that was not true.
    const float SpawnDistance = FMath::Clamp(DashRefreshDistance,
        FMath::Min(WaveSpawnBandMinCm, WaveSpawnBandMaxCm),
        FMath::Max(WaveSpawnBandMinCm, WaveSpawnBandMaxCm));

    // CONTAINED IN THE FIELD, not merely offset from the player. The band above
    // still says how far a fight should start; the field says where there is
    // room for one. Solved in FIELD coordinates because that is the frame the
    // grammar speaks and the only one a yard's boundary exists in.
    FVector ArenaCenter = Origin + Forward * SpawnDistance;
    if (bFieldFrameSet)
    {
        // The zone's band where a zone built one, the gym's where this actor
        // authored the field itself.
        const FBreakerCoverFieldParams FieldParams =
            bActiveFieldParamsSet ? ActiveFieldParams : MakeCoverFieldParams();
        const FVector Offset = Origin - Frame.Ground;
        const float PlayerF = static_cast<float>(FVector::DotProduct(Offset, Frame.Forward));
        const float PlayerR = static_cast<float>(FVector::DotProduct(Offset, Frame.Right));
        const float FacingF = static_cast<float>(FVector::DotProduct(Forward, Frame.Forward));
        const float FacingR = static_cast<float>(FVector::DotProduct(Forward, Frame.Right));

        float CentreF = 0.0f;
        float CentreR = 0.0f;
        float Afforded = 0.0f;
        const bool bFits = UBreakerCoverLayoutLibrary::SolveContainedSpawnCentre(
            FieldParams, PlayerF, PlayerR, FacingF, FacingR,
            WaveSpawnBandMinCm, SpawnDistance, EncounterRadius,
            CentreF, CentreR, Afforded);
        ArenaCenter = Frame.At(CentreF, CentreR, 0.0f);
        if (!bFits)
        {
            // THE YARD COULD NOT HOLD THE BAND. Said out loud rather than
            // absorbed: this is the case GROUND's report predicted for a
            // 100 x 50 yard, and it is a design signal about the yard's size or
            // the band's floor, not a runtime error to swallow.
            UE_LOG(LogTemp, Warning,
                TEXT("[BreakerWave] the field afforded only %.0f cm against a %.0f cm spawn floor; ")
                TEXT("the pack is placed as far out as it fits. The yard is too small for the authored band."),
                Afforded, WaveSpawnBandMinCm);
        }
    }

    // THE WAVE IS SOLVED, NOT RAMPED. What was here was `4 + wave * 3` capped
    // at 24, an elite every third wave and `wave/2` Lattices: no budget, no
    // archetype costs, no rest waves, no boss wave, no variety rule, and 24
    // live enemies against Encounter-Design §5.3's ceiling of TWELVE.
    // UBreakerWaveBudgetLibrary is §4.2's arithmetic as pure world-free maths,
    // so what a wave IS can be asserted by automation, and all this function
    // does is place the answer in the world.

    FString IllegalReason;
    if (!UBreakerWaveBudgetLibrary::IsCompositionLegal(Composition, 1, WaveBudget, IllegalReason))
    {
        // Loud, never silent, and never trimmed: a spawner that quietly drops
        // an enemy makes the instrument report a wave that did not happen.
        UE_LOG(LogTemp, Warning, TEXT("[BreakerGym] wave %d composition is ILLEGAL: %s"), CurrentWave, *IllegalReason);
    }
    const int32 AreaLevel = GetAreaLevelForWave(CurrentWave);
    UE_LOG(LogTemp, Display, TEXT("[BreakerGym] %s | area level %d"),
        *UBreakerWaveBudgetLibrary::DescribeComposition(Composition), AreaLevel);

    // --- The boss wave (§4.2, wave 12) -------------------------------------
    // The Field Marshal and nothing else. It deploys its own adds and respawns
    // its own gallery Lattices, and §5.3's density ceiling is enforced at that
    // SOURCE — a wave budget spent alongside it would blow the cap from two
    // directions at once and neither would know about the other.
    //
    // Rift identity comes from the same authored mission content that names
    // its completion beat. The gym retains its explicitly authored Marshal.
    if (Composition.bBoss)
    {
        const UBreakerGameInstance* Session = GetGameInstance<UBreakerGameInstance>();
        const FName BossName = bRiftInstance && Session
            ? UBreakerMissionLibrary::BossForRift(Session->PendingRift) : NAME_None;
        const TSubclassOf<ABreakerBossEnemy> BossClass = BossName.IsNone()
            ? TSubclassOf<ABreakerBossEnemy>(ABreakerBossEnemy::StaticClass())
            : ABreakerBossEnemy::ClassForBossName(BossName);
        if (!BossClass)
        {
            UE_LOG(LogTemp, Error, TEXT("[Rift] authored boss '%s' has no runtime class."), *BossName.ToString());
            return;
        }
        SpawnBossOfClass(BossClass, bRiftInstance
            ? TOptional<FVector>(ArenaCenter + FVector(0, 0, 140)) : TOptional<FVector>());
        if (IsValid(ActiveBoss))
        {
            ActiveBoss->ConfigureWave(AreaLevel);
            WaveEnemies.Add(ActiveBoss);
            // THE BOSS IS THE RIFT'S TERMINATOR (O168). Inside a run the thing
            // holding the rift open is the thing the run culminates in — the
            // only body in this game that already means "this is the end", so
            // marking it invents no design. Marking and binding are one act,
            // and FIELD's raise is mark-guarded, so an unmarked boss in the gym
            // costs one bool and completes nothing.
            //
            // WHICH BODY HOLDS A RIFT OPEN IS STILL THE SEAT'S TO RULE. This is
            // the least arbitrary choice available, not a ruling: if a rift
            // should end on something else, this is the one call that moves.
            if (bRiftInstance) MarkRiftTerminator(ActiveBoss);
        }
        return;
    }

    // --- Melee, including the elite promotions ------------------------------
    // An elite is a PROMOTED body, not an extra one, which is what keeps the
    // density ceiling honest: the solver counts elites inside Skitters.
    // THE DRUDGE IN WAVE MODE, as a SUBSTITUTION. BreakerWaveBudget.h has no
    // Drudge row and this lane may not add one, so a Drudge is rendered in place
    // of a melee body the solver already paid for — exactly the precedent the
    // solver itself sets for elites and modifier carriers, which are promoted
    // Skitters folded into Composition.Skitters rather than extra bodies. That
    // keeps 5.3's density ceiling honest without touching the solver.
    //
    // WHAT THE SOLVER WOULD NEED, if the owner wants a Drudge to be a real
    // archetype rather than a re-skin — reported, not made:
    //   FBreakerWaveBudgetParams: int32 DrudgeCost = 2;  int32 DrudgeFromWave = 3;
    //   FBreakerWaveComposition:  int32 Drudges = 0;  folded into TotalEnemies()
    //                             and counted as melee, not as a ranged source.
    // Cost 2 rather than the Skitter's 1: 2.0x health and no lunge is roughly
    // two Skitters' worth of time-to-kill and none of a Skitter's pressure.
    //
    // Substituted from the END of the melee list so the elite and carrier
    // promotions, which take the low indices, are never turned into Drudges —
    // the Drudge overrides its own capsule and chassis and a promoted one would
    // be reading two archetypes at once.
    int32 WaveDrudges = 0;
    if (bSpawnDrudges && CurrentWave >= DrudgeFromWave)
    {
        WaveDrudges = FMath::Clamp(CurrentWave / FMath::Max(1, DrudgeWaveDivisor), 0, MaximumDrudgesPerWave);
        WaveDrudges = FMath::Min(WaveDrudges,
            FMath::Max(0, Composition.Skitters - Composition.Elites - Composition.ModifierCarriers));
    }
    const int32 FirstDrudgeIndex = Composition.Skitters - WaveDrudges;

    for (int32 Index = 0; Index < Composition.Skitters; ++Index)
    {
        const FVector SpawnLocation = ArenaCenter + MeleeOffsets[Index];
        if (WaveDrudges > 0 && Index >= FirstDrudgeIndex)
        {
            if (ABreakerAlteredEnemy* Drudge = SpawnDrudge(SpawnLocation, Index * 1.3f, AreaLevel))
            {
                Drudge->ConfigureWave(AreaLevel);
                SetEnemyDropsLoot(Drudge, Composition.bDropsLoot);
                WaveEnemies.Add(Drudge);
            }
            continue;
        }
        ABreakerEnemy* Enemy = AcquirePooledEnemy(ABreakerEnemy::StaticClass(), SpawnLocation);
        if (!Enemy) continue;
        Enemy->ConfigureEncounter(SpawnLocation, Index * 1.3f);
        Enemy->ConfigureWave(AreaLevel);
        if (Index < Composition.Elites)
        {
            Enemy->ConfigureElite();
            // Seeded on the WAVE and the index, so wave 8 meets the same
            // Champion every run and a TTK sample taken across two sessions
            // compares. The solver decided HOW MANY modifiers it could afford;
            // the roll decides which, subject to §1.3's composition rules.
            GrantModifiers(Enemy, ModifierSeedBase + CurrentWave * 7919 + Index);
        }
        else if (Index < Composition.Elites + Composition.ModifierCarriers)
        {
            // Non-elite modifier carriers (O27's kill-bucket producer): KEEP
            // rank ModifierBearing rather than restoring an authored rank, the
            // same distinction GrantModifierCarrier draws against GrantModifiers
            // above. Seeded the same way, offset past the elite slots so the
            // two draws never collide.
            GrantModifierCarrier(Enemy, ModifierSeedBase + CurrentWave * 7919 + Index);
        }
        SetEnemyDropsLoot(Enemy, Composition.bDropsLoot);
        UBreakerKillTelemetryComponent::AttachTo(Enemy);
        WaveEnemies.Add(Enemy);
    }

    // --- LATTICE ------------------------------------------------------------
    // A ring further out and spread evenly around the arena, so the melee packs
    // push the player ACROSS the ranged fire lanes instead of away from them.
    for (int32 Index = 0; Index < Composition.Lattices; ++Index)
    {
        const FVector SpawnLocation = ArenaCenter + LatticeOffsets[Index];
        ABreakerEnemy* Ranged = AcquirePooledEnemy(ABreakerRangedEnemy::StaticClass(), SpawnLocation);
        if (!Ranged) continue;
        Ranged->ConfigureEncounter(SpawnLocation, Index * 0.9f);
        Ranged->ConfigureWave(AreaLevel);
        SetEnemyDropsLoot(Ranged, Composition.bDropsLoot);
        UBreakerKillTelemetryComponent::AttachTo(Ranged);
        WaveEnemies.Add(Ranged);
    }

    // --- SKIRMISHER ---------------------------------------------------------
    // Placed against COVER, never on a bearing. Wave mode spawns wherever the
    // playtest happens to be standing, so the anchor it finds is whatever the
    // field built nearby — a pocket's cover ring inside a pocket, the sniper
    // lane's hard-cover piece on the lane. In the open it degrades to a Lattice
    // with a longer telegraph, which is strictly worse than a Lattice.
    for (int32 Index = 0; Index < Composition.Skirmishers; ++Index)
    {
        const float Angle = 360.0f * Index / FMath::Max(1, Composition.Skirmishers) + 200.0f;
        const FVector Around = ArenaCenter
            + FVector(1.0f, 0.0f, 0.0f).RotateAngleAxis(Angle, FVector::UpVector) * (CombatPocketRadius * 0.8f);
        if (ABreakerSkirmisherEnemy* Skirmisher = SpawnSkirmisherNearCover(Around, Origin, Index * 1.1f))
        {
            Skirmisher->ConfigureWave(AreaLevel);
            SetEnemyDropsLoot(Skirmisher, Composition.bDropsLoot);
            WaveEnemies.Add(Skirmisher);
        }
    }

    // --- WARDEN -------------------------------------------------------------
    // BETWEEN the player and the pack. §2.4's axis is "Wardens punish
    // approaching from the front", and a Warden behind the pack is a Warden the
    // player never has to solve.
    for (int32 Index = 0; Index < Composition.Wardens; ++Index)
    {
        const FVector SpawnLocation = ArenaCenter + WardenOffsets[Index];
        ABreakerEnemy* Warden = AcquirePooledEnemy(ABreakerWardenEnemy::StaticClass(), SpawnLocation);
        if (!Warden) continue;
        Warden->ConfigureEncounter(SpawnLocation, 1.7f + Index);
        Warden->ConfigureWave(AreaLevel);
        SetEnemyDropsLoot(Warden, Composition.bDropsLoot);
        UBreakerKillTelemetryComponent::AttachTo(Warden);
        WaveEnemies.Add(Warden);
    }

    if (Composition.Kind == EBreakerWaveKind::Rest)
    {
        // §4.3: the rest wave exists so wave mode measures COMBAT rather than
        // endurance. Half budget, no elites, loot on, and a breather — the
        // breather itself is TickWaveAdvance's (the rest breather runs on the
        // clear, F4 cuts it short), so what this does is restock and say so.
        RefillPlayerAmmo();
        UE_LOG(LogTemp, Display,
            TEXT("[BreakerGym] wave %d is a REST wave: half budget, no elites, loot enabled, ammo restocked. The next wave follows the clear by %.0fs; F4 skips the wait."),
            CurrentWave, WaveBudget.RestBreatherSeconds);
    }
}

ABreakerEnemy* ABreakerGameMode::AcquirePooledEnemy(UClass* EnemyClass, const FVector& SpawnLocation)
{
    // Pop-until-valid: the weak pointers go stale whenever something outside
    // the pool destroys a reserve body (ResetPlaytestTargets nukes every
    // enemy in the world), and a stale entry just means "spawn fresh".
    if (TArray<TWeakObjectPtr<ABreakerEnemy>>* Reserve = WaveEnemyPool.Find(EnemyClass))
    {
        while (Reserve->Num() > 0)
        {
            ABreakerEnemy* Parked = Reserve->Pop().Get();
            if (!Parked) continue;
            Parked->ReviveFromPool(SpawnLocation);
            // Every wave arrival passes through this one function — first
            // wave and later, gym, rift and breach — so the emergence window
            // is granted here rather than at each caller.
            Parked->GrantEmergenceWindow();
            return Parked;
        }
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    ABreakerEnemy* Enemy = GetWorld()->SpawnActor<ABreakerEnemy>(EnemyClass, SpawnLocation, FRotator::ZeroRotator, Params);
    if (Enemy)
    {
        Enemy->SetPooledByGameMode(true);
        Enemy->OnParkedForPool.BindUObject(this, &ABreakerGameMode::HandleEnemyParkedForPool);
        Enemy->GrantEmergenceWindow();
    }
    return Enemy;
}

void ABreakerGameMode::HandleEnemyParkedForPool(ABreakerEnemy* Enemy)
{
    if (!Enemy) return;
    // Keyed by the EXACT class the body was spawned as, so a Lattice can only
    // ever come back as a Lattice.
    WaveEnemyPool.FindOrAdd(Enemy->GetClass()).Add(Enemy);
}

void ABreakerGameMode::SetEnemyDropsLoot(ABreakerEnemy* Enemy, bool bDrops) const
{
    if (!Enemy) return;

    // §4.3: "Loot only on rest and boss waves. Otherwise the gym becomes a farm
    // and pollutes drop-rate data" — and drop-rate data is one of the two
    // numbers wave mode exists to produce.
    //
    // WHY REFLECTION AND NOT A SETTER. `bDropsLoot` is protected on
    // ABreakerEnemy and there is no mutator; adding one is a one-line change to
    // Combat/, which this lane does not own and which two other agents are
    // editing in parallel. The property is marked BlueprintReadWrite, so it is
    // deliberately writable from outside the class — this reaches it the way a
    // Blueprint would. A missing property WARNS rather than failing silently,
    // because the failure mode is a farm that nobody notices.
    static const FName DropsLootName(TEXT("bDropsLoot"));
    if (FBoolProperty* Property = FindFProperty<FBoolProperty>(ABreakerEnemy::StaticClass(), DropsLootName))
    {
        Property->SetPropertyValue_InContainer(Enemy, bDrops);
        return;
    }
    UE_LOG(LogTemp, Warning,
        TEXT("[BreakerGym] ABreakerEnemy::bDropsLoot not found by reflection; Encounter-Design 4.3's loot rule is not being applied."));
}

FBreakerWaveComposition ABreakerGameMode::GetWaveComposition(int32 WaveIndex) const
{
    const auto* Session = GetGameInstance<UBreakerGameInstance>();
    if (bRiftInstance && WaveIndex == 1 && Session
        && Session->PendingRift.EncounterId == FName(TEXT("fernhall.entry")))
        return UBreakerWaveBudgetLibrary::MakeEntryRiftOpening(WaveBudget);
    // Solo, because solo is the primary balance target and the gym has one
    // player. Party sizes go through the same solver with a different count.
    return UBreakerWaveBudgetLibrary::SolveWave(WaveIndex, 1, WaveBudget);
}

int32 ABreakerGameMode::GetAreaLevelForWave(int32 WaveIndex) const
{
    // The gym's area level climbs with the wave. This is CONTENT escalation:
    // wave 5 is a harder area than wave 1 regardless of who is standing in it.
    const int32 Level = GymAreaLevel + FMath::Max(WaveIndex, 0) * FMath::Max(AreaLevelPerWave, 0);
    return UBreakerMonsterChassisLibrary::ClampAreaLevel(Level);
}

int32 ABreakerGameMode::GetWaveEnemiesAlive() const
{
    int32 Alive = 0;
    for (const TObjectPtr<ABreakerEnemy>& Enemy : WaveEnemies)
    {
        // IsDeadEnemy(), not a string comparison against a PRESENTATION label.
        // This asked `GetEnemyStateLabel() != "DEAD"`, and that label carried
        // the modifier banner prefixed onto it — so any dead enemy with a
        // modifier answered "WARDED | VOLATILE\nDEAD", compared unequal, and
        // counted as alive permanently. A wave containing a modifier-bearing
        // enemy could not clear. Asking the enemy whether it is dead cannot
        // drift when someone edits a label.
        if (IsValid(Enemy) && !Enemy->IsDeadEnemy()) ++Alive;
    }
    return Alive;
}

void ABreakerGameMode::ResetPlaytestTargets()
{
    if (!GetWorld()) return;
    // THE GYM ONLY. This function destroys EVERY enemy in the world and then
    // rebuilds the GYM's target dummies and standing encounter — and it ran
    // wherever the player pressed F1. In Fernhall that deleted the yard's
    // authored patrols, the courtyard, the quest's elites and the repopulation
    // slots' occupants, and spawned gym content on top: "the gym enemies are
    // suddenly inside fernhall".
    //
    // Refusing rather than rebuilding the yard, deliberately. F1 is dev
    // tooling; the CHARACTER half of the reset (health, ammo, stats, spawn
    // transform) still runs and is the useful part outside the gym, and
    // rebuilding a destination's population correctly is the repopulation
    // clock's job, not a debug key's.
    if (!bGymFieldBuilt)
    {
        UE_LOG(LogTemp, Display,
            TEXT("[BreakerGym] RESET refused: this map has no gym field. The player reset; the world is left alone."));
        return;
    }
    for (TActorIterator<ABreakerTargetDummy> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It) It->Destroy();
    // The destroy loop above just killed the parked reserve too; the weak
    // pointers would skip themselves at the next acquire anyway, but an
    // empty map says what actually happened.
    WaveEnemyPool.Empty();
    bPlaytestTargetsSpawned = false;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get(); PC && PC->GetPawn())
        {
            SpawnPlaytestTargets();
            SpawnCombatEncounter();
            break;
        }
    }
}
