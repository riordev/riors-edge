#include "Save/BreakerSaveGame.h"

namespace
{
    // MIGRATION LITERALS ARE FROZEN. They deliberately do not reference the
    // named constants in BreakerQuestContent.h: a migration describes what a
    // file written in the PAST contains, and if it followed a constant that
    // gets renamed again the step would quietly stop matching anything.
    const FName V1AcceptedFirstContract(TEXT("Quest.AcceptedFirstContract"));
    const FName V2FirstContractAccepted(TEXT("Quest.FirstContract.Accepted"));
    const FName V2FirstContractOffered(TEXT("Quest.FirstContract.Offered"));
}

bool UBreakerSaveGame::MigrateQuestFlagsV1ToV2(TArray<FName>& Flags)
{
    bool bChanged = false;

    // 1. Rename. v1 spelled the contract flag outside the family it belongs to
    //    (Quest.AcceptedFirstContract vs the Quest.FirstContract.* the quest
    //    object now derives its state from). Without this remap an existing
    //    player who accepted the contract would be offered it again — the save
    //    would load without error and mean the wrong thing, which is the exact
    //    failure an unread version field invites.
    for (FName& Flag : Flags)
    {
        if (Flag == V1AcceptedFirstContract)
        {
            Flag = V2FirstContractAccepted;
            bChanged = true;
        }
    }

    // 2. Backfill the prerequisite. Quest state is DERIVED from flags, and the
    //    derivation reads Offered before Accepted; a v1 save records only the
    //    acceptance. Nobody can accept a contract that was never offered, so
    //    the missing flag is recoverable rather than lost.
    if (Flags.Contains(V2FirstContractAccepted) && !Flags.Contains(V2FirstContractOffered))
    {
        Flags.Add(V2FirstContractOffered);
        bChanged = true;
    }

    // Every other flag is carried through untouched, including any this build
    // does not recognise.
    return bChanged;
}

bool UBreakerSaveGame::MigrateToCurrent(UBreakerSaveGame& Save, FString& OutNote)
{
    OutNote.Reset();

    if (Save.SaveVersion > CurrentSaveVersion)
    {
        OutNote = FString::Printf(
            TEXT("save version %d is newer than this build's %d; this character was saved by a newer version"),
            Save.SaveVersion, CurrentSaveVersion);
        return false;
    }

    // A file written before the field existed deserializes to the default 1,
    // and a hand-edited 0 or negative means the same thing: the oldest shape we
    // know how to read. Clamp rather than refuse — refusing here would strand a
    // player over a number they cannot see.
    if (Save.SaveVersion < 1) Save.SaveVersion = 1;

    int32 Steps = 0;
    while (Save.SaveVersion < CurrentSaveVersion)
    {
        switch (Save.SaveVersion)
        {
        case 1:
            MigrateQuestFlagsV1ToV2(Save.QuestFlags);
            // QuestCounters is additive; an empty map is the correct value for
            // a file written before any objective could be counted.
            break;
        case 2:
            // ForgeWallet is additive, exactly like QuestCounters at the v1 ->
            // v2 step: a v2 file has no such property, so deserialization
            // already left it at the struct's own default-constructed zero
            // wallet, which is exactly correct for a save written before
            // wallet persistence existed. Nothing to transform.
            break;
        case 3:
            // The one-currency consolidation (owner ruling 2026-08-16): a v3
            // file's wallet holds Slag/Flux/Sigil in the legacy Amounts array.
            // Fold them into Riftglass at the conversion stated in
            // CollapseLegacyDenominations (1/6/60 — total value preserved). A
            // v2-or-older file arrives here with an empty legacy array and
            // this is a no-op, exactly as it should be.
            Save.ForgeWallet.CollapseLegacyDenominations();
            break;
        default:
            // Unreachable while every version below CurrentSaveVersion has a
            // step. Left as a hard stop so ADDING a version without adding its
            // migration hangs nothing and loses nothing.
            OutNote = FString::Printf(TEXT("no migration step from save version %d"), Save.SaveVersion);
            return false;
        }
        ++Save.SaveVersion;
        ++Steps;
    }

    if (Steps > 0)
    {
        OutNote = FString::Printf(TEXT("migrated save to version %d in %d step(s)"), Save.SaveVersion, Steps);
    }
    return true;
}
