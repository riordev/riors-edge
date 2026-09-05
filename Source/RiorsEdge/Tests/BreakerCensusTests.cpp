#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Data/BreakerCensus.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS

// THE COMMITTED CENSUS IS THE LIVE LIBRARY, OR THIS IS RED.
//
// Docs/STATE.md regenerates from Data/progression.json (status.py
// --from-data). A node edit that lands without a re-export would make the
// report measure last week's trees and say nothing about it: the silent
// staleness the census moved to data to get rid of. So the file is pinned to
// a fresh export of the same trees, byte for byte after line endings.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCensusFreshTest,
    "RiorsEdge.Data.Census.Fresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCensusFreshTest::RunTest(const FString& Parameters)
{
    const TArray<UBreakerProgressionTree*>& Trees = UBreakerProgressionLibrary::GetAllFallbackTrees();
    int32 NodeCount = 0;
    for (const UBreakerProgressionTree* Tree : Trees)
    {
        NodeCount += Tree ? Tree->Nodes.Num() : 0;
    }
    // The shipped configuration: an export of nothing cannot be fresh.
    TestEqual(TEXT("Every fallback tree is exported"), Trees.Num(), 16);
    TestTrue(TEXT("The census carries the authored library, not a stub"), NodeCount >= 100);

    const FString Fresh = BreakerCensus::Serialize(BreakerCensus::Export(Trees));

    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::RelativePath());
    FString Committed;
    if (!FFileHelper::LoadFileToString(Committed, *Path))
    {
        AddError(FString::Printf(TEXT("%s is missing. Run `bash Scripts/ue-census.sh` and commit the file."), *Path));
        return false;
    }
    Committed.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

    if (Committed != Fresh)
    {
        AddError(FString::Printf(
            TEXT("%s is STALE against the built trees (%d trees, %d nodes). ")
            TEXT("Run `bash Scripts/ue-census.sh` and commit Data/progression.json in the same change as the library edit."),
            *Path, Trees.Num(), NodeCount));
        return false;
    }
    return true;
}

#endif
