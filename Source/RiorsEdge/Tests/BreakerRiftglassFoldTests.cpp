#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "Save/BreakerAccountSave.h"
#include "Save/BreakerRiftglassFold.h"
#include "Save/BreakerSaveGame.h"

// ---------------------------------------------------------------------------
// THE RIFTGLASS FOLD (LEDGER-2, O51): the character file's balance moves to
// the account through a journal, and the process is killed after every one
// of the three writes. "Killed" means the writer refuses the next write and
// the fold returns; "rebooted" means fresh objects are deserialized from the
// bytes the last SUCCESSFUL write left behind, exactly what a slot would
// hold. The suite never touches a slot. The verdict in every case is the
// same: the balance lands in the account exactly once.
// ---------------------------------------------------------------------------

namespace BreakerRiftglassFoldTest
{
    struct FBreakerMemoryFoldWriter final : public IBreakerFoldWriter
    {
        TArray<uint8> AccountBytes;
        TMap<FString, TArray<uint8>> CharacterBytes;
        int32 WritesAllowed = MAX_int32;
        int32 WritesDone = 0;

        // The state "on disk" before the fold starts; not a counted write.
        void Seed(UBreakerAccountSave& Account, UBreakerSaveGame& Save, const FString& SlotName)
        {
            UGameplayStatics::SaveGameToMemory(&Account, AccountBytes);
            UGameplayStatics::SaveGameToMemory(&Save, CharacterBytes.FindOrAdd(SlotName));
        }

        virtual bool WriteAccount(const UBreakerAccountSave& Account) override
        {
            if (WritesDone >= WritesAllowed) return false;
            ++WritesDone;
            return UGameplayStatics::SaveGameToMemory(const_cast<UBreakerAccountSave*>(&Account), AccountBytes);
        }

        virtual bool WriteCharacter(const UBreakerSaveGame& Save, const FString& SlotName) override
        {
            if (WritesDone >= WritesAllowed) return false;
            ++WritesDone;
            return UGameplayStatics::SaveGameToMemory(const_cast<UBreakerSaveGame*>(&Save), CharacterBytes.FindOrAdd(SlotName));
        }

        UBreakerAccountSave* RebootAccount() const
        {
            return Cast<UBreakerAccountSave>(UGameplayStatics::LoadGameFromMemory(AccountBytes));
        }

        UBreakerSaveGame* RebootCharacter(const FString& SlotName) const
        {
            const TArray<uint8>* Bytes = CharacterBytes.Find(SlotName);
            return Bytes ? Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromMemory(*Bytes)) : nullptr;
        }
    };

    static const FString BreakerSlot(TEXT("BreakerChar_TEST"));

    static UBreakerSaveGame* BreakerMakeUnfoldedSave(int32 Balance)
    {
        UBreakerSaveGame* Save = NewObject<UBreakerSaveGame>();
        Save->SaveVersion = UBreakerSaveGame::CurrentSaveVersion;
        Save->ForgeWallet.Riftglass = Balance;
        return Save;
    }

    // Runs the fold up to a crash after WritesAllowed writes, reboots both
    // files, then resumes with an unlimited writer. Returns the rebooted
    // pair AFTER the resume so the test can read the end state.
    struct FBreakerFoldOutcome
    {
        UBreakerAccountSave* Account = nullptr;
        UBreakerSaveGame* Save = nullptr;
        bool bCrashed = false;
        bool bResumed = false;
        int32 ResumeWrites = 0;
    };

    static FBreakerFoldOutcome BreakerCrashThenResume(UBreakerAccountSave& Account, UBreakerSaveGame& Save, int32 WritesAllowed)
    {
        FBreakerMemoryFoldWriter Writer;
        Writer.Seed(Account, Save, BreakerSlot);
        Writer.WritesAllowed = WritesAllowed;
        FBreakerFoldOutcome Outcome;
        Outcome.bCrashed = !FBreakerRiftglassFold::FoldCharacter(Account, Save, BreakerSlot, Writer);

        // The reboot: nothing survives but the bytes.
        UBreakerAccountSave* RebootedAccount = Writer.RebootAccount();
        UBreakerSaveGame* RebootedSave = Writer.RebootCharacter(BreakerSlot);
        if (!RebootedAccount || !RebootedSave) return Outcome;
        FString Note;
        UBreakerSaveGame::MigrateToCurrent(*RebootedSave, Note);

        Writer.WritesAllowed = MAX_int32;
        const int32 Before = Writer.WritesDone;
        Outcome.bResumed = FBreakerRiftglassFold::FoldCharacter(*RebootedAccount, *RebootedSave, BreakerSlot, Writer);
        Outcome.ResumeWrites = Writer.WritesDone - Before;
        Outcome.Account = RebootedAccount;
        Outcome.Save = RebootedSave;
        return Outcome;
    }
}

// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftglassFoldSweepTest,
    "RiorsEdge.Save.RiftglassFold.Sweep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftglassFoldSweepTest::RunTest(const FString& Parameters)
{
    using namespace BreakerRiftglassFoldTest;

    // Three characters: a v3 file whose balance is still the legacy array
    // (journaled at the stated 1/6/60 conversion), a stamped file whose
    // stale wallet number must NOT be counted, and a zero balance that has
    // nothing to journal.
    UBreakerSaveGame* Legacy = NewObject<UBreakerSaveGame>();
    Legacy->SaveVersion = 3;
    Legacy->ForgeWallet.Amounts = { 42, 7, 1 };
    UBreakerSaveGame* Stamped = BreakerMakeUnfoldedSave(999);
    Stamped->bRiftglassFoldedToAccount = true;
    UBreakerSaveGame* Empty = BreakerMakeUnfoldedSave(0);

    TMap<FString, UBreakerSaveGame*> Files;
    Files.Add(TEXT("A"), Legacy);
    Files.Add(TEXT("B"), Stamped);
    Files.Add(TEXT("C"), Empty);
    TArray<FString> Slots = { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("Missing") };
    auto Loader = [&Files](const FString& Slot) -> UBreakerSaveGame*
    {
        UBreakerSaveGame* const* Found = Files.Find(Slot);
        return Found ? *Found : nullptr;
    };

    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    FBreakerMemoryFoldWriter Writer;

    TestTrue(TEXT("the sweep succeeds"), FBreakerRiftglassFold::JournalRoster(*Account, Slots, Loader, Writer));
    TestEqual(TEXT("one write for the whole roster"), Writer.WritesDone, 1);
    TestEqual(TEXT("exactly the one unfolded, non-zero balance is journaled"), Account->PendingRiftglassFolds.Num(), 1);
    if (Account->PendingRiftglassFolds.Num() == 1)
    {
        TestEqual(TEXT("it is the legacy character"), Account->PendingRiftglassFolds[0].SlotName, FString(TEXT("A")));
        TestEqual(TEXT("at the v3 conversion"), Account->PendingRiftglassFolds[0].Amount, 42 + 7 * 6 + 60);
    }
    TestEqual(TEXT("the sweep credits nothing"), Account->Riftglass, 0);

    // A re-run (the crash-before-the-version-stamp case) journals nothing
    // twice and writes nothing.
    TestTrue(TEXT("the sweep re-runs cleanly"), FBreakerRiftglassFold::JournalRoster(*Account, Slots, Loader, Writer));
    TestEqual(TEXT("no second write"), Writer.WritesDone, 1);
    TestEqual(TEXT("still one entry"), Account->PendingRiftglassFolds.Num(), 1);
    return true;
}

// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftglassFoldCrashAfterJournalTest,
    "RiorsEdge.Save.RiftglassFold.CrashAfterJournal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftglassFoldCrashAfterJournalTest::RunTest(const FString& Parameters)
{
    using namespace BreakerRiftglassFoldTest;
    // Window 1 — the fold-then-zero duplication: the journal is written, the
    // character file is not. An account already holding another character's
    // balance proves the credit ADDS rather than replaces.
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    Account->Riftglass = 100;
    UBreakerSaveGame* Save = BreakerMakeUnfoldedSave(50);

    const FBreakerFoldOutcome Outcome = BreakerCrashThenResume(*Account, *Save, 1);
    TestTrue(TEXT("the fold stopped at the crash"), Outcome.bCrashed);
    if (!Outcome.Account || !Outcome.Save) { AddError(TEXT("the reboot did not deserialize both files")); return false; }
    TestTrue(TEXT("the resume completes"), Outcome.bResumed);
    TestEqual(TEXT("the balance lands once, on top of what was there"), Outcome.Account->Riftglass, 150);
    TestEqual(TEXT("the journal is empty"), Outcome.Account->PendingRiftglassFolds.Num(), 0);
    TestTrue(TEXT("the file is stamped"), Outcome.Save->bRiftglassFoldedToAccount);
    TestEqual(TEXT("the file carries zero"), Outcome.Save->ForgeWallet.Get(), 0);
    TestEqual(TEXT("the resume needed the character write and the credit"), Outcome.ResumeWrites, 2);
    return true;
}

// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftglassFoldCrashAfterCharacterWriteTest,
    "RiorsEdge.Save.RiftglassFold.CrashAfterCharacterWrite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftglassFoldCrashAfterCharacterWriteTest::RunTest(const FString& Parameters)
{
    using namespace BreakerRiftglassFoldTest;
    // Window 2 — the zero-then-fold loss: the file is zeroed and stamped,
    // the account is not yet credited. On disk the balance is in NEITHER
    // file; only the journal knows it.
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    UBreakerSaveGame* Save = BreakerMakeUnfoldedSave(50);

    FBreakerMemoryFoldWriter Probe;
    Probe.Seed(*Account, *Save, BreakerSlot);
    Probe.WritesAllowed = 2;
    TestFalse(TEXT("the fold stops after the character write"), FBreakerRiftglassFold::FoldCharacter(*Account, *Save, BreakerSlot, Probe));
    if (UBreakerAccountSave* Disk = Probe.RebootAccount())
    {
        TestEqual(TEXT("on disk the account is uncredited"), Disk->Riftglass, 0);
        TestEqual(TEXT("on disk the journal holds the balance"), Disk->PendingRiftglassFolds.Num(), 1);
    }
    if (UBreakerSaveGame* Disk = Probe.RebootCharacter(BreakerSlot))
    {
        TestTrue(TEXT("on disk the file is stamped"), Disk->bRiftglassFoldedToAccount);
        TestEqual(TEXT("on disk the file is zero"), Disk->ForgeWallet.Get(), 0);
    }

    // The same window, through the reboot-and-resume rig.
    UBreakerAccountSave* Account2 = NewObject<UBreakerAccountSave>();
    Account2->bNeverPersist = true;
    UBreakerSaveGame* Save2 = BreakerMakeUnfoldedSave(50);
    const FBreakerFoldOutcome Outcome = BreakerCrashThenResume(*Account2, *Save2, 2);
    TestTrue(TEXT("the fold stopped at the crash"), Outcome.bCrashed);
    if (!Outcome.Account || !Outcome.Save) { AddError(TEXT("the reboot did not deserialize both files")); return false; }
    TestTrue(TEXT("the resume completes"), Outcome.bResumed);
    TestEqual(TEXT("the journaled balance is recovered, once"), Outcome.Account->Riftglass, 50);
    TestEqual(TEXT("the journal is empty"), Outcome.Account->PendingRiftglassFolds.Num(), 0);
    TestEqual(TEXT("the resume needed only the credit"), Outcome.ResumeWrites, 1);
    return true;
}

// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftglassFoldCrashAfterCreditTest,
    "RiorsEdge.Save.RiftglassFold.CrashAfterCredit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftglassFoldCrashAfterCreditTest::RunTest(const FString& Parameters)
{
    using namespace BreakerRiftglassFoldTest;
    // Window 3 — the receipt: the fold completed, and a later load must not
    // credit again. A stamped file with an empty journal is the receipt,
    // and it is the CHARACTER file's field, not its version number.
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    UBreakerSaveGame* Save = BreakerMakeUnfoldedSave(50);

    const FBreakerFoldOutcome Outcome = BreakerCrashThenResume(*Account, *Save, 3);
    TestFalse(TEXT("three writes complete the fold; nothing crashed"), Outcome.bCrashed);
    if (!Outcome.Account || !Outcome.Save) { AddError(TEXT("the reboot did not deserialize both files")); return false; }
    TestTrue(TEXT("the second load is a no-op that succeeds"), Outcome.bResumed);
    TestEqual(TEXT("the balance is credited once"), Outcome.Account->Riftglass, 50);
    TestEqual(TEXT("the second load writes nothing"), Outcome.ResumeWrites, 0);

    // A THIRD load, same verdict — idempotent from the fixed point.
    FBreakerMemoryFoldWriter Again;
    TestTrue(TEXT("a third load succeeds"), FBreakerRiftglassFold::FoldCharacter(*Outcome.Account, *Outcome.Save, BreakerSlot, Again));
    TestEqual(TEXT("and still writes nothing"), Again.WritesDone, 0);
    TestEqual(TEXT("and credits nothing"), Outcome.Account->Riftglass, 50);

    // A v7-shaped file with nothing to fold: stamped in one character write,
    // no journal entry, no credit.
    UBreakerSaveGame* Zero = BreakerMakeUnfoldedSave(0);
    FBreakerMemoryFoldWriter ZeroWriter;
    TestTrue(TEXT("a zero balance folds"), FBreakerRiftglassFold::FoldCharacter(*Outcome.Account, *Zero, TEXT("BreakerChar_ZERO"), ZeroWriter));
    TestTrue(TEXT("it is stamped"), Zero->bRiftglassFoldedToAccount);
    TestEqual(TEXT("with one character write and no account write"), ZeroWriter.WritesDone, 1);
    TestEqual(TEXT("the journal stays empty"), Outcome.Account->PendingRiftglassFolds.Num(), 0);
    TestEqual(TEXT("the account is unchanged"), Outcome.Account->Riftglass, 50);
    return true;
}

// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftglassFoldMixedBuildTest,
    "RiorsEdge.Save.RiftglassFold.MixedBuildRejournals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftglassFoldMixedBuildTest::RunTest(const FString& Parameters)
{
    using namespace BreakerRiftglassFoldTest;
    // The sweep journaled 50, then the character was played on an older
    // build and its file now holds 80, still unstamped. The FILE is the
    // truth: the entry is re-journaled to 80 before anything is zeroed, and
    // a crash after that re-journal resumes to the same 80.
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    FBreakerRiftglassFoldEntry Stale;
    Stale.SlotName = BreakerSlot;
    Stale.Amount = 50;
    Account->PendingRiftglassFolds.Add(Stale);
    UBreakerSaveGame* Save = BreakerMakeUnfoldedSave(80);

    const FBreakerFoldOutcome Outcome = BreakerCrashThenResume(*Account, *Save, 1);
    TestTrue(TEXT("the fold stopped after the re-journal"), Outcome.bCrashed);
    if (!Outcome.Account || !Outcome.Save) { AddError(TEXT("the reboot did not deserialize both files")); return false; }
    TestTrue(TEXT("the resume completes"), Outcome.bResumed);
    TestEqual(TEXT("the file's balance wins, once"), Outcome.Account->Riftglass, 80);
    TestEqual(TEXT("the journal is empty"), Outcome.Account->PendingRiftglassFolds.Num(), 0);
    TestTrue(TEXT("the file is stamped"), Outcome.Save->bRiftglassFoldedToAccount);
    return true;
}

// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftglassFoldShippedConfigurationTest,
    "RiorsEdge.Save.RiftglassFold.ShippedConfiguration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftglassFoldShippedConfigurationTest::RunTest(const FString& Parameters)
{
    // The default-constructed shapes are what an OLD file deserializes to,
    // and both must read as "not yet folded": a save from before the field
    // is unstamped with no id, and an account from before the sweep holds
    // nothing. Anything else would fold a file that was never folded, or
    // skip one that was.
    UBreakerSaveGame* Save = NewObject<UBreakerSaveGame>();
    TestEqual(TEXT("a default save is the oldest version"), Save->SaveVersion, 1);
    TestFalse(TEXT("a default save is unfolded"), Save->bRiftglassFoldedToAccount);
    TestFalse(TEXT("a default save carries no id"), Save->CharacterId.IsValid());
    TestEqual(TEXT("the head version carries the receipt"), UBreakerSaveGame::CurrentSaveVersion, 8);

    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    TestEqual(TEXT("a fresh account is at the swept version"), Account->AccountVersion, 2);
    TestEqual(TEXT("a fresh account holds nothing"), Account->Riftglass, 0);
    TestEqual(TEXT("a fresh account has an empty journal"), Account->PendingRiftglassFolds.Num(), 0);

    // The receipt and the id survive the real serializer.
    Save->SaveVersion = UBreakerSaveGame::CurrentSaveVersion;
    Save->bRiftglassFoldedToAccount = true;
    Save->CharacterId = FGuid::NewGuid();
    TArray<uint8> Bytes;
    TestTrue(TEXT("the save serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
    UBreakerSaveGame* Read = Cast<UBreakerSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!Read) { AddError(TEXT("the save did not deserialize")); return false; }
    TestTrue(TEXT("the receipt survives"), Read->bRiftglassFoldedToAccount);
    TestEqual(TEXT("the id survives"), Read->CharacterId, Save->CharacterId);

    Account->Riftglass = 7;
    FBreakerRiftglassFoldEntry Entry;
    Entry.SlotName = TEXT("BreakerChar_X");
    Entry.Amount = 3;
    Account->PendingRiftglassFolds.Add(Entry);
    TArray<uint8> AccountBytes;
    TestTrue(TEXT("the account serializes"), UGameplayStatics::SaveGameToMemory(Account, AccountBytes));
    UBreakerAccountSave* ReadAccount = Cast<UBreakerAccountSave>(UGameplayStatics::LoadGameFromMemory(AccountBytes));
    if (!ReadAccount) { AddError(TEXT("the account did not deserialize")); return false; }
    TestEqual(TEXT("the balance survives"), ReadAccount->Riftglass, 7);
    TestEqual(TEXT("the journal survives"), ReadAccount->PendingRiftglassFolds.Num(), 1);
    return true;
}

#endif
