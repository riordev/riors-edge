#include "Data/BreakerCensusCommandlet.h"

#include "Data/BreakerCensus.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"

DEFINE_LOG_CATEGORY_STATIC(LogBreakerCensus, Log, All);

UBreakerCensusCommandlet::UBreakerCensusCommandlet()
{
    IsClient = false;
    IsServer = false;
    // An EDITOR commandlet, deliberately. With IsEditor false the process
    // boots the game engine without GEditor, and the Kismet module asserts
    // on load before Main() is reached.
    IsEditor = true;
    LogToConsole = true;
}

int32 UBreakerCensusCommandlet::Main(const FString& Params)
{
    const TArray<UBreakerProgressionTree*>& Trees = UBreakerProgressionLibrary::GetAllFallbackTrees();
    const FString Json = BreakerCensus::Serialize(BreakerCensus::Export(Trees));

    int32 NodeCount = 0;
    for (const UBreakerProgressionTree* Tree : Trees)
    {
        NodeCount += Tree ? Tree->Nodes.Num() : 0;
    }

    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::RelativePath());
    if (!FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogBreakerCensus, Error, TEXT("could not write %s"), *Path);
        return 1;
    }
    UE_LOG(LogBreakerCensus, Display, TEXT("wrote %s: %d trees, %d nodes"), *Path, Trees.Num(), NodeCount);
    return 0;
}
