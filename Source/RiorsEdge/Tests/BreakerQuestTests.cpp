#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "Save/BreakerQuestJournal.h"
#include "Save/BreakerSaveGame.h"

// THE REGRESSION TEST FOR THE DATA-LOSS BUG.
//
// Shipped behaviour: ABreakerCharacter::AddQuestFlag was `QuestFlags.AddUnique`
// into a bare array, and SaveGameState() was called only from EndPlay and a few
// menu commit points. A flag earned in conversation reached disk only if the
// session later ended cleanly, so a crash after a story beat lost the beat.
//
// What this pins is the contract that fixes it: setting a NEW flag requests a
// save in the same call, and setting one that is already present requests
// nothing. If the write-through is removed, the first block fails.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerQuestFlagPersistenceTest,
    "RiorsEdge.Save.QuestFlagPersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerQuestFlagPersistenceTest::RunTest(const FString& Parameters)
{
    UBreakerQuestJournal* Journal = NewObject<UBreakerQuestJournal>();
    int32 PersistRequests = 0;
    Journal->OnPersistRequested.AddLambda([&PersistRequests]() { ++PersistRequests; });

    TestTrue(TEXT("A new flag is accepted"), Journal->SetFlag(TEXT("Quest.Test.Beat")));
    TestEqual(TEXT("Setting a new flag persists exactly once"), PersistRequests, 1);
    TestTrue(TEXT("The flag reads back"), Journal->HasFlag(TEXT("Quest.Test.Beat")));

    TestFalse(TEXT("A repeated flag is not a change"), Journal->SetFlag(TEXT("Quest.Test.Beat")));
    TestEqual(TEXT("A repeated flag costs no disk write"), PersistRequests, 1);

    TestFalse(TEXT("NAME_None is never a flag"), Journal->SetFlag(NAME_None));
    TestEqual(TEXT("NAME_None costs no disk write"), PersistRequests, 1);
    TestFalse(TEXT("NAME_None never reads back as set"), Journal->HasFlag(NAME_None));
    TestEqual(TEXT("Only the one real flag is stored"), Journal->GetFlags().Num(), 1);

    // Counters: progress persists too, and crossing the threshold sets the
    // objective's flag in the same call so no caller can advance progress and
    // forget to close the objective.
    const int32 BeforeProgress = PersistRequests;
    TestFalse(TEXT("Below threshold does not complete"), Journal->AddProgress(TEXT("Quest.Test.Kills"), 1, 3, TEXT("Quest.Test.Killed")));
    TestEqual(TEXT("Partial progress is persisted"), PersistRequests, BeforeProgress + 1);
    TestEqual(TEXT("Counter advanced"), Journal->GetCounter(TEXT("Quest.Test.Kills")), 1);
    Journal->AddProgress(TEXT("Quest.Test.Kills"), 1, 3, TEXT("Quest.Test.Killed"));
    TestFalse(TEXT("Still short of the threshold"), Journal->HasFlag(TEXT("Quest.Test.Killed")));
    TestTrue(TEXT("Reaching the threshold completes"), Journal->AddProgress(TEXT("Quest.Test.Kills"), 1, 3, TEXT("Quest.Test.Killed")));
    TestTrue(TEXT("Objective flag is set"), Journal->HasFlag(TEXT("Quest.Test.Killed")));

    // Monotonic: there is no remove, and a counter never walks backwards.
    FBreakerQuestFlagSet Set = Journal->GetState();
    TestFalse(TEXT("A counter never decreases"), Set.RaiseCounter(TEXT("Quest.Test.Kills"), 1));
    TestEqual(TEXT("Counter held its value"), Set.GetCounter(TEXT("Quest.Test.Kills")), 3);
    return true;
}

// The other half of the same bug: whatever the journal holds must survive a
// real serialization round trip through UBreakerSaveGame.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerQuestFlagRoundTripTest,
    "RiorsEdge.Save.QuestFlagRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerQuestFlagRoundTripTest::RunTest(const FString& Parameters)
{
    UBreakerSaveGame* Written = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Written) { AddError(TEXT("Could not create a save object")); return false; }

    Written->QuestFlags = { TEXT("Quest.FirstContract.Offered"), TEXT("Quest.FirstContract.Accepted") };
    Written->QuestCounters.Add(TEXT("Quest.FirstContract.Kills"), 5);
    Written->SaveVersion = UBreakerSaveGame::CurrentSaveVersion;

    // Memory rather than a slot so the suite never touches the player's real
    // BreakerSave0, while still exercising the actual UPROPERTY serializer.
    TArray<uint8> Bytes;
    TestTrue(TEXT("Save serializes"), UGameplayStatics::SaveGameToMemory(Written, Bytes));
    UBreakerSaveGame* Read = Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!Read) { AddError(TEXT("Save did not deserialize")); return false; }

    TestEqual(TEXT("Flag count survives"), Read->QuestFlags.Num(), 2);
    TestTrue(TEXT("Offered survives"), Read->QuestFlags.Contains(FName(TEXT("Quest.FirstContract.Offered"))));
    TestTrue(TEXT("Accepted survives"), Read->QuestFlags.Contains(FName(TEXT("Quest.FirstContract.Accepted"))));
    TestEqual(TEXT("Counter survives"), Read->QuestCounters.FindRef(TEXT("Quest.FirstContract.Kills")), 5);

    // And back into a journal, which is how the character restores it.
    UBreakerQuestJournal* Journal = NewObject<UBreakerQuestJournal>();
    Journal->RestoreFrom(Read->QuestFlags, Read->QuestCounters);
    TestTrue(TEXT("Restored journal reads the flag"), Journal->HasFlag(TEXT("Quest.FirstContract.Accepted")));
    TestEqual(TEXT("Restored journal reads the counter"), Journal->GetCounter(TEXT("Quest.FirstContract.Kills")), 5);
    return true;
}

// A player's existing BreakerSave0 must still load. Version 1 is what shipped;
// this proves the v1 -> v2 step is a migration rather than a silent misread.
//
// THE NAME IS FLAT ON PURPOSE, and it was not: as "RiorsEdge.Save.Migration" it
// was a strict PREFIX of RiorsEdge.Save.Migration.V4ToV5, so UE's automation
// tree read it as a parent NODE of that test rather than as a test, and it was
// never enumerated and never ran. It compiled, it was declared, and every
// count balanced -- 424 declared against 424 started -- because a second defect
// hid one name on the other side of the comparison. This is the SECOND instance
// of the collision in this suite; the first is recorded at
// FBreakerRuleBandImpactTest, which was renamed off PowerBand.RuleImpact for
// exactly this. Never name a test as a prefix of another test.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSaveMigrationTest,
    "RiorsEdge.Save.MigrationV1ToV2",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSaveMigrationTest::RunTest(const FString& Parameters)
{
    // A faithful v1 payload: version 1, the v1 flag spellings, and no counters
    // (a v1 file has no such property, so it deserializes empty).
    UBreakerSaveGame* Old = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Old) { AddError(TEXT("Could not create a save object")); return false; }
    Old->SaveVersion = 1;
    Old->QuestFlags = { TEXT("Quest.MetForgeKeeper"), TEXT("Quest.AcceptedFirstContract"), TEXT("Mod.SomeBranchFlag") };

    FString Note;
    TestTrue(TEXT("A v1 save loads"), UBreakerSaveGame::MigrateToCurrent(*Old, Note));
    TestEqual(TEXT("It is now current"), Old->SaveVersion, UBreakerSaveGame::CurrentSaveVersion);
    TestFalse(TEXT("The migration reports itself"), Note.IsEmpty());

    TestFalse(TEXT("The v1 contract flag is gone"), Old->QuestFlags.Contains(FName(TEXT("Quest.AcceptedFirstContract"))));
    TestTrue(TEXT("It was renamed, not dropped"), Old->QuestFlags.Contains(FName(TEXT("Quest.FirstContract.Accepted"))));
    TestTrue(TEXT("The offered prerequisite is backfilled"), Old->QuestFlags.Contains(FName(TEXT("Quest.FirstContract.Offered"))));
    TestTrue(TEXT("Unrelated flags are untouched"), Old->QuestFlags.Contains(FName(TEXT("Quest.MetForgeKeeper"))));
    TestTrue(TEXT("Unknown flags are quarantined, never dropped"), Old->QuestFlags.Contains(FName(TEXT("Mod.SomeBranchFlag"))));

    // Idempotent: migrating an already-current save changes nothing.
    const int32 CountAfterFirst = Old->QuestFlags.Num();
    FString SecondNote;
    TestTrue(TEXT("A current save loads"), UBreakerSaveGame::MigrateToCurrent(*Old, SecondNote));
    TestTrue(TEXT("A current save reports no migration"), SecondNote.IsEmpty());
    TestEqual(TEXT("A current save is unchanged"), Old->QuestFlags.Num(), CountAfterFirst);

    // A save that never accepted the contract gets no backfill.
    UBreakerSaveGame* Untouched = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    Untouched->SaveVersion = 1;
    Untouched->QuestFlags = { TEXT("Quest.CheckedVendor") };
    TestTrue(TEXT("A v1 save with no contract loads"), UBreakerSaveGame::MigrateToCurrent(*Untouched, Note));
    TestEqual(TEXT("Nothing was invented"), Untouched->QuestFlags.Num(), 1);

    // Refuse-to-load: a file from a newer build is not opened, not repaired and
    // above all not overwritten (Save-Architecture 5.2).
    UBreakerSaveGame* Future = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    Future->SaveVersion = UBreakerSaveGame::CurrentSaveVersion + 1;
    TestFalse(TEXT("A newer save is refused"), UBreakerSaveGame::MigrateToCurrent(*Future, Note));
    TestFalse(TEXT("The refusal is explained"), Note.IsEmpty());
    return true;
}


// ---------------------------------------------------------------------------
// O111 - the v5 -> v6 class-currency step.
//
// It refunds nothing and reads nothing, so what has to be asserted is what it
// CLEARS and what it LEAVES ALONE. The second half is the one a migration gets
// wrong: a step that reaches past the fifteen ids it knows about would clear a
// commitment it has no business touching, and there is no way to tell
// afterwards.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSaveMigrationV5ToV6Test,
    "RiorsEdge.Save.Migration.V5ToV6",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSaveMigrationV5ToV6Test::RunTest(const FString& Parameters)
{
    // A faithful v5 payload: a class build the player paid for, an unspent
    // class wallet, and a commitment naming one of the fifteen retiring ids.
    UBreakerSaveGame* Save = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Save) { AddError(TEXT("Could not create a save object")); return false; }
    Save->SaveVersion = 5;
    Save->Progression.PermanentClass = EBreakerClassId::Swift;
    Save->Progression.ClassNodeRanks.Add({TEXT("Swift.Kinetic.Carry"), 2});
    Save->Progression.ClassNodeRanks.Add({TEXT("Swift.Marksman.Steady"), 1});
    Save->Progression.UnspentClassPoints = 7;
    Save->Progression.LevelClassPointsGranted = 30;
    Save->Progression.CommittedBranch = TEXT("Swift.Kinetic");
    // Core state is NOT this step's business and is asserted untouched below.
    Save->Progression.CoreNodeRanks.Add({TEXT("Core.Precision.Sightline"), 1});
    Save->Progression.UnspentCorePoints = 11;

    FString Note;
    TestTrue(TEXT("A v5 save loads"), UBreakerSaveGame::MigrateToCurrent(*Save, Note));
    TestEqual(TEXT("It is stamped current"), Save->SaveVersion, UBreakerSaveGame::CurrentSaveVersion);
    TestFalse(TEXT("The migration reports itself"), Note.IsEmpty());

    TestEqual(TEXT("The class rank array is cleared"), Save->Progression.ClassNodeRanks.Num(), 0);
    TestEqual(TEXT("The class wallet is zeroed"), Save->Progression.UnspentClassPoints, 0);
    TestEqual(TEXT("The class granted-counter is zeroed"), Save->Progression.LevelClassPointsGranted, 0);
    TestEqual(TEXT("A commitment naming a retiring branch is cleared"),
        Save->Progression.CommittedBranch, FName(NAME_None));

    // NOTHING IS REFUNDED. O27 deletes the freed points rather than folding
    // them, so a migrated character opens with an empty doctrine wallet and is
    // paid its eight when it re-commits at the Forge.
    TestEqual(TEXT("The doctrine wallet is seeded empty, not with eight"),
        Save->Progression.UnspentDoctrinePoints, 0);
    TestEqual(TEXT("No doctrine ranks are invented"), Save->Progression.DoctrineNodeRanks.Num(), 0);

    // Core is another pool's business.
    TestEqual(TEXT("Core ranks survive untouched"), Save->Progression.CoreNodeRanks.Num(), 1);
    TestEqual(TEXT("The core wallet survives untouched"), Save->Progression.UnspentCorePoints, 11);

    // Idempotent: a save already at v6 is not stepped again.
    FString SecondNote;
    TestTrue(TEXT("A current save loads"), UBreakerSaveGame::MigrateToCurrent(*Save, SecondNote));
    TestTrue(TEXT("A current save reports no migration"), SecondNote.IsEmpty());
    TestEqual(TEXT("The core wallet is still untouched"), Save->Progression.UnspentCorePoints, 11);

    // A commitment naming anything OUTSIDE the frozen fifteen is left alone.
    // This is the half that cannot be recovered if it is wrong: the step must
    // not reach past the ids it knows about.
    UBreakerSaveGame* Foreign = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    Foreign->SaveVersion = 5;
    Foreign->Progression.CommittedBranch = TEXT("Mod.SomeOtherBranch");
    TestTrue(TEXT("A v5 save with a foreign commitment loads"), UBreakerSaveGame::MigrateToCurrent(*Foreign, Note));
    TestEqual(TEXT("A commitment outside the fifteen is untouched"),
        Foreign->Progression.CommittedBranch, FName(TEXT("Mod.SomeOtherBranch")));
    return true;
}

#endif
