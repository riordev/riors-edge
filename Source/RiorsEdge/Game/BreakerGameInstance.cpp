#include "Game/BreakerGameInstance.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

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

void UBreakerGameInstance::TravelTo(const UObject* WorldContext, FName MapName)
{
    if (MapName.IsNone()) return;
    // OpenLevel rather than a seamless transition: the maps share no geometry
    // and the load is the natural place to build the destination, so there is
    // nothing to keep alive across it except this object.
    UGameplayStatics::OpenLevel(WorldContext, MapName);
}
