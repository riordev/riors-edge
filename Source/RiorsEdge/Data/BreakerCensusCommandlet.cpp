#include "Data/BreakerCensusCommandlet.h"

#include "Data/BreakerCensus.h"
#include "Items/BreakerAffixLibrary.h"
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

    // The affix library, re-serialised from what the game loaded. A file that
    // failed validation loaded as EMPTY pools, and writing those back would
    // erase the library the author was mid-way through editing — so a dirty
    // load refuses to write and reports every complaint instead.
    const TArray<FString>& AffixErrors = UBreakerAffixLibrary::GetDataErrors();
    if (!AffixErrors.IsEmpty())
    {
        for (const FString& Error : AffixErrors)
        {
            UE_LOG(LogBreakerCensus, Error, TEXT("%s"), *Error);
        }
        UE_LOG(LogBreakerCensus, Error, TEXT("%s did not load clean; not rewriting it"), *BreakerCensus::AffixesRelativePath());
        return 1;
    }
    const FBreakerAffixLibraryData& Affixes = UBreakerAffixLibrary::GetData();
    const FString AffixJson = BreakerCensus::ExportAffixes(Affixes);
    const FString AffixPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::AffixesRelativePath());
    if (!FFileHelper::SaveStringToFile(AffixJson, *AffixPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogBreakerCensus, Error, TEXT("could not write %s"), *AffixPath);
        return 1;
    }
    int32 LeanRows = 0;
    for (const FBreakerArchetypeLeans& Table : Affixes.Leans) { LeanRows += Table.Rows.Num(); }
    UE_LOG(LogBreakerCensus, Display, TEXT("wrote %s: %d slice, %d aberrant, %d anomalous, %d downside, %d elemental, %d leans across %d archetypes, %d caps"),
        *AffixPath, Affixes.Slice.Num(), Affixes.Aberrant.Num(), Affixes.Anomalous.Num(), Affixes.Downsides.Num(),
        Affixes.Elemental.AffixId.IsNone() ? 0 : 1, LeanRows, Affixes.Leans.Num(), Affixes.Caps.Num());
    return 0;
}
