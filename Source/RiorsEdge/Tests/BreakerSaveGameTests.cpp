#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerForgeLibrary.h"
#include "Save/BreakerSaveGame.h"

// FORGE WALLET PERSISTENCE (CONTEXT.md next-action 3: "the crafting wallet is
// not in UBreakerSaveGame"). FBreakerForgeWallet was replicated on
// UBreakerEquipmentComponent but SaveGameState/LoadGameState never touched
// it, so every balance a player had earned was gone the moment the process
// restarted. Since the one-currency consolidation (owner ruling 2026-08-16)
// the wallet holds a single Riftglass balance; the v3 -> v4 migration folds
// the old Slag/Flux/Sigil array into it, covered below.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeWalletRoundTripTest,
    "RiorsEdge.Save.ForgeWalletRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeWalletRoundTripTest::RunTest(const FString& Parameters)
{
    UBreakerSaveGame* Written = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Written) { AddError(TEXT("Could not create a save object")); return false; }

    Written->ForgeWallet.Add(144);
    Written->SaveVersion = UBreakerSaveGame::CurrentSaveVersion;

    // Memory rather than a slot, exactly like the quest round trip
    // (RiorsEdge.Save.QuestFlagRoundTrip), so the suite never touches the
    // player's real BreakerSave0 while still exercising the actual UPROPERTY
    // serializer.
    TArray<uint8> Bytes;
    TestTrue(TEXT("Save serializes"), UGameplayStatics::SaveGameToMemory(Written, Bytes));
    UBreakerSaveGame* Read = Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!Read) { AddError(TEXT("Save did not deserialize")); return false; }

    TestEqual(TEXT("The Riftglass balance survives serialization"), Read->ForgeWallet.Get(), 144);

    // And into the equipment component, the way ABreakerCharacter::LoadGameState
    // restores it.
    AActor* Owner = NewObject<AActor>();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    Equipment->RestoreForgeWallet(Read->ForgeWallet);
    TestEqual(TEXT("The restored wallet reaches the equipment component"),
        Equipment->GetForgeWallet().Get(), 144);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSaveWalletMigrationTest,
    "RiorsEdge.Save.WalletMigrationToRiftglass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSaveWalletMigrationTest::RunTest(const FString& Parameters)
{
    // NOT a pin on the current version — this test owns the v3 -> v4 wallet
    // step, and pinning the head version here made an unrelated migration
    // (O100's v4 -> v5 unlock step) fail a wallet test. What it needs is that
    // the step it owns still exists below the head.
    TestTrue(TEXT("The one-currency step is still below the head version"), UBreakerSaveGame::CurrentSaveVersion >= 4);

    // A faithful v3 payload: version 3, real quest state, and a wallet whose
    // balance lives in the legacy Slag/Flux/Sigil array — exactly what a
    // pre-consolidation build serialized.
    UBreakerSaveGame* Old = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Old) { AddError(TEXT("Could not create a save object")); return false; }
    Old->SaveVersion = 3;
    Old->QuestFlags = { TEXT("Quest.FirstContract.Accepted"), TEXT("Quest.FirstContract.Offered") };
    Old->QuestCounters.Add(TEXT("Quest.FirstContract.Kills"), 3);
    Old->ForgeWallet.Amounts = { 42, 7, 1 };  // Slag, Flux, Sigil

    FString Note;
    TestTrue(TEXT("A v3 save loads"), UBreakerSaveGame::MigrateToCurrent(*Old, Note));
    TestEqual(TEXT("It is now current"), Old->SaveVersion, UBreakerSaveGame::CurrentSaveVersion);
    // A migration step ran (v3 -> v4), so it reports itself exactly like the
    // v1 -> v2 step's own test expects.
    TestFalse(TEXT("The migration reports itself"), Note.IsEmpty());

    // THE CONVERSION, total value preserved at the stated 1/6/60 rates
    // (FBreakerForgeWallet::CollapseLegacyDenominations): 42 Slag + 7 Flux +
    // 1 Sigil = 42 + 42 + 60 = 144 Riftglass, and the legacy array is gone.
    TestEqual(TEXT("The three denominations fold into one Riftglass balance"), Old->ForgeWallet.Get(), 144);
    TestTrue(TEXT("The legacy array is emptied"), Old->ForgeWallet.Amounts.IsEmpty());

    // Everything else survives untouched — the "preserves everything else"
    // half of the requirement.
    TestEqual(TEXT("Flag count is untouched"), Old->QuestFlags.Num(), 2);
    TestTrue(TEXT("Accepted survives"), Old->QuestFlags.Contains(FName(TEXT("Quest.FirstContract.Accepted"))));
    TestTrue(TEXT("Offered survives"), Old->QuestFlags.Contains(FName(TEXT("Quest.FirstContract.Offered"))));
    TestEqual(TEXT("Counter survives"), Old->QuestCounters.FindRef(TEXT("Quest.FirstContract.Kills")), 3);

    // Idempotent, same discipline as RiorsEdge.Save.Migration's own v1 -> v2
    // assertion: migrating an already-current save changes nothing — above
    // all it must not fold twice and double a balance.
    FString SecondNote;
    TestTrue(TEXT("A current save loads"), UBreakerSaveGame::MigrateToCurrent(*Old, SecondNote));
    TestTrue(TEXT("A current save reports no migration"), SecondNote.IsEmpty());
    TestEqual(TEXT("A current save's wallet is unchanged"), Old->ForgeWallet.Get(), 144);

    // A v2 payload (no wallet property at all) still arrives empty rather
    // than invented, across BOTH remaining steps.
    UBreakerSaveGame* Ancient = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Ancient) { AddError(TEXT("Could not create a save object")); return false; }
    Ancient->SaveVersion = 2;
    FString AncientNote;
    TestTrue(TEXT("A v2 save loads"), UBreakerSaveGame::MigrateToCurrent(*Ancient, AncientNote));
    TestEqual(TEXT("A migrated v2 file has an empty Riftglass balance"), Ancient->ForgeWallet.Get(), 0);

    // Refuse-to-load still holds at the new ceiling (Save-Architecture 5.2):
    // never repair a file from a newer build, above all not one carrying
    // currency this build does not know how to trust.
    UBreakerSaveGame* Future = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Future) { AddError(TEXT("Could not create a save object")); return false; }
    Future->SaveVersion = UBreakerSaveGame::CurrentSaveVersion + 1;
    Future->ForgeWallet.Add(9);
    FString FutureNote;
    TestFalse(TEXT("A save from a build newer than v4 is still refused"), UBreakerSaveGame::MigrateToCurrent(*Future, FutureNote));
    TestFalse(TEXT("The refusal is explained"), FutureNote.IsEmpty());
    TestEqual(TEXT("A refused file is not repaired"), Future->ForgeWallet.Get(), 9);
    TestEqual(TEXT("A refused file keeps its own out-of-range version"),
        Future->SaveVersion, UBreakerSaveGame::CurrentSaveVersion + 1);
    return true;
}

#endif
