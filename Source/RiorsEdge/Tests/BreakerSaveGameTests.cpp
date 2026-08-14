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
// it, so every Slag/Flux/Sigil balance a player had earned was gone the
// moment the process restarted.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeWalletRoundTripTest,
    "RiorsEdge.Save.ForgeWalletRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeWalletRoundTripTest::RunTest(const FString& Parameters)
{
    UBreakerSaveGame* Written = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Written) { AddError(TEXT("Could not create a save object")); return false; }

    Written->ForgeWallet.Add(EBreakerForgeCurrency::Slag, 42);
    Written->ForgeWallet.Add(EBreakerForgeCurrency::Flux, 7);
    Written->ForgeWallet.Add(EBreakerForgeCurrency::Sigil, 1);
    Written->SaveVersion = UBreakerSaveGame::CurrentSaveVersion;

    // Memory rather than a slot, exactly like the quest round trip
    // (RiorsEdge.Save.QuestFlagRoundTrip), so the suite never touches the
    // player's real BreakerSave0 while still exercising the actual UPROPERTY
    // serializer.
    TArray<uint8> Bytes;
    TestTrue(TEXT("Save serializes"), UGameplayStatics::SaveGameToMemory(Written, Bytes));
    UBreakerSaveGame* Read = Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!Read) { AddError(TEXT("Save did not deserialize")); return false; }

    TestEqual(TEXT("Slag survives serialization"), Read->ForgeWallet.Get(EBreakerForgeCurrency::Slag), 42);
    TestEqual(TEXT("Flux survives serialization"), Read->ForgeWallet.Get(EBreakerForgeCurrency::Flux), 7);
    TestEqual(TEXT("Sigil survives serialization"), Read->ForgeWallet.Get(EBreakerForgeCurrency::Sigil), 1);

    // And into the equipment component, the way ABreakerCharacter::LoadGameState
    // restores it.
    AActor* Owner = NewObject<AActor>();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
    Equipment->RestoreForgeWallet(Read->ForgeWallet);
    TestEqual(TEXT("The restored wallet reaches the equipment component"),
        Equipment->GetForgeWallet().Get(EBreakerForgeCurrency::Slag), 42);
    TestEqual(TEXT("Every currency reaches the equipment component"),
        Equipment->GetForgeWallet().Get(EBreakerForgeCurrency::Flux), 7);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSaveWalletMigrationTest,
    "RiorsEdge.Save.WalletMigrationV2ToV3",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSaveWalletMigrationTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("The current version is 3, the wallet's version"), UBreakerSaveGame::CurrentSaveVersion, 3);

    // A faithful v2 payload: version 2, real quest state, and no ForgeWallet
    // property at all — it deserializes to the struct's own
    // default-constructed zero wallet, exactly like QuestCounters did across
    // the v1 -> v2 step.
    UBreakerSaveGame* Old = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Old) { AddError(TEXT("Could not create a save object")); return false; }
    Old->SaveVersion = 2;
    Old->QuestFlags = { TEXT("Quest.FirstContract.Accepted"), TEXT("Quest.FirstContract.Offered") };
    Old->QuestCounters.Add(TEXT("Quest.FirstContract.Kills"), 3);

    FString Note;
    TestTrue(TEXT("A v2 save loads"), UBreakerSaveGame::MigrateToCurrent(*Old, Note));
    TestEqual(TEXT("It is now current"), Old->SaveVersion, UBreakerSaveGame::CurrentSaveVersion);
    // A migration step ran (v2 -> v3), so it reports itself exactly like the
    // v1 -> v2 step's own test expects.
    TestFalse(TEXT("The migration reports itself"), Note.IsEmpty());

    // Everything else survives untouched — the "preserves everything else"
    // half of the requirement.
    TestEqual(TEXT("Flag count is untouched"), Old->QuestFlags.Num(), 2);
    TestTrue(TEXT("Accepted survives"), Old->QuestFlags.Contains(FName(TEXT("Quest.FirstContract.Accepted"))));
    TestTrue(TEXT("Offered survives"), Old->QuestFlags.Contains(FName(TEXT("Quest.FirstContract.Offered"))));
    TestEqual(TEXT("Counter survives"), Old->QuestCounters.FindRef(TEXT("Quest.FirstContract.Kills")), 3);

    // The wallet a v2 file never had comes up empty, not invented.
    TestEqual(TEXT("A migrated v2 file has an empty Slag balance"), Old->ForgeWallet.Get(EBreakerForgeCurrency::Slag), 0);
    TestEqual(TEXT("Flux is empty too"), Old->ForgeWallet.Get(EBreakerForgeCurrency::Flux), 0);
    TestEqual(TEXT("Sigil is empty too"), Old->ForgeWallet.Get(EBreakerForgeCurrency::Sigil), 0);

    // Idempotent, same discipline as RiorsEdge.Save.Migration's own v1 -> v2
    // assertion: migrating an already-current save changes nothing.
    const int32 SlagAfterFirst = Old->ForgeWallet.Get(EBreakerForgeCurrency::Slag);
    FString SecondNote;
    TestTrue(TEXT("A current save loads"), UBreakerSaveGame::MigrateToCurrent(*Old, SecondNote));
    TestTrue(TEXT("A current save reports no migration"), SecondNote.IsEmpty());
    TestEqual(TEXT("A current save's wallet is unchanged"), Old->ForgeWallet.Get(EBreakerForgeCurrency::Slag), SlagAfterFirst);

    // Refuse-to-load still holds at the new ceiling (Save-Architecture 5.2):
    // never repair a file from a newer build, above all not one carrying
    // currency this build does not know how to trust.
    UBreakerSaveGame* Future = Cast<UBreakerSaveGame>(UGameplayStatics::CreateSaveGameObject(UBreakerSaveGame::StaticClass()));
    if (!Future) { AddError(TEXT("Could not create a save object")); return false; }
    Future->SaveVersion = UBreakerSaveGame::CurrentSaveVersion + 1;
    Future->ForgeWallet.Add(EBreakerForgeCurrency::Sigil, 9);
    FString FutureNote;
    TestFalse(TEXT("A save from a build newer than v3 is still refused"), UBreakerSaveGame::MigrateToCurrent(*Future, FutureNote));
    TestFalse(TEXT("The refusal is explained"), FutureNote.IsEmpty());
    TestEqual(TEXT("A refused file is not repaired"), Future->ForgeWallet.Get(EBreakerForgeCurrency::Sigil), 9);
    TestEqual(TEXT("A refused file keeps its own out-of-range version"),
        Future->SaveVersion, UBreakerSaveGame::CurrentSaveVersion + 1);
    return true;
}

#endif
