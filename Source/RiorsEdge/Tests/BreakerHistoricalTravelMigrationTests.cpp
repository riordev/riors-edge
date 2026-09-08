#include "Misc/AutomationTest.h"
#include "Save/BreakerSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerHistoricalTravelMigrationTest,
    "RiorsEdge.Save.HistoricalTravelMigration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerHistoricalTravelMigrationTest::RunTest(const FString& Parameters)
{
    int32 Count = 0;
    for (const TCHAR* Suffix : { TEXT("Weapon"), TEXT("Ability"), TEXT("All") })
    {
        for (int32 Ring = 0; Ring < 12; ++Ring)
            for (int32 Position = 1; Position <= 3; ++Position)
            {
                const FName Id(*FString::Printf(TEXT("Core.Travel.Ring%dP%d%s"), Ring, Position, Suffix));
                TestEqual(Id.ToString(), UBreakerSaveGame::LegacyCoreRankCost(Id), 1); ++Count;
            }
        for (int32 Chord = 0; Chord < 3; ++Chord)
            for (int32 Position = 1; Position <= 5; ++Position)
            {
                const FName Id(*FString::Printf(TEXT("Core.Travel.Chord%dP%d%s"), Chord, Position, Suffix));
                TestEqual(Id.ToString(), UBreakerSaveGame::LegacyCoreRankCost(Id), 1); ++Count;
            }
    }
    TestEqual(TEXT("Exact historical three-choice roster"), Count, 153);
    auto* Save = NewObject<UBreakerSaveGame>();
    Save->Progression.UnspentCorePoints = 7;
    Save->Progression.CoreNodeRanks = {
        {TEXT("Core.Precision.CalledShot"), 1}, {TEXT("Core.Precision.Fixate"), 1},
        {TEXT("Core.Travel.Ring0P1Weapon"), 1}, {TEXT("Core.Travel.Ring11P3All"), 1},
        {TEXT("Core.Travel.Chord0P1Ability"), 1}, {TEXT("Core.Travel.Chord2P5All"), 1}};
    FString Note;
    if (!TestTrue(TEXT("Untouched historical ranks migrate with current ranks"), UBreakerSaveGame::MigrateCoreLayout(*Save, 2, Note))) return false;
    TestEqual(TEXT("Exact costs 2+3+four one-point stops refunded"), Save->Progression.UnspentCorePoints, 16);
    TestEqual(TEXT("Old ranks cleared"), Save->Progression.CoreNodeRanks.Num(), 0);
    TestEqual(TEXT("Successful layout stamped"), Save->CoreLayoutVersion, 2);
    TestTrue(TEXT("Repeated migration succeeds"), UBreakerSaveGame::MigrateCoreLayout(*Save, 2, Note));
    TestEqual(TEXT("Refund is exactly once"), Save->Progression.UnspentCorePoints, 16);
    for (const TCHAR* Invalid : {TEXT("Core.Travel.Ring12P1Weapon"), TEXT("Core.Travel.Ring0P0All"),
        TEXT("Core.Travel.Ring0P4Ability"), TEXT("Core.Travel.Chord3P1Weapon"),
        TEXT("Core.Travel.Chord0P6All"), TEXT("Core.Travel.Chord0P1Unknown")})
    {
        TestEqual(TEXT("Unrecognized historical-looking ID remains unknown"), UBreakerSaveGame::LegacyCoreRankCost(FName(Invalid)), 0);
        auto* Bad = NewObject<UBreakerSaveGame>();
        Bad->Progression.UnspentCorePoints = 2;
        Bad->Progression.CoreNodeRanks = {{TEXT("Core.Travel.Ring0P1Weapon"), 1}, {FName(Invalid), 1}};
        TestFalse(TEXT("Invalid ID refuses entire transaction"), UBreakerSaveGame::MigrateCoreLayout(*Bad, 2, Note));
        TestEqual(TEXT("Refusal preserves wallet"), Bad->Progression.UnspentCorePoints, 2);
        TestEqual(TEXT("Refusal preserves both ranks"), Bad->Progression.CoreNodeRanks.Num(), 2);
        TestEqual(TEXT("Refusal preserves layout version"), Bad->CoreLayoutVersion, 1);
    }
    TestEqual(TEXT("Historical refunds coexist with the active replacement roster"), UBreakerSaveGame::ActiveCoreLayoutVersion, 2);
    return true;
}
#endif
