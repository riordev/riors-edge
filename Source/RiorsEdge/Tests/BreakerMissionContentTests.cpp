#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Save/BreakerMissionContent.h"

#if WITH_DEV_AUTOMATION_TESTS

// THE POINT BUDGETS ARE HELD BY THE VALIDATOR, NOT BY THE AUTHOR'S ARITHMETIC.
//
// Doctrine points are paid two per main-story benchmark, one benchmark per
// act, eight in all (O111); Core world points are fifteen named sources, each
// granted once, in the act that names them (O7). A mission file that pays
// seven, or nine, or a source nobody registered, or the same source twice,
// must refuse to load — because the alternative is a character paid the
// wrong number of points by a file that looked fine. Every bad file below
// goes through the same parse the loader uses; nothing here touches disk.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMissionPointBudgetsTest,
    "RiorsEdge.Missions.PointBudgets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    struct FBreakerMissionTestRow
    {
        int32 Act = 1;
        int32 DoctrinePoints = 0;
        TArray<FString> CorePoints;
    };

    // A file of Unlock-only missions, one per row. Unlocks are the only kind
    // with nothing to resolve against another file, so the file isolates the
    // budget rules from the id rules.
    FString BreakerMissionTestFile(const TArray<FBreakerMissionTestRow>& Rows)
    {
        FString Out = TEXT("{ \"version\": 1, \"rifts\": [], \"missions\": [");
        for (int32 Index = 0; Index < Rows.Num(); ++Index)
        {
            const FBreakerMissionTestRow& Row = Rows[Index];
            if (Index > 0) { Out += TEXT(","); }
            Out += FString::Printf(TEXT("{ \"id\": \"Act%d.Test%d\", \"act\": %d, \"title\": \"\", \"quests\": [], \"beats\": ["), Row.Act, Index, Row.Act);
            bool bFirstBeat = true;
            if (Row.DoctrinePoints > 0)
            {
                Out += FString::Printf(TEXT("{ \"id\": \"Doctrine\", \"kind\": \"Unlock\", \"doctrinePoints\": %d }"), Row.DoctrinePoints);
                bFirstBeat = false;
            }
            for (int32 Core = 0; Core < Row.CorePoints.Num(); ++Core)
            {
                if (!bFirstBeat) { Out += TEXT(","); }
                Out += FString::Printf(TEXT("{ \"id\": \"Core%d\", \"kind\": \"Unlock\", \"corePoint\": \"%s\" }"), Core, *Row.CorePoints[Core]);
                bFirstBeat = false;
            }
            Out += TEXT("] }");
        }
        Out += TEXT("] }");
        return Out;
    }

    bool BreakerMissionTestErrorsMention(const TArray<FString>& Errors, const TCHAR* Needle)
    {
        for (const FString& Error : Errors)
        {
            if (Error.Contains(Needle)) { return true; }
        }
        return false;
    }
}

bool FBreakerMissionPointBudgetsTest::RunTest(const FString& Parameters)
{
    const int32 PerBenchmark = UBreakerProgressionLibrary::DoctrinePointsPerBenchmark;
    const int32 WholeGrant = UBreakerProgressionLibrary::DoctrinePointGrant;
    const int32 Benchmarks = WholeGrant / PerBenchmark;
    TestEqual(TEXT("The shipped configuration: four benchmarks of two"), WholeGrant, 8);
    TestEqual(TEXT("A benchmark pays two"), PerBenchmark, 2);

    FBreakerMissionData Data;
    TArray<FString> Errors;

    // ---- doctrine: one act, one benchmark ----------------------------------
    TestTrue(TEXT("One act paying one benchmark is clean"),
        UBreakerMissionLibrary::ParseMissionsJson(BreakerMissionTestFile({ { 1, PerBenchmark, {} } }), Data, Errors));
    TestEqual(TEXT("No complaints"), Errors.Num(), 0);
    TestEqual(TEXT("The arrival flag is registered for the parsed mission"), Data.Flags.Num(), 1);

    TestFalse(TEXT("One act paying one short is refused"),
        UBreakerMissionLibrary::ParseMissionsJson(BreakerMissionTestFile({ { 1, PerBenchmark - 1, {} } }), Data, Errors));
    TestTrue(TEXT("...and the refusal names the doctrine sum"), BreakerMissionTestErrorsMention(Errors, TEXT("doctrine points")));
    TestTrue(TEXT("...and a refused file serves nothing"), Data.Missions.IsEmpty() && Data.Flags.IsEmpty());

    TestFalse(TEXT("One act paying one over is refused"),
        UBreakerMissionLibrary::ParseMissionsJson(BreakerMissionTestFile({ { 1, PerBenchmark + 1, {} } }), Data, Errors));
    TestTrue(TEXT("...and the refusal names the doctrine sum"), BreakerMissionTestErrorsMention(Errors, TEXT("doctrine points")));

    TestFalse(TEXT("An act paying nothing is refused"),
        UBreakerMissionLibrary::ParseMissionsJson(BreakerMissionTestFile({ { 1, 0, {} } }), Data, Errors));
    TestTrue(TEXT("...and the refusal names the doctrine sum"), BreakerMissionTestErrorsMention(Errors, TEXT("doctrine points")));

    // ---- doctrine: the whole grant across every benchmark ------------------
    TArray<FBreakerMissionTestRow> AllActs;
    for (int32 Act = 1; Act <= Benchmarks; ++Act) { AllActs.Add({ Act, PerBenchmark, {} }); }
    TestTrue(TEXT("Every benchmark paid is the whole grant and is clean"),
        UBreakerMissionLibrary::ParseMissionsJson(BreakerMissionTestFile(AllActs), Data, Errors));
    TestEqual(TEXT("No complaints"), Errors.Num(), 0);

    TArray<FBreakerMissionTestRow> Seven = AllActs;
    Seven.Last().DoctrinePoints = PerBenchmark - 1;
    TestFalse(TEXT("A file summing to seven is refused"),
        UBreakerMissionLibrary::ParseMissionsJson(BreakerMissionTestFile(Seven), Data, Errors));
    TestTrue(TEXT("...and the refusal names the doctrine sum"), BreakerMissionTestErrorsMention(Errors, TEXT("doctrine points")));

    TArray<FBreakerMissionTestRow> Nine = AllActs;
    Nine.Last().DoctrinePoints = PerBenchmark + 1;
    TestFalse(TEXT("A file summing to nine is refused"),
        UBreakerMissionLibrary::ParseMissionsJson(BreakerMissionTestFile(Nine), Data, Errors));
    TestTrue(TEXT("...and the refusal names the whole grant"), BreakerMissionTestErrorsMention(Errors, TEXT("the whole grant")));

    TArray<FBreakerMissionTestRow> FiveBenchmarks = AllActs;
    FiveBenchmarks.Add({ Benchmarks + 1, PerBenchmark, {} });
    TestFalse(TEXT("A fifth benchmark breaks the cap"),
        UBreakerMissionLibrary::ParseMissionsJson(BreakerMissionTestFile(FiveBenchmarks), Data, Errors));
    TestTrue(TEXT("...and the refusal names the whole grant"), BreakerMissionTestErrorsMention(Errors, TEXT("the whole grant")));

    // ---- Core: known source, once, in its act -------------------------------
    TestTrue(TEXT("A known act-one source in an act-one mission is clean"),
        UBreakerMissionLibrary::ParseMissionsJson(BreakerMissionTestFile({ { 1, PerBenchmark, { TEXT("FirstForge") } } }), Data, Errors));
    TestEqual(TEXT("No complaints"), Errors.Num(), 0);

    TestFalse(TEXT("An unregistered source is refused"),
        UBreakerMissionLibrary::ParseMissionsJson(BreakerMissionTestFile({ { 1, PerBenchmark, { TEXT("NotASource") } } }), Data, Errors));
    TestTrue(TEXT("...and the refusal names the source"), BreakerMissionTestErrorsMention(Errors, TEXT("not a world point source")));

    TestFalse(TEXT("A source granted twice is refused"),
        UBreakerMissionLibrary::ParseMissionsJson(BreakerMissionTestFile({ { 1, PerBenchmark, { TEXT("FirstForge"), TEXT("FirstForge") } } }), Data, Errors));
    TestTrue(TEXT("...and the refusal says twice"), BreakerMissionTestErrorsMention(Errors, TEXT("granted twice")));

    TestFalse(TEXT("An act-two source in an act-one mission is refused"),
        UBreakerMissionLibrary::ParseMissionsJson(BreakerMissionTestFile({ { 1, PerBenchmark, { TEXT("ActTwoBoss") } } }), Data, Errors));
    TestTrue(TEXT("...and the refusal names the act"), BreakerMissionTestErrorsMention(Errors, TEXT("act 2 source in an act 1 mission")));

    // ---- Unlock shape: exactly one grant ------------------------------------
    TestFalse(TEXT("An Unlock granting two things is refused"),
        UBreakerMissionLibrary::ParseMissionsJson(
            TEXT("{ \"version\": 1, \"rifts\": [], \"missions\": [ { \"id\": \"Act1.Test\", \"act\": 1, \"title\": \"\", \"quests\": [], \"beats\": [ { \"id\": \"Both\", \"kind\": \"Unlock\", \"doctrinePoints\": 2, \"corePoint\": \"FirstForge\" } ] } ] }"),
            Data, Errors));
    TestTrue(TEXT("...and the refusal says exactly one"), BreakerMissionTestErrorsMention(Errors, TEXT("exactly one")));

    TestFalse(TEXT("A field the kind does not own is refused"),
        UBreakerMissionLibrary::ParseMissionsJson(
            TEXT("{ \"version\": 1, \"rifts\": [], \"missions\": [ { \"id\": \"Act1.Test\", \"act\": 1, \"title\": \"\", \"quests\": [], \"beats\": [ { \"id\": \"Doctrine\", \"kind\": \"Unlock\", \"doctrinePoints\": 2, \"npc\": \"Quartermaster\" } ] } ] }"),
            Data, Errors));
    TestTrue(TEXT("...and the refusal names the field"), BreakerMissionTestErrorsMention(Errors, TEXT("not a field of this kind")));

    return true;
}

#endif
