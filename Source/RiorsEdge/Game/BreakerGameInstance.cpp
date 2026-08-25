#include "Game/BreakerGameInstance.h"

#include "Combat/BreakerEnemy.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UI/BreakerLoadingScreen.h"
#include "Widgets/SWindow.h"

namespace
{
    // The map's short name, which is what the code compares against. A world's
    // name is the map name without the /Game/... path or the _C suffix PIE
    // adds, and PIE also prefixes it with "UEDPIE_0_" — so a naive comparison
    // works in a packaged build and silently fails in the editor, which is the
    // worst possible split for something the owner tests in PIE.
    FString BreakerCurrentMapName(const UObject* WorldContext)
    {
        const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
        if (!World) return FString();
        FString Name = World->GetMapName();
        Name.RemoveFromStart(World->StreamingLevelsPrefix);
        return Name;
    }
}

bool UBreakerGameInstance::IsFrontEndMap(const UObject* WorldContext)
{
    return BreakerCurrentMapName(WorldContext) == FrontEndMapName();
}

bool UBreakerGameInstance::IsAnchorMap(const UObject* WorldContext)
{
    return BreakerCurrentMapName(WorldContext) == AnchorMapName();
}

bool UBreakerGameInstance::IsGymMap(const UObject* WorldContext)
{
    // THE FALLBACK IS THE GYM, and it is load-bearing. Every existing entry
    // point — the capture harness, a PIE drop-in on the old template map,
    // -BreakerAutoPlay — runs in a map that is none of the three by name, and
    // every one of them expects the gym field to be there. Treating "not the
    // front end and not the anchor" as the gym is what keeps all of that
    // working while the three maps are still empty shells.
    const FString Name = BreakerCurrentMapName(WorldContext);
    return Name != FrontEndMapName() && Name != AnchorMapName();
}

// ---------------------------------------------------------------------------
// THE HOLDS ARE TUNED FOR READING, NOT FOR COVERING A LOAD. The briefing
// carries six data points — an area name, a line of fiction, the area level,
// an item-level range, two monster multipliers and the death allowance — and
// a briefing you cannot read is worse than none. The map underneath loads in
// ~0.26s; the hold is long ON PURPOSE, and it also answers the owner's own
// report that travel is instant and disorienting. Whoever finds a 2.6s hold
// over a quarter-second load: it is not a wait to optimise away, it is the
// arrival being named. Owner-tunable live (they are console variables), and
// O2 PLACEHOLDER until the owner has read a few.
// ---------------------------------------------------------------------------
static float GBreakerDeployHoldSeconds = 2.6f;   // O2 PLACEHOLDER
static FAutoConsoleVariableRef CVarBreakerDeployHold(
    TEXT("Breaker.DeployHoldSeconds"), GBreakerDeployHoldSeconds,
    TEXT("How long the deployment briefing holds before the travel begins. Tuned for reading the briefing, not for covering the load."));
static float GBreakerArriveHoldSeconds = 0.9f;   // O2 PLACEHOLDER
static FAutoConsoleVariableRef CVarBreakerArriveHold(
    TEXT("Breaker.ArriveHoldSeconds"), GBreakerArriveHoldSeconds,
    TEXT("How long the briefing lingers after the destination has loaded, while the stage line says the arrival."));

void UBreakerGameInstance::Init()
{
    Super::Init();
    // The far side of the load: the arrival beat's cue. The deploy side needs
    // no delegate — BeginTravel is the door.
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UBreakerGameInstance::HandlePostLoadMap);
}

void UBreakerGameInstance::TravelTo(const UObject* WorldContext, FName MapName)
{
    if (MapName.IsNone()) return;
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    UBreakerGameInstance* Session = World ? World->GetGameInstance<UBreakerGameInstance>() : nullptr;
    if (Session)
    {
        Session->BeginTravel(MapName);
        return;
    }
    // No session (a bare test world): travel the old way rather than not at
    // all.
    UGameplayStatics::OpenLevel(WorldContext, MapName);
}

void UBreakerGameInstance::BeginTravel(FName MapName)
{
    if (bDeployBeatActive) return;

    // The capture harness cannot author a rift and the beat is exactly the
    // kind of surface that must not ship unphotographed — the FORGEBENCH
    // precedent. -BreakerCaptureDeployBeat seeds the plate's own authored
    // example when nothing set one; a command-line switch by construction, so
    // a shipped build cannot reach it.
    if (!PendingRift.IsSet() && FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureDeployBeat")))
    {
        PendingRift.AreaName = FText::FromString(TEXT("Fernhall Substation"));
        PendingRift.AreaLine = FText::FromString(
            TEXT("A relay yard the rift took first. The lines still hum with something that is not power."));
        PendingRift.AreaLevel = 42;
        PendingRift.Tier = EBreakerRiftTier::Campaign;
    }

    // Only a travel with something to say gets the beat: dev drops, captures
    // and every legacy path stay instant. OpenLevel rather than a seamless
    // transition, as before — the maps share no geometry and this object is
    // the only thing that must survive the load.
    TSharedPtr<SWindow> Window = (PendingRift.IsSet() && GEngine && GEngine->GameViewport)
        ? GEngine->GameViewport->GetWindow() : nullptr;
    if (!Window.IsValid())
    {
        UGameplayStatics::OpenLevel(this, MapName);
        return;
    }

    // The briefing composes through the game's own derivations: the elite
    // loot bonus from the enemy's authored default (read, never transcribed)
    // and O82's solo budget feeding O123's readout — campaign ignores it,
    // and the endgame decrement stays parked behind O122 either way.
    const int32 EliteBonus = GetDefault<ABreakerEnemy>()->GetEliteDropItemLevelBonus();
    const FBreakerDeploymentBriefing Briefing = SBreakerLoadingScreen::MakeBriefing(
        PendingRift, EliteBonus, UBreakerRiftLibrary::SoloEndgameDeathBudget);

    DeployScreen = SNew(SBreakerLoadingScreen).Briefing(Briefing);
    DeployScreen->SetStage(FText::FromString(TEXT("OPENING THE RIFT")));
    DeployWindow = Window;
    Window->AddOverlaySlot(1000)
    [
        DeployScreen.ToSharedRef()
    ];
    bDeployBeatActive = true;

    // A CORE ticker, not a world timer, for the capture harness's stated
    // reason: every beat-eligible travel starts from a paused menu, and a
    // paused world's timers never fire. Weak, so a dying session cancels its
    // own travel instead of crashing it.
    const FName CapturedMap = MapName;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this, CapturedMap](float)
        {
            UGameplayStatics::OpenLevel(this, CapturedMap);
            return false;
        }), FMath::Max(GBreakerDeployHoldSeconds, 0.0f));
}

void UBreakerGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
    if (!bDeployBeatActive || !DeployScreen.IsValid()) return;
    // The far side: the stage line says the arrival, lingers long enough to
    // be read as one, and the pane leaves.
    DeployScreen->SetStage(FText::FromString(TEXT("ON SITE")));
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this](float)
        {
            EndDeployBeat();
            return false;
        }), FMath::Max(GBreakerArriveHoldSeconds, 0.0f));
}

void UBreakerGameInstance::EndDeployBeat()
{
    if (TSharedPtr<SWindow> Window = DeployWindow.Pin())
    {
        if (DeployScreen.IsValid())
        {
            Window->RemoveOverlaySlot(DeployScreen.ToSharedRef());
        }
    }
    DeployScreen.Reset();
    DeployWindow.Reset();
    bDeployBeatActive = false;
}
