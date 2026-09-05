#include "Save/BreakerAccountSave.h"

#include "Kismet/GameplayStatics.h"
#include "Save/BreakerCharacterRoster.h"
#include "Save/BreakerRiftglassFold.h"
#include "Save/BreakerSaveGame.h"

UBreakerAccountSave* UBreakerAccountSave::CachedAccount = nullptr;

namespace
{
    // THE ROSTER SWEEP, once per account: every character's unfolded balance
    // is journaled in one write so the account shows the whole sum before
    // each alt has been loaded. Runs for a fresh account (characters may
    // predate the account file) and for a version-1 file; version 2 is the
    // receipt. Reads whole character files, which the roster header forbids
    // for LISTING — this is a one-time migration, not the select screen.
    void BreakerSweepRosterIntoFoldJournal(UBreakerAccountSave& Account)
    {
        TArray<FString> Slots;
        if (const UBreakerCharacterRoster* Roster = UBreakerCharacterRoster::LoadOrCreate())
        {
            for (const FBreakerCharacterSummary& Summary : Roster->Characters)
            {
                Slots.Add(UBreakerCharacterRoster::SlotNameForCharacter(Summary.CharacterId));
            }
        }
        FBreakerSlotFoldWriter Writer;
        const bool bJournaled = FBreakerRiftglassFold::JournalRoster(Account, Slots,
            [](const FString& SlotName) -> UBreakerSaveGame*
            {
                if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return nullptr;
                return Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
            }, Writer);
        // The version stamps only after the journal write; a crash between
        // the two re-runs the sweep, which journals nothing twice. A fresh
        // account that journaled nothing is not written — its first write is
        // the first save, like before.
        if (bJournaled)
        {
            const bool bWasFile = Account.AccountVersion < UBreakerAccountSave::CurrentAccountVersion;
            Account.AccountVersion = UBreakerAccountSave::CurrentAccountVersion;
            if (bWasFile || Account.PendingRiftglassFolds.Num() > 0) Account.SaveAccount();
        }
    }
}

UBreakerAccountSave* UBreakerAccountSave::LoadOrCreate()
{
    if (CachedAccount) return CachedAccount;
    if (UGameplayStatics::DoesSaveGameExist(AccountSlotName(), 0))
    {
        if (UBreakerAccountSave* Loaded = Cast<UBreakerAccountSave>(
            UGameplayStatics::LoadGameFromSlot(AccountSlotName(), 0)))
        {
            // The roster's own rule, applied to the account: a file from a
            // newer build is refused, not repaired.
            if (Loaded->AccountVersion > CurrentAccountVersion)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("Account save is version %d but this build understands %d. Refusing to load it; ")
                    TEXT("the file has NOT been modified."), Loaded->AccountVersion, CurrentAccountVersion);
                return nullptr;
            }
            Loaded->AddToRoot();
            CachedAccount = Loaded;
            if (Loaded->AccountVersion < 2) BreakerSweepRosterIntoFoldJournal(*Loaded);
            return CachedAccount;
        }
    }
    UBreakerAccountSave* Fresh = Cast<UBreakerAccountSave>(
        UGameplayStatics::CreateSaveGameObject(UBreakerAccountSave::StaticClass()));
    if (Fresh) Fresh->AddToRoot();
    CachedAccount = Fresh;
    if (Fresh) BreakerSweepRosterIntoFoldJournal(*Fresh);
    return CachedAccount;
}

void UBreakerAccountSave::InjectForTesting(UBreakerAccountSave* Account)
{
    ResetCacheForTesting();
    if (Account)
    {
        Account->AddToRoot();
        CachedAccount = Account;
    }
}

void UBreakerAccountSave::ResetCacheForTesting()
{
    if (CachedAccount) CachedAccount->RemoveFromRoot();
    CachedAccount = nullptr;
}

bool UBreakerAccountSave::SaveAccount() const
{
    if (bNeverPersist) return true;
    return UGameplayStatics::SaveGameToSlot(const_cast<UBreakerAccountSave*>(this), AccountSlotName(), 0);
}
