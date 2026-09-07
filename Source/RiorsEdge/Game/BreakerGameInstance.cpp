#include "Game/BreakerGameInstance.h"

#include "Combat/BreakerEnemy.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Styling/CoreStyle.h"
#include "UI/BreakerLoadingScreen.h"
#include "Widgets/Layout/SBorder.h"
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

bool UBreakerGameInstance::IsFernhallMap(const UObject* WorldContext)
{
    return BreakerCurrentMapName(WorldContext) == FernhallMapName();
}

bool UBreakerGameInstance::IsGymMap(const UObject* WorldContext)
{
    return IsGymMapName(BreakerCurrentMapName(WorldContext));
}

bool UBreakerGameInstance::IsErasedEarthMap(const UObject* WorldContext)
{
    return BreakerCurrentMapName(WorldContext) == ErasedEarthMapName();
}

bool UBreakerGameInstance::IsDestinationMap(const UObject* WorldContext, FName DestinationId)
{
    const FString Map = BreakerCurrentMapName(WorldContext);
    if (DestinationId == ABreakerTravelPoint::HubDestinationId) return Map == AnchorMapName();
    if (DestinationId == ABreakerTravelPoint::FernhallDestinationId) return Map == FernhallMapName();
    if (DestinationId == ABreakerTravelPoint::ErasedEarthDestinationId) return Map == ErasedEarthMapName();
    if (DestinationId == ABreakerTravelPoint::StrippedEarthDestinationId) return Map == StrippedEarthMapName();
    if (DestinationId == ABreakerTravelPoint::WinningEarthDestinationId) return Map == WinningEarthMapName();
    if (DestinationId == ABreakerTravelPoint::GymDestinationId) return Map == GymMapName();
    if (DestinationId == ABreakerTravelPoint::RiftDestinationId) return Map == FernhallMapName();
    return false;
}

bool UBreakerGameInstance::IsStrippedEarthMap(const UObject* WorldContext)
{
    return BreakerCurrentMapName(WorldContext) == StrippedEarthMapName();
}

bool UBreakerGameInstance::IsWinningEarthMap(const UObject* WorldContext)
{
    return BreakerCurrentMapName(WorldContext) == WinningEarthMapName();
}

bool UBreakerGameInstance::IsGymMapName(const FString& Name)
{
    // THE FALLBACK IS THE GYM, and it is load-bearing. Every existing entry
    // point — the capture harness, a PIE drop-in on the old template map,
    // -BreakerAutoPlay — runs in a map that is none of the named maps, and
    // every one of them expects the gym field to be there. Treating "none of
    // the named maps" as the gym is what keeps all of that working. The cost
    // of the fallback is that every NEW named map must be excluded here by
    // hand, or it silently fills with targets and a boss key — which is why
    // this is a name-in, bool-out function the suite can hold.
    return Name != FrontEndMapName() && Name != AnchorMapName() && Name != FernhallMapName()
        && Name != ErasedEarthMapName() && Name != StrippedEarthMapName() && Name != WinningEarthMapName();
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
//
// The arrive side is the gate in BreakerArrivalMath.h: one static struct
// that ShippedArrivalHold() hands to the reveal ticker and to the suite, with
// each member bound to a console variable so the owner can tune it live.
// A static rather than a UPROPERTY on the instance: the game instance class
// is set by name in DefaultEngine.ini with no Blueprint subclass to carry
// edited defaults, so a property here would be a second place for the same
// figure that nothing edits.
// ---------------------------------------------------------------------------
static float GBreakerDeployHoldSeconds = 2.6f;   // O2 PLACEHOLDER
static FAutoConsoleVariableRef CVarBreakerDeployHold(
    TEXT("Breaker.DeployHoldSeconds"), GBreakerDeployHoldSeconds,
    TEXT("How long the deployment briefing holds before the travel begins. Tuned for reading the briefing, not for covering the load."));
static FBreakerArrivalHold GBreakerArrivalHold;   // O2 PLACEHOLDER — defaults in BreakerArrivalMath.h
// The longest a travel cover may wait for the far side before it leaves on
// its own: the deploy hold plus a load, with room for a slow disk.
static float GBreakerCoverWatchdogSeconds = 12.0f;   // O2 PLACEHOLDER
static FAutoConsoleVariableRef CVarBreakerCoverWatchdogSeconds(
    TEXT("Breaker.CoverWatchdogSeconds"), GBreakerCoverWatchdogSeconds,
    TEXT("Seconds a travel cover waits for the loaded world before it leaves on its own."));
static FAutoConsoleVariableRef CVarBreakerArriveHold(
    TEXT("Breaker.ArriveHoldSeconds"), GBreakerArrivalHold.MinHoldSeconds,
    TEXT("The shortest the arrival cover holds after the destination has loaded, while the stage line says the arrival."));
static FAutoConsoleVariableRef CVarBreakerArriveSettleFrames(
    TEXT("Breaker.ArriveSettleFrames"), GBreakerArrivalHold.MinSettleFrames,
    TEXT("The fewest frames the destination renders under the cover before it lifts, so Lumen and exposure have settled."));
static FAutoConsoleVariableRef CVarBreakerArriveFadeIn(
    TEXT("Breaker.ArriveFadeInSeconds"), GBreakerArrivalHold.FadeInSeconds,
    TEXT("How long the arrival cover takes to fade once both gates are met."));

const FBreakerArrivalHold& UBreakerGameInstance::ShippedArrivalHold()
{
    return GBreakerArrivalHold;
}

void UBreakerGameInstance::Init()
{
    Super::Init();
    // The far side of the load: the arrival beat's cue. The deploy side needs
    // no delegate — BeginTravel is the door.
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UBreakerGameInstance::HandlePostLoadMap);
}

void UBreakerGameInstance::Shutdown()
{
    // The ticker holds a weak reference and would skip a dead instance, but a
    // cover left on the window outlives the session; take both down.
    RevealWorld();
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
    Super::Shutdown();
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
    if (bTravelPending) return;

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

    // OpenLevel rather than a seamless transition — the maps share no
    // geometry and this object is the only thing that must survive the load.
    // A travel with something to say wears the briefing and holds it for
    // reading; every other travel wears a plain black and loads at once. Both
    // lift through the same gated reveal on the far side.
    if (PendingRift.IsSet())
    {
        // The briefing composes through the game's own derivations: the
        // elite loot bonus from the enemy's authored default (read, never
        // transcribed) and O82's solo budget feeding O123's readout —
        // campaign ignores it, and the endgame decrement stays parked behind
        // O122 either way.
        const int32 EliteBonus = GetDefault<ABreakerEnemy>()->GetEliteDropItemLevelBonus();
        const FBreakerDeploymentBriefing Briefing = SBreakerLoadingScreen::MakeBriefing(
            PendingRift, EliteBonus, UBreakerRiftLibrary::SoloEndgameDeathBudget);

        TSharedRef<SBreakerLoadingScreen> Pane = SNew(SBreakerLoadingScreen).Briefing(Briefing);
        Pane->SetStage(FText::FromString(TEXT("OPENING THE RIFT")));
        if (!AddCover(Pane))
        {
            // Headless: no window to put a cover on.
            UGameplayStatics::OpenLevel(this, MapName);
            return;
        }
        DeployScreen = Pane;
        bWorldReady = false;
        ArmCoverWatchdog();
        bTravelPending = true;

        // A CORE ticker, not a world timer, for the capture harness's stated
        // reason: every briefing travel starts from a paused menu, and a
        // paused world's timers never fire. Weak, so a dying session cancels
        // its own travel instead of crashing it.
        const FName CapturedMap = MapName;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
            [this, CapturedMap](float)
            {
                bTravelPending = false;
                UGameplayStatics::OpenLevel(this, CapturedMap);
                return false;
            }), FMath::Max(GBreakerDeployHoldSeconds, 0.0f));
        return;
    }

    // The plain black. OpenLevel defers the load to the next engine tick and
    // Slate paints after this one, so the cover is on screen before the load
    // blocks; a black already up from HoldBlack is kept as it is.
    if (!Cover.IsValid() && !AddCover(MakeBlackCover()))
    {
        UGameplayStatics::OpenLevel(this, MapName);
        return;
    }
    if (RevealTicker.IsValid())
    {
        // A reveal in flight is cancelled: the cover goes back to opaque and
        // the destination gets its own.
        FTSTicker::GetCoreTicker().RemoveTicker(RevealTicker);
        RevealTicker.Reset();
    }
    Cover->SetRenderOpacity(1.0f);
    bWorldReady = false;
    ArmCoverWatchdog();
    UGameplayStatics::OpenLevel(this, MapName);
}

void UBreakerGameInstance::ArmCoverWatchdog()
{
    DisarmCoverWatchdog();
    CoverWatchdog = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this](float)
        {
            CoverWatchdog.Reset();
            if (!bWorldReady && Cover.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("[BreakerTravel] no loaded world after %.1f s; the cover leaves on its own."),
                    GBreakerCoverWatchdogSeconds);
                bWorldReady = true;
                RevealWorld();
            }
            return false;
        }), FMath::Max(GBreakerCoverWatchdogSeconds, 0.1f));
}

void UBreakerGameInstance::DisarmCoverWatchdog()
{
    if (CoverWatchdog.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(CoverWatchdog);
        CoverWatchdog.Reset();
    }
}

TSharedRef<SWidget> UBreakerGameInstance::MakeBlackCover()
{
    // An opaque border over the whole window. SBorder rather than SColorBlock
    // because a border is also a hit-test wall: nothing under the cover takes
    // a click while the world is not yet shown.
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor::Black)
        .Padding(0.0f);
}

bool UBreakerGameInstance::AddCover(TSharedRef<SWidget> Widget)
{
    TSharedPtr<SWindow> Window = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWindow() : nullptr;
    if (!Window.IsValid()) return false;
    RevealWorld();
    Cover = Widget;
    CoverWindow = Window;
    Window->AddOverlaySlot(1000)
    [
        Widget
    ];
    return true;
}

void UBreakerGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
    bWorldReady = true;
    DisarmCoverWatchdog();
    if (!Cover.IsValid()) return;
    // The far side: the stage line says the arrival, and the cover lifts
    // through the gate.
    if (DeployScreen.IsValid())
    {
        DeployScreen->SetStage(FText::FromString(TEXT("ON SITE")));
    }
    BeginReveal();
}

void UBreakerGameInstance::HoldBlack()
{
    if (Cover.IsValid()) return;
    AddCover(MakeBlackCover());
}

void UBreakerGameInstance::ReleaseBlack()
{
    if (!Cover.IsValid()) return;
    BeginReveal();
}

void UBreakerGameInstance::BeginReveal()
{
    if (RevealTicker.IsValid()) return;
    ReadyTimeSeconds = FPlatformTime::Seconds();
    ReadyFrame = GFrameCounter;
    // A CORE ticker for the same reason as the deploy hold, at 0 so it runs
    // every frame: the frame gate is counted in frames. Weak, so a dying
    // session drops its own reveal.
    RevealTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this](float)
        {
            if (!Cover.IsValid())
            {
                RevealTicker.Reset();
                return false;
            }
            const float Alpha = BreakerArrivalRevealAlpha(bWorldReady,
                FPlatformTime::Seconds() - ReadyTimeSeconds, GFrameCounter - ReadyFrame, ShippedArrivalHold());
            Cover->SetRenderOpacity(1.0f - Alpha);
            if (Alpha >= 1.0f)
            {
                RevealTicker.Reset();
                RevealWorld();
                return false;
            }
            return true;
        }), 0.0f);
}

void UBreakerGameInstance::RevealWorld()
{
    DisarmCoverWatchdog();
    if (RevealTicker.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(RevealTicker);
        RevealTicker.Reset();
    }
    if (TSharedPtr<SWindow> Window = CoverWindow.Pin())
    {
        if (Cover.IsValid())
        {
            Window->RemoveOverlaySlot(Cover.ToSharedRef());
        }
    }
    Cover.Reset();
    DeployScreen.Reset();
    CoverWindow.Reset();
}
