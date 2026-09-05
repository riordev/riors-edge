#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Data/BreakerCensus.h"
#include "Items/BreakerAffixLibrary.h"
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

// THE COMMITTED AFFIX FILE IS THE LOADED LIBRARY, OR THIS IS RED.
//
// Data/affixes.json is what UBreakerAffixLibrary loads, so the file cannot go
// stale against the pools the way the census could against the trees; what
// it CAN do is drift from the canonical spelling — a row out of pool order,
// a hand-typed "helmet", a number written with a trailing zero — and load
// fine while the commandlet would rewrite it differently on the next run.
// Pinning the file to a re-export of what was loaded keeps one spelling, so
// a diff on this file is always a change of content and never of form. The
// validator's complaints are reported first: a broken file should name its
// breaks, not fail a byte comparison.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAffixesFreshTest,
    "RiorsEdge.Data.Affixes.Fresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAffixesFreshTest::RunTest(const FString& Parameters)
{
    const TArray<FString>& LoadErrors = UBreakerAffixLibrary::GetDataErrors();
    for (const FString& Error : LoadErrors)
    {
        AddError(Error);
    }
    if (!LoadErrors.IsEmpty())
    {
        return false;
    }

    const FBreakerAffixLibraryData& Data = UBreakerAffixLibrary::GetData();
    int32 LeanRows = 0;
    for (const FBreakerArchetypeLeans& Table : Data.Leans)
    {
        LeanRows += Table.Rows.Num();
    }
    // The shipped configuration: an export of nothing cannot be fresh.
    TestTrue(TEXT("The slice pool carries the authored library, not a stub"), Data.Slice.Num() >= 20);
    TestTrue(TEXT("The Aberrant pool is populated"), Data.Aberrant.Num() > 0);
    TestTrue(TEXT("The Anomalous pool is populated"), Data.Anomalous.Num() > 0);
    TestTrue(TEXT("The downside pool is populated"), Data.Downsides.Num() > 0);
    TestTrue(TEXT("The elemental row is Core.ElementalResist"),
        Data.Elemental.AffixId == FName(TEXT("Core.ElementalResist")));
    TestEqual(TEXT("Every archetype has a lean table"),
        Data.Leans.Num(), static_cast<int32>(EBreakerWeaponArchetype::Count));
    TestTrue(TEXT("The lean tables carry rows"), LeanRows > 0);
    AddInfo(FString::Printf(TEXT("Affix library: %d slice, %d aberrant, %d anomalous, %d downside, 1 elemental, %d leans across %d archetypes, %d caps"),
        Data.Slice.Num(), Data.Aberrant.Num(), Data.Anomalous.Num(), Data.Downsides.Num(), LeanRows, Data.Leans.Num(), Data.Caps.Num()));

    const FString Fresh = BreakerCensus::ExportAffixes(Data);

    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::AffixesRelativePath());
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
            TEXT("%s is not in canonical form against what the library loaded. ")
            TEXT("Run `bash Scripts/ue-census.sh` and commit Data/affixes.json in the same change."),
            *Path));
        return false;
    }
    return true;
}

#endif
