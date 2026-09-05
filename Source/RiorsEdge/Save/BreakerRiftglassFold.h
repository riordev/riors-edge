#pragma once

#include "CoreMinimal.h"

class UBreakerAccountSave;
class UBreakerSaveGame;

// ---------------------------------------------------------------------------
// THE RIFTGLASS FOLD — Riftglass is account-wide and scalar (O51), and it
// used to live in every character file. Moving it is a migration across TWO
// files with no atomic write between them, and every naive shape has a crash
// window: fold-then-zero duplicates (the un-zeroed wallet folds again), zero-
// then-fold loses (the balance is in neither file), and a version stamp as
// receipt duplicates again (the stamp only lands with the character write).
//
// The exactly-once shape is a JOURNAL IN THE ACCOUNT FILE keyed by the
// character's slot name, which is the identity the loader already knows:
//   1. journal {slot, amount} — one account write, crediting nothing;
//   2. zero the wallet, set the receipt, write the character file;
//   3. credit the account and clear the entry — one account write.
// Every persisted state between those writes replays to the same end state:
// an entry with an unstamped file re-runs 2 and 3, an entry with a stamped
// file runs 3, a stamped file with no entry does nothing. No path counts
// twice or drops a balance. The roster sweep (JournalRoster) runs step 1 for
// every character at once so the account shows the whole sum before each
// alt has been loaded; FoldCharacter journals anything the sweep did not see
// (the legacy single slot, a file written by an older build afterwards).
//
// The writes sit behind IBreakerFoldWriter so the suite can kill the
// process after any one of them and prove the replay, without a slot.
// ---------------------------------------------------------------------------
struct RIORSEDGE_API IBreakerFoldWriter
{
    virtual ~IBreakerFoldWriter() = default;
    virtual bool WriteAccount(const UBreakerAccountSave& Account) = 0;
    virtual bool WriteCharacter(const UBreakerSaveGame& Save, const FString& SlotName) = 0;
};

// The game's writer: the account's own SaveAccount (which honours
// bNeverPersist) and the character's slot.
struct RIORSEDGE_API FBreakerSlotFoldWriter final : public IBreakerFoldWriter
{
    virtual bool WriteAccount(const UBreakerAccountSave& Account) override;
    virtual bool WriteCharacter(const UBreakerSaveGame& Save, const FString& SlotName) override;
};

struct RIORSEDGE_API FBreakerRiftglassFold
{
    // The sweep. Journals every unfolded, non-zero balance among Slots in
    // ONE account write and credits nothing. Loader returns the deserialized
    // (unmigrated) payload for a slot, or null. Returns false only when the
    // write failed; the sweep is re-runnable, and a slot already journaled
    // is not journaled again.
    static bool JournalRoster(UBreakerAccountSave& Account, const TArray<FString>& Slots,
        TFunctionRef<UBreakerSaveGame*(const FString&)> Loader, IBreakerFoldWriter& Writer);

    // Per character, at load, after MigrateToCurrent. Replays the fold to
    // its fixed point from whatever state the two files are in. Returns
    // false when a write failed, in which case the fold stops where it is —
    // which is a state the next load replays from, by construction.
    static bool FoldCharacter(UBreakerAccountSave& Account, UBreakerSaveGame& Save, const FString& SlotName,
        IBreakerFoldWriter& Writer);
};
