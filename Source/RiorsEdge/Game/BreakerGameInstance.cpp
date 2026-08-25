#include "Game/BreakerGameInstance.h"

#include "Brushes/SlateImageBrush.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "MoviePlayer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"

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

void UBreakerGameInstance::Init()
{
    Super::Init();
    // The loading screen rides the load itself: PreLoadMap fires before the
    // blocking OpenLevel work and the movie player keeps drawing the widget
    // on the Slate thread while the game thread loads. This is the engine's
    // own mechanism for exactly this moment.
    FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UBreakerGameInstance::HandlePreLoadMap);
}

void UBreakerGameInstance::HandlePreLoadMap(const FString& MapName)
{
    // The plate is a PLACE read (a rift deployment), so it fronts the
    // Anchor/Gym travels and stays off the front end's own boot.
    if (MapName.Contains(FrontEndMapName())) return;
    if (!GetMoviePlayer() || GetMoviePlayer()->IsMovieCurrentlyPlaying()) return;

    if (!LoadingPlateTexture)
    {
        // The Fieldplate export, shipped as the raw PNG and loaded at
        // runtime: FImageUtils gives an uncompressed sRGB texture with no
        // mips, which is the pack's own import sheet (never DXT — the flat
        // panels band) satisfied by construction. The fonts are baked into
        // the plate, which is also why the plate ships before the live
        // widget: the widget needs Archivo and the two Plex faces imported,
        // and the stock Slate face is ruled out by the pack.
        const FString PlatePath = FPaths::ProjectContentDir()
            / TEXT("Breaker/UI/Loading/loading_campaign_1920.png");
        LoadingPlateTexture = FImageUtils::ImportFileAsTexture2D(PlatePath);
        if (LoadingPlateTexture)
        {
            LoadingPlateBrush = MakeShared<FSlateImageBrush>(
                LoadingPlateTexture, FVector2D(1920.0, 1080.0));
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[BreakerTravel] loading plate missing at %s — travelling without a screen."), *PlatePath);
        }
    }
    if (!LoadingPlateBrush.IsValid()) return;

    FLoadingScreenAttributes Attributes;
    Attributes.bAutoCompleteWhenLoadingCompletes = true;
    Attributes.bWaitForManualStop = false;
    Attributes.MinimumLoadingScreenDisplayTime = -1.0f;
    // Uniform scale-to-fit on a letterboxing background the plate's own
    // void colour, so an ultrawide gets bars rather than a stretch.
    Attributes.WidgetLoadingScreen =
        SNew(SScaleBox)
        .Stretch(EStretch::ScaleToFit)
        [
            SNew(SImage).Image(LoadingPlateBrush.Get())
        ];
    GetMoviePlayer()->SetupLoadingScreen(Attributes);
}

void UBreakerGameInstance::TravelTo(const UObject* WorldContext, FName MapName)
{
    if (MapName.IsNone()) return;
    // OpenLevel rather than a seamless transition: the maps share no geometry
    // and the load is the natural place to build the destination, so there is
    // nothing to keep alive across it except this object.
    UGameplayStatics::OpenLevel(WorldContext, MapName);
}
