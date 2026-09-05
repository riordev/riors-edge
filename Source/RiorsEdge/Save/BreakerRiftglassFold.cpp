#include "Save/BreakerRiftglassFold.h"

#include "Kismet/GameplayStatics.h"
#include "Save/BreakerAccountSave.h"
#include "Save/BreakerSaveGame.h"

bool FBreakerSlotFoldWriter::WriteAccount(const UBreakerAccountSave& Account)
{
    return Account.SaveAccount();
}

bool FBreakerSlotFoldWriter::WriteCharacter(const UBreakerSaveGame& Save, const FString& SlotName)
{
    return UGameplayStatics::SaveGameToSlot(const_cast<UBreakerSaveGame*>(&Save), SlotName, 0);
}

namespace
{
    FBreakerRiftglassFoldEntry* BreakerFindFoldEntry(UBreakerAccountSave& Account, const FString& SlotName)
    {
        return Account.PendingRiftglassFolds.FindByPredicate(
            [&SlotName](const FBreakerRiftglassFoldEntry& Entry) { return Entry.SlotName == SlotName; });
    }
}

bool FBreakerRiftglassFold::JournalRoster(UBreakerAccountSave& Account, const TArray<FString>& Slots,
    TFunctionRef<UBreakerSaveGame*(const FString&)> Loader, IBreakerFoldWriter& Writer)
{
    bool bAdded = false;
    for (const FString& SlotName : Slots)
    {
        if (BreakerFindFoldEntry(Account, SlotName)) continue;
        UBreakerSaveGame* Save = Loader(SlotName);
        if (!Save) continue;
        // The balance is read AFTER the in-memory migration so a v3 file's
        // legacy Slag/Flux/Sigil array is counted at its stated conversion.
        // A file from a newer build is refused here as everywhere; it is
        // not journaled and not touched.
        FString Note;
        if (!UBreakerSaveGame::MigrateToCurrent(*Save, Note)) continue;
        if (Save->bRiftglassFoldedToAccount || Save->ForgeWallet.Get() == 0) continue;
        FBreakerRiftglassFoldEntry Entry;
        Entry.SlotName = SlotName;
        Entry.Amount = Save->ForgeWallet.Get();
        Account.PendingRiftglassFolds.Add(Entry);
        bAdded = true;
    }
    // ONE write, crediting nothing: a crash before it re-runs the sweep and a
    // crash after it leaves every entry for FoldCharacter to settle.
    return !bAdded || Writer.WriteAccount(Account);
}

bool FBreakerRiftglassFold::FoldCharacter(UBreakerAccountSave& Account, UBreakerSaveGame& Save, const FString& SlotName,
    IBreakerFoldWriter& Writer)
{
    FBreakerRiftglassFoldEntry* Entry = BreakerFindFoldEntry(Account, SlotName);

    if (!Save.bRiftglassFoldedToAccount)
    {
        const int32 FileBalance = Save.ForgeWallet.Get();
        // STEP 1 — the journal is the commit point and it is written FIRST.
        // No entry: this file was not swept (the legacy slot, or a character
        // created on an older build after the sweep). An entry whose amount
        // disagrees with an UNSTAMPED file: the file was played on an older
        // build after the sweep and the file is the truth, so re-journal.
        // A stamped file never reaches here, so a disagreement can never
        // mean "already zeroed".
        if ((!Entry && FileBalance != 0) || (Entry && Entry->Amount != FileBalance))
        {
            if (!Entry)
            {
                FBreakerRiftglassFoldEntry Fresh;
                Fresh.SlotName = SlotName;
                Account.PendingRiftglassFolds.Add(Fresh);
                Entry = BreakerFindFoldEntry(Account, SlotName);
            }
            Entry->Amount = FileBalance;
            if (!Writer.WriteAccount(Account)) return false;
        }

        // STEP 2 — zero and stamp, then the character write. A crash after
        // this write leaves an entry against a stamped file, which is
        // exactly step 3's precondition. The legacy array is cleared too:
        // it was already folded into the balance by the v3 -> v4 step.
        Save.ForgeWallet.Riftglass = 0;
        Save.ForgeWallet.Amounts.Reset();
        Save.bRiftglassFoldedToAccount = true;
        if (!Writer.WriteCharacter(Save, SlotName)) return false;
    }

    // STEP 3 — credit and clear in one account write. A stamped file with no
    // entry has already been settled; a crash before this write replays to
    // here and nowhere earlier, because the stamp is on disk.
    if (Entry)
    {
        Account.Riftglass += Entry->Amount;
        Account.PendingRiftglassFolds.RemoveAll(
            [&SlotName](const FBreakerRiftglassFoldEntry& Existing) { return Existing.SlotName == SlotName; });
        return Writer.WriteAccount(Account);
    }
    return true;
}
